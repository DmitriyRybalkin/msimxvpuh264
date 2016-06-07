
#ifndef MSIMXVPUH264_DECODER_H
#define MSIMXVPUH264_DECODER_H

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

#define MS_IMX_VPU_DECODER_DEBUG 1

class MSIMXVPUH264Decoder {

public:
	MSIMXVPUH264Decoder(MSFilter *f);
	virtual ~MSIMXVPUH264Decoder();
	void initialize();
	void process();
	void uninitialize();
	void provideSpropParameterSets(char *value, int valueSize);
	void resetFirstImageDecoded();
	MSVideoSize getSize() const;
	float getFps() const;
	const MSFmtDescriptor *getOutFmt() const;
	
private:
	static void loggingFn(ImxVpuLogLevel level, char const *file, int const line, char const *fn, const char *format, ...);
	static int initialInfoCallback(ImxVpuDecoder *decoder, ImxVpuDecInitialInfo *new_initial_info, unsigned int output_code, void *user_data);
	void enlargeBitstream(int newSize);
	int nalusToFrame(MSQueue *nalus);
	
	static unsigned int ctx_height;
	static unsigned int ctx_width;
	
	MSFilter *mFilter;
	Rfc3984Context *mUnpacker;
	MSPicture mOutbuf;
	MSAverageFPS mFPS;
	mblk_t *mSPS;
	mblk_t *mPPS;
	mblk_t *mYUVMsg;
	int mWidth;
	int mHeight;
	bool mInitialized;
	bool mFirstImageDecoded;
	uint64_t mLastErrorReportTime;
	uint8_t *mBitstream;
	int mBitstreamSize;
	
	/* iMX VPU speicifc pointers and variables */
	struct _Context
	{
		ImxVpuDecoder *vpudec;
		ImxVpuDMABuffer *bitstream_buffer;
		size_t bitstream_buffer_size;
		unsigned int bitstream_buffer_alignment;
		ImxVpuDecInitialInfo initial_info;
		ImxVpuFramebuffer *framebuffers;
		ImxVpuDMABuffer **fb_dmabuffers;
		unsigned int num_framebuffers;
		ImxVpuFramebufferSizes calculated_sizes;
		unsigned int frame_id_counter;
		ImxVpuDecOpenParams open_params;
	};
	
	typedef struct _Context Context;
	
	Context *ctx;
};

#endif