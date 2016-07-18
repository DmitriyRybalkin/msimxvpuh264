
/*
* H.264 decoder plugin for mediastreamer2 based on the GStreamer library
* and Freescale iMX VPU Coder/Decoder based plugin to GStreamer
* 
* Filename: msimxvpuh264dec.h
* Author: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
* Created: Mon Jul 11 14:56:30 2016 +0600
* Version: 1.0
* Last-Updated: Mon Jul 11 14:56:30 2016 +0600
* By: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
*
* Target platform for cross-compilation: ARCH=arm && proc=imx6
*/
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

#include <wels/codec_api.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#undef MS_IMX_VPU_DECODER_DEBUG 1
#define YUV_OUTPUT_FILE "/home/root/openh264dec.yuv"
#define H264_OUTPUT_FILE "/home/root/openh264dec.h264"

#define SHMEM_FTOK_FILE "/usr/bin/linphone"
#define SHMEM_FREE 1
#define SHMEM_BUSY 0
#define SHMEM_SERVER 1
#define SHMEM_CLIENT 0

class MSIMXVPUH264Decoder {

public:
	MSIMXVPUH264Decoder(MSFilter *f);
	virtual ~MSIMXVPUH264Decoder();
	bool isInitialized() const { return mInitialized; }
	void initialize();
	void process();
	void uninitialize();
	void provideSpropParameterSets(char *value, int valueSize);
	void resetFirstImageDecoded();
	MSVideoSize getSize() const;
	float getFps()const;
	const MSFmtDescriptor *getOutFmt()const;
private:
	int nalusToFrame(MSQueue *nalus);
	int nalusToFrame(MSQueue *nalus, bool *newSpsPps);
	void enlargeBitstream(int newSize);
	void write2File(FILE* pFp, unsigned char* pData[3], int iStride[2], int iWidth, int iHeight);
	void writeEncodedFile(FILE* pFp, uint8_t * pData, int size);
	void processFile(void* pDst[3], SBufferInfo* pInfo, FILE* pFp);
	int32_t getFrameNum();
	int32_t getIDRPicId();
	int32_t getTemporalId();
	int32_t getVCLNal();
	void restartPipeline();
	void playPipeline();
	void updateSps(mblk_t *sps);
	void updatePps(mblk_t *sps);
	bool checkSpsChange(mblk_t *sps);
	bool checkPpsChange(mblk_t *pps);
	
	void createSharedMem();
	void mapGstElementOntoSharedMem();
	void releaseSharedMem();
	
	static void startGstFeed(GstElement * pipeline, guint size);
	static void stopGstFeed(GstElement * pipeline);
	static gboolean pushGstBuffer();
	static guint sourceid;
	static int nalusSize;
	static MSQueue nalusQ;
	
	FILE *pYuvFile;
	FILE *pH264File;
	MSFilter *mFilter;
	Rfc3984Context *mUnpacker;
	MSPicture mOutbuf;
	MSAverageFPS mFPS;
	mblk_t *mSPS;
	mblk_t *mPPS;
	mblk_t *mYUVMsg;
	uint8_t *mBitstream;
	int mBitstreamSize;
	uint64_t mLastErrorReportTime;
	int mWidth;
	int mHeight;
	bool mInitialized;
	bool mFirstImageDecoded;
	bool mPipelinePlaying;
	bool mPipelineRestarted;
	bool mRestartPipeline;
	unsigned int frameNum;
	static GstElement *pipeline, *appsrc;
	static GstClockTime timestamp;
	GstElement *transform;
	
	struct mShmemBlock
	{
		int server_lock;
		int client_lock;
		int turn;
		int readlast;
		GstElement * gstDisplayElement;
	};
	int shmid;
	struct mShmemBlock * mblock;
};

#endif