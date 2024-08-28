#ifndef __TS_COMMON_H__
#define __TS_COMMON_H__

#include "CNodes.h"
#include <iostream>
#include <vector>

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
#define TS_TABLE_ID_PAT 0x00
#define TS_TABLE_ID_PMT 0x02
#define TS_TABLE_ID_MGT 0xc7
#define TS_TABLE_ID_TVCT 0xc8
#define TS_TABLE_ID_RRT 0xca
#define TS_TABLE_ID_EIT 0xcb
#define TS_TABLE_ID_ETT 0xcc
#define TS_TABLE_ID_STT 0xcd

//AUDIO CODEC
#define TS_AUDIO_TYPE_AAC 0x0f
#define TS_AUDIO_TYPE_AC3 0x81
#define TS_AUDIO_TYPE_EAC3 0x87
#define TS_AUDIO_TYPE_BSAC 0x11

//VIDEO
#define TS_VIDEO_TYPE_MPEG2 0x02

#define DESCRIPTOR_TAG_CONTENT_ADVISORY 0x87
#define DESCRIPTOR_TAG_ISO_639_LANGUAGE 0x0a
#define DESCRIPTOR_TAG_CAPTION 0x86

#define PSI_ONLY_PROGRAM_TIMEOUT    (1.5 * 1000 * 1000)


//SDL 플레이어 사이즈
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define SDL_USE 1

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
	PES_TYPE_NONE = 0,
	PES_TYPE_MPEG2_VIDEO,
	PES_TYPE_H265_VIDEO,
	PES_TYPE_AC3_AUDIO,
	PES_TYPE_EAC3_AUDIO,
	PES_TYPE_AAC_AUDIO,
	PES_TYPE_BSAC_AUDIO
} PES_TYPE;

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
	std::string language;
	unsigned char audio_type = 0;
	unsigned int crc32 = 0;
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


//플레이 관련
#define SIMPLE_QUEUE_LENGTH 1000
#pragma warning(push)
#pragma warning(disable : 4996)

class d3d_context {
public:
	HMODULE d3dlib;
	IDirect3D9* d3d;
	IDirect3DDevice9* d3ddevice;
};

class d3d_vertex {
public:
	float x, y, z;
	float rhw;
	D3DCOLOR color;
	float tu, tv;
};

class SimpleQueue
{
private:
	char m_Codec[SIMPLE_QUEUE_LENGTH][6];
	int m_TOI[SIMPLE_QUEUE_LENGTH];
	int m_Pos[SIMPLE_QUEUE_LENGTH];
	unsigned long long m_DecodeTime[SIMPLE_QUEUE_LENGTH];
	unsigned char* m_data[SIMPLE_QUEUE_LENGTH];
	unsigned int m_Length[SIMPLE_QUEUE_LENGTH];
	int m_SessionID[SIMPLE_QUEUE_LENGTH];
	int m_param1[SIMPLE_QUEUE_LENGTH];
	int m_param2[SIMPLE_QUEUE_LENGTH];
	int head;
	int tail;
public:
	SimpleQueue()
	{
		for (int i = 0; i < SIMPLE_QUEUE_LENGTH; i++)
		{
			m_Codec[i][0] = 0;
			m_TOI[i] = 0;
			m_Pos[i] = 0;
			m_DecodeTime[i] = 0;
			m_data[i] = 0;
			m_Length[i] = 0;
			m_SessionID[i] = -1;
			m_param1[i] = 0;
			m_param2[i] = 0;
		}
		head = 0;
		tail = 0;
	}
	bool push(const char* Codec, unsigned TOI, int pos, unsigned long long decode_time_us, unsigned int Length, unsigned char* data, int SessionID, int param1, int param2)
	{
		bool ret = true;

		int next_head = head + 1;
		if (SIMPLE_QUEUE_LENGTH <= next_head)
		{
			next_head = 0;
		}
		if (next_head == tail)
		{
			ret = false;

			printf("Simple Queue Full\n");
		}
		else
		{
			strcpy(m_Codec[head], Codec);
			m_TOI[head] = TOI;
			m_Pos[head] = pos;
			m_DecodeTime[head] = decode_time_us;
			m_data[head] = (unsigned char*)malloc(Length);
			if (m_data[head] == 0) return false;
			memcpy(m_data[head], data, Length);
			m_Length[head] = Length;
			m_SessionID[head] = SessionID;
			m_param1[head] = param1;
			m_param2[head] = param2;
			head = next_head;
		}

		return ret;
	}
	unsigned char* pop(char* Codec, int* TOI, int* pos, unsigned long long* decode_time_us, int* Length, int* SessionID, int* param1, int* param2)
	{
		unsigned char* ret = NULL;

		if (tail == head)
		{
			*Length = 0;
		}
		else
		{
			strcpy(Codec, m_Codec[tail]);
			*TOI = m_TOI[tail];
			*pos = m_Pos[tail];
			*decode_time_us = m_DecodeTime[tail];
			*Length = m_Length[tail];
			*SessionID = m_SessionID[tail];
			*param1 = m_param1[tail];
			*param2 = m_param2[tail];
			ret = m_data[tail];

			int next_tail = tail + 1;
			if (SIMPLE_QUEUE_LENGTH <= next_tail)
			{
				next_tail = 0;
			}
			tail = next_tail;
		}

		return ret;
	}
	void reset()
	{
		while (1)
		{
			char Codec[6];
			int TOI = 0;
			int pos = 0;
			unsigned long long us;
			int Length = 0;
			int SessionID = -1;
			int param1 = 0;
			int param2 = 0;
			unsigned char* ret = pop(Codec, &TOI, &pos, &us, &Length, &SessionID, &param1, &param2);
			if (ret == 0) break;
			if (ret) free(ret);
		}
		printf("RESET SIMPLE QUEUE\n");
	}
	int GetCountPercent()
	{
		int nCount = (tail <= head) ? (head - tail) : (SIMPLE_QUEUE_LENGTH + head - tail);

		return nCount * 100 / SIMPLE_QUEUE_LENGTH;
	}
	int GetHead()
	{
		return head;
	}
	int GetTail()
	{
		return tail;
	}
	~SimpleQueue()
	{
		reset();
		printf("DELETE SIMPLE QUEUE\n");
	}
};

