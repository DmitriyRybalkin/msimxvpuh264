
/*
* H.264 decoder plugin for mediastreamer2 based on the GStreamer library
* and Freescale iMX VPU Coder/Decoder based plugin to GStreamer
* 
* Filename: msimxvpuh264dec.cpp
* Author: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
* Created: Mon Jul 11 14:56:30 2016 +0600
* Version: 1.0
* Last-Updated: Mon Jul 11 14:56:30 2016 +0600
* By: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
*
* Target platform for cross-compilation: ARCH=arm && proc=imx6
*/
#include <mediastreamer2/msticker.h>
#include <ortp/b64.h>

#include "msimxvpuh264dec.h"

GstElement * MSIMXVPUH264Decoder::pipeline = NULL;
GstElement * MSIMXVPUH264Decoder::appsrc = NULL;
GstClockTime MSIMXVPUH264Decoder::timestamp = 0;
guint MSIMXVPUH264Decoder::sourceid = 0;

MSIMXVPUH264Decoder::MSIMXVPUH264Decoder(MSFilter *f)
	: mFilter(f), mUnpacker(0), mSPS(0), mPPS(0), mYUVMsg(0),
	mBitstream(0), mBitstreamSize(65536), mLastErrorReportTime(0),
	mWidth(MS_VIDEO_SIZE_UNKNOWN_W), mHeight(MS_VIDEO_SIZE_UNKNOWN_H),
	mInitialized(false), mFirstImageDecoded(false), mPipelinePlaying(false),
	mPipelineRestarted(false), mRestartPipeline(false), frameNum(0)
{
	mBitstream = static_cast<uint8_t *>(ms_malloc0(mBitstreamSize));
	pYuvFile = NULL;
	pH264File = NULL;
	
	gst_init(NULL, NULL);
		
	pipeline = gst_pipeline_new ("pipeline");
	appsrc = gst_element_factory_make ("appsrc", "source");

	transform = gst_parse_bin_from_description("h264parse ! vpudec low-latency=true ! mfw_ipucsc qos=false ! qwidgetvideosink name=VoipVideo force-aspect-ratio=false sync=false async=false qos=false", TRUE, FALSE);

	gst_bin_add_many (GST_BIN (pipeline), appsrc, transform, NULL);
	gst_element_link_many (appsrc, transform, NULL);
	
	gst_app_src_set_stream_type(GST_APP_SRC(appsrc), GST_APP_STREAM_TYPE_STREAM);
	gst_app_src_set_emit_signals(GST_APP_SRC(appsrc), TRUE);

	g_object_set (G_OBJECT (appsrc), "caps", gst_caps_new_simple ("video/x-h264", "stream-format", G_TYPE_STRING, "byte-stream", "alignment", G_TYPE_STRING, "au", NULL), NULL);

	g_object_set (G_OBJECT (appsrc), "stream-type", 0, "format", GST_FORMAT_TIME, "is-live", FALSE, "do-timestamp", FALSE, NULL);
	
	createSharedMem();
	mapGstElementOntoSharedMem();
}

MSIMXVPUH264Decoder::~MSIMXVPUH264Decoder()
{
	if (mBitstream != 0) {
		ms_free(mBitstream);
	}
	
	releaseSharedMem();
	
	if(pipeline) {
		gst_element_set_state (pipeline, GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (pipeline));
	}
}

void MSIMXVPUH264Decoder::createSharedMem()
{
	key_t key;
	key = ftok(SHMEM_FTOK_FILE, 1); 

	if (key == -1)
	{
	    ms_warning("iMX VPU decoder: Failed to generate unique key for shmem, errno = %d (%s)", errno, strerror(errno));
	}

	shmid = shmget(key, sizeof(struct mShmemBlock), 0666 | IPC_CREAT | IPC_EXCL); 
	if(shmid == -1)
	    ms_warning("iMX VPU decoder: Failed to allocate shared memory with errno = %d (%s)", errno, strerror(errno));
	else
	    ms_message("iMX VPU decoder: Created Shared Memory Block %i", shmid);
}

void MSIMXVPUH264Decoder::mapGstElementOntoSharedMem()
{
	mblock = (struct mShmemBlock *) shmat(shmid, 0, 0);
	/* initialize starting values of shmem block */
	mblock->turn = SHMEM_CLIENT;
	mblock->server_lock = SHMEM_FREE;
	mblock->client_lock = SHMEM_FREE;
	mblock->readlast = SHMEM_SERVER;
	
	GstElement * sipVideoDisplay = gst_bin_get_by_name(GST_BIN(pipeline), "VoipVideo");
	
	if(sipVideoDisplay) {
		mblock->server_lock = SHMEM_BUSY;
		mblock->turn = SHMEM_CLIENT;
		mblock->readlast = SHMEM_SERVER;
		
		mblock->gstDisplayElement = sipVideoDisplay;
		
		mblock->server_lock = SHMEM_FREE;
	} else
		ms_warning("iMX VPU decoder: Failed to map GstElement onto shared memory block");
}
	
void MSIMXVPUH264Decoder::releaseSharedMem()
{
	/* release mapping (stack principle) */
	shmdt((void *) mblock);
	mblock = NULL;
	
	/* release shared memory block */
	shmctl(shmid, IPC_RMID, 0);
}

void MSIMXVPUH264Decoder::initialize()
{
	if (!mInitialized) {
		mFirstImageDecoded = false;
		mUnpacker = rfc3984_new();
		
		ms_average_fps_init(&mFPS, "iMX VPU decoder: FPS=%f");
		//pYuvFile = fopen(YUV_OUTPUT_FILE, "w");
		//pH264File = fopen(H264_OUTPUT_FILE, "w");

		/*if (pYuvFile == NULL) {
			ms_error("iMX VPU decoder: Can not open yuv file to output result of decoding");
		}
		
		if (pH264File == NULL) {
			ms_error("iMX VPU decoder: Can not open h264 file to output encoded frames");
		}*/
		
		mInitialized = true;
	}
}

void MSIMXVPUH264Decoder::startGstFeed(GstElement * source, guint size)
{
	if (sourceid == 0) {
		sourceid = g_idle_add ((GSourceFunc)pushGstBuffer, NULL);
	}
}

void MSIMXVPUH264Decoder::stopGstFeed(GstElement * source)
{
	if (sourceid != 0) {
		g_source_remove (sourceid);
		sourceid = 0;
	}
}
	
gboolean MSIMXVPUH264Decoder::pushGstBuffer()
{
	ms_message("iMX VPU decoder: push GST buffer");
}

void MSIMXVPUH264Decoder::process()
{
	if (!isInitialized()){
		ms_error("MSIMXVPUH264Decoder::feed(): not initialized");
		ms_queue_flush(mFilter->inputs[0]);
		return;
	}
	
	if(!mPipelinePlaying) {
		if(mRestartPipeline) {
			this->restartPipeline();
		} else {
			this->playPipeline();
		}	
	}
		
	MSQueue nalus;
	ms_queue_init(&nalus);

	mblk_t *im;
	while ((im = ms_queue_get(mFilter->inputs[0])) != NULL) {
		if ((frameNum == 0) && (mSPS != 0) && (mPPS != 0)) {
			/* Push the sps/pps given in sprop-parameter-sets if any */
			mblk_set_timestamp_info(mSPS, mblk_get_timestamp_info(im));
			mblk_set_timestamp_info(mPPS, mblk_get_timestamp_info(im));
			rfc3984_unpack(mUnpacker, mSPS, &nalus);
			rfc3984_unpack(mUnpacker, mPPS, &nalus);
			mSPS = 0;
			mPPS = 0;
		}
		rfc3984_unpack(mUnpacker, im, &nalus);
		if (!ms_queue_empty(&nalus)) {
			bool needReinit = false;
			int len = nalusToFrame(&nalus, &needReinit);
			
			if(needReinit) {
				ms_message("iMX VPU decoder: restart pipeline");
				this->restartPipeline();
			}
			//this->writeEncodedFile(pH264File, mBitstream, len);
			
			GstFlowReturn ret;
			
			GstBuffer *buffer = gst_buffer_new_and_alloc(len);
			memcpy(GST_BUFFER_DATA(buffer), mBitstream, len);
			
			if(buffer) {
				
				uint32_t frame_ts = mblk_get_timestamp_info(ms_queue_peek_first(&nalus));
				
				GstClockTime timestamp = frame_ts;
				GstClockTime running_time = pipeline->base_time;
				GstClockTime next_time = gst_util_uint64_scale_int (frameNum * GST_SECOND, 1, (guint64)getFps());
				
				GST_BUFFER_TIMESTAMP(buffer) = timestamp;
				
				GST_BUFFER_DURATION(buffer) = next_time - running_time;

				ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
#ifdef MS_IMX_VPU_DECODER_DEBUG
				ms_message("iMX VPU decoder: pushed GST_BUFFER");
#endif
				if (ret != GST_FLOW_OK)
					ms_message("iMX VPU decoder: unable to push appsrc");
			  
			} else
				ms_warning("iMX VPU decoder: failed to allocate GST_BUFFER");
		}
		frameNum++;
	}
}

void MSIMXVPUH264Decoder::restartPipeline()
{
	ms_warning("iMX VPU decoder: Restart pipeline");
	gst_element_send_event(pipeline, gst_event_new_eos ());
	
	gst_element_set_state(pipeline, GST_STATE_READY);
	
	gst_element_unlink(appsrc, transform);
	
	gst_bin_remove_many(GST_BIN (pipeline), appsrc, transform, NULL);
	
	gst_bin_add_many(GST_BIN (pipeline), appsrc, transform, NULL);
	gst_element_link_many(appsrc, transform, NULL);
		
	gst_element_set_state(pipeline, GST_STATE_PAUSED);
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	
	mPipelineRestarted = true;
	mPipelinePlaying = true;
	mRestartPipeline = false;
}

void MSIMXVPUH264Decoder::playPipeline()
{
	gst_element_set_state(pipeline, GST_STATE_READY);
	gst_element_set_state(pipeline, GST_STATE_PAUSED);
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	
	mPipelineRestarted = false;
	mRestartPipeline = false;
	mPipelinePlaying = true;
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
	if (mUnpacker) {
		rfc3984_destroy(mUnpacker);
		mUnpacker = NULL;
	}
	if (pYuvFile) {
		fclose(pYuvFile);
		pYuvFile = NULL;
	}
	if (pH264File) {
		fclose(pH264File);
		pH264File = NULL;
	}
	
	if(mPipelinePlaying) {
		gst_element_set_state(pipeline, GST_STATE_PAUSED);
		gst_element_set_state(pipeline, GST_STATE_READY);
		
		timestamp = 0;
		
		mPipelinePlaying = false;
		mPipelineRestarted = false;
		mRestartPipeline = true;
	}
	
	mInitialized = false;
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
	return ms_factory_get_video_format(mFilter->factory,"YUV420P",vsize,0,NULL);
}

void MSIMXVPUH264Decoder::write2File(FILE* pFp, unsigned char* pData[3], int iStride[2], int iWidth, int iHeight) 
{
	int i;
	unsigned char*  pPtr = NULL;

	pPtr = pData[0];
	
	for (i = 0; i < iHeight; i++) {
		fwrite (pPtr, 1, iWidth, pFp);
		pPtr += iStride[0];
	}

	iHeight = iHeight / 2;
	iWidth = iWidth / 2;
	pPtr = pData[1];
	
	for (i = 0; i < iHeight; i++) {
		fwrite (pPtr, 1, iWidth, pFp);
		pPtr += iStride[1];
	}

	pPtr = pData[2];

	for (i = 0; i < iHeight; i++) {
		fwrite (pPtr, 1, iWidth, pFp);
		pPtr += iStride[1];
	}
}

void MSIMXVPUH264Decoder::writeEncodedFile(FILE* pFp, uint8_t * pData, int size)
{
	uint8_t * pPtr = NULL;
	pPtr = pData;
	
	fwrite(pPtr, 1, size, pFp);
}

void MSIMXVPUH264Decoder::processFile(void* pDst[3], SBufferInfo* pInfo, FILE* pFp)
{
	if (pFp && pDst[0] && pDst[1] && pDst[2] && pInfo) {
		int iStride[2];
		int iWidth = pInfo->UsrData.sSystemBuffer.iWidth;
		int iHeight = pInfo->UsrData.sSystemBuffer.iHeight;
		iStride[0] = pInfo->UsrData.sSystemBuffer.iStride[0];
		iStride[1] = pInfo->UsrData.sSystemBuffer.iStride[1];

		this->write2File(pFp, (unsigned char**)pDst, iStride, iWidth, iHeight);
	}
}

void MSIMXVPUH264Decoder::updateSps(mblk_t *sps)
{
	if(mSPS)
		freemsg(mSPS);
	mSPS = dupb(sps);
}

void MSIMXVPUH264Decoder::updatePps(mblk_t *pps)
{
	if(mPPS)
		freemsg(mPPS);
	if(pps) mPPS = dupb(pps);
	else mPPS = NULL;
}

bool MSIMXVPUH264Decoder::checkSpsChange(mblk_t *sps)
{
	bool ret = false;
	if(mSPS) {
		ret = (msgdsize(sps) != msgdsize(mSPS)) || (memcmp(mSPS->b_rptr, sps->b_rptr, msgdsize(sps)) != 0);
		if(ret) {
			ms_message("iMX VPU decoder: SPS changed ! %i, %i", (int)msgdsize(sps), (int)msgdsize(mSPS));
			updateSps(sps);
			updatePps(NULL);
		}
	} else {
		ms_message("iMX VPU decoder: Receiving first SPS");
		updateSps(sps);
	}
	return ret;
}

bool MSIMXVPUH264Decoder::checkPpsChange(mblk_t *pps)
{
	bool ret = false;
	if(mPPS) {
		ret = (msgdsize(pps) != msgdsize(mPPS)) || (memcmp(mPPS->b_rptr, pps->b_rptr, msgdsize(pps)) != 0);
		if(ret) {
			ms_message("iMX VPU decoder: PPS changed ! %i, %i", (int)msgdsize(pps), (int)msgdsize(mPPS));
			updatePps(mPPS);
		}
	} else {
		ms_message("iMX VPU decoder: Receiving first PPS");
		updatePps(pps);
	}
	return ret;
}

int MSIMXVPUH264Decoder::nalusToFrame(MSQueue *nalus, bool *newSpsPps)
{
	mblk_t *im;
	uint8_t *dst = mBitstream;
	uint8_t *end = mBitstream + mBitstreamSize;
	bool startPicture = true;
	*newSpsPps = false;
	int nalLen;
	uint8_t naluType;
	
	while((im = ms_queue_get(nalus)) != NULL) {
		uint8_t *src = im->b_rptr;
		nalLen = im->b_wptr - src;
		if ((dst + nalLen + 100) > end) {
			int pos = dst - mBitstream;
			enlargeBitstream(mBitstreamSize + nalLen + 100);
			dst = mBitstream + pos;
			end = mBitstream + mBitstreamSize;
		}
		if((src[0] == 0) && (src[1] == 0) && (src[2] == 0) && (src[3] == 1)) {
			ms_warning("iMX VPU decoder: stupid RTP H264 encoder");

			int size = im->b_wptr - src;
			memcpy(dst, src, size);
			dst += size;
		} else {
			uint8_t naluType = (*src) & ((1<<5)-1);
			if(naluType == 7)
				*newSpsPps = (checkSpsChange(im) || *newSpsPps);
			if(naluType == 8)
				*newSpsPps = (checkPpsChange(im) || *newSpsPps);
			if(startPicture || naluType == 7/*SPS*/ || naluType == 8/*PPS*/) {
				*dst++ = 0;
				startPicture = false;
			}
			
			/* prepend nal marker */
			*dst++ = 0;
			*dst++ = 0;
			*dst++ = 1;
			*dst++ = *src++;
			while(src < (im->b_wptr - 3)) {
				if((src[0] == 0) && (src[1] == 0) && (src[2] < 3)) {
					*dst++ = 0;
					*dst++ = 0;
					*dst++ = 3;
					src += 2;
				}
				*dst++ = *src++;
			}
			*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = *src++;
		}
		freemsg(im);
	}
	return dst - mBitstream;
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
			enlargeBitstream(mBitstreamSize + nalLen + 128);
			dst = mBitstream + pos;
			end = mBitstream + mBitstreamSize;
		}
		if ((src[0] == 0) && (src[1] == 0) && (src[2] == 0) && (src[3] == 1)) {
			// Workaround for stupid RTP H264 sender that includes nal markers

			ms_warning("iMX VPU decoder: stupid RTP H264 encoder");

			int size = im->b_wptr - src;
			memcpy(dst, src, size);
			dst += size;
		} else {
			uint8_t naluType = *src & 0x1f;
			  
			if ((naluType != 1) && (naluType != 7) && (naluType != 8)) {
				ms_message("iMX VPU decoder: naluType=%d", naluType);
			}
			if (naluType == 7) {
				ms_message("iMX VPU decoder: Got SPS");
			}
			if (naluType == 8) {
				ms_message("iMX VPU decoder: Got PPS");
			}

			if (startPicture
				|| (naluType == 6) // SEI
				|| (naluType == 7) // SPS
				|| (naluType == 8) // PPS
				|| ((naluType >= 14) && (naluType <= 18))) { // Reserved
				*dst++ = 0;
				startPicture = false;
			}

			// Prepend nal marker
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
