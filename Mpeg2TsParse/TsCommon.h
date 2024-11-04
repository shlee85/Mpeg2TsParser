#ifndef __TS_COMMON_H__
#define __TS_COMMON_H__

#include "CNodes.h"
#include <iostream>
#include <vector>
#include <iomanip>

#include <zlib.h>
extern "C" {	//C로 만들어진 라이브러리를 참조 하기 위함.
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
}

#include <d3d9.h>

#define PID_PAT 0x00
#define PID_PSIP 0x1ffb

//PSI
#define TS_TABLE_ID_PAT								0x00
#define TS_TABLE_ID_PMT								0x02
#define TS_TABLE_ID_ODT								0x05
#define TS_TABLE_ID_MGT								0xc7
#define TS_TABLE_ID_TVCT							0xc8
#define TS_TABLE_ID_RRT								0xca
#define TS_TABLE_ID_EIT								0xcb
#define TS_TABLE_ID_ETT								0xcc
#define TS_TABLE_ID_STT								0xcd

//AUDIO CODEC
#define TS_AUDIO_TYPE_AAC							0x0f
#define TS_AUDIO_TYPE_AC3							0x81
#define TS_AUDIO_TYPE_EAC3							0x87
#define TS_AUDIO_TYPE_BSAC							0x11
#define STREAM_TYPE_MPEG_4_GENERIC					0x12 
#define STREAM_TYPE_ISO_14496_1_SL_PKT				0x13

//VIDEO
#define TS_VIDEO_TYPE_MPEG2							0x02
#define TS_VIDEO_TYPE_H264							0x1b

#define DESCRIPTOR_TAG_CONTENT_ADVISORY				0x87
#define DESCRIPTOR_TAG_ISO_639_LANGUAGE				0x0a
#define DESCRIPTOR_TAG_CAPTION						0x86

#define PSI_ONLY_PROGRAM_TIMEOUT					(1.5 * 1000 * 1000)

//DMB
#define DESCRIPTOR_TAG_ISO_639_LANGUAGE				0x0a
#define DESCRIPTOR_TAG_AC3_AUDIO					0x81
#define DESCRIPTOR_TAG_EAC3_AUDIO					0xCC
#define DESCRIPTOR_TAG_CAPTION						0x86
#define DESCRIPTOR_TAG_CONTENT_ADVISORY				0x87
#define DESCRIPTOR_TAG_INITIAL_OBJECT				0x1D
#define DESCRIPTOR_TAG_SYNC_LAYER					0x1E

#define DESCRIPTOR_TAG_MPEG_4_OBJECT				0x01
#define DESCRIPTOR_TAG_MPEG_4_INITIAL_OBJECT		0x02
#define DESCRIPTOR_TAG_MPEG_4_ELEMENTARY_STREAM		0x03
#define DESCRIPTOR_TAG_MPEG_4_DECODER_CONFIG		0x04
#define DESCRIPTOR_TAG_MPEG_4_DECODER_SPECIFIC_INFO	0x05
#define DESCRIPTOR_TAG_MPEG_4_SYNC_LAYER_CONFIG		0x06

#define MPEG_4_OBJECT_TYPE_VISUAL_ISO_14496_10		0x21
#define MPEG_4_OBJECT_TYPE_AUDIO_ISO_14496_3		0x40


//SDL 플레이어 사이즈
#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define SDL_USE 1

#define H264 0	//H264 코덱전용, 사용안하면 DMB용도로~

typedef enum {
	PID_TYPE_NONE = 0,
	PID_TYPE_SECTION,
	PID_TYPE_PES
} PID_TYPE;

class PID_TABLE {
public:
	int rf = 0;
	unsigned short pid = 0;
	bool payload_unit_start = false;
	unsigned char continuity_counter = 0;
	PID_TYPE type = PID_TYPE_NONE;
	CNodes* nodes = nullptr;
};

typedef enum {
	CODEC_TYPE_NONE = 0,
	CODEC_TYPE_MPEG2_VIDEO,
	CODEC_TYPE_H264_VIDEO,
	CODEC_TYPE_H265_VIDEO,
	CODEC_TYPE_AC3_AUDIO,
	CODEC_TYPE_EAC3_AUDIO,
	CODEC_TYPE_AAC_AUDIO,
	CODEC_TYPE_BSAC_AUDIO
} CODEC_TYPE;


typedef enum {
	PES_TYPE_NONE = 0,
	PES_TYPE_MPEG2_VIDEO,
	PES_TYPE_H264_VIDEO,
	PES_TYPE_H265_VIDEO,
	PES_TYPE_AC3_AUDIO,
	PES_TYPE_EAC3_AUDIO,
	PES_TYPE_AAC_AUDIO,
	PES_TYPE_MPEG_4_GENERIC_H264_VIDEO,
	PES_TYPE_MPEG_4_GENERIC_BSAC_AUDIO
} PES_TYPE;

class SYNC_LAYER_CONFIG {
public:
	unsigned char useAccessUnitStartFlag = 0;
	unsigned char useAccessUnitEndFlag = 0;
	unsigned char useRandomAccessPointFlag = 0;
	unsigned char hasRandomAccessUnitsOnlyFlag = 0;
	unsigned char usePaddingFlag = 0;
	unsigned char useTimeStampsFlag = 0;
	unsigned char useIdleFlag = 0;
	unsigned char durationFlag = 0;
	unsigned int timeStampResolution = 0;
	unsigned int OCRResolution = 0;
	unsigned char timeStampLength = 0;
	unsigned char OCRLength = 0;
	unsigned char AU_Length = 0;
	unsigned char instantBitrateLength = 0;
	unsigned char degradationPriorityLength = 0;
	unsigned char AU_seqNumLength = 0;
	unsigned char packetSeqNumLength = 0;

	unsigned int timeScale = 0;
	unsigned short accessUnitDuration = 0;
	unsigned short compositionUnitDuration = 0;

	unsigned long long startDecodingTimeStamp = 0;
	unsigned long long startCompositionTimeStamp = 0;
};

class PES_TABLE {
public:
	int rf = 0;
	unsigned short pid = 0;
	PES_TYPE type = PES_TYPE_NONE;
};

class PCR_TABLE {
public:
	int rf = 0;
	unsigned short program_number = 0;
	unsigned short pcr_pid = 0;
	unsigned long long pcr = 0;
	unsigned long long pcr_receive_time_us = 0;
	unsigned long long buffer_time_us = 0;
	unsigned int crc32 = 0;
};

class PROGRAM_TABLE {
public:
	int rf = 0;
	unsigned short program_number = 0;
	unsigned short program_map_pid = 0;
	unsigned short major_channel_number = 0;
	unsigned short minor_channel_number = 0;
	unsigned short short_name[7] = { 0, 0, 0, 0, 0, 0, 0 };
	std::string name;
	unsigned char hidden = 0;
	unsigned char service_type = 0;
	unsigned short source_id = 0;
	bool available = false;
	unsigned long long add_time_us = 0;
	unsigned int crc32 = 0;
};

class STREAM_TABLE {
public:
	int rf = 0;
	unsigned short program_number = 0;
	unsigned short elementary_pid = 0;
	unsigned char stream_type = 0;
	unsigned char codec_type = 0;
	unsigned short es_id = 0;
	std::string init_data;
	std::string language;
	unsigned char audio_type = 0;
	unsigned int crc32 = 0;
	int sample_freq = 0;
	int channel_conf = 0;
	SYNC_LAYER_CONFIG sl_config;
};

class MASTER_GUIDE_TABLE {
public:
	int rf = 0;
	unsigned short table_type = 0;
	unsigned short table_type_pid = 0;
	unsigned int crc32 = 0;
};

class CONTENT_ADVISORY_DIMENSION_TABLE {
public:
	unsigned char dimension = 0;
	unsigned char value = 0;
};

class CONTENT_ADVISORY_TABLE {
public:
	unsigned char region = 0;
	std::vector<CONTENT_ADVISORY_DIMENSION_TABLE> dimension_table;
};

class MULTIPLE_STRING_TABLE {
public:
	std::string iso_639_language_code;
	std::string string;
};

class EPG_TABLE {
public:
	int rf = 0;
	unsigned short source_id = 0;
	unsigned short event_id = 0;
	unsigned int start_time = 0;
	unsigned int length = 0;
	std::vector<CONTENT_ADVISORY_TABLE> content_advisory_table;
	std::vector<MULTIPLE_STRING_TABLE> title;
	std::vector<MULTIPLE_STRING_TABLE> desc;
	std::string desc_ori;
};

class RATING_VALUE_TABLE {
public:
	std::vector<MULTIPLE_STRING_TABLE> abbrev_value;
	std::vector<MULTIPLE_STRING_TABLE> value;
};

class RATING_DIMENSION_TABLE {
public:
	std::vector<MULTIPLE_STRING_TABLE> dimension_name;
	bool graduated_scale = false;
	std::vector<RATING_VALUE_TABLE> value_table;
};

class RATING_TABLE {
public:
	int rf = 0;
	unsigned char region = 0;
	std::vector<MULTIPLE_STRING_TABLE> region_name;
	std::vector<RATING_DIMENSION_TABLE> dimension_table;
	unsigned int crc32 = 0;
};

class ISO_639_LANGUAGE_TABLE {
public:
	std::string iso_639_language_code;
	unsigned char audio_type = 0;
};

class SYNC_LAYER_PACKET_HEADER {
public:
	unsigned long long objectClockReference = 0;
	unsigned long long decodingTimeStamp = 0;
	unsigned long long compositionTimeStamp = 0;
};


#endif