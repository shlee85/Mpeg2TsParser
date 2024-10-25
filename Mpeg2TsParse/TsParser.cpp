#include "TsParser.h"
#include "TsCommon.h"
#include "CByteBuffer.h"
#include "CBitBuffer.h"
#include "HvccParser.h"

#include <thread>  // std::this_thread::sleep_for
#include <chrono>  // std::chrono::seconds
#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>

int count = 0;
TsParser::TsParser(std::string source, std::string output) : source_path(source.c_str()), output_path(output.c_str()) {
	std::cout << "source path = " << source_path << std::endl;

	errno_t err = fopen_s(&tsFile, source_path, "rb");
	if (err == 0) {
		std::cout << "Success file open" << std::endl;
	}
	else {
		std::cout << "Failure file open" << std::endl;
	}

	currentPacket = 0;

	m_vec_pid_table.clear();
	m_vec_pes_table.clear();
	m_vec_pcr_table.clear();
	m_vec_program_table.clear();

	memset(buffer, 0x00, sizeof(buffer));

	nodesAudio = new CNodes;
	nodesVideo = new CNodes;

	esFile.open(output_video, std::ios::binary);
	esFileAudio.open(output_audio, std::ios::binary);

	hvccParser = new HvCCParser();
}

TsParser::~TsParser() {
	fclose(tsFile);
	std::cout << "Done!, " << output_path << std::endl;
}

unsigned long long getTimeUs() {
	std::chrono::system_clock::time_point StartTime;
	std::chrono::system_clock::time_point EndTime = std::chrono::system_clock::now();

	std::chrono::microseconds micro = std::chrono::duration_cast<std::chrono::microseconds>(
		EndTime - StartTime);

	return micro.count();
}

#pragma warning(push)
#pragma warning(disable:4996)

void TsParser::test() {
	int i = 0;
	TsParser::Init();
	/*while (1) {
		printf("parser Thread = %d\n",i++);
		Sleep(1000);
	}*/
}

void TsParser::startThread(
	void(*av_callback)(const char* codec, int TOI, int pos, unsigned long long decode_time_us, 
		unsigned int data_length, unsigned char* data, int SessionID, unsigned long long minBufferTime, int param1, int param2)
) {
	g_av_callback = av_callback;
	parserThread = std::thread (&TsParser::test, this);
}

void TsParser::joinThread() {
	if (parserThread.joinable()) {
		parserThread.join();
	}
}