#define PICTURE_FRAME_COUNT 240
class FrameQueue
{
private:
	AVPicture m_pict[PICTURE_FRAME_COUNT];
	unsigned short m_width[PICTURE_FRAME_COUNT];
	unsigned short m_height[PICTURE_FRAME_COUNT];
	unsigned long long m_us[PICTURE_FRAME_COUNT];
	int m_SessionID[PICTURE_FRAME_COUNT];
	int head;
	int tail;
	bool bStopPush;
public:
	FrameQueue()
	{
		memset(m_pict, 0, PICTURE_FRAME_COUNT * sizeof(AVPicture));
		memset(m_width, 0, PICTURE_FRAME_COUNT * sizeof(unsigned short));
		memset(m_height, 0, PICTURE_FRAME_COUNT * sizeof(unsigned short));
		memset(m_us, 0, PICTURE_FRAME_COUNT * sizeof(unsigned long long));
		memset(m_SessionID, 0, PICTURE_FRAME_COUNT * sizeof(int));
		head = 0;
		tail = 0;
		bStopPush = false;
	}
	~FrameQueue()
	{
		StopPush();
		AVPicture pict;
		unsigned short width = 0;
		unsigned short height = 0;
		unsigned long long us = 0;
		int SessionID = -1;
		while (pop(&pict, &us, &width, &height, &SessionID) == true)
		{
			if (us)
			{
				if (pict.data[0]) free(pict.data[0]);
				if (pict.data[1]) free(pict.data[1]);
				if (pict.data[2]) free(pict.data[2]);
			}
		}
	}
	void StopPush()
	{
		bStopPush = true;
	}
	bool push(AVPicture pict, unsigned long long us, unsigned short width, unsigned short height, int SessionID)
	{
		bool ret = true;

		if (bStopPush)
		{
			printf("Frame Push Stop\n");

			return false;
		}

		int next_head = head + 1;
		if (PICTURE_FRAME_COUNT <= next_head)
		{
			next_head = 0;
		}
		if (next_head == tail)
		{
			ret = false;

			printf("Frame Queue Full\n");
		}
		else
		{
			m_pict[head] = pict;
			m_width[head] = width;
			m_height[head] = height;
			m_us[head] = us;
			m_SessionID[head] = SessionID;
			head = next_head;
		}

		return ret;
	}
	bool pop(AVPicture* pict, unsigned long long* us, unsigned short* width, unsigned short* height, int* SessionID)
	{
		bool ret = true;

		if (tail == head)
		{
			ret = false;
		}
		else
		{
			*pict = m_pict[tail];
			*width = m_width[tail];
			*height = m_height[tail];
			*us = m_us[tail];
			*SessionID = m_SessionID[tail];
			int next_tail = tail + 1;
			if (PICTURE_FRAME_COUNT <= next_tail)
			{
				next_tail = 0;
			}
			tail = next_tail;
		}

		return ret;
	}
	int GetCountPercent()
	{
		int nCount = (tail <= head) ? (head - tail) : (SIMPLE_QUEUE_LENGTH + head - tail);

		return nCount * 100 / SIMPLE_QUEUE_LENGTH;
	}
	int GetHead()
	{
		return head;
	}
	int GetTail()
	{
		return tail;
	}
};
#pragma warning(pop)

#endif