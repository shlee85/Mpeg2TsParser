#pragma once
#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "TsCommon.h"
#include <stdio.h>
#include <iostream>

#include <chrono>  // std::chrono::seconds

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_system.h>
#include <SDL_syswm.h>

#include <thread>

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

class Player
{
	//SDL : 파싱된 데이터를 실시간으로 플레이하여 확인하기 위한 용도. SDL2버전을 사용하였다.
private:
#if SDL_USE
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	SDL_Texture* texture = nullptr;
	SDL_Event event;

	unsigned char* pixelData = nullptr;

	bool initializeSDL();
	void cleanUpSDL();
	void updateTexture(SDL_Texture* texture, unsigned char* pixelData, int width, int height);
	void renderFrame();

	//플레이어
	void avPlayerSDL(unsigned char* data, int size);
	void AvPlayerProc();
	
	static int VideoThread(void* ptr);
	static int AudioThread(void* ptr);
	static int ProcessAAC(int State = 0, int Pos = 0, unsigned long long us = 0, int nReceiveSize = 0, unsigned char* pReceiveBuff = 0, int SessionID = -1, int SampleRate = 0, int AudioNumberOfChannels = 0);
	static int ProcessAC3(int State = 0, int Pos = 0, unsigned long long us = 0, int nReceiveSize = 0, unsigned char* pReceiveBuff = 0, int SessionID = -1, int SampleRate = 0, int AudioNumberOfChannels = 0);

	static int ProcessMPEG2(int State, int TOI, int pos, unsigned long long us, int nReceiveSize, unsigned char* pReceiveBuff, int SessionID, int Width, int Height, void* ptr);
	static int ProcessH264(int State, int TOI, int pos, unsigned long long us, int nReceiveSize, unsigned char* pReceiveBuff, int SessionID, int Width, int Height, void* ptr);
	void d3d_copy_surface(IDirect3DDevice9* device, IDirect3DSurface9* src, RECT* src_rect, IDirect3DSurface9* dst, RECT* dst_rect);
	void d3d_present(IDirect3DDevice9* device);
	void d3d_fill_color(IDirect3DDevice9* device, IDirect3DSurface9* surface, RECT* rect, D3DCOLOR color);
	void d3d_free_surface(IDirect3DSurface9* surface);
	static void audio_callback(void* userdata, Uint8* stream, int len);

	static void AT3APP_AvCallbackSub(const char* codec, int TOI, int pos, unsigned long long decode_time_us, unsigned int data_length, unsigned char* data, int SessionID, unsigned long long minBufferTime, int param1, int param2);

	d3d_context* d3d_init(HWND hwnd, int backbuf_width, int backbuf_height);
	IDirect3DSurface9* d3d_create_surface_from_backbuffer(IDirect3DDevice9* device);
	IDirect3DSurface9* d3d_create_render_target_surface(IDirect3DDevice9* device, int width, int height, D3DFORMAT format);

	bool g_bForceSwDecoding = false; // DXVA2 사용하지 않는 모드

	void playerThread();
	void play();
	std::thread t;
#endif

public:
	Player();
	~Player();

	void start(std::string source, std::string output);
	void mainLoop();
};

//플레이 관련
#define SIMPLE_QUEUE_LENGTH 1000
#pragma warning(push)
#pragma warning(disable : 4996)


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

#define BYTE_QUEUE_LENGTH (1024*1024)
class sAUDIO_LR
{
public:
	unsigned int l;
	unsigned int r;
};

class ByteQueue
{
private:
	sAUDIO_LR m_data[BYTE_QUEUE_LENGTH];
	unsigned long long m_us[BYTE_QUEUE_LENGTH];
	int m_SessionID[BYTE_QUEUE_LENGTH];
	int head;
	int tail;
public:
	ByteQueue()
	{
		memset(m_data, 0, BYTE_QUEUE_LENGTH * sizeof(sAUDIO_LR));
		head = 0;
		tail = 0;
	}
	bool push(unsigned int lvalue, unsigned int rValue, unsigned long long us, int SessionID)
	{
		bool ret = true;

		int next_head = head + 1;
		if (BYTE_QUEUE_LENGTH <= next_head)
		{
			next_head = 0;
		}
		if (next_head == tail)
		{
			ret = false;

			printf("Byte Queue Full\n");
		}
		else
		{
			m_data[head].l = lvalue;
			m_data[head].r = rValue;
			m_us[head] = us;
			m_SessionID[head] = SessionID;
			head = next_head;
		}

		return ret;
	}
	bool pop(sAUDIO_LR* data, unsigned long long* us, int* SessionID) // 데이터가 없어도 0으로 반환
	{
		bool ret = true;

		if (tail == head)
		{
			ret = false;
		}
		else
		{
			*data = m_data[tail];
			*us = m_us[tail];
			*SessionID = m_SessionID[tail];
			int next_tail = tail + 1;
			if (BYTE_QUEUE_LENGTH <= next_tail)
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
	unsigned long long getTime_us()
	{
		unsigned long long ret;

		if (tail == head)
		{
			ret = 0;
		}
		else
		{
			ret = m_us[tail];
		}
		return ret;
	}
	int getSessionID()
	{
		return (tail == head) ? -1 : m_SessionID[tail];
	}
};
#pragma warning(pop)
#endif
