#pragma once
#ifndef __TS_PARSER_H__
#define __TS_PARSER_H__

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <thread>

#include "TsCommon.h"
#include "HvccParser.h"

static void processMultipleString(unsigned char* ucData, unsigned int usLength,
	std::vector<MULTIPLE_STRING_TABLE>& multiple_string_table);

unsigned long long getTimeUs();

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
	void processH264VideoEsData(int rf, int serviceId, unsigned long long dts, unsigned long long pts, unsigned char* ucData, unsigned int usLength, unsigned long long buffer_time_us);
	void processMpeg2VideoEsData(int rf, int serviceId, unsigned long long dts, unsigned long long pts, unsigned char* ucData, unsigned int usLength, unsigned long long buffer_time_us);
	void proccessMpeg2VideoEsUserData(unsigned char frame_type, unsigned char* ucData, unsigned int usLength);
	void proccessMpeg2VideoEsUserDataReorder(unsigned char frame_type, unsigned char* ucData, unsigned int usLength);
	void process_cc_data(unsigned char frame_type, unsigned char* ucData, unsigned int usLength);
	
	int g_SessionID = -1;

	void test();

	std::thread parserThread;
	void (*g_av_callback)(const char* codec, int TOI, int pos, unsigned long long decode_time_us,
		unsigned int data_length, unsigned char* data, int SessionID, unsigned long long minBufferTime, int param1, int param2)	= nullptr;

public:
	TsParser(std::string, std::string);
	~TsParser();

	void startThread(void(*av_callback)(const char* codec, int TOI, int pos, unsigned long long decode_time_us,
		unsigned int data_length, unsigned char* data, int SessionID, unsigned long long minBufferTime, int param1, int param2));
	void joinThread();
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

	const char* output_video = "h264_video.mp4";
	const char* output_audio = "output_audio.ac3";
	std::ofstream esFile;// (output_filename, std::ios::binary);
	std::ofstream esFileAudio;// (output_filename, std::ios::binary);

	HvCCParser *hvccParser = nullptr;
};
#endif

