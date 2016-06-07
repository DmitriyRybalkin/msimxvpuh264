
#include <mediastreamer2/msticker.h>
#include <ortp/b64.h>

#include "msimxvpuh264enc.h"

#define COLOR_FORMAT IMX_VPU_COLOR_FORMAT_YUV420

VideoStarter::VideoStarter()
	: mActive(true), mNextTime(0), mFrameCount(0)
{}

VideoStarter::~VideoStarter()
{}

void VideoStarter::firstFrame(uint64_t curtime)
{
	mNextTime = curtime + 2000;
}

bool VideoStarter::needIFrame(uint64_t curtime)
{
	if (!mActive || (mNextTime == 0)) return false;
	if (curtime >= mNextTime) {
		mFrameCount++;
		if (mFrameCount == 1) {
			mNextTime += 2000;
		} else {
			mNextTime = 0;
		}
		return true;
	}
	return false;
}

void VideoStarter::deactivate()
{
	mActive = false;
}

#if defined(ANDROID) || (TARGET_OS_IPHONE == 1) || defined(__arm__)
	#define MS_IMXVPUH264_CONF(required_bitrate, bitrate_limit, resolution, fps_pc, cpus_pc, fps_mobile, cpus_mobile) \
		{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H },fps_mobile, cpus_mobile, NULL }
#else
	#define MS_IMXVPUH264_CONF(required_bitrate, bitrate_limit, resolution, fps_pc, cpus_pc, fps_mobile, cpus_mobile) \
		{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H },fps_pc, cpus_pc, NULL }
#endif

static const MSVideoConfiguration imxvpuh264_conf_list[] = {
	MS_IMXVPUH264_CONF(2048000,	1000000,	UXGA, 25,  4, 12, 2), /*1200p*/

	MS_IMXVPUH264_CONF(1024000,	5000000,	SXGA_MINUS, 25,  4, 12, 2),

	MS_IMXVPUH264_CONF(1024000,	5000000,	720P, 25,  4, 12, 2),

	MS_IMXVPUH264_CONF(750000,	2048000,	XGA, 20,  4, 12, 2),

	MS_IMXVPUH264_CONF(500000,	1024000,	SVGA, 20,  2, 12, 2),

	MS_IMXVPUH264_CONF(256000,	800000,		VGA, 15,  2, 12, 2), /*480p*/

	MS_IMXVPUH264_CONF(128000,	512000,		CIF, 15,  1, 12, 2),

	MS_IMXVPUH264_CONF(100000,	380000,		QVGA, 15,  1, 15, 2), /*240p*/
	MS_IMXVPUH264_CONF(100000,	380000,		QVGA, 15,  1, 12, 1),

	MS_IMXVPUH264_CONF(128000,	170000,		QCIF, 10,  1, 10, 1),
	MS_IMXVPUH264_CONF(64000,	128000,		QCIF, 10,  1, 7, 1),
	MS_IMXVPUH264_CONF(0,		64000,		QCIF, 10,  1, 5, 1)
};

MSIMXVPUH264Encoder::MSIMXVPUH264Encoder(MSFilter *f) : mFilter(f), mVConfList(imxvpuh264_conf_list), mInitialized(false), mPacketisationMode(0), mFrameCount(0) {
	mVConf = ms_video_find_best_configuration_for_bitrate(mVConfList, 384000, ms_get_cpu_count());
}

MSIMXVPUH264Encoder::~MSIMXVPUH264Encoder() {

	if(ctx) {
		free(ctx);
	}
}

void MSIMXVPUH264Encoder::initialize()
{
	if(!mInitialized) {
		mPacker = rfc3984_new();
		rfc3984_set_mode(mPacker, mPacketisationMode);
		rfc3984_enable_stap_a(mPacker, FALSE);
		
		ImxVpuEncOpenParams open_params;
		unsigned int i;

		ctx = calloc(1, sizeof(Context));

		/* Set the open params. Use the default values (note that memset must still
		* be called to ensure all values are set to 0 initially; the
		* imx_vpu_enc_set_default_open_params() function does not do this!).
		* Then, set a bitrate of 0 kbps, which tells the VPU to use constant quality
		* mode instead (controlled by the quant_param field in ImxVpuEncParams).
		* Frame width & height are also necessary, as are the frame rate numerator
		* and denominator. Also, access unit delimiters are enabled to demonstrate
		* their use. */
		memset(&open_params, 0, sizeof(open_params));
		imx_vpu_enc_set_default_open_params(IMX_VPU_CODEC_FORMAT_H264, &open_params);
		/* VPU requires bitrate in kbps */
		open_params.bitrate = ((mVConf.required_bitrate > 100) ? mVConf.required_bitrate/1000 : mVConf.required_bitrate);
		open_params.frame_width = mVConf.vsize.width;;
		open_params.frame_height = mVConf.vsize.height;
		open_params.frame_rate_numerator = mVConf.fps;
		open_params.frame_rate_denominator = 1;
		open_params.codec_params.h264_params.enable_access_unit_delimiters = 1;
		
		/* Load the VPU firmware */
		imx_vpu_enc_load();

		/* Retrieve information about the required bitstream buffer and allocate one based on this */
		imx_vpu_enc_get_bitstream_buffer_info(&(ctx->bitstream_buffer_size), &(ctx->bitstream_buffer_alignment));
		ctx->bitstream_buffer = imx_vpu_dma_buffer_allocate(
			imx_vpu_enc_get_default_allocator(),
			ctx->bitstream_buffer_size,
			ctx->bitstream_buffer_alignment,
			0
		);

		/* Open an encoder instance, using the previously allocated bitstream buffer */
		imx_vpu_enc_open(&(ctx->vpuenc), &open_params, ctx->bitstream_buffer);
		
		/* Retrieve the initial information to allocate framebuffers for the
		* encoding process (unlike with decoding, these framebuffers are used
		* only internally by the encoder as temporary storage; encoded data
		* doesn't go in there, nor do raw input frames) */
		imx_vpu_enc_get_initial_info(ctx->vpuenc, &(ctx->initial_info));

		ctx->num_framebuffers = ctx->initial_info.min_num_required_framebuffers;

		/* Using the initial information, calculate appropriate framebuffer sizes */
		imx_vpu_calc_framebuffer_sizes(COLOR_FORMAT, open_params.frame_width, open_params.frame_height, ctx->initial_info.framebuffer_alignment, 0, 0, &(ctx->calculated_sizes));
		
		/* Allocate memory blocks for the framebuffer and DMA buffer structures,
		* and allocate the DMA buffers themselves */

		ctx->framebuffers = (ImxVpuFramebuffer*)malloc(sizeof(ImxVpuFramebuffer) * ctx->num_framebuffers);
		ctx->fb_dmabuffers = (ImxVpuDMABuffer**)malloc(sizeof(ImxVpuDMABuffer*) * ctx->num_framebuffers);

		for (i = 0; i < ctx->num_framebuffers; ++i)
		{
			/* Allocate a DMA buffer for each framebuffer. It is possible to specify alternate allocators;
			* all that is required is that the allocator provides physically contiguous memory
			* (necessary for DMA transfers) and respecs the alignment value. */
			ctx->fb_dmabuffers[i] = imx_vpu_dma_buffer_allocate(imx_vpu_dec_get_default_allocator(), ctx->calculated_sizes.total_size, ctx->initial_info.framebuffer_alignment, 0);

			imx_vpu_fill_framebuffer_params(&(ctx->framebuffers[i]), &(ctx->calculated_sizes), ctx->fb_dmabuffers[i], 0);
		}
		
		/* allocate DMA buffers for the raw input frames. Since the encoder can only read
		* raw input pixels from a DMA memory region, it is necessary to allocate one,
		* and later copy the pixels into it. In production, it is generally a better
		* idea to make sure that the raw input frames are already placed in DMA memory
		* (either allocated by imx_vpu_dma_buffer_allocate() or by some other means of
		* getting DMA / physically contiguous memory with known physical addresses). */
		ctx->input_fb_dmabuffer = imx_vpu_dma_buffer_allocate(imx_vpu_dec_get_default_allocator(), ctx->calculated_sizes.total_size, ctx->initial_info.framebuffer_alignment, 0);
		imx_vpu_fill_framebuffer_params(&(ctx->input_framebuffer), &(ctx->calculated_sizes), ctx->input_fb_dmabuffer, 0);

		/* Actual registration is done here. From this moment on, the VPU knows which buffers to use for
		* storing temporary frames into. This call must not be done again until encoding is shut down. */
		imx_vpu_enc_register_framebuffers(ctx->vpuenc, ctx->framebuffers, ctx->num_framebuffers);
		
		ms_message(
		"iMX VPU Encoder has been initialized: calculated sizes:  frame width&height: %dx%d  Y stride: %u  CbCr stride: %u  Y size: %u  CbCr size: %u  MvCol size: %u  total size: %u",
		ctx->calculated_sizes.aligned_frame_width, ctx->calculated_sizes.aligned_frame_height,
		ctx->calculated_sizes.y_stride, ctx->calculated_sizes.cbcr_stride,
		ctx->calculated_sizes.y_size, ctx->calculated_sizes.cbcr_size, ctx->calculated_sizes.mvcol_size,
		ctx->calculated_sizes.total_size
		);
		
		mInitialized = true;
	}
}

void MSIMXVPUH264Encoder::process()
{
	if (!isInitialized()){
		ms_queue_flush(mFilter->inputs[0]);
		return;
	}
	
	ImxVpuRawFrame input_frame;
	ImxVpuEncodedFrame output_frame;
	unsigned int output_code;
	
	mblk_t *im;
	MSQueue nalus;
	ms_queue_init(&nalus);
	long long int ts = mFilter->ticker->time * 90LL;
	
	/* Set up the input frame. The only field that needs to be
	 * set is the input framebuffer. The encoder will read from it.
	 * The rest can remain zero/NULL. */
	memset(&input_frame, 0, sizeof(input_frame));
	input_frame.framebuffer = &(ctx->input_framebuffer);

	/* Set the encoding parameters for this frame. quant_param 0 is
	 * the highest quality in h.264 constant quality encoding mode.
	 * (The range in h.264 is 0-51, where 0 is best quality and worst
	 * compression, and 51 vice versa.) */
	memset(&(ctx->enc_params), 0, sizeof(ctx->enc_params));
	//enc_params.quant_param = 0;
	ctx->enc_params.acquire_output_buffer = acquireOutputBuffer;
	ctx->enc_params.finish_output_buffer = finishOutputBuffer;
	ctx->enc_params.output_buffer_context = NULL;
	
	/* Set up the output frame. Simply setting all fields to zero/NULL
	 * is enough. The encoder will fill in data. */
	memset(&output_frame, 0, sizeof(output_frame));
	
	while ((im = ms_queue_get(mFilter->inputs[0])) != NULL) {
		MSPicture pic;
		if (ms_yuv_buf_init_from_mblk(&pic, im) == 0) {
			/* The actual encoding */
			imx_vpu_enc_encode(ctx->vpuenc, &input_frame, &output_frame, &(ctx->enc_params), &output_code);
		}
		freemsg(im);
	}
}

void MSIMXVPUH264Encoder::uninitialize()
{
	unsigned int i;
	
	if (mPacker != 0) {
		rfc3984_destroy(mPacker);
		mPacker = 0;
	}

	/* Close the previously opened encoder instance */
	imx_vpu_enc_close(ctx->vpuenc);

	/* Free all allocated memory (both regular and DMA memory) */
	imx_vpu_dma_buffer_deallocate(ctx->input_fb_dmabuffer);
	free(ctx->framebuffers);
	
	for (i = 0; i < ctx->num_framebuffers; ++i)
		imx_vpu_dma_buffer_deallocate(ctx->fb_dmabuffers[i]);
	
	free(ctx->fb_dmabuffers);
	imx_vpu_dma_buffer_deallocate(ctx->bitstream_buffer);

	/* Unload the VPU firmware */
	imx_vpu_enc_unload();
	
	if(ctx) {
		free(ctx);
	}
	
	mInitialized = false;
  
}

void * MSIMXVPUH264Encoder::acquireOutputBuffer(void *context, size_t size, void **acquired_handle)
{
	void *mem;

	((void)(context));

	/* In this example, "acquire" a buffer by simply allocating it with malloc() */
	mem = malloc(size);
	*acquired_handle = mem;
	ms_message("iMX VPU Encoder: acquired output buffer, handle %p", *acquired_handle);
	return mem;
}

void MSIMXVPUH264Encoder::finishOutputBuffer(void *context, void *acquired_handle)
{
	((void)(context));

	/* Nothing needs to be done here in this example. Just log this call. */
	ms_message("iMX VPU Encoder: finished output buffer, handle %p", acquired_handle);
}

void MSIMXVPUH264Encoder::setFPS(float fps)
{
	mVConf.fps = fps;
	this->setConfiguration(mVConf);
}

void MSIMXVPUH264Encoder::setBitrate(int bitrate)
{
	mVConf.bitrate_limit = bitrate;
	mVConf.required_bitrate = mVConf.bitrate_limit;
	imx_vpu_enc_configure_bitrate(ctx->vpuenc, ((bitrate > 100) ? (unsigned int)bitrate/1000 : (unsigned int)bitrate));
}

void MSIMXVPUH264Encoder::setSize(MSVideoSize size)
{
	MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_size(mVConfList, size, ms_get_cpu_count());
	mVConf.vsize = size;
	mVConf.fps = best_vconf.fps;
	mVConf.bitrate_limit = best_vconf.bitrate_limit;
	this->setConfiguration(mVConf);
}

void MSIMXVPUH264Encoder::addFmtp(const char *fmtp)
{
	char value[12];
	if (fmtp_get_value(fmtp, "packetization-mode", value, sizeof(value))) {
		mPacketisationMode = atoi(value);
		ms_message("packetization-mode set to %i", mPacketisationMode);
	}
}
void MSIMXVPUH264Encoder::requestVFU()
{
	/* If we receive a VFU request, stop the video starter */
	mVideoStarter.deactivate();
	this->generateKeyframe();
}

void MSIMXVPUH264Encoder::generateKeyframe()
{
	if(isInitialized()) {
	  ms_filter_lock(mFilter);
	  if (mFrameCount > 0) {
		/* If set to 1, this forces the encoder to produce an I frame.
		* 0 disables this. Default value is 0. */
		ctx->enc_params.force_I_frame = 1;  
	  } else ms_message("iMX VPU encoder: ignored forcing I frame generation since no frame has been generated yet.");
	  
	  ms_filter_unlock(mFilter);
	}
}

void MSIMXVPUH264Encoder::setConfiguration(MSVideoConfiguration conf)
{
	mVConf = conf;
	if (mVConf.required_bitrate > mVConf.bitrate_limit)
		mVConf.required_bitrate = mVConf.bitrate_limit;
	if (isInitialized()) {
		ms_filter_lock(mFilter);
		this->setBitrate(mVConf.required_bitrate);
		ms_filter_unlock(mFilter);
		return;
	}

	ms_message("iMX VPU encoder: Video configuration set: bitrate=%dbits/s, fps=%f, vsize=%dx%d",
		mVConf.required_bitrate, mVConf.fps, mVConf.vsize.width, mVConf.vsize.height);
}
