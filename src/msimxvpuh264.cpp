
/*
* Interface that combines OpenH264 encoder methods
* and GStreamer decoder ones
* 
* Filename: msimxvpuh264.cpp
* Author: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
* Created: Mon Jul 11 14:56:30 2016 +0600
* Version: 1.0
* Last-Updated: Mon Jul 11 14:56:30 2016 +0600
* By: Dmitriy Rybalkin <dmitriy.rybalkin@gmail.com>
*
* Target platform for cross-compilation: ARCH=arm && proc=imx6
*/
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/msvideo.h>
#include <mediastreamer2/rfc3984.h>

#include "msimxvpuh264dec.h"
#include "msimxvpuh264enc.h"

#ifndef VERSION
#define VERSION "0.1.0"
#endif

/* 
 * Implementation of decoder
 */
static void msimxvpuh264_dec_init(MSFilter *f) {
	MSIMXVPUH264Decoder *decoder = new MSIMXVPUH264Decoder(f);
	f->data = decoder;
}

static void msimxvpuh264_dec_preprocess(MSFilter *f) {
	MSIMXVPUH264Decoder *decoder = static_cast<MSIMXVPUH264Decoder *>(f->data);
	decoder->initialize();
}

static void msimxvpuh264_dec_process(MSFilter *f) {
	MSIMXVPUH264Decoder *decoder = static_cast<MSIMXVPUH264Decoder *>(f->data);
	decoder->process();
}

static void msimxvpuh264_dec_uninit(MSFilter *f) {
	MSIMXVPUH264Decoder *decoder = static_cast<MSIMXVPUH264Decoder *>(f->data);
	decoder->uninitialize();
	delete decoder;
}

/*
 * Methods to configure the decoder
 */
static int msimxvpuh264_dec_add_fmtp(MSFilter *f, void *arg) {
	MSIMXVPUH264Decoder *decoder = static_cast<MSIMXVPUH264Decoder *>(f->data);
	const char *fmtp = static_cast<const char *>(arg);
	char value[256];
	if (fmtp_get_value(fmtp, "sprop-parameter-sets", value, sizeof(value))) {
		decoder->provideSpropParameterSets(value, sizeof(value));
	}
	return 0;
}

static int msimxvpuh264_dec_reset_first_image(MSFilter *f, void *arg) {
	MSIMXVPUH264Decoder *decoder = static_cast<MSIMXVPUH264Decoder *>(f->data);
	decoder->resetFirstImageDecoded();
	return 0;
}

static int msimxvpuh264_dec_get_size(MSFilter *f, void *arg) {
	MSIMXVPUH264Decoder *decoder = static_cast<MSIMXVPUH264Decoder *>(f->data);
	MSVideoSize *size = static_cast<MSVideoSize *>(arg);
	*size = decoder->getSize();
	return 0;
}

static int msimxvpuh264_dec_get_fps(MSFilter *f, void *arg){
	MSIMXVPUH264Decoder *decoder = static_cast<MSIMXVPUH264Decoder *>(f->data);
	*(float*)arg = decoder->getFps();
	return 0;
}

static int msimxvpuh264_dec_get_out_fmt(MSFilter *f, void *arg){
	MSIMXVPUH264Decoder *decoder = static_cast<MSIMXVPUH264Decoder *>(f->data);
	((MSPinFormat*)arg)->pin = 0;
	((MSPinFormat*)arg)->fmt = decoder->getOutFmt();
	return 0;
}

static MSFilterMethod msimxvpuh264_dec_methods[] = {
	{ MS_FILTER_ADD_FMTP,                              msimxvpuh264_dec_add_fmtp          },
	{ MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION, msimxvpuh264_dec_reset_first_image },
	{ MS_FILTER_GET_VIDEO_SIZE,                        msimxvpuh264_dec_get_size          },
	{ MS_FILTER_GET_FPS,                               msimxvpuh264_dec_get_fps           },
	{ MS_FILTER_GET_OUTPUT_FMT,                        msimxvpuh264_dec_get_out_fmt       },
	{ 0,                                               NULL                             }
};

/*
 * Definition of the decoder
 */

#define MSIMXVPUH264_DEC_NAME		"MSIMXVPUH264Dec"
#define MSIMXVPUH264_DEC_DESCRIPTION	"GStreamer with Freescale iMX VPU Coder/Decoder"
#define MSIMXVPUH264_DEC_CATEGORY	MS_FILTER_DECODER
#define MSIMXVPUH264_DEC_ENC_FMT	"H264"
#define MSIMXVPUH264_DEC_NINPUTS	1
#define MSIMXVPUH264_DEC_NOUTPUTS	1
#define MSIMXVPUH264_DEC_FLAGS		0

#ifndef _MSC_VER

MSFilterDesc msimxvpuh264_dec_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = MSIMXVPUH264_DEC_NAME,
	.text = MSIMXVPUH264_DEC_DESCRIPTION,
	.category = MSIMXVPUH264_DEC_CATEGORY,
	.enc_fmt = MSIMXVPUH264_DEC_ENC_FMT,
	.ninputs = MSIMXVPUH264_DEC_NINPUTS,
	.noutputs = MSIMXVPUH264_DEC_NOUTPUTS,
	.init = msimxvpuh264_dec_init,
	.preprocess = msimxvpuh264_dec_preprocess,
	.process = msimxvpuh264_dec_process,
	.postprocess = NULL,
	.uninit = msimxvpuh264_dec_uninit,
	.methods = msimxvpuh264_dec_methods,
	.flags = MSIMXVPUH264_DEC_FLAGS
};

#else

MSFilterDesc msimxvpuh264_dec_desc = {
	MS_FILTER_PLUGIN_ID,
	MSIMXVPUH264_DEC_NAME,
	MSIMXVPUH264_DEC_DESCRIPTION,
	MSIMXVPUH264_DEC_CATEGORY,
	MSIMXVPUH264_DEC_ENC_FMT,
	MSIMXVPUH264_DEC_NINPUTS,
	MSIMXVPUH264_DEC_NOUTPUTS,
	msimxvpuh264_dec_init,
	msimxvpuh264_dec_preprocess,
	msimxvpuh264_dec_process,
	NULL,
	msimxvpuh264_dec_uninit,
	msimxvpuh264_dec_methods,
	MSIMXVPUH264_DEC_FLAGS
};

#endif

/* Implementation of encoder */

static void msimxvpuh264_enc_init(MSFilter *f) {
	MSOpenH264Encoder *e = new MSOpenH264Encoder(f);
	f->data = e;
}

static void msimxvpuh264_enc_preprocess(MSFilter *f) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	e->initialize();
}

static void msimxvpuh264_enc_process(MSFilter *f) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	e->feed();
}

static void msimxvpuh264_enc_postprocess(MSFilter *f) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	e->uninitialize();
}

static void msimxvpuh264_enc_uninit(MSFilter *f) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	delete e;
}

/* 
 * Methods to configure the encoder
 */

static int msimxvpuh264_enc_set_fps(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	float *fps = static_cast<float *>(arg);
	e->setFPS(*fps);
	return 0;
}

static int msimxvpuh264_enc_get_fps(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	float *fps = static_cast<float *>(arg);
	*fps = e->getFPS();
	return 0;
}

static int msimxvpuh264_enc_set_bitrate(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	int *bitrate = static_cast<int *>(arg);
	e->setBitrate(*bitrate);
	return 0;
}

static int msimxvpuh264_enc_get_bitrate(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	int *bitrate = static_cast<int *>(arg);
	*bitrate = e->getBitrate();
	return 0;
}

static int msimxvpuh264_enc_set_vsize(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
	e->setSize(*vsize);
	return 0;
}

static int msimxvpuh264_enc_get_vsize(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
	*vsize = e->getSize();
	return 0;
}

static int msimxvpuh264_enc_add_fmtp(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	const char *fmtp = static_cast<const char *>(arg);
	e->addFmtp(fmtp);
	return 0;
}

static int msimxvpuh264_enc_req_vfu(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	e->requestVFU();
	return 0;
}

static int msimxvpuh264_enc_get_configuration_list(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	const MSVideoConfiguration **vconf_list = static_cast<const MSVideoConfiguration **>(arg);
	*vconf_list = e->getConfigurationList();
	return 0;
}

static int msimxvpuh264_enc_set_configuration(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	MSVideoConfiguration *vconf = static_cast<MSVideoConfiguration *>(arg);
	e->setConfiguration(*vconf);
	return 0;
}

static MSFilterMethod msimxvpuh264_enc_methods[] = {
	{ MS_FILTER_SET_FPS,                       msimxvpuh264_enc_set_fps                },
	{ MS_FILTER_GET_FPS,                       msimxvpuh264_enc_get_fps                },
	{ MS_FILTER_SET_BITRATE,                   msimxvpuh264_enc_set_bitrate            },
	{ MS_FILTER_GET_BITRATE,                   msimxvpuh264_enc_get_bitrate            },
	{ MS_FILTER_SET_VIDEO_SIZE,                msimxvpuh264_enc_set_vsize              },
	{ MS_FILTER_GET_VIDEO_SIZE,                msimxvpuh264_enc_get_vsize              },
	{ MS_FILTER_ADD_FMTP,                      msimxvpuh264_enc_add_fmtp               },
	{ MS_FILTER_REQ_VFU,                       msimxvpuh264_enc_req_vfu                },
#ifdef MS_VIDEO_ENCODER_REQ_VFU
	{ MS_VIDEO_ENCODER_REQ_VFU,                msimxvpuh264_enc_req_vfu                },
#endif
	{ MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST, msimxvpuh264_enc_get_configuration_list },
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION,      msimxvpuh264_enc_set_configuration      },
	{ 0,                                       NULL                                  }
};

#define MSIMXVPUH264_ENC_NAME        	"MSIMXVPUH264Enc"
#define MSIMXVPUH264_ENC_DESCRIPTION 	"GStreamer with Freescale iMX VPU Coder/Decoder"
#define MSIMXVPUH264_ENC_CATEGORY    	MS_FILTER_ENCODER
#define MSIMXVPUH264_ENC_FMT     	"H264"
#define MSIMXVPUH264_ENC_NINPUTS     	1
#define MSIMXVPUH264_ENC_NOUTPUTS    	1
#define MSIMXVPUH264_ENC_FLAGS       	0

#ifndef _MSC_VER

MSFilterDesc msimxvpuh264_enc_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = MSIMXVPUH264_ENC_NAME,
	.text = MSIMXVPUH264_ENC_DESCRIPTION,
	.category = MSIMXVPUH264_ENC_CATEGORY,
	.enc_fmt = MSIMXVPUH264_ENC_FMT,
	.ninputs = MSIMXVPUH264_ENC_NINPUTS,
	.noutputs = MSIMXVPUH264_ENC_NOUTPUTS,
	.init = msimxvpuh264_enc_init,
	.preprocess = msimxvpuh264_enc_preprocess,
	.process = msimxvpuh264_enc_process,
	.postprocess = msimxvpuh264_enc_postprocess,
	.uninit = msimxvpuh264_enc_uninit,
	.methods = msimxvpuh264_enc_methods,
	.flags = MSIMXVPUH264_ENC_FLAGS
};

#else

MSFilterDesc msimxvpuh264_enc_desc = {
	MS_FILTER_PLUGIN_ID,
	MSIMXVPUH264_ENC_NAME,
	MSIMXVPUH264_ENC_DESCRIPTION,
	MSIMXVPUH264_ENC_CATEGORY,
	MSIMXVPUH264_ENC_FMT,
	MSIMXVPUH264_ENC_NINPUTS,
	MSIMXVPUH264_ENC_NOUTPUTS,
	msimxvpuh264_enc_init,
	msimxvpuh264_enc_preprocess,
	msimxvpuh264_enc_process,
	msimxvpuh264_enc_postprocess,
	msimxvpuh264_enc_uninit,
	msimxvpuh264_enc_methods,
	MSIMXVPUH264_ENC_FLAGS
};

#endif

#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) extern "C" __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) extern "C" type
#endif

MS_PLUGIN_DECLARE(void) libmsimxvpuh264_init(void) {
	ms_filter_register(&msimxvpuh264_enc_desc);
	ms_filter_register(&msimxvpuh264_dec_desc);
	ms_message("msimxvpuh264-" VERSION " plugin, based on GStreamer and OpenH264 libraries, has been registered.");
}