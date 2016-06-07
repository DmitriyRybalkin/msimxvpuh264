
#ifndef MSIMXVPUH264_ENCODER_H
#define MSIMXVPUH264_ENCODER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msvideo.h>
#include <mediastreamer2/rfc3984.h>

#include <imxvpuapi/imxvpuapi.h>

#define MS_IMX_VPU_ENCODER_DEBUG 1

/**
 * The goal of this small object is to tell when to send I frames at startup: at 2 and 4 seconds
 */
class VideoStarter {
public:
	VideoStarter();
	virtual ~VideoStarter();
	void firstFrame(uint64_t curtime);
	bool needIFrame(uint64_t curtime);
	void deactivate();

private:
	bool mActive;
	uint64_t mNextTime;
	int mFrameCount;
};

class MSIMXVPUH264Encoder {

public:
	MSIMXVPUH264Encoder(MSFilter *f);
	virtual ~MSIMXVPUH264Encoder();
	void initialize();
	bool isInitialized() const { return mInitialized; }
	void process();
	void uninitialize();
	
	void setFPS(float fps);
	float getFPS() const { return mVConf.fps; }
	void setBitrate(int bitrate);
	int getBitrate() const { return mVConf.required_bitrate; }
	void setSize(MSVideoSize size);
	MSVideoSize getSize() const { return mVConf.vsize; }
	const MSVideoConfiguration * getConfigurationList() const { return mVConfList; }
	void addFmtp(const char *fmtp);
	void requestVFU();
	void setConfiguration(MSVideoConfiguration conf);
	
private:

	static void * acquireOutputBuffer(void *context, size_t size, void **acquired_handle);
	static void finishOutputBuffer(void *context, void *acquired_handle);
	void generateKeyframe();
	
	MSFilter *mFilter;
	Rfc3984Context *mPacker;
	const MSVideoConfiguration *mVConfList;
	MSVideoConfiguration mVConf;
	int mPacketisationMode;
	bool mInitialized;
	uint64_t mFrameCount;
	
	struct _Context
	{
		ImxVpuEncoder *vpuenc;
		ImxVpuEncParams enc_params;
		
		ImxVpuDMABuffer *bitstream_buffer;
		size_t bitstream_buffer_size;
		unsigned int bitstream_buffer_alignment;

		ImxVpuEncInitialInfo initial_info;

		ImxVpuFramebuffer input_framebuffer;
		ImxVpuDMABuffer *input_fb_dmabuffer;

		ImxVpuFramebuffer *framebuffers;
		ImxVpuDMABuffer **fb_dmabuffers;
		unsigned int num_framebuffers;
		ImxVpuFramebufferSizes calculated_sizes;
	};

	typedef struct _Context Context;
	
	Context *ctx;
};

#endif