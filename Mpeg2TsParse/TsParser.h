#pragma once
#ifndef __TS_PARSER_H__
#define __TS_PARSER_H__

#include <stdio.h>
#include <iostream>
#include <fstream>

#include "TsCommon.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_system.h>
#include <SDL_syswm.h>
//#include <SDL_thread.h>

static void processMultipleString(unsigned char* ucData, unsigned int usLength,
	std::vector<MULTIPLE_STRING_TABLE>& multiple_string_table);


static bool bSystemRun = true;
static int g_SessionID = -1;
class TsParser
{
private:
	FILE* tsFile;
	uint8_t buffer[188];	
	int32_t currentPacket;

	const char* source_path;
	const char* output_path;
	std::string strServiceDataTitle;
	
	//unsigned long long getTimeUs();
	void processSectionData(int, int, unsigned short, unsigned char*, unsigned int);
	void processPatData(int rf, int serviceId, unsigned short pid, unsigned char* ucData, unsigned int usLength);
	void processPmtData(int rf, int serviceId, unsigned short pid, unsigned char* ucData, unsigned int usLength);
	void processPesData(int rf, int serviceId, unsigned short pid, unsigned char* ucData, unsigned int usLength);
	std::string processContentAdvisoryToString(int rf, std::vector<CONTENT_ADVISORY_TABLE>& content_advisory_table);

	void processAc3AudioEsData(int rf, int serviceId, unsigned long long pts, unsigned char* ucData, unsigned int usLength, unsigned long long buffer_time_us);
	void processAacAudioEsData(int rf, int serviceId, unsigned long long pts, unsigned char* ucData, unsigned int usLength, unsigned long long buffer_time_us);
	void processMpeg2VideoEsData(int rf, int serviceId, unsigned long long dts, unsigned long long pts, unsigned char* ucData, unsigned int usLength, unsigned long long buffer_time_us);
	void proccessMpeg2VideoEsUserData(unsigned char frame_type, unsigned char* ucData, unsigned int usLength);
	void proccessMpeg2VideoEsUserDataReorder(unsigned char frame_type, unsigned char* ucData, unsigned int usLength);
	void process_cc_data(unsigned char frame_type, unsigned char* ucData, unsigned int usLength);

	//SDL : 파싱된 데이터를 실시간으로 플레이하여 확인하기 위한 용도. SDL2버전을 사용하였다.
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
	static int ProcessMPEG2(int State, int TOI, int pos, unsigned long long us, int nReceiveSize, unsigned char* pReceiveBuff, int SessionID, int Width, int Height, void* ptr);
	void d3d_copy_surface(IDirect3DDevice9* device, IDirect3DSurface9* src, RECT* src_rect, IDirect3DSurface9* dst, RECT* dst_rect);
	void d3d_present(IDirect3DDevice9* device);
	void d3d_fill_color(IDirect3DDevice9* device, IDirect3DSurface9* surface, RECT* rect, D3DCOLOR color);
	void d3d_free_surface(IDirect3DSurface9* surface);

	static void AT3APP_AvCallbackSub(const char* codec, int TOI, int pos, unsigned long long decode_time_us, unsigned int data_length, unsigned char* data, int SessionID, unsigned long long minBufferTime, int param1, int param2);

	void mainLoop();

	d3d_context* d3d_init(HWND hwnd, int backbuf_width, int backbuf_height);
	IDirect3DSurface9* d3d_create_surface_from_backbuffer(IDirect3DDevice9* device);
	IDirect3DSurface9* d3d_create_render_target_surface(IDirect3DDevice9* device, int width, int height, D3DFORMAT format);

	bool g_bForceSwDecoding = false; // DXVA2 사용하지 않는 모드
#endif

public:
	TsParser(std::string, std::string);
	~TsParser();

	void Init();

public:
	//공통 SI정보
	std::vector<PID_TABLE> m_vec_pid_table;
	std::vector<PES_TABLE> m_vec_pes_table;
	std::vector<PCR_TABLE> m_vec_pcr_table;
	std::vector<PROGRAM_TABLE> m_vec_program_table;
	std::vector<STREAM_TABLE> m_vec_stream_table;
	std::vector<MASTER_GUIDE_TABLE> m_vec_master_guide_table;
	std::vector<EPG_TABLE> m_vec_epg_table;
	std::vector<RATING_TABLE> m_vec_rating_table;

	CNodes* nodesVideo = nullptr;
	CNodes* nodesAudio = nullptr;

	const char* output_video = "output_video2.mpg";
	const char* output_audio = "output_audio.ac3";
	std::ofstream esFile;// (output_filename, std::ios::binary);
	std::ofstream esFileAudio;// (output_filename, std::ios::binary);	
};


#endif

