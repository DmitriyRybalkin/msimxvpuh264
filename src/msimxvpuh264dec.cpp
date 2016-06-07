
#include <mediastreamer2/msticker.h>
#include <ortp/b64.h>

#include "msimxvpuh264dec.h"

unsigned int MSIMXVPUH264Decoder::ctx_height = 0;
unsigned int MSIMXVPUH264Decoder::ctx_width = 0;

MSIMXVPUH264Decoder::MSIMXVPUH264Decoder(MSFilter *f) : mFilter(f), mWidth(MS_VIDEO_SIZE_UNKNOWN_W), mHeight(MS_VIDEO_SIZE_UNKNOWN_H), mLastErrorReportTime(0), mBitstream(0), mBitstreamSize(65536), mInitialized(false), mFirstImageDecoded(false)
{
	mBitstream = static_cast<uint8_t *>(ms_malloc0(mBitstreamSize));
}

MSIMXVPUH264Decoder::~MSIMXVPUH264Decoder()
{
	if (mBitstream != 0) {
		ms_free(mBitstream);
	}
	if(ctx)
	  free(ctx);
}

void MSIMXVPUH264Decoder::loggingFn(ImxVpuLogLevel level, char const *file, int const line, char const *fn, const char *format, ...)
{
	va_list args;

	char const *lvlstr = "";
	switch (level)
	{
		case IMX_VPU_LOG_LEVEL_ERROR: lvlstr = "ERROR"; break;
		case IMX_VPU_LOG_LEVEL_WARNING: lvlstr = "WARNING"; break;
		case IMX_VPU_LOG_LEVEL_INFO: lvlstr = "info"; break;
		case IMX_VPU_LOG_LEVEL_DEBUG: lvlstr = "debug"; break;
		case IMX_VPU_LOG_LEVEL_TRACE: lvlstr = "trace"; break;
		case IMX_VPU_LOG_LEVEL_LOG: lvlstr = "log"; break;
		default: break;
	}

	fprintf(stderr, "%s:%d (%s)   %s: ", file, line, fn, lvlstr);

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	fprintf(stderr, "\n");
}

void MSIMXVPUH264Decoder::initialize()
{
	if(!mInitialized) {
		mUnpacker = rfc3984_new();
		
		imx_vpu_set_logging_threshold(IMX_VPU_LOG_LEVEL_ERROR);
		imx_vpu_set_logging_function(loggingFn);
		  
		ctx = NULL;
		
		ctx = (MSIMXVPUH264Decoder::Context*)calloc(1, sizeof(Context));
		ctx->frame_id_counter = 100;
		
		/* Set the open params. Enable frame reordering, use h.264 as the codec format.
		* The memset() call ensures the other values are set to their default. */
		memset(&(ctx->open_params), 0, sizeof(ImxVpuDecOpenParams));
		ctx->open_params.codec_format = IMX_VPU_CODEC_FORMAT_H264;
		ctx->open_params.enable_frame_reordering = 1;
		
		/* Load the VPU firmware */
		imx_vpu_dec_load();
		
		/* Retrieve information about the required bitstream buffer and allocate one based on this */
		imx_vpu_dec_get_bitstream_buffer_info(&(ctx->bitstream_buffer_size), &(ctx->bitstream_buffer_alignment));
		ctx->bitstream_buffer = imx_vpu_dma_buffer_allocate(imx_vpu_dec_get_default_allocator(), ctx->bitstream_buffer_size, ctx->bitstream_buffer_alignment, 0);
		
		/* Open a decoder instance, using the previously allocated bitstream buffer */
		imx_vpu_dec_open(&(ctx->vpudec), &(ctx->open_params), ctx->bitstream_buffer, initialInfoCallback, ctx);
		ms_average_fps_init(&mFPS, "iMX VPU decoder: FPS=%f");
		mInitialized = true;
	}
}

int MSIMXVPUH264Decoder::initialInfoCallback(ImxVpuDecoder *decoder, ImxVpuDecInitialInfo *new_initial_info, unsigned int output_code, void *user_data)
{
	unsigned int i;
	Context *ctx = (Context *)user_data;

	((void)(decoder));
	((void)(output_code));


	/* Keep a copy of the initial information around */
	ctx->initial_info = *new_initial_info;

#if MS_IMX_VPU_DECODER_DEBUG
	ms_message(
		"initial info:  size: %ux%u pixel  rate: %u/%u  min num required framebuffers: %u  interlacing: %d  framebuffer alignment: %u",
		ctx->initial_info.frame_width,
		ctx->initial_info.frame_height,
		ctx->initial_info.frame_rate_numerator,
		ctx->initial_info.frame_rate_denominator,
		ctx->initial_info.min_num_required_framebuffers,
		ctx->initial_info.interlacing,
		ctx->initial_info.framebuffer_alignment
	);
#endif
	ctx->num_framebuffers = ctx->initial_info.min_num_required_framebuffers;

	/* Using the initial information, calculate appropriate framebuffer sizes */
	imx_vpu_calc_framebuffer_sizes(
		ctx->initial_info.color_format,
		ctx->initial_info.frame_width,
		ctx->initial_info.frame_height,
		ctx->initial_info.framebuffer_alignment,
		ctx->initial_info.interlacing,
		0,
		&(ctx->calculated_sizes)
	);
#if MS_IMX_VPU_DECODER_DEBUG
	ms_message(
		"calculated sizes:  frame width&height: %dx%d  Y stride: %u  CbCr stride: %u  Y size: %u  CbCr size: %u  MvCol size: %u  total size: %u",
		ctx->calculated_sizes.aligned_frame_width, ctx->calculated_sizes.aligned_frame_height,
		ctx->calculated_sizes.y_stride, ctx->calculated_sizes.cbcr_stride,
		ctx->calculated_sizes.y_size, ctx->calculated_sizes.cbcr_size, ctx->calculated_sizes.mvcol_size,
		ctx->calculated_sizes.total_size
	);
#endif

	/* If any framebuffers were allocated previously, deallocate them now.
	 * This can happen when video sequence parameters change, for example. */

	if (ctx->framebuffers != NULL)
		free(ctx->framebuffers);

	if (ctx->fb_dmabuffers != NULL)
	{
		for (i = 0; i < ctx->num_framebuffers; ++i)
			imx_vpu_dma_buffer_deallocate(ctx->fb_dmabuffers[i]);
		free(ctx->fb_dmabuffers);
	}


	/* Allocate memory blocks for the framebuffer and DMA buffer structures,
	 * and allocate the DMA buffers themselves */

	ctx->framebuffers = (ImxVpuFramebuffer*)malloc(sizeof(ImxVpuFramebuffer) * ctx->num_framebuffers);
	ctx->fb_dmabuffers = (ImxVpuDMABuffer**)malloc(sizeof(ImxVpuDMABuffer*) * ctx->num_framebuffers);

	for (i = 0; i < ctx->num_framebuffers; ++i)
	{
		/* Allocate a DMA buffer for each framebuffer. It is possible to specify alternate allocators;
		 * all that is required is that the allocator provides physically contiguous memory
		 * (necessary for DMA transfers) and respecs the alignment value. */
		ctx->fb_dmabuffers[i] = imx_vpu_dma_buffer_allocate(
			imx_vpu_dec_get_default_allocator(),
			ctx->calculated_sizes.total_size,
			ctx->initial_info.framebuffer_alignment,
			0
		);

		/* The last parameter (the one with 0x2000 + i) is the context data for the framebuffers in the pool.
		 * It is possible to attach user-defined context data to them. Note that it is not related to the
		 * context data in en- and decoded frames. For purposes of demonstrations, the context pointer
		 * is just a simple monotonically increasing integer. First framebuffer has context 0x2000, second 0x2001 etc. */
		imx_vpu_fill_framebuffer_params(
			&(ctx->framebuffers[i]),
			&(ctx->calculated_sizes),
			ctx->fb_dmabuffers[i],
			(void*)((uintptr_t)(0x2000 + i))
		);
	}


	/* Actual registration is done here. From this moment on, the VPU knows which buffers to use for
	 * storing decoded raw frames into. This call must not be done again until decoding is shut down or
	 * IMX_VPU_DEC_OUTPUT_CODE_INITIAL_INFO_AVAILABLE is set again. */
	imx_vpu_dec_register_framebuffers(ctx->vpudec, ctx->framebuffers, ctx->num_framebuffers);
	ctx_height = ctx->initial_info.frame_height;
	ctx_width = ctx->initial_info.frame_width;

	return 1;
}

void MSIMXVPUH264Decoder::process()
{
	MSQueue nalus;
	ms_queue_init(&nalus);
	
	ImxVpuEncodedFrame encoded_frame;
	unsigned int output_code;
	ImxVpuDecReturnCodes ret;
	
	mblk_t *im;
	
	if (imx_vpu_dec_is_drain_mode_enabled(ctx->vpudec))
	{
		/* In drain mode there is no input data */
		encoded_frame.data = NULL;
		encoded_frame.data_size = 0;
		encoded_frame.context = NULL;

		imx_vpu_dec_set_codec_data(ctx->vpudec, NULL, 0);

	} else {
		while ((im = ms_queue_get(mFilter->inputs[0])) != NULL) {
		
			if((mSPS != 0) && (mPPS != 0)) {
				/* Push the sps/pps given in sprop-parameter sets if any */
				mblk_set_timestamp_info(mSPS, mblk_get_timestamp_info(im));
				mblk_set_timestamp_info(mPPS, mblk_get_timestamp_info(im));
				rfc3984_unpack(mUnpacker, mSPS, &nalus);
				rfc3984_unpack(mUnpacker, mPPS, &nalus);
				mSPS = 0;
				mPPS = 0;
			}
			rfc3984_unpack(mUnpacker, im, &nalus);
			if(!ms_queue_empty(&nalus)) {
			  
				this->nalusToFrame(&nalus);
				if(mBitstream != 0) {
					encoded_frame.data = mBitstream;
					encoded_frame.data_size = mBitstreamSize;
				} else {
					ms_warning("iMX VPU decoder: bitstream is empty !");
					break;
				}
				
				/* Codec data is out-of-band data that is typically stored in a separate space in
				* containers for each elementary stream; h.264 byte-stream does not need it */
				imx_vpu_dec_set_codec_data(ctx->vpudec, NULL, 0);
		
				/* The frame id counter is used to give the encoded frames an example context.
				* The context of an encoded frame is a user-defined pointer that is passed along
				* to the corresponding decoded raw frame. This way, it can be determined which
				* decoded raw frame is the result of what encoded frame.
				* For example purposes (to be able to print some log output), the context
				* is just a monotonically increasing integer. */
				encoded_frame.context = (void *)((uintptr_t)(ctx->frame_id_counter));
		
				encoded_frame.pts = 0;
				encoded_frame.dts = 0;
				
				if ((ret = imx_vpu_dec_decode(ctx->vpudec, &encoded_frame, &output_code)) != IMX_VPU_DEC_RETURN_CODE_OK)
				{
					ms_warning("imx_vpu_dec_decode() failed: %s", imx_vpu_dec_error_string(ret));
					if (((mFilter->ticker->time - mLastErrorReportTime) > 5000) || (mLastErrorReportTime == 0)) {
						mLastErrorReportTime = mFilter->ticker->time;
						ms_filter_notify_no_arg(mFilter, MS_VIDEO_DECODER_DECODING_ERRORS);
					}
				}
				
				if((output_code & IMX_VPU_DEC_OUTPUT_CODE_VIDEO_PARAMS_CHANGED) || (msgdsize(im) == 0))
				{
					/* Video sequence parameters changed. Decoding cannot continue with the
					* existing decoder. Drain it, and open a new one to resume decoding. */
#if MS_IMX_VPU_DECODER_DEBUG
					ms_message("imx_vpu_dec = drain_mode: IMX_VPU_DEC_OUTPUT_CODE_VIDEO_PARAMS_CHANGED");
#endif
					imx_vpu_dec_enable_drain_mode(ctx->vpudec, 1);

					imx_vpu_dec_enable_drain_mode(ctx->vpudec, 0);

					rfc3984_uninit(mUnpacker);
					rfc3984_init(mUnpacker);
					
					imx_vpu_dec_close(ctx->vpudec);

					imx_vpu_dec_open(&(ctx->vpudec), &(ctx->open_params), ctx->bitstream_buffer, initialInfoCallback, ctx);
					
					freemsg(im);
					/* Feed the data that caused the IMX_VPU_DEC_OUTPUT_CODE_VIDEO_PARAMS_CHANGED
					* output code flag again, but this time into the new decoder */
					/*if ((ret = imx_vpu_dec_decode(ctx->vpudec, &encoded_frame, &output_code)) != IMX_VPU_DEC_RETURN_CODE_OK)
					{
						ms_warning("imx_vpu_dec_decode() failed: %s\n", imx_vpu_dec_error_string(ret));
						break;
					}*/
				}
				
				if(output_code & IMX_VPU_DEC_OUTPUT_CODE_DECODED_FRAME_AVAILABLE)
				{
#if MS_IMX_VPU_DECODER_DEBUG
					 ms_message("imx_vpu_dec = drain_mode: IMX_VPU_DEC_OUTPUT_CODE_DECODED_FRAME_AVAILABLE");
#endif
					 /* A decoded raw frame is available for further processing. Retrieve it, do something
					 * with it, and once the raw frame is no longer needed, mark it as displayed. This
					 * marks it internally as available for further decoding by the VPU. */

					 ImxVpuRawFrame decoded_frame;
					 unsigned int frame_id;
					 //uint8_t *mapped_virtual_address;
					 //size_t num_out_byte = ctx->calculated_sizes.y_size + ctx->calculated_sizes.cbcr_size * 2;
					 
					 /* This call retrieves information about the decoded raw frame, including
					 * a pointer to the corresponding framebuffer structure. This must not be called more
					 * than once after IMX_VPU_DEC_OUTPUT_CODE_DECODED_FRAME_AVAILABLE was set. */
					 imx_vpu_dec_get_decoded_frame(ctx->vpudec, &decoded_frame);
					 frame_id = (unsigned int)((uintptr_t)(decoded_frame.context));
		
					 /* Map buffer to the local address space, dump the decoded frame to file,
					  * and unmap again. The decoded frame uses the I420 color format for all
					  * bitstream formats (h.264, MPEG2 etc.), with one exception; with motion JPEG data,
					  * the format can be different. See imxvpuapi.h for details. */
					  //mapped_virtual_address = imx_vpu_dma_buffer_map(decoded_frame.framebuffer->dma_buffer, IMX_VPU_MAPPING_FLAG_READ);
					  if((mWidth != ctx->initial_info.frame_width) || (mHeight != ctx->initial_info.frame_height))
					  {
						if (mYUVMsg) {
							freemsg(mYUVMsg);
						}
						
						mWidth = ctx_width;
						mHeight = ctx_height;
						mYUVMsg = ms_yuv_buf_alloc(&mOutbuf, mWidth, mHeight);
						ms_filter_notify_no_arg(mFilter, MS_FILTER_OUTPUT_FMT_CHANGED);
					  }
					  
					  imx_vpu_phys_addr_t phys_addr;
					  phys_addr = imx_vpu_dma_buffer_get_physical_address(decoded_frame.framebuffer->dma_buffer);
					  
					  uint8_t *pbufYUV[3] = { 0 };
					  unsigned int strideYUV[3];
					  
					  /* Stride sizes, in bytes, with alignment applied. The Cb and Cr planes always
					   * use the same stride, so they share the same value. 
					   */
					  unsigned int strideY, strideCr, strideCb;
					  
					  uint8_t *pbufY = (uint8_t *)(phys_addr + decoded_frame.framebuffer->y_offset);
					  uint8_t *pbufCb = (uint8_t *)(phys_addr + decoded_frame.framebuffer->cb_offset); 
					  uint8_t *pbufCr = (uint8_t *)(phys_addr + decoded_frame.framebuffer->cr_offset); 
					  strideY = decoded_frame.framebuffer->y_stride;
					  strideCb = strideCr = decoded_frame.framebuffer->cbcr_stride;
					  
					  pbufYUV[0] = pbufY;
					  pbufYUV[1] = pbufCb;
					  pbufYUV[2] = pbufCr;
					  
					  strideYUV[0] = strideY;
					  strideYUV[1] = strideCb;
					  strideYUV[2] = strideCr;
					  /* Scale/copy frame to destination mblk_t */
					  for (int i = 0; i < 3; i++) {
						uint8_t *dst = mOutbuf.planes[i];
						uint8_t *src = (uint8_t *)pbufYUV[i];
						int h = mHeight >> (( i > 0) ? 1 : 0);

						for(int j = 0; j < h; j++) {
						      memcpy(dst, src, mOutbuf.strides[i]);
						      dst += mOutbuf.strides[i];
						      src += strideYUV[(i == 0) ? 0 : 1];
						}
					  }
					  
					  ms_queue_put(mFilter->outputs[0], dupmsg(mYUVMsg));
					  
					  //imx_vpu_dma_buffer_unmap(decoded_frame.framebuffer->dma_buffer);

					  /* Update average FPS */
					  if (ms_average_fps_update(&mFPS, mFilter->ticker->time)) {
						  ms_message("iMX VPU decoder: Frame size: %dx%d", mWidth, mHeight);
					  }
					  
					  /* Notify first decoded image */
					  if (!mFirstImageDecoded) {
						  mFirstImageDecoded = true;
						  ms_filter_notify_no_arg(mFilter, MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
					  }
					  
					  /* Mark the framebuffer as displayed, thus returning it to the list of
					  *framebuffers available for decoding. */
					  imx_vpu_dec_mark_framebuffer_as_displayed(ctx->vpudec, decoded_frame.framebuffer);
		
				}
				else if (output_code & IMX_VPU_DEC_OUTPUT_CODE_DROPPED)
				{
#if MS_IMX_VPU_DECODER_DEBUG
					ms_message("imx_vpu_dec = drain_mode: IMX_VPU_DEC_OUTPUT_CODE_DROPPED");
#endif
					/* A frame was dropped. The context of the dropped frame can be retrieved
					* if this is necessary for timestamping etc. */
					void* dropped_frame_id;
					imx_vpu_dec_get_dropped_frame_info(ctx->vpudec, &dropped_frame_id, NULL, NULL);
					ms_message("iMX VPU decoder: dropped frame, frame id: 0x%x", (unsigned int)((uintptr_t)dropped_frame_id));
					
					/* Update average FPS */
					if (ms_average_fps_update(&mFPS, mFilter->ticker->time)) {
						  ms_message("iMX VPU decoder: Frame size: %dx%d", mWidth, mHeight);
					}
				}
			}
			ctx->frame_id_counter++;
		}
	}
}

int MSIMXVPUH264Decoder::nalusToFrame(MSQueue *nalus)
{
	mblk_t *im;
	uint8_t *dst = mBitstream;
	uint8_t *end = mBitstream + mBitstreamSize;
	bool startPicture = true;

	while ((im = ms_queue_get(nalus)) != NULL) {
		uint8_t *src = im->b_rptr;
		int nalLen = im->b_wptr - src;
		if ((dst + nalLen + 128) > end) {
			int pos = dst - mBitstream;
			
			this->enlargeBitstream(mBitstreamSize + nalLen + 128);
			
			dst = mBitstream + pos;
			end = mBitstream + mBitstreamSize;
		}
		if ((src[0] == 0) && (src[1] == 0) && (src[2] == 0) && (src[3] == 1)) {
			/* Workaround for stupid RTP H264 sender that includes nal markers */
#if MS_IMX_VPU_DECODER_DEBUG
			ms_warning("iMX VPU decoder: stupid RTP H264 encoder");
#endif
			int size = im->b_wptr - src;
			memcpy(dst, src, size);
			dst += size;
		} else {
			uint8_t naluType = *src & 0x1f;
#if MS_IMX_VPU_DECODER_DEBUG
			if ((naluType != 1) && (naluType != 7) && (naluType != 8)) {
				ms_message("iMX VPU decoder: naluType=%d", naluType);
			}
			if (naluType == 7) {
				ms_message("iMX VPU decoder: Got SPS");
			}
			if (naluType == 8) {
				ms_message("iMX VPU decoder: Got PPS");
			}
#endif
			if (startPicture
				|| (naluType == 6) /* SEI */
				|| (naluType == 7) /* SPS */
				|| (naluType == 8) /* PPS */
				|| ((naluType >= 14) && (naluType <= 18))) { /* Reserved */
				*dst++ = 0;
				startPicture = false;
			}

			/* Prepend nal marker */
			*dst++ = 0;
			*dst++ = 0;
			*dst++ = 1;
			*dst++ = *src++;
			while (src < (im->b_wptr - 3)) {
				if ((src[0] == 0) && (src[1] == 0) && (src[2] < 3)) {
					*dst++ = 0;
					*dst++ = 0;
					*dst++ = 3;
					src += 2;
				}
				*dst++ = *src++;
			}
			while (src < im->b_wptr) {
				*dst++ = *src++;
			}
		}
		freemsg(im);
	}
	return dst - mBitstream;
}

void MSIMXVPUH264Decoder::enlargeBitstream(int newSize)
{
	mBitstreamSize = newSize;
	mBitstream = static_cast<uint8_t *>(ms_realloc(mBitstream, mBitstreamSize));
}

void MSIMXVPUH264Decoder::uninitialize()
{
	if (mSPS != 0) {
		freemsg(mSPS);
		mSPS = NULL;
	}
	
	if (mPPS != 0) {
		freemsg(mPPS);
		mPPS = NULL;
	}
	
	if (mYUVMsg != 0) {
		freemsg(mYUVMsg);
		mYUVMsg = NULL;
	}
	
	if (mUnpacker){
		rfc3984_destroy(mUnpacker);
		mUnpacker = NULL;
	}
	
	unsigned int i;

	/* Close the previously opened decoder instance */
	imx_vpu_dec_close(ctx->vpudec);

	/* Free all allocated memory (both regular and DMA memory) */
	free(ctx->framebuffers);
	for (i = 0; i < ctx->num_framebuffers; ++i)
		imx_vpu_dma_buffer_deallocate(ctx->fb_dmabuffers[i]);
	free(ctx->fb_dmabuffers);
	imx_vpu_dma_buffer_deallocate(ctx->bitstream_buffer);

	/* Unload the VPU firmware */
	imx_vpu_dec_unload();
}

void MSIMXVPUH264Decoder::provideSpropParameterSets(char *value, int valueSize)
{
	char *b64_sps = value;
	char *b64_pps = strchr(value, ',');
	if (b64_pps) {
		*b64_pps = '\0';
		++b64_pps;
		ms_message("iMX VPU decoder: Got sprop-parameter-sets sps=%s, pps=%s", b64_sps, b64_pps);
		mSPS = allocb(valueSize, 0);
		mSPS->b_wptr += b64::b64_decode(b64_sps, strlen(b64_sps), mSPS->b_wptr, valueSize);
		mPPS = allocb(valueSize, 0);
		mPPS->b_wptr += b64::b64_decode(b64_pps, strlen(b64_pps), mPPS->b_wptr, valueSize);
	}
}

void MSIMXVPUH264Decoder::resetFirstImageDecoded()
{
	mFirstImageDecoded = false;
	mWidth = MS_VIDEO_SIZE_UNKNOWN_W;
	mHeight = MS_VIDEO_SIZE_UNKNOWN_H;
}

MSVideoSize MSIMXVPUH264Decoder::getSize() const
{
	MSVideoSize size;
	size.width = mWidth;
	size.height = mHeight;
	return size;
}

float MSIMXVPUH264Decoder::getFps() const
{
	return ms_average_fps_get(&mFPS);
}

const MSFmtDescriptor * MSIMXVPUH264Decoder::getOutFmt() const
{
	MSVideoSize vsize = {mWidth, mHeight};
	return ms_factory_get_video_format(mFilter->factory, "YUV420P", vsize, 0, NULL);
}