void TsParser::Init() {
	std::cout << "init!!!" << std::endl;

	PID_TABLE pid_table;
	PCR_TABLE pcr_table;

	unsigned int rf = 63000000;
	unsigned int serviceId = 256;	//입력 pid값 파싱할 때 굉장히 중요함.

	for (PID_TABLE& table : m_vec_pid_table) {
		delete table.nodes;
	}
	m_vec_pid_table.clear();

	//초기화 (Live에서는 다르다. luna참고)
	pid_table.rf = rf;
	pid_table.pid = PID_PAT;
	pid_table.payload_unit_start = false;
	pid_table.continuity_counter = 0;
	pid_table.type = PID_TYPE_SECTION;
	pid_table.nodes = new CNodes;
	m_vec_pid_table.push_back(pid_table);

	pid_table.rf = rf;
	pid_table.pid = PID_PSIP;
	pid_table.payload_unit_start = false;
	pid_table.continuity_counter = 0;
	pid_table.type = PID_TYPE_SECTION;
	pid_table.nodes = new CNodes;
	m_vec_pid_table.push_back(pid_table);

	while (!feof(tsFile)) {
		size_t readTs = fread(&buffer, 1, 188, tsFile);
		//std::cout << "readTs : " << readTs << std::endl;
		//std::cout << "bufferSize = " << buffer.length() << std::endl;

		if (readTs == 0) {
			if (feof(tsFile)) {
				std::cout << "파일의 끝에 도달 함" << std::endl;
				break;
			}
			else if (ferror(tsFile)) {
				std::cerr << "읽기 오류가 발생" << std::endl;
				break;
			}
		}

		if ((readTs % 188) != 0) {
			std::cerr << "읽은 데이터 크기가 예상과 다름. " << std::endl;
			break;
		}

		for (PROGRAM_TABLE& table : m_vec_program_table) {
			//printf("g_program_table = [%hu]\n", table.program_number);
			if (rf != table.rf)
				continue;

			if (serviceId != table.program_number)
				continue;

			std::stringstream strRet;
			strRet << table.major_channel_number << "." << table.minor_channel_number << " "
				<< table.name.c_str();

			if (strRet.str() != strServiceDataTitle) {
				strServiceDataTitle = strRet.str();
				//LOGD("ATSC1.0 title: %s\n", strServiceDataTitle.c_str());
			}
		}

		auto& v = m_vec_pid_table;
		for (size_t i = v.size(); 0 < i; i--) {
			if (rf != v[i - 1].rf)
				continue;
			if (v[i - 1].pid == PID_PAT || v[i - 1].pid == PID_PSIP)
				continue;
			bool bFind = false;
			for (STREAM_TABLE& table : m_vec_stream_table) {
				if (rf != table.rf)
					continue;
				if (v[i - 1].pid == table.elementary_pid) {
					bFind = true;
					break;
				}
			}
			for (PROGRAM_TABLE& table : m_vec_program_table) {
				if (rf != table.rf)
					continue;
				if (v[i - 1].pid == table.program_map_pid) {
					bFind = true;
					break;
				}
			}
			for (MASTER_GUIDE_TABLE& table : m_vec_master_guide_table) {
				if (rf != table.rf)
					continue;
				if (v[i - 1].pid == table.table_type_pid) {
					bFind = true;
					break;
				}
			}
			if (!bFind) {
				if (v[i - 1].type == PID_TYPE_PES) {
					auto& vv = m_vec_pes_table;
					for (size_t j = vv.size(); 0 < j; j--) {
						if (rf != vv[j - 1].rf)
							continue;
						if (v[i - 1].pid == vv[j - 1].pid) {
							//printf("Erase elementary PID 0x%x\n", vv[j - 1].pid);

							vv.erase(vv.begin() + (int)j - 1);
							break;
						}
					}
				}
				else {
					//printf("Erase section PID 0x%x\n", v[i - 1].pid);
				}
				delete v[i - 1].nodes;
				v.erase(v.begin() + (int)i - 1);
			}
		}

		for (STREAM_TABLE& table : m_vec_stream_table) {
			if (rf != table.rf)
				continue;
			if (serviceId != table.program_number)
				continue;
			if (table.stream_type != TS_VIDEO_TYPE_MPEG2 && table.stream_type != TS_VIDEO_TYPE_H264)
				continue;
			unsigned short elementary_pid = table.elementary_pid;
			bool bFind = false;
			for (PID_TABLE& table2 : m_vec_pid_table) {
				if (rf != table2.rf || elementary_pid != table2.pid) continue;
				bFind = true;
				break;
			}
			if (!bFind) {
				g_SessionID = (g_SessionID > 100000) ? 0 : g_SessionID + 1;
			#if 0
				if (g_av_callback)
					g_av_callback(this, tunerId, rf, serviceId, "reset", 0, 0, 0, 0, 0, nullptr,
						g_SessionID, 0, 0, 0, 0, 0,
						""); // AV RESET for Channel Change or Source Change
			#endif
				std::stringstream strXML;
				strXML << "<reset demod='8vsb' rf='" << rf << "' sid='" << serviceId
					<< "' sessionid='" << g_SessionID << "' />";
				//dataCallback("reset", strXML);
				std::cout << "dataCallback reset:" << strXML.str() << std::endl;
				std::cout << "nodesVideo->reset!!!" << std::endl;
				nodesVideo->reset();

				PES_TABLE pes_table;
				pes_table.rf = rf;
				pes_table.pid = elementary_pid;
				pes_table.type = (table.stream_type == TS_VIDEO_TYPE_MPEG2) ? PES_TYPE_MPEG2_VIDEO : PES_TYPE_H264_VIDEO;	//MPEG2가 아니면 H264로 처리 한다.
				m_vec_pes_table.push_back(pes_table);

				PID_TABLE pid_table;
				pid_table.rf = rf;
				pid_table.pid = elementary_pid;
				pid_table.payload_unit_start = false;
				pid_table.continuity_counter = 0;
				pid_table.type = PID_TYPE_PES;
				pid_table.nodes = new CNodes;
				m_vec_pid_table.push_back(pid_table);

				printf("Add video elementary PID 0x%x\n", elementary_pid);
			}
			break;
		}

		//SelectAudioID_PMT(rf, serviceId);	//오디오 언어 선택 관련 처리 ( 국내향 / 미주향별 )
		for (STREAM_TABLE& table : m_vec_stream_table) {
			if (rf != table.rf) {				
				continue;
			}
			if (serviceId != table.program_number) {			
				continue;
			}
			if (table.stream_type != TS_AUDIO_TYPE_AAC && table.stream_type != TS_AUDIO_TYPE_AC3 
				&& table.stream_type != TS_AUDIO_TYPE_BSAC ) {			
				continue;
			}
			//if (std::to_string(table.elementary_pid) != GetSelectedAudioID())
			//	continue;
			unsigned short elementary_pid = table.elementary_pid;
			bool bFind = false;
			for (PID_TABLE& table2 : m_vec_pid_table) {
				if (rf != table2.rf) continue;
				if (elementary_pid != table2.pid) continue;
				bFind = true;
				break;
			}
			if (!bFind) {
				g_SessionID = (g_SessionID > 100000) ? 0 : g_SessionID + 1;
			#if 0
				if (g_av_callback)
					g_av_callback(this, tunerId, rf, serviceId, "reset", 0, 0, 0, 0, 0, nullptr,
						g_SessionID, 0, 0, 0, 0, 0,
						""); // AV RESET for Channel Change or Source Change
			#endif
				std::stringstream strXML;
				strXML << "<reset demod='8vsb' rf='" << rf << "' sid='" << serviceId
					<< "' sessionid='" << g_SessionID << "' />";
				//dataCallback("reset", strXML);
				std::cout << "AUDIO?dataCallback reset:" << strXML.str() << std::endl;
				nodesAudio->reset();

				auto& v2 = m_vec_pes_table;
				for (size_t i = v2.size(); 0 < i; i--) {
					if (rf != v2[i - 1].rf)
						continue;
					if (PES_TYPE_AC3_AUDIO == v2[i - 1].type ||
						PES_TYPE_BSAC_AUDIO == v2[i - 1].type ||
						PES_TYPE_AAC_AUDIO == v2[i - 1].type) {
						auto& vv = m_vec_pid_table;
						for (size_t j = vv.size(); 0 < j; j--) {
							if (rf != vv[j - 1].rf)
								continue;
							if (PID_TYPE_PES != vv[j - 1].type)
								continue;
							if (v2[i - 1].pid == vv[j - 1].pid) {
								//printf("Erase audio elementary PID 0x%x\n", vv[j - 1].pid);
								delete vv[j - 1].nodes;
								vv.erase(vv.begin() + (int)j - 1);
								break;
							}
						}
						v2.erase(v2.begin() + (int)i - 1);
						break;
					}
				}

				PES_TABLE pes_table;
				pes_table.rf = rf;
				pes_table.pid = elementary_pid;
				pes_table.type = (table.stream_type == TS_AUDIO_TYPE_AC3) ? PES_TYPE_AC3_AUDIO : PES_TYPE_BSAC_AUDIO;
					//(table.stream_type == TS_AUDIO_TYPE_EAC3) ? PES_TYPE_EAC3_AUDIO : PES_TYPE_AAC_AUDIO;
				m_vec_pes_table.push_back(pes_table);

				PID_TABLE pid_table;
				pid_table.rf = rf;
				pid_table.pid = elementary_pid;
				pid_table.payload_unit_start = false;
				pid_table.continuity_counter = 0;
				pid_table.type = PID_TYPE_PES;
				pid_table.nodes = new CNodes;
				m_vec_pid_table.push_back(pid_table);

				printf("Add audio elementary PID 0x%x\n", elementary_pid);
			}
			break;
		}

		for (PROGRAM_TABLE& table : m_vec_program_table) {
			if (rf != table.rf)
				continue;
			unsigned short program_map_pid = table.program_map_pid;
			bool bFind = false;
			for (PID_TABLE& table2 : m_vec_pid_table) {
				if (rf != table2.rf)
					continue;
				if (program_map_pid != table2.pid)
					continue;
				bFind = true;
				break;
			}
			if (!bFind) {
				PID_TABLE pid_table;
				pid_table.rf = rf;
				pid_table.pid = program_map_pid;
				pid_table.payload_unit_start = false;
				pid_table.continuity_counter = 0;
				pid_table.type = PID_TYPE_SECTION;
				pid_table.nodes = new CNodes;
				m_vec_pid_table.push_back(pid_table);

				//printf("Add PMT PID 0x%x\n", program_map_pid);
			}
		}

		for (MASTER_GUIDE_TABLE& table : m_vec_master_guide_table) {
			if (rf != table.rf)
				continue;
			if (table.table_type < 0x100)
				continue;
			if (0x17f < table.table_type)
				continue;
			unsigned short table_type_pid = table.table_type_pid;
			bool bFind = false;
			for (PID_TABLE& table3 : m_vec_pid_table) {
				if (rf != table3.rf)
					continue;
				if (table_type_pid != table3.pid)
					continue;
				bFind = true;
				break;
			}
			if (!bFind) {
				PID_TABLE pid_table;
				pid_table.rf = rf;
				pid_table.pid = table_type_pid;
				pid_table.payload_unit_start = false;
				pid_table.continuity_counter = 0;
				pid_table.type = PID_TYPE_SECTION;
				pid_table.nodes = new CNodes;
				m_vec_pid_table.push_back(pid_table);

				//printf("Add EIT PID 0x%x\n", table_type_pid);
			}
		}

		for (MASTER_GUIDE_TABLE& table : m_vec_master_guide_table) {
			if (rf != table.rf)
				continue;
			if (table.table_type < 0x200)
				continue;
			if (0x27f < table.table_type)
				continue;
			unsigned short table_type_pid = table.table_type_pid;
			bool bFind = false;
			for (PID_TABLE& table4 : m_vec_pid_table) {
				if (rf != table4.rf)
					continue;
				if (table_type_pid != table4.pid)
					continue;
				bFind = true;
				break;
			}
			if (!bFind) {
				PID_TABLE pid_table;
				pid_table.rf = rf;
				pid_table.pid = table_type_pid;
				pid_table.payload_unit_start = false;
				pid_table.continuity_counter = 0;
				pid_table.type = PID_TYPE_SECTION;
				pid_table.nodes = new CNodes;
				m_vec_pid_table.push_back(pid_table);

				printf("Add ETT PID 0x%x\n", table_type_pid);
			}
		}

		//파싱!
		for (unsigned short i = 0; i < readTs; i += 188) {
			CByteBuffer bb(&buffer[i], 188);
			unsigned char sync_byte = bb.get();

			if (sync_byte != 0x47) {
				std::cout << "Not TS sync = 0x" << sync_byte << std::endl;
				continue;
			}
			unsigned short b = bb.get2();	//2바이트 이동

			unsigned char transport_error_indicator = (b >> 15) & 0x0001;
			if (transport_error_indicator) {
				continue;
			}
			unsigned char payload_unit_start_indicator = (b >> 14) & 0x0001;
			unsigned char transport_priority = (b >> 13) & 0x0001;
			unsigned short pid = (b >> 0) & 0x1fff;
			b = bb.get();
			unsigned char transport_scrambling_control = (b >> 6) & 0x03;
			unsigned char adaptation_field_control = (b >> 4) & 0x03;
			unsigned char continuity_counter = (b >> 0) & 0x0f;			//각 pid의 값의 증가 값을 표시 (15가 초과되면 0으로 초기화 된다)

	#if 0
			printf("transport_error_indicator[%d]\n", transport_error_indicator);
			printf("payload_unit_start_indicator[%d]\n", payload_unit_start_indicator);
			printf("pid[%d]\n", pid);
			printf("transport_scrambling_control[%d]\n", transport_scrambling_control);
			printf("adaptation_field_control[%d]\n", payload_unit_start_indicator);
			printf("continuity_counter[%d]\n", continuity_counter);
	#endif		
			//std::cout << "===================================================" << std::endl;

			if (adaptation_field_control == 0x02 || adaptation_field_control == 0x03) {
				//std::cout << "adaptationFieldControl1" << std::endl;
				unsigned char adaptation_field_length = bb.get();
				if (adaptation_field_length) {
					b = bb.get();
					unsigned char pcr_flag = (b >> 4) & 0x01;
					adaptation_field_length -= 1;
					if (pcr_flag) {
						unsigned long long pcr = (unsigned long long) bb.get4() << 1;
						pcr |= (bb.get2() >> 15) & 0x0001;
						adaptation_field_length -= 6;

						for (PCR_TABLE& table : m_vec_pcr_table) {
							if (rf != table.rf)
								continue;
							/*
							if (serviceId != table.program_number)
								continue;
							*/
							if (pid != table.pcr_pid)
								continue;
							if (!table.pcr) {
								table.pcr = pcr;
								table.pcr_receive_time_us = getTimeUs();
							}
							break;
						}
					}
					bb.gets(adaptation_field_length);
				}
			}

			if (adaptation_field_control == 0x01 || adaptation_field_control == 0x03) {
				//std::cout << "adaptationFieldControl2" << std::endl;
				for (PID_TABLE& table : m_vec_pid_table) {
					//std::cout << "mVecPidTable!" << std::endl;
					//printf("rf[%d] table.rf[%d] , pid[%d] table.pid[%d]\n", rf, table.rf, pid, table.pid);

					if (rf == table.rf && pid == table.pid) {
						if (table.nodes == nullptr) {
							std::cout << "table nodes is null" << std::endl;
							continue;
						}

						if (continuity_counter != ((table.continuity_counter + 1) & 0x0f)) {
							table.payload_unit_start = false;

							table.nodes->reset();
							for (PES_TABLE& table2 : m_vec_pes_table) {
								if (rf == table2.rf && pid == table2.pid) {
									switch (table2.type)
									{
									case PES_TYPE_H264_VIDEO:
									case PES_TYPE_MPEG2_VIDEO:
										std::cout << "PES_TYPE_MPEG2_VIDEO or PES_TYPE_H26" << std::endl;
										nodesVideo->reset();
										break;
									case PES_TYPE_AC3_AUDIO:
									case PES_TYPE_AAC_AUDIO:
									case PES_TYPE_BSAC_AUDIO:
										std::cout << "PES_TYPE_AC3_AUDIO | PES_TYPE_AAC_AUDIO | PES_TYPE_BSAC_AUDIO" << std::endl;
										nodesAudio->reset();
										break;							
										
									default:
										std::cout << "table2 type = " << table2.type << std::endl;
										break;
									}
									break;
								}
							}
						}
						table.continuity_counter = continuity_counter;
						//printf("payload_unit_start_indicator[%d]\n", payload_unit_start_indicator);
						if (payload_unit_start_indicator) {
							//printf("pid = [%d] table_pid[%d]\n", pid, table.pid);
							table.payload_unit_start = true;

							switch (table.type) {
							case PID_TYPE_SECTION: {
								//std::cout << "PID_TYPE_SECTION" << std::endl;
								unsigned char pointer_field = bb.get();
								if (pointer_field) {
									unsigned char* data = bb.gets(pointer_field);

									if (table.nodes->len()) {
										table.nodes->add(table.nodes->len(), data, pointer_field, 0);
									}
								}

								if (table.nodes->len()) {
									//todo. processSectionData() 구현.
									processSectionData(rf, serviceId, pid, table.nodes->get(0, table.nodes->len()), table.nodes->len());
								}

								break;
							}
							case PID_TYPE_PES: {
								//std::cout << "PID_TYPE_PES" << std::endl;
								if (table.nodes->len()) {
									//todo. processPesData() 구현.
									processPesData(rf, serviceId, pid, table.nodes->get(0, table.nodes->len()), table.nodes->len());
								}
								break;
							}
							default:
								break;
							}

							unsigned int len = bb.remain();
							unsigned char* data = bb.gets(len);

							table.nodes->reset();
							table.nodes->add(0, data, len, 0);
						}
						else if (table.payload_unit_start) {
							unsigned int len = bb.remain();
							unsigned char* data = bb.gets(len);

							table.nodes->add(table.nodes->len(), data, len, 0);
						}
						break;
					}
				}
			}
			count++;
		}
		
		//SHLEE
		if (count == 500) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			count = 0;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}//while
}
#pragma warning(pop)

static unsigned int calcCRC32(const std::string& strData) {
	auto* data = (unsigned char*)strData.c_str();
	size_t len = strData.length();
	static unsigned int crcTable[256];
	static bool bFirst = true;
	if (bFirst) {
		bFirst = false;
		for (int i = 0; i < 256; i++) {
			unsigned int crc = 0;
			for (int j = 7; j >= 0; j--) {
				if (((i >> j) ^ (crc >> 31)) & 1)
					crc = (crc << 1) ^ 0x04C11DB7;
				else
					crc <<= 1;
			}
			crcTable[i] = crc;
		}
	}
	unsigned int crc = 0xFFFFFFFF;
	for (size_t i = 0; i < len; ++i)
		crc = (crc << 8) ^ crcTable[(crc >> 24) ^ (*data++)];
	return crc;
}

static unsigned short calcCRC16(const std::string& strData) {
	auto* data = (unsigned char*)strData.c_str();
	size_t len = strData.length();
	const unsigned short crcTable[257] = {
			0x0000, 0x0580, 0x0F80, 0x0A00, 0x1B80, 0x1E00, 0x1400, 0x1180,
			0x3380, 0x3600, 0x3C00, 0x3980, 0x2800, 0x2D80, 0x2780, 0x2200,
			0x6380, 0x6600, 0x6C00, 0x6980, 0x7800, 0x7D80, 0x7780, 0x7200,
			0x5000, 0x5580, 0x5F80, 0x5A00, 0x4B80, 0x4E00, 0x4400, 0x4180,
			0xC380, 0xC600, 0xCC00, 0xC980, 0xD800, 0xDD80, 0xD780, 0xD200,
			0xF000, 0xF580, 0xFF80, 0xFA00, 0xEB80, 0xEE00, 0xE400, 0xE180,
			0xA000, 0xA580, 0xAF80, 0xAA00, 0xBB80, 0xBE00, 0xB400, 0xB180,
			0x9380, 0x9600, 0x9C00, 0x9980, 0x8800, 0x8D80, 0x8780, 0x8200,
			0x8381, 0x8601, 0x8C01, 0x8981, 0x9801, 0x9D81, 0x9781, 0x9201,
			0xB001, 0xB581, 0xBF81, 0xBA01, 0xAB81, 0xAE01, 0xA401, 0xA181,
			0xE001, 0xE581, 0xEF81, 0xEA01, 0xFB81, 0xFE01, 0xF401, 0xF181,
			0xD381, 0xD601, 0xDC01, 0xD981, 0xC801, 0xCD81, 0xC781, 0xC201,
			0x4001, 0x4581, 0x4F81, 0x4A01, 0x5B81, 0x5E01, 0x5401, 0x5181,
			0x7381, 0x7601, 0x7C01, 0x7981, 0x6801, 0x6D81, 0x6781, 0x6201,
			0x2381, 0x2601, 0x2C01, 0x2981, 0x3801, 0x3D81, 0x3781, 0x3201,
			0x1001, 0x1581, 0x1F81, 0x1A01, 0x0B81, 0x0E01, 0x0401, 0x0181,
			0x0383, 0x0603, 0x0C03, 0x0983, 0x1803, 0x1D83, 0x1783, 0x1203,
			0x3003, 0x3583, 0x3F83, 0x3A03, 0x2B83, 0x2E03, 0x2403, 0x2183,
			0x6003, 0x6583, 0x6F83, 0x6A03, 0x7B83, 0x7E03, 0x7403, 0x7183,
			0x5383, 0x5603, 0x5C03, 0x5983, 0x4803, 0x4D83, 0x4783, 0x4203,
			0xC003, 0xC583, 0xCF83, 0xCA03, 0xDB83, 0xDE03, 0xD403, 0xD183,
			0xF383, 0xF603, 0xFC03, 0xF983, 0xE803, 0xED83, 0xE783, 0xE203,
			0xA383, 0xA603, 0xAC03, 0xA983, 0xB803, 0xBD83, 0xB783, 0xB203,
			0x9003, 0x9583, 0x9F83, 0x9A03, 0x8B83, 0x8E03, 0x8403, 0x8183,
			0x8002, 0x8582, 0x8F82, 0x8A02, 0x9B82, 0x9E02, 0x9402, 0x9182,
			0xB382, 0xB602, 0xBC02, 0xB982, 0xA802, 0xAD82, 0xA782, 0xA202,
			0xE382, 0xE602, 0xEC02, 0xE982, 0xF802, 0xFD82, 0xF782, 0xF202,
			0xD002, 0xD582, 0xDF82, 0xDA02, 0xCB82, 0xCE02, 0xC402, 0xC182,
			0x4382, 0x4602, 0x4C02, 0x4982, 0x5802, 0x5D82, 0x5782, 0x5202,
			0x7002, 0x7582, 0x7F82, 0x7A02, 0x6B82, 0x6E02, 0x6402, 0x6182,
			0x2002, 0x2582, 0x2F82, 0x2A02, 0x3B82, 0x3E02, 0x3402, 0x3182,
			0x1382, 0x1602, 0x1C02, 0x1982, 0x0802, 0x0D82, 0x0782, 0x0202,
			0x0001 };
	const unsigned char* end = data + len;
	unsigned short crc = 0;

	while (data < end)
		crc = crcTable[((unsigned char)crc) ^ *data++] ^ (crc >> 8);

	return crc;
}

void TsParser::processPesData(int rf, int serviceId, unsigned short pid, unsigned char* ucData, unsigned int usLength) {
	CByteBuffer bb(ucData, usLength);
	unsigned int packet_start_code_prefix = bb.get3();
	if (packet_start_code_prefix != 0x000001) {
		printf("Not PES packet start code prefix = 0x%x\n", packet_start_code_prefix);
		return;
	}
	unsigned char stream_id = bb.get();
	unsigned short pes_packet_length = bb.get2();
	unsigned short b = bb.get();
	if (((b >> 6) & 0x03) != 0x02) {
		//return;
	}
	unsigned char pes_scrambling_code = (b >> 4) & 0x3;
	unsigned char pes_priority = (b >> 3) & 0x01;
	unsigned char data_alignment_indicator = (b >> 2) & 0x01;
	unsigned char copyright = (b >> 1) & 0x01;
	unsigned char original_or_copy = (b >> 0) & 0x01;
	b = bb.get();
	unsigned char pts_dts_flag = (b >> 6) & 0x03;
	unsigned char escr_flag = (b >> 5) & 0x01;
	unsigned char es_rate_flag = (b >> 4) & 0x01;
	unsigned char dsm_trick_mode_flag = (b >> 3) & 0x01;
	unsigned char additional_copy_info_flag = (b >> 2) & 0x01;
	unsigned char pes_crc_flag = (b >> 1) & 0x01;
	unsigned char pes_extension_flag = (b >> 0) & 0x01;
	unsigned char pes_header_data_length = bb.get();

	unsigned long long pts = 0;
	unsigned long long dts = 0;
	if (pts_dts_flag == 0x02) {
		b = bb.get();
		if (((b >> 4) & 0x0f) != 0x02) {
			//return;
		}
		pts = (b >> 1) & 0x07;
		if (((b >> 0) & 0x01) != 0x01) {
			//return;
		}
		b = bb.get2();
		pts = (pts << 15) | ((b >> 1) & 0x7fff);
		if (((b >> 0) & 0x0001) != 0x0001) {
			//return;
		}
		b = bb.get2();
		pts = (pts << 15) | ((b >> 1) & 0x7fff);
		if (((b >> 0) & 0x0001) != 0x0001) {
			//return;
		}

		bb.gets(pes_header_data_length - 5);
	}
	else if (pts_dts_flag == 0x03) {
		b = bb.get();
		if (((b >> 4) & 0x0f) != 0x03) {
			//return;
		}
		pts = (b >> 1) & 0x07;
		if (((b >> 0) & 0x01) != 0x01) {
			//return;
		}
		b = bb.get2();
		pts = (pts << 15) | ((b >> 1) & 0x7fff);
		if (((b >> 0) & 0x0001) != 0x0001) {
			//return;
		}
		b = bb.get2();
		pts = (pts << 15) | ((b >> 1) & 0x7fff);
		if (((b >> 0) & 0x0001) != 0x0001) {
			//return;
		}

		b = bb.get();
		if (((b >> 4) & 0x0f) != 0x01) {
			//return;
		}
		dts = (b >> 1) & 0x07;
		if (((b >> 0) & 0x01) != 0x01) {
			//return;
		}
		b = bb.get2();
		dts = (dts << 15) | ((b >> 1) & 0x7fff);
		if (((b >> 0) & 0x0001) != 0x0001) {
			//return;
		}
		b = bb.get2();
		dts = (dts << 15) | ((b >> 1) & 0x7fff);
		if (((b >> 0) & 0x0001) != 0x0001) {
			//return;
		}

		bb.gets(pes_header_data_length - 10);
	}

	unsigned int len = bb.remain();
	unsigned char* data = bb.gets(len);

	for (PCR_TABLE& table : m_vec_pcr_table) {
		if (rf != table.rf)
			continue;
		if (serviceId != table.program_number)
			continue;
		/*if (pid != table.pcr_pid)
			continue;*/
		if (table.pcr) {
			unsigned long long pcr =
				(table.pcr + ((getTimeUs() - table.pcr_receive_time_us) * 9 / 100)) &
				0x1ffffffff;
			if (!table.buffer_time_us) {
				table.buffer_time_us = ((dts ? dts : pts) - pcr) * 100 / 9;
				if (table.buffer_time_us < (900 * 1000)) {
					table.buffer_time_us = 900 * 1000;
				}
			}
		}
		break;
	}

	unsigned long long buffer_time_us = 0;
	for (PCR_TABLE& table : m_vec_pcr_table) {
		if (rf != table.rf)
			continue;
		if (serviceId != table.program_number)
			continue;
		buffer_time_us = table.buffer_time_us;
		break;
	}
	// PCR PTS DTS가 잘못된 경우에 buffer_time_us가 잘못된 경우 임시 보정
	// 추후 제대로 수정 필요 2019.12.18 cglee
	if (buffer_time_us > 900 * 1000) buffer_time_us = 900 * 1000;
	for (PES_TABLE& table : m_vec_pes_table) {
		if (rf == table.rf && pid == table.pid) {
			switch (table.type) {
			case PES_TYPE_H264_VIDEO:
				//std::cout << "PES_TYPE_H264_VIDEO" << std::endl;
				processH264VideoEsData(rf, serviceId, dts, pts, data, len, buffer_time_us);
				break;
			case PES_TYPE_MPEG2_VIDEO: {
				//std::cout << "PES_TYPE_MPEG2_VIDEO" << std::endl;
				processMpeg2VideoEsData(rf, serviceId, dts, pts, data, len, buffer_time_us);

				break;
			}
			case PES_TYPE_EAC3_AUDIO:
				break;
			case PES_TYPE_AC3_AUDIO:
				processAc3AudioEsData(rf, serviceId, pts, data, len, buffer_time_us);
				break;
			case PES_TYPE_AAC_AUDIO: {
				//std::cout << "PES_TYPE_AAC_AUDIO || AC3" << std::endl;
				processAacAudioEsData(rf, serviceId, dts ? dts : pts, data, len, buffer_time_us);

				break;
			}case PES_TYPE_BSAC_AUDIO:
				//std::cout << "PES_TYPE_BSAC_AUDIO" << std::endl;
				break;
			default:
				std::cout << "table type = " << table.type << std::endl;
				break;
			}
		}
	}
}

void TsParser::processSectionData(int rf, int serviceId, unsigned short pid, unsigned char* ucData,
	unsigned int usLength) {
	CByteBuffer bb(ucData, usLength);
	unsigned char table_id = bb.get();
	unsigned short b = bb.get2();
	unsigned short section_length = (b >> 0) & 0x0fff;
	usLength = section_length + 3;
	unsigned int crc32 = calcCRC32(std::string((char*)ucData, usLength));
	if (crc32 != 0) {
		//printf("Wrong Section CRC32 0x%x, PID 0x%x\n", crc32, pid);

		return;
	}

	switch (table_id) {
	case TS_TABLE_ID_PAT: {
		processPatData(rf, serviceId, pid, ucData, usLength);

		break;
	}

	case TS_TABLE_ID_PMT: {	
		processPmtData(rf, serviceId, pid, ucData, usLength);

		break;
	}

	case TS_TABLE_ID_MGT: {
//		processMgtData(rf, serviceId, pid, ucData, usLength);

		break;
	}
/*
	case TABLE_ID_TVCT: {
		processTvctData(rf, serviceId, pid, ucData, usLength);

		break;
	}

	case TABLE_ID_RRT: {
		processRrtData(rf, serviceId, pid, ucData, usLength);

		break;
	}

	case TABLE_ID_EIT: {
		processEitData(rf, serviceId, pid, ucData, usLength);

		break;
	}

	case TABLE_ID_ETT: {
		processEttData(rf, serviceId, pid, ucData, usLength);

		break;
	}

	case TABLE_ID_STT: {
		processSttData(rf, serviceId, pid, ucData, usLength);

		break;
	}
*/

	default:
		break;
	}
}

void TsParser::processPatData(int rf, int serviceId, unsigned short pid, unsigned char* ucData,
	unsigned int usLength) {
	unsigned int crc32 = ucData[usLength - 4];
	crc32 = crc32 << 8 | ucData[usLength - 3];
	crc32 = crc32 << 8 | ucData[usLength - 2];
	crc32 = crc32 << 8 | ucData[usLength - 1];
	bool bErase = false;
	auto& v = m_vec_program_table;
	for (size_t i = v.size(); 0 < i; i--) {
		if (rf != v[i - 1].rf)
			continue;
		if (crc32 != v[i - 1].crc32) {
			//printf("Erase program program_number %d\n", v[i - 1].program_number);

			v.erase(v.begin() + (int)i - 1);
			bErase = true;
		}
	}
	if (bErase) {
		auto& _pcr_table = m_vec_pcr_table;
		for (size_t i = _pcr_table.size(); 0 < i; i--) {
			if (rf == _pcr_table[i - 1].rf) {
				//printf("Erase pcr pcr_pid 0x%x\n", v[i - 1].pcr_pid);

				_pcr_table.erase(_pcr_table.begin() + (int)i - 1);
			}
		}
		auto& vv = m_vec_stream_table;
		for (size_t i = vv.size(); 0 < i; i--) {
			if (rf == vv[i - 1].rf) {
				//printf("Erase stream elementary_pid 0x%x stream_type 0x%x \n", vv[i - 1].elementary_pid, vv[i - 1].stream_type);

				vv.erase(vv.begin() + (int)i - 1);
			}
		}
		auto& vvv = m_vec_master_guide_table;
		for (size_t i = vvv.size(); 0 < i; i--) {
			if (rf == vvv[i - 1].rf) {
				//printf("Erase table table_type 0x%x table_type_pid 0x%x\n", vvv[i - 1].table_type, vvv[i - 1].table_type_pid);

				vvv.erase(vvv.begin() + (int)i - 1);
			}
		}
		auto& vvvv = m_vec_epg_table;
		for (size_t i = vvvv.size(); 0 < i; i--) {
			if (rf == vvvv[i - 1].rf) {
				//printf("Erase EPG source_id 0x%x event_id 0x%x\n", vvvv[i - 1].source_id, vvvv[i - 1].event_id);

				vvvv.erase(vvvv.begin() + (int)i - 1);
			}
		}
	}

	CByteBuffer bb(ucData, usLength);
	unsigned char table_id = bb.get();
	if (table_id != TS_TABLE_ID_PAT) {
		printf("Not PAT table id = 0x%x\n", table_id);
		return;
	}
	unsigned short b = bb.get2();
	unsigned char section_syntax_indicator = b >> 15 & 0x0001;
	unsigned short section_length = b >> 0 & 0x0fff;
	unsigned short transport_stream_id = bb.get2();
	b = bb.get();
	unsigned char version_number = b >> 1 & 0x1f;
	unsigned char current_next_indicator = b >> 0 & 0x01;
	unsigned char section_number = bb.get();
	unsigned char last_section_number = bb.get();

	for (int i = 0; 4 < bb.remain(); i++) {
		unsigned short program_number = bb.get2();
		unsigned short program_map_pid = bb.get2() & (unsigned short)0x1fff;
		if (program_number == 0) {
			continue;
		}

		bool bFind = false;
		for (PROGRAM_TABLE& table : m_vec_program_table) {
			if (rf != table.rf)
				continue;
			if (program_number != table.program_number)
				continue;
			if (program_map_pid != table.program_map_pid)
				continue;
			bFind = true;
			if (PSI_ONLY_PROGRAM_TIMEOUT < (double)(getTimeUs() - table.add_time_us)) {
				if (!table.available) {
					table.available = true;

					//printf("Update program program_number %d available = true\n", program_number);
				}
			}
			break;
		}
		if (!bFind) {
			PROGRAM_TABLE program_table;
			program_table.rf = rf;
			program_table.program_number = program_number;
			program_table.program_map_pid = program_map_pid;
			program_table.major_channel_number = rf;//frequency2Number(rf);
			program_table.minor_channel_number = i + 1;
			memset(&program_table.short_name, 0, sizeof(program_table.short_name));
			program_table.name = "";
			program_table.hidden = 0;
			program_table.service_type = 0;
			program_table.source_id = 0;
			program_table.available = false;
			program_table.add_time_us = getTimeUs();
			program_table.crc32 = crc32;
			std::cout << "g_program_table push back" << std::endl;
			m_vec_program_table.push_back(program_table);

			//printf("Add program program_number %d\n", program_number);
		}
	}

	//SendChannelMap(rf);	//slt callback처리. 임시 주석 처리 한다.
}

static void processContentAdvisoryDescriptorData(unsigned char* ucData, unsigned int usLength,
	std::vector<CONTENT_ADVISORY_TABLE>& table) {
	CByteBuffer bb(ucData, usLength);
	unsigned char rating_region_count = bb.get() & (unsigned char)0x3f;
	for (int i = 0; i < rating_region_count; i++) {
		unsigned char rating_region = bb.get();
		unsigned char rated_dimensions = bb.get();
		std::vector<CONTENT_ADVISORY_DIMENSION_TABLE> dimension_table;
		for (int j = 0; j < rated_dimensions; j++) {
			unsigned char rating_dimension_j = bb.get();
			unsigned char rating_value = bb.get() & (unsigned char)0x0f;

			CONTENT_ADVISORY_DIMENSION_TABLE content_advisory_dimension_table;
			content_advisory_dimension_table.dimension = rating_dimension_j;
			content_advisory_dimension_table.value = rating_value;
			dimension_table.push_back(content_advisory_dimension_table);
		}
		unsigned char rating_description_length = bb.get();
		std::string rating_description_text;
		if (rating_description_length) {
			std::vector<MULTIPLE_STRING_TABLE> str;
			processMultipleString(bb.gets(rating_description_length), rating_description_length, str);
			if (!str.empty()) {
				rating_description_text = str[0].string;
			}
		}

		std::sort(dimension_table.begin(), dimension_table.end(),
			[](CONTENT_ADVISORY_DIMENSION_TABLE& t1,
				CONTENT_ADVISORY_DIMENSION_TABLE& t2) {
					return t1.dimension < t2.dimension;
			});

		CONTENT_ADVISORY_TABLE content_advisory_table;
		content_advisory_table.region = rating_region;
		content_advisory_table.dimension_table = dimension_table;
		table.push_back(content_advisory_table);
	}
}

std::string wchar_to_UTF8(const wchar_t* in) {
	std::string out;
	unsigned int codepoint = 0;
	for (; *in != 0; ++in) {
		if (*in >= 0xd800 && *in <= 0xdbff)
			codepoint = ((*in - 0xd800) << 10) + 0x10000;
		else {
			if (*in >= 0xdc00 && *in <= 0xdfff)
				codepoint |= *in - 0xdc00;
			else
				codepoint = *in;

			if (codepoint <= 0x7f)
				out.append(1, static_cast<char>(codepoint));
			else if (codepoint <= 0x7ff) {
				out.append(1, static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f)));
				out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
			}
			else if (codepoint <= 0xffff) {
				out.append(1, static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f)));
				out.append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
				out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
			}
			else {
				out.append(1, static_cast<char>(0xf0 | ((codepoint >> 18) & 0x07)));
				out.append(1, static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
				out.append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
				out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
			}
			codepoint = 0;
		}
	}
	return out;
}

static void processMultipleString(unsigned char* ucData, unsigned int usLength,
	std::vector<MULTIPLE_STRING_TABLE>& multiple_string_table) {
	CByteBuffer bb(ucData, usLength);
	unsigned char number_strings = bb.get();
	for (int i = 0; i < number_strings; i++) {
		std::stringstream iso_639_language_code;
		iso_639_language_code << bb.get();
		iso_639_language_code << bb.get();
		iso_639_language_code << bb.get();
		unsigned char number_segments = bb.get();
		for (int j = 0; j < number_segments; j++) {
			unsigned char compression_type = bb.get();
			unsigned char mode = bb.get();
			unsigned char number_bytes = bb.get();
			std::string string;
			if (compression_type == 0) {
				if (mode == 0x00) {
					std::stringstream t;
					for (int k = 0; k < number_bytes; k++) {
						t << bb.get();
					}
					string = t.str();
				}
				else if (mode == 0x3f) {
					auto* t = new wchar_t[number_bytes / 2 + 1];
					for (int k = 0; k < (number_bytes / 2); k++) {
						t[k] = bb.get();
						t[k] = t[k] << 8 | bb.get();
					}
					t[number_bytes / 2] = 0;
					string = wchar_to_UTF8((const wchar_t*)t);
					delete[] t;
				}

				MULTIPLE_STRING_TABLE table;
				table.iso_639_language_code = iso_639_language_code.str();
				table.string = string;
				multiple_string_table.push_back(table);
			}
			else {
				printf("Unknown compression_type %d\n", compression_type);
			}
		}
	}
}

std::string TsParser::processContentAdvisoryToString(int rf,
	std::vector<CONTENT_ADVISORY_TABLE>& content_advisory_table) {
	std::stringstream ret;
	for (CONTENT_ADVISORY_TABLE& t1 : content_advisory_table) {
		std::stringstream dim, desc;
		for (CONTENT_ADVISORY_DIMENSION_TABLE& t2 : t1.dimension_table) {
			std::string str;
			int key = t2.dimension * 10 + t2.value;
			switch (t1.region) {
			case 1:
				if (key == 1) str = "None";
				if (key == 2) str = "TV-G";
				if (key == 3) str = "TV-PG";
				if (key == 4) str = "TV-14";
				if (key == 5) str = "TV-MA";
				if (key == 11) str = "D";
				if (key == 21) str = "L";
				if (key == 31) str = "S";
				if (key == 41) str = "V";
				if (key == 51) str = "TV-Y";
				if (key == 52) str = "TV-Y7";
				if (key == 61) str = "FV";
				if (key == 71) str = "N/A";
				if (key == 72) str = "G";
				if (key == 73) str = "PG";
				if (key == 74) str = "PG-13";
				if (key == 75) str = "R";
				if (key == 76) str = "NC-17";
				if (key == 77) str = "X";
				if (key == 78) str = "NR";
				break;
			case 5:
				for (RATING_TABLE& table : m_vec_rating_table) {
					if (rf != table.rf || t1.region != table.region) continue;
					if (t2.dimension < table.dimension_table.size()) {
						if (t2.value <
							table.dimension_table[t2.dimension].value_table.size()) {
							if (table.dimension_table[t2.dimension].value_table[t2.value].abbrev_value.size()) {
								str = table.dimension_table[t2.dimension].value_table[t2.value].abbrev_value[0].string;
							}
						}
					}
					break;
				}
				break;
			default:
				printf("Unknown rating region %d\n", t1.region);
				break;
			}
			dim << "{" << (unsigned int)t2.dimension << " '" << str << "'}";
			if (desc.str().length()) desc << '-';
			desc << str;
		}
		if (ret.str().length()) ret << ',';
		ret << (unsigned int)t1.region << ",'" << desc.str() << "',"
			<< (dim.str().length() ? dim.str() : "{}");
	}
	return ret.str();
}

static void processISO639LanguageDescriptorData(unsigned char* ucData, unsigned int usLength,
	ISO_639_LANGUAGE_TABLE& table) {
	CByteBuffer bb(ucData, usLength);
	std::stringstream iso_639_language_code;
	iso_639_language_code << bb.get();
	iso_639_language_code << bb.get();
	iso_639_language_code << bb.get();
	unsigned char audio_type = bb.get();

	table.iso_639_language_code = iso_639_language_code.str();
	table.audio_type = audio_type;
}

static std::string iso639(char* strIn) {
	if (!strcmp(strIn, "kor") || !strcmp(strIn, "ko")) return "Korean";
	if (!strcmp(strIn, "eng") || !strcmp(strIn, "en")) return "English";
	if (!strcmp(strIn, "fre") || !strcmp(strIn, "fra") || !strcmp(strIn, "fr")) return "French";
	if (!strcmp(strIn, "spa") || !strcmp(strIn, "es")) return "Spanish";
	if (!strcmp(strIn, "jpa") || !strcmp(strIn, "ja")) return "Japanese";
	if (!strcmp(strIn, "chi") || !strcmp(strIn, "zho") || !strcmp(strIn, "zh")) return "Chinese";
	if (!strcmp(strIn, "ger") || !strcmp(strIn, "deu") || !strcmp(strIn, "de")) return "German";
	if (!strcmp(strIn, "por") || !strcmp(strIn, "pt")) return "Portuguese";
	if (!strcmp(strIn, "ita") || !strcmp(strIn, "it")) return "Italian";
	if (!strcmp(strIn, "dut") || !strcmp(strIn, "nl")) return "Dutch";
	if (!strcmp(strIn, "vi") || !strcmp(strIn, "vie")) return "Vietnamese";
	if (!strcmp(strIn, "el") || !strcmp(strIn, "ell") || !strcmp(strIn, "gre")) return "Greek";
	return "Unknown";
}

void processCaptionDescriptorData(int rf, int serviceId, unsigned char* ucData,
	unsigned int usLength) {
	CByteBuffer dd(ucData, usLength);
	unsigned char tmp = dd.get();
	//unsigned char reserved_1 = (tmp >> 5) & 0x1F;   // reserved: '111'
	unsigned char number_of_services = tmp & 0x1F;
	std::stringstream strXML;
	strXML << "<caption>" << std::endl;
	int valid_count = 0;
	for (int i = 0; i < number_of_services; i++) {
		if (dd.remain() < 4) break;
		char language[4] = "nul";
		language[0] = dd.get();
		language[1] = dd.get();
		language[2] = dd.get();

		tmp = dd.get();
		unsigned char digital_cc = tmp >> 7 & 0x01;
		//unsigned char reserved_2 = (tmp >> 6) & 0x01;   // reserved: '1'
		unsigned short caption_service_number = tmp & 0x3F;

		if (digital_cc != 1)    // 608 line21 처리 무시
		{
			if (dd.remain() < 2) break;
			dd.get2();
			continue;
		}
		if (dd.remain() < 2) break;
		unsigned short tmp2 = dd.get2();
		unsigned short easy_reader = tmp2 >> 15 & 1;
		unsigned short wide_aspect_ratio = tmp2 >> 14 & 1;
		unsigned short korean_code = tmp2 >> 13 & 1;
		//unsigned short reserved_3 = tmp2 & 0x1fff;  // reserved: ‘1111111111111’

		strXML << "<info demod='8vsb' rf='" << rf << "' sid='" << serviceId << "' lang='"
			<< iso639((char*)language);
		// ATSC 3.0과 일치시키기 위해서 wide_aspect_ratio이 wide면 0으로 콜백
		strXML << "' id='" << caption_service_number << "' role='0' aspect_ratio='"
			<< (wide_aspect_ratio ? 0 : 1);
		strXML << "' easy_reader='" << easy_reader << "' profile='0' three_d_support='0'";
		strXML << " korean_code='" << korean_code << "'/>" << std::endl;
		valid_count++;
	}
	strXML << "</caption>" << std::endl;
	if (valid_count > 0) {
		//dataCallback("caption", strXML);	
		//printf("caption : [%s]\n", strXML);
		//std::cout << "dataCallback caption : " << strXML.str() << std::endl;
	}
}

void TsParser::processPmtData(int rf, int serviceId, unsigned short pid, unsigned char* ucData, unsigned int usLength) {
	unsigned short program_number = 0;
	for (PROGRAM_TABLE& table : m_vec_program_table) {
		if (rf != table.rf)
			continue;
		if (pid != table.program_map_pid)
			continue;
		program_number = table.program_number;
		break;
	}
	if (program_number == 0) {
		return;
	}

	unsigned int crc32 = ucData[usLength - 4];
	crc32 = crc32 << 8 | ucData[usLength - 3];
	crc32 = crc32 << 8 | ucData[usLength - 2];
	crc32 = crc32 << 8 | ucData[usLength - 1];
	auto& v = m_vec_pcr_table;
	for (size_t i = v.size(); 0 < i; i--) {
		if (rf != v[i - 1].rf)
			continue;
		if (program_number != v[i - 1].program_number)
			continue;
		if (crc32 != v[i - 1].crc32) {
			//printf("Erase pcr pcr_pid 0x%x\n", v[i - 1].pcr_pid);

			v.erase(v.begin() + (int)i - 1);
		}
	}
	auto& vv = m_vec_stream_table;
	for (size_t i = vv.size(); 0 < i; i--) {
		if (rf != vv[i - 1].rf)
			continue;
		if (program_number != vv[i - 1].program_number)
			continue;
		if (crc32 != vv[i - 1].crc32) {
			//printf("Erase stream elementary_pid 0x%x stream_type 0x%x \n", vv[i - 1].elementary_pid, vv[i - 1].stream_type);

			vv.erase(vv.begin() + (int)i - 1);
		}
	}

	CByteBuffer bb(ucData, usLength);
	unsigned char table_id = bb.get();
	if (table_id != TS_TABLE_ID_PMT) {
		printf("Not PMT table id = 0x%x\n", table_id);
		return;
	}
	unsigned short b = bb.get2();
	unsigned char section_syntax_indicator = b >> 15 & 0x0001;
	unsigned short section_length = b >> 0 & 0x0fff;
	unsigned short transport_stream_id = bb.get2();
	b = bb.get();
	unsigned char version_number = b >> 1 & 0x1f;
	unsigned char current_next_indicator = b >> 0 & 0x01;
	unsigned char section_number = bb.get();
	unsigned char last_section_number = bb.get();

	unsigned short pcr_pid = bb.get2() & (unsigned short)0x1fff;
	bool bFind = false;
	for (PCR_TABLE& table : m_vec_pcr_table) {
		if (rf != table.rf)
			continue;
		if (program_number != table.program_number)
			continue;
		if (pcr_pid != table.pcr_pid)
			continue;
		bFind = true;
		break;
	}
	if (!bFind) {
		PCR_TABLE pcr_table;
		pcr_table.rf = rf;
		pcr_table.program_number = program_number;
		pcr_table.pcr_pid = pcr_pid;
		pcr_table.pcr = 0;
		pcr_table.pcr_receive_time_us = 0;
		pcr_table.buffer_time_us = 0;
		pcr_table.crc32 = crc32;
		m_vec_pcr_table.push_back(pcr_table);

		//printf("Add pcr pcr_pid 0x%x\n", pcr_pid);
	}
	unsigned short program_info_length = bb.get2() & (unsigned short)0x03ff;
	if (program_info_length) {
		for (CByteBuffer pp(bb.gets(program_info_length), program_info_length); pp.remain();) {
			unsigned char descriptor_tag = pp.get();
			unsigned char descriptor_length = pp.get();
			if (descriptor_tag == DESCRIPTOR_TAG_CONTENT_ADVISORY) {
				std::vector<CONTENT_ADVISORY_TABLE> content_advisory_table;
				processContentAdvisoryDescriptorData(pp.gets(descriptor_length),
					descriptor_length, content_advisory_table);
				std::string rating = processContentAdvisoryToString(rf, content_advisory_table);
				if (serviceId == program_number) {
					std::stringstream ret;
					ret << "<rating>" << std::endl;
					ret << R"(<info src="PMT" demod="8vsb" rf=")" << rf << "\" sid=\""
						<< serviceId << "\" value=\"" << rating << "\"/>" << std::endl;
					ret << "</rating>";
					//dataCallback("rating", ret);			
					//printf("dataCallback rating = [%s]\n", ret);
					std::cout << "dataCallback rating = " << ret.str() << std::endl;
				}
			}
			else {
				pp.gets(descriptor_length);
			}
		}
	}
	for (; 4 < bb.remain();) {
		unsigned char stream_type = bb.get();
		unsigned short elementary_pid = bb.get2() & 0x1fff;
		unsigned short es_info_length = bb.get2() & 0x03ff;
		ISO_639_LANGUAGE_TABLE iso_639_language_table;	
		if (es_info_length) {
			CByteBuffer ee(bb.gets(es_info_length), es_info_length);
			for (; ee.remain();) {
				unsigned char descriptor_tag = ee.get();
				unsigned char descriptor_length = ee.get();
				switch (descriptor_tag) {
				case DESCRIPTOR_TAG_ISO_639_LANGUAGE:
					processISO639LanguageDescriptorData(ee.gets(descriptor_length),
						descriptor_length,
						iso_639_language_table);
					break;

				case DESCRIPTOR_TAG_CAPTION:
					if (serviceId == program_number)
						processCaptionDescriptorData(rf, serviceId,
							ee.gets(descriptor_length),
							descriptor_length);
					break;

				default:
					ee.gets(descriptor_length);
					break;
				}
			}
		}

		bool bFind2 = false;
		for (STREAM_TABLE& table : m_vec_stream_table) {
			if (rf != table.rf)
				continue;
			if (program_number != table.program_number)
				continue;
			if (elementary_pid != table.elementary_pid)
				continue;
			if (stream_type != table.stream_type)
				continue;
			bFind2 = true;
			break;
		}
		if (!bFind2) {
			std::string language;
			unsigned char audio_type = 0;			
			
			language = iso_639_language_table.iso_639_language_code;
			audio_type = iso_639_language_table.audio_type;			

			STREAM_TABLE stream_table;
			stream_table.rf = rf;
			stream_table.program_number = program_number;
			stream_table.elementary_pid = elementary_pid;
			stream_table.stream_type = stream_type;
			stream_table.language = language;
			stream_table.audio_type = audio_type;
			stream_table.crc32 = crc32;
			m_vec_stream_table.push_back(stream_table);

			//printf("Add stream elementary_pid 0x%x stream_type 0x%x language %s\n", elementary_pid, stream_type, language.c_str());
		}
	}

	std::stringstream strXML;
	strXML << "<ainfo>" << std::endl;
	int nCount = 0;
	for (STREAM_TABLE& table : m_vec_stream_table) {
		if (table.rf != rf)
			continue;
		if (table.program_number != serviceId)
			continue;
		if (table.stream_type != TS_AUDIO_TYPE_AAC && table.stream_type != TS_AUDIO_TYPE_BSAC)
			continue;
		strXML << "<info demod='8vsb' rf='" << rf << "' sid='" << serviceId << "' lang='"
			<< iso639((char*)table.language.c_str());
		strXML << "' id='" << table.elementary_pid;
		strXML << "' codecs='" << "bsac";
		strXML << "' commentary='" << (table.audio_type == 3 ? 1 : 0);
		strXML << "' select=''/>";
		//strXML << ((std::to_string(table.elementary_pid) == GetSelectedAudioID()) ? 1 : 0)
		//	<< "'/>" << std::endl;
		nCount++;
	}
	strXML << "</ainfo>" << std::endl;
	if (nCount > 0) {
		//dataCallback("ainfo", strXML);
		//std::cout << "dataCallback ainfo = " << strXML.str() << std::endl;
	}
}

void TsParser::processH264VideoEsData(int rf, int serviceId, unsigned long long dts, unsigned long long pts,
	unsigned char* ucData, unsigned int usLength, unsigned long long buffer_time_us) {
	//abort();
#if 0
	printf("pts %llu\n", pts);
	static FILE* vfp = NULL;
	if (!vfp)
	{
		fopen_s(&vfp, "video.ts", "wb");
	}

	if (vfp)
	{
		fwrite(ucData, 1, usLength, vfp);
	}
#endif

	unsigned char* buf = ucData;
	unsigned int idx = 0;

	for (; idx < (usLength - 7);) {
		unsigned int start_code = buf[idx + 0];
		start_code = (start_code << 8) | buf[idx + 1];
		start_code = (start_code << 8) | buf[idx + 2];
		start_code = (start_code << 8) | buf[idx + 3];
		//unsigned int start_code = (buf[idx + 0] << 24) | (buf[idx + 1] << 16) | (buf[idx + 2] << 8) | buf[idx + 3];
		unsigned char nal_unit_type = (buf[idx + 4] & 0x1f);
		//unsigned char pic_type = (buf[idx + 6] >> 5) & 0x07;

		if (start_code == 0x00000001 && nal_unit_type == hvccParser->AUD_NUT) {
			unsigned int len = usLength - idx;
			//std::cout << "0x00000001(AUD)" << std::endl;
		#if 1
			for (unsigned int next = idx + 5; next < (usLength - 5); next++) {
				unsigned int next_start_code = buf[next + 0];
				next_start_code = (next_start_code << 8) | buf[next + 1];
				next_start_code = (next_start_code << 8) | buf[next + 2];
				next_start_code = (next_start_code << 8) | buf[next + 3];
				//unsigned int next_start_code = (buf[next + 0] << 24) | (buf[next + 1] << 16) | (buf[next + 2] << 8) | buf[next + 3];
				unsigned char next_nal_unit_type = (buf[next + 4] & 0x1f);
				if (next_start_code == 0x00000001) {
					if (next_nal_unit_type == hvccParser->AUD_NUT) {
						len = next - idx;
						break;
					}
				}
			}
		#endif

			if ((idx + len) <= usLength) {
				for (unsigned int i = idx + 5; i < (idx + len - 10); i++) {
					unsigned int start_code2 = buf[i + 0];
					start_code2 = (start_code2 << 8) | buf[i + 1];
					start_code2 = (start_code2 << 8) | buf[i + 2];
					start_code2 = (start_code2 << 8) | buf[i + 3];
					//unsigned int start_code2 = (buf[i + 0] << 24) | (buf[i + 1] << 16) | (buf[i + 2] << 8) | buf[i + 3];
				#if 0
					if (buf[i + 4] == 67) {
						printf("0x67!!! -> [0x%x]\n", 0x67 & 0x1f);
					}
					else if(buf[i + 4] == 68){
						printf("0x68!!! -> [0x%x]\n", 0x68 & 0x1f);
					}
				#endif
					unsigned char nal_unit_type2 = (buf[i + 4] & 0x1f);
				#if 0
					//if (nal_unit_type2 == 7 || nal_unit_type2 == 8) {
						printf("##################\n");
						printf("idx[%d], buf[i+4]:][0x%02x], buf[i+4] & 0x1f : %d, start_code2[0x%02x]\n", idx, buf[i + 4], nal_unit_type2, start_code2);
						printf("##################\n");
					//}
				#endif
					if (start_code2 == 0x00000001 && (nal_unit_type2 == hvccParser->SPS_NUT || nal_unit_type2 == hvccParser->PPS_NUT)) {
						#if 0
							printf("pts %llu\n", pts);
							static FILE* vfp = NULL;
							if (!vfp)
							{
								fopen_s(&vfp, "video11.es", "wb");
							}

							if (vfp)
							{
								fwrite(ucData, 1, usLength, vfp);
							}
						#endif
						unsigned int param_len = 0;
						if (nal_unit_type2 == hvccParser->SPS_NUT) {
							printf("SPS Found!!!\n");
						}
						if (nal_unit_type2 == hvccParser->PPS_NUT) {
							printf("PPS Found!!!\n");
							//abort();
						}
							
						//printf("start!! nal_unit_type2=0x%x\n", nal_unit_type2);
						for (unsigned int j = i + 5; j < (idx + len - 5); j++) {
							unsigned int next_start_code = buf[j + 0];
							next_start_code = (next_start_code << 8) | buf[j + 1];
							next_start_code = (next_start_code << 8) | buf[j + 2];
							next_start_code = (next_start_code << 8) | buf[j + 3];
							unsigned char next_nal_unit_type = (buf[j + 4] & 0x1f);
							//printf("next_nal_unit_type[0x%x]\n", next_nal_unit_type);
							if (next_start_code == 0x00000001 && !(next_nal_unit_type == hvccParser->SPS_NUT || next_nal_unit_type == hvccParser->PPS_NUT)) {
								param_len = j - i;
								break;
							}
						}

						if (param_len) {
							if (g_av_callback) {
								//printf("g_av_callback SPS_NUT or PPS_NUT buf=%x",buf[i]);
							#if 1
								g_av_callback("h264", (int)((dts ? dts : pts) / 90000), 0,
									(dts ? dts : pts) * 100 / 9,
									param_len, &buf[i], g_SessionID, buffer_time_us, 0, 0);

								//abort();
							#else
								esFile.write(reinterpret_cast<char*>(&buf[idx]), len);
								printf("first write!!!\n");
							#endif
							}

							break;
						}
					}
				}

				if (buffer_time_us) {
					int presentation_time_offset_us = 0;
					if (dts && dts > pts) {
						presentation_time_offset_us = (int)(dts - pts) * 100 / 9;
						presentation_time_offset_us *= -1;
					}
					if (dts && dts < pts) {
						presentation_time_offset_us = (int)(pts - dts) * 100 / 9;
					}

					if (g_av_callback)
						if (g_av_callback) {							
						#if 1
							printf("g_av_callback!!!!!\n");
							g_av_callback("h264", (int)((dts ? dts : pts) / 90000), 1,
								(dts ? dts : pts) * 100 / 9,
								len, &buf[idx], g_SessionID, buffer_time_us, 0, 0);
						#else
							esFile.write(reinterpret_cast<char*>(&buf[idx]), len);
							printf("second write!!!\n");						
						#endif
						}
				}
				idx += len;
			}
			else {
				break;
			}
		}
		else {
			idx++;
		}
	}
}

void TsParser::processMpeg2VideoEsData(int rf, int serviceId, unsigned long long dts, unsigned long long pts,
	unsigned char* ucData, unsigned int usLength, unsigned long long buffer_time_us) {

	unsigned char* buf = ucData;
	unsigned int idx = 0;

	for (; idx < (usLength - 4);) {
		unsigned int start_code = buf[idx + 0];
		start_code = (start_code << 8) | buf[idx + 1];
		start_code = (start_code << 8) | buf[idx + 2];
		start_code = (start_code << 8) | buf[idx + 3];
		if (start_code == 0x00000100 || start_code == 0x000001b3) {
			unsigned int len = usLength - idx;

			for (unsigned int next = idx + 4; next < (usLength - 4); next++) {
				unsigned int next_start_code = buf[next + 0];
				next_start_code = (next_start_code << 8) | buf[next + 1];
				next_start_code = (next_start_code << 8) | buf[next + 2];
				next_start_code = (next_start_code << 8) | buf[next + 3];
				if (next_start_code == 0x00000100 || next_start_code == 0x000001b3) {
					len = next - idx;

					break;
				}
			}

			if ((idx + len) <= usLength) {
				if (start_code == 0x00000100) {
					unsigned short temperal_sequence_number = buf[idx + 4];
					unsigned char frame_type = (buf[idx + 5] >> 3) & 0x07;
					for (unsigned int i = idx + 6; i < (idx + len - 7); i++) {
						unsigned int user_data_start_code = buf[i + 0];
						user_data_start_code = (user_data_start_code << 8) | buf[i + 1];
						user_data_start_code = (user_data_start_code << 8) | buf[i + 2];
						user_data_start_code = (user_data_start_code << 8) | buf[i + 3];
						if (user_data_start_code == 0x000001b2) {
							unsigned int user_data_len = 0;

							for (unsigned int j = i + 4; j < (idx + len - 3); j++) {
								unsigned int user_data_term_code = buf[j + 0];
								user_data_term_code = (user_data_term_code << 8) | buf[j + 1];
								user_data_term_code = (user_data_term_code << 8) | buf[j + 2];
								if (user_data_term_code == 0x000001) {
									user_data_len = j + 3 - i;

									break;
								}
							}

							if (user_data_len) {
								proccessMpeg2VideoEsUserData(frame_type, &buf[i],
									user_data_len);

								break;
							}
						}
					}					
				}
		
				if (buffer_time_us) {
					int presentation_time_offset_us = 0;
					if (dts && dts > pts) {
						presentation_time_offset_us = (int)(dts - pts) * 100 / 9;
						presentation_time_offset_us *= -1;
					}
					if (dts && dts < pts) {
						presentation_time_offset_us = (int)(pts - dts) * 100 / 9;
					}
#if 1
					if (g_av_callback) {						
						//std::cout << "len = [" << len << "]" << std::endl;
						g_av_callback("mpeg2", (int)((dts ? dts : pts) / 90000),
							start_code == 0x000001b3 ? 0 : 1, (dts ? dts : pts) * 100 / 9,
							len, &buf[idx], g_SessionID, buffer_time_us, 0, 0);
					}


						/*g_av_callback(this, tunerId, rf, serviceId, "mpeg2",
							(int)((dts ? dts : pts) / 90000),
							start_code == 0x000001b3 ? 0 : 1,
							(dts ? dts : pts) * 100 / 9, presentation_time_offset_us,
							len, &buf[idx], g_SessionID, buffer_time_us, 0, 0, 0, 0,
							"");*/
#else

					//printf("callback(processMpeg2VideoEsData)(%s)\n",&buf[idx]);
					//printf("video[0x%x]\n", &buf[idx]);
					esFile.write(reinterpret_cast<char*>(&buf[idx]), len);	//파싱된 비디오 데이터를 파일로 만든다. (정상적으로 동작됨을 확인)			
					if (&buf[idx] == nullptr) {
						std::cout << " 널이다!" << std::endl;
					}
					else {
						//updateTexture(texture, &buf[idx], SCREEN_WIDTH, SCREEN_HEIGHT);
						//avPlayerSDL(&buf[idx], len);
						
						/*AT3APP_AvCallbackSub("mpeg2", (int)((dts ? dts : pts) / 90000), start_code == 0x000001b3 ? 0 : 1, (dts ? dts : pts) * 100 / 9,
							len, &buf[idx], g_SessionID, buffer_time_us, 0, 0);
							*/
					}
#endif
				}
				idx += len;
			}
			else {
				break;
			}
		}
		else {
			idx++;
		}
	}
}

std::string g_cc_userdata;

void TsParser::proccessMpeg2VideoEsUserData(unsigned char frame_type, unsigned char* ucData,
	unsigned int usLength) {
#if 0
	printBinary("Mpeg2VideoEsUserData", ucData, usLength);
#endif
	if (frame_type == 3) {
		proccessMpeg2VideoEsUserDataReorder(frame_type, ucData, usLength);
	}
	else {
		proccessMpeg2VideoEsUserDataReorder(3, (unsigned char*)g_cc_userdata.c_str(),
			(unsigned int)g_cc_userdata.length());
		g_cc_userdata = std::string((char*)ucData, usLength);

	}
}

void TsParser::proccessMpeg2VideoEsUserDataReorder(unsigned char frame_type, unsigned char* ucData,
	unsigned int usLength) {
	if (usLength < 9) return;
	CByteBuffer bb(ucData, usLength);
	if (bb.get() != 0x00) return;
	if (bb.get() != 0x00) return;
	if (bb.get() != 0x01) return;
	if (bb.get() != 0xB2) return;
	if (bb.get() != 0x47) return;
	if (bb.get() != 0x41) return;
	if (bb.get() != 0x39) return;
	if (bb.get() != 0x34) return;
	if (bb.get() != 0x03) return;
	unsigned int nLen = bb.remain();
	if (nLen >= 5) process_cc_data(frame_type, bb.gets(nLen), nLen);
}

void TsParser::process_cc_data(unsigned char frame_type, unsigned char* ucData, unsigned int usLength) {
	CByteBuffer bb(ucData, usLength);
	unsigned char tmp = bb.get();
	bool process_cc_data_flag = (tmp >> 6 & 1) != 0;
	unsigned char cc_count = tmp & 0x1f;
	if (!process_cc_data_flag) return;
	bb.get();
	for (unsigned char i = 0; i < cc_count; i++) {
		if (bb.remain() < 3) break;
		unsigned char tmp2 = bb.get();
		bool cc_valid = (tmp2 >> 2 & 1) != 0;
		unsigned char cc_type = tmp2 & 0x03;
		unsigned char cc_data_1 = bb.get();
		unsigned char cc_data_2 = bb.get();
		//if (cc_valid) process_cc_packet(frame_type, cc_type, cc_data_1, cc_data_2);
	}
}

#if 0
void process_cc_packet(unsigned char frame_type, unsigned char cc_type, unsigned char cc_data_1,
	unsigned char cc_data_2) {
	if (cc_type == 0 || cc_type == 1) return; // ignore 608
	if (cc_type == 3) // DTVCC START
	{
		unsigned char sequency_number = (cc_data_1 >> 6) & 3;
		if ((g_cc_sequency_number + 1) % 4 != sequency_number && g_cc_sequency_number != 0xFF)
			dataCallback("cc", "<cc sequence_error='1' />");
		g_cc_sequency_number = sequency_number;
		unsigned char packet_size = cc_data_1 & 0x3F;
		g_cc_packet_data_size = packet_size * 2 - 1;
		if (packet_size == 0) g_cc_packet_data_size = 127 - 1;
		g_cc_packet_data[0] = cc_data_2;
		g_cc_packet_data_pos = 1;
		//LOGD("**packet header:%d s(%d) %d %d\n", frame_type, g_cc_sequency_number, g_cc_packet_data_size, g_cc_packet_data_pos);
	}
	if (cc_type == 2) // DTVCC DATA
	{
		if (g_cc_packet_data_size == 0) return;
		g_cc_packet_data[g_cc_packet_data_pos] = cc_data_1;
		g_cc_packet_data_pos++;
		//LOGD("packet header:%d %d %d %d\n", frame_type, g_cc_sequency_number, g_cc_packet_data_size, g_cc_packet_data_pos);
		if (g_cc_packet_data_pos >= g_cc_packet_data_size) {
			process_cc_service(g_cc_packet_data, g_cc_packet_data_size);
			g_cc_packet_data_size = 0;
			g_cc_packet_data_pos = 0;
		}
		g_cc_packet_data[g_cc_packet_data_pos] = cc_data_2;
		g_cc_packet_data_pos++;
		//LOGD("packet header:%d %d %d %d\n", frame_type, g_cc_sequency_number, g_cc_packet_data_size, g_cc_packet_data_pos);
		if (g_cc_packet_data_pos >= g_cc_packet_data_size) {
			process_cc_service(g_cc_packet_data, g_cc_packet_data_size);
			g_cc_packet_data_size = 0;
			g_cc_packet_data_pos = 0;
		}
	}
}
#endif

void TsParser::processAacAudioEsData(int rf, int serviceId, unsigned long long pts, unsigned char* ucData,
	unsigned int usLength, unsigned long long buffer_time_us) {
	bool fragmented = nodesAudio->len() ? true : false;

	nodesAudio->add(nodesAudio->len(), ucData, usLength, 0);

	unsigned char* buf = nodesAudio->get(0, nodesAudio->len());
	unsigned int idx = 0;

	for (; idx < (nodesAudio->len() - 9);) {
		CBitBuffer header(&buf[idx]);
		if (header.get(12) == 0xfff) {
			unsigned char id = header.get(1);
			unsigned char layer = header.get(2);
			unsigned char protection_absent = header.get(1);
			unsigned char profile = header.get(2);
			unsigned char sample_freq_index = header.get(4);
			unsigned char private_bit = header.get(1);
			unsigned char channel_config = header.get(3);
			unsigned char original_copy = header.get(1);
			unsigned char home = header.get(1);

			unsigned char copyright_id_bit = header.get(1);
			unsigned char copyright_id_start = header.get(1);
			unsigned short aac_frame_length = header.get(13);
			unsigned short adts_buf_fullness = header.get(11);
			unsigned short num_raw_data_blks = header.get(2);
			unsigned short crc_check = protection_absent == 0 ? header.get(16) : 0;
			unsigned char header_length = protection_absent == 0 ? 9 : 7;

			unsigned int samplerate = 48000;
			if (sample_freq_index <= 12) {
				const unsigned int sample_freq_table[] = {
					96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
					16000, 12000, 11025, 8000, 7350 };
				samplerate = sample_freq_table[sample_freq_index];
			}

			unsigned char channels = 2;
			if (channel_config <= 7) {
				const unsigned char channel_config_table[] = { 0, 1, 2, 3, 4, 5, 6, 8 };
				channels = channel_config_table[channel_config];
			}

			if (aac_frame_length) {
				if ((idx + aac_frame_length) <= nodesAudio->len()) {
					if (buffer_time_us) {
						if (fragmented) {
							fragmented = false;
							pts -= (1000000 * 1024 / samplerate) * 9 / 100;
						}
					#if 0
						if (g_av_callback)
							g_av_callback(this, tunerId, rf, serviceId, "aac", 0, 1,
								pts * 100 / 9, 0, aac_frame_length - header_length, &buf[idx + header_length], g_SessionID,
								buffer_time_us, samplerate, channels, 0, 0, "");
					#endif
						esFileAudio.write(reinterpret_cast<char*>(&buf[idx]), aac_frame_length);
					}
					pts += (1000000 * 1024 / samplerate) * 9 / 100;
					idx += aac_frame_length;
				}
				else {
					break;
				}
			}
			else {
				idx++;
			}
		}
		else {
			idx++;
		}
	}

	auto* nodesTemp = new CNodes;
	nodesTemp->add(0, nodesAudio->get(idx, nodesAudio->len() - idx), nodesAudio->len() - idx,
		0);
	nodesAudio->reset();
	nodesAudio->add(0, nodesTemp->get(0, nodesTemp->len()), nodesTemp->len(), 0);
	delete nodesTemp;
}

void TsParser::processAc3AudioEsData(int rf, int serviceId, unsigned long long pts, unsigned char* ucData,
	unsigned int usLength, unsigned long long buffer_time_us) {
#if 0
	printf("pts %llu\n", pts);

	static FILE* afp = NULL;
	if (!afp)
	{
		fopen_s(&afp, "a.es", "wb");
	}

	if (afp)
	{
		fwrite(ucData, 1, usLength, afp);
	}
#endif

	bool fragmented = nodesAudio->len() ? true : false;

	nodesAudio->add(nodesAudio->len(), ucData, usLength, 0);

	unsigned char* buf = nodesAudio->get(0, nodesAudio->len());
	unsigned int idx = 0;

	for (; idx < (nodesAudio->len() - 6);) {
		if (buf[idx] == 0x0b && buf[idx + 1] == 0x77) {
			unsigned char fscod = (buf[idx + 4] >> 6) & 0x03;
			unsigned char frmsizcod = buf[idx + 4] & 0x3f;
			unsigned char acmod = (buf[idx + 6] >> 5) & 0x07;

			unsigned short samplerate = 48000;
			if (fscod == 0x00) {
				samplerate = 48000;
			}
			else if (fscod == 0x01) {
				samplerate = 44100;
			}
			else if (fscod == 0x02) {
				samplerate = 32000;
			}

			unsigned int len = 0;
			if (frmsizcod <= 0x25) {
				const unsigned short frmsizcod_table[][4] = { {0x00, 96,   69,   64},
						{0x01, 96,   70,   64},
						{0x02, 120,  87,   80},
						{0x03, 120,  88,   80},
						{0x04, 144,  104,  96},
						{0x05, 144,  105,  96},
						{0x06, 168,  121,  112},
						{0x07, 168,  122,  112},
						{0x08, 192,  139,  128},
						{0x09, 192,  140,  128},
						{0x0a, 240,  174,  160},
						{0x0b, 240,  175,  160},
						{0x0c, 288,  208,  192},
						{0x0d, 288,  209,  192},
						{0x0e, 336,  243,  224},
						{0x0f, 336,  244,  224},
						{0x10, 384,  278,  256},
						{0x11, 384,  279,  256},
						{0x12, 480,  348,  320},
						{0x13, 480,  349,  320},
						{0x14, 576,  417,  384},
						{0x15, 576,  418,  384},
						{0x16, 672,  487,  448},
						{0x17, 672,  488,  448},
						{0x18, 768,  557,  512},
						{0x19, 768,  558,  512},
						{0x1a, 960,  696,  640},
						{0x1b, 960,  697,  640},
						{0x1c, 1152, 835,  768},
						{0x1d, 1152, 836,  768},
						{0x1e, 1344, 975,  896},
						{0x1f, 1344, 976,  896},
						{0x20, 1536, 1114, 1024},
						{0x21, 1536, 1115, 1024},
						{0x22, 1728, 1253, 1152},
						{0x23, 1728, 1254, 1152},
						{0x24, 1920, 1393, 1280},
						{0x25, 1920, 1394, 1280} };

				if (fscod == 0x00) {
					len = frmsizcod_table[frmsizcod][3] * 2;
				}
				else if (fscod == 0x01) {
					len = frmsizcod_table[frmsizcod][2] * 2;
				}
				else if (fscod == 0x02) {
					len = frmsizcod_table[frmsizcod][1] * 2;
				}
			}

			unsigned char channels = 2;
			switch (acmod) {
			case 1:
				channels = 1;
				break;
			case 3:
			case 4:
				channels = 3;
				break;
			case 5:
			case 6:
				channels = 4;
				break;
			case 7:
				channels = 5;
				break;
			default:
				channels = 2;
				break;
			}

			if (len) {
				if ((idx + len) <= nodesAudio->len()) {
					unsigned int len_58 = (len >> 1) + (len >> 3);
					unsigned short crc1 = calcCRC16(
						std::string((char*)&buf[idx + 2], len_58 - 2));
					unsigned short crc2 = calcCRC16(
						std::string((char*)&buf[idx + 2], len - 2));
					if (crc1 == 0 && crc2 == 0) {
						if (buffer_time_us) {
							if (fragmented) {
								fragmented = false;
								pts -= (1000000 * 1536 / samplerate) * 9 / 100;
							}
						#if 1
							if (g_av_callback) {
								g_av_callback("ac3", 0, 1, pts * 100 / 9, len, &buf[idx], g_SessionID, buffer_time_us, samplerate, channels);
							}
							/*
							if (g_av_callback)
								g_av_callback(this, tunerId, rf, serviceId, "ac3", 0, 1,
									pts * 100 / 9, 0, len, &buf[idx], g_SessionID,
									buffer_time_us, samplerate, channels, 0, 0, "");
							*/
						#else
							esFileAudio.write(reinterpret_cast<char*>(&buf[idx]), len);	//AC3 파일 저장하여 동작됨 확인( output_audio.ac3 )
						#endif
						}
						pts += (1000000 * 1536 / samplerate) * 9 / 100;
						idx += len;
					}
					else {
						//printf("Wrong AC3 CRC1 0x%x or CRC2 0x%x\n", crc1, crc2);

						idx++;
					}
				}
				else {
					break;
				}
			}
			else {
				idx++;
			}
		}
		else {
			idx++;
		}
	}

	auto* nodesTemp = new CNodes;
	nodesTemp->add(0, nodesAudio->get(idx, nodesAudio->len() - idx), nodesAudio->len() - idx,
		0);
	nodesAudio->reset();
	nodesAudio->add(0, nodesTemp->get(0, nodesTemp->len()), nodesTemp->len(), 0);
	delete nodesTemp;
}
#pragma warning(pop)