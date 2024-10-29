#pragma once
#include <iostream>
#include "CBitBuffer.h"

class HvCCParser {
public:
	std::string strVPS;
	std::string strSPS;
	std::string strPPS;
	unsigned int separate_colour_plane_flag = 0;
	unsigned int log2_max_pic_order_cnt_lsb_minus4 = 0;
	unsigned int MaxPicOrderCntLsb = 0;
	unsigned int bit_depth_chroma_minus8 = 0;
	unsigned int vui_num_units_in_tick = 0;
	unsigned int vui_time_scale = 0;
	unsigned int sps_max_dec_pic_buffering_minus1 = 0;
	unsigned int sps_max_num_reorder_pics = 0;
	unsigned int sps_max_latency_increase_plus1 = 0;

	unsigned int dependent_slice_segments_enabled_flag = 0;
	unsigned int output_flag_present_flag = 0;
	unsigned int num_extra_slice_header_bits = 0;

	unsigned long long pos0_decode_time_us = 0;
	//int pos0_poc = -1;

	// nal_unit_type
	const int TRAIL_N = 0;
	//const int TRAIL_R = 1;
	//const int TSA_N = 2;
	//const int TSA_R = 3;
	//const int STSA_N = 4;
	//const int STSA_R = 5;
	//const int RADL_N = 6;
	//const int RADL_R = 7;
	//const int RASL_N = 8;
	//const int RASL_R = 9;
	//const int RSV_VCL_N10 = 10;
	//const int RSV_VCL_N12 = 12;
	//const int RSV_VCL_N14 = 14;
	//const int RSV_VCL_R11 = 11;
	//const int RSV_VCL_R13 = 13;
	//const int RSV_VCL_R15 = 15;
	//const int BLA_W_LP = 16;
	//const int BLA_W_RADL = 17;
	//const int BLA_N_LP = 18;
	//const int IDR_W_RADL = 19;
	//const int IDR_N_LP = 20;
	//const int CRA_NUT = 21;
	//const int RSV_IRAP_VCL22 = 22;
	//const int RSV_IRAP_VCL23 = 23;
	//const int RSV_VCL24 = 24;
	//const int RSV_VCL25 = 25;
	//const int RSV_VCL26 = 26;
	const int RSV_VCL27 = 27;
	const int RSV_VCL28 = 28;
	const int RSV_VCL29 = 29;
	const int RSV_VCL30 = 30;
	const int RSV_VCL31 = 31;
	const int VPS_NUT = 32;
	const int SPS_NUT = 7;
	const int PPS_NUT = 8;
	const int AUD_NUT = 9; //35; (HEVC)
	const int EOS_NUT = 36;
	const int EOB_NUT = 37;
	const int FD_NUT = 38;
	const int PREFIX_SEI_NUT = 39;
	const int SUFFIX_SEI_NUT = 40;
	const int RSV_NVCL41 = 41;
	const int RSV_NVCL42 = 42;
	const int RSV_NVCL43 = 43;
	const int RSV_NVCL44 = 44;
	const int RSV_NVCL45 = 45;
	const int RSV_NVCL46 = 46;
	const int RSV_NVCL47 = 47;
	const int UNSPEC48 = 48;
	const int UNSPEC49 = 49;
	const int UNSPEC50 = 50;
	const int UNSPEC51 = 51;
	const int UNSPEC52 = 52;
	const int UNSPEC53 = 53;
	const int UNSPEC54 = 54;
	const int UNSPEC55 = 55;
	const int UNSPEC56 = 56;
	const int UNSPEC57 = 57;
	const int UNSPEC58 = 58;
	const int UNSPEC59 = 59;
	const int UNSPEC60 = 60;
	const int UNSPEC61 = 61;
	const int UNSPEC62 = 62;
	const int UNSPEC63 = 63;

	[[nodiscard]] const char* getNalUnitTypeString(int nal_unit_type) const {
		const char* nal_unit_type_name[64] = { "TRAIL_N", "TRAIL_R", "TSA_N", "TSA_R", "STSA_N",
											  "STSA_R", "RADL_N", "RADL_R", "RASL_N", "RASL_R",
											  "RSV_VCL_N10", "RSV_VCL_N12", "RSV_VCL_N14",
											  "RSV_VCL_R11", "RSV_VCL_R13", "RSV_VCL_R15",
											  "BLA_W_LP", "BLA_W_DLP", "BLA_N_LP", "IDR_W_DLP",
											  "IDR_N_LP", "CRA_NUT", "RSV_IRAP_VCL22",
											  "RSV_IRAP_VCL23", "RSV_VCL24", "RSV_VCL25",
											  "RSV_VCL26", "RSV_VCL27", "RSV_VCL28",
											  "RSV_VCL29", "RSV_VCL30", "RSV_VCL31", "VPS_NUT",
											  "SPS_NUT", "PPS_NUT", "AUD_NUT", "EOS_NUT",
											  "EOB_NUT", "FD_NUT", "PREFIX_SEI_NUT",
											  "SUFFIX_SEI_NUT", "RSV_NVCL41", "RSV_NVCL42",
											  "RSV_NVCL43", "RSV_NVCL44", "RSV_NVCL45",
											  "RSV_NVCL46", "RSV_NVCL47", "UNSPEC48",
											  "UNSPEC49", "UNSPEC50", "UNSPEC51", "UNSPEC52",
											  "UNSPEC53", "UNSPEC54", "UNSPEC55", "UNSPEC56",
											  "UNSPEC57", "UNSPEC58", "UNSPEC59", "UNSPEC60",
											  "UNSPEC61", "UNSPEC62", "UNSPEC63" };
		if (nal_unit_type < TRAIL_N || nal_unit_type > UNSPEC63) return "UNKNOWN";
		return nal_unit_type_name[nal_unit_type];
	}

	void reset();
	std::string generalNalConvert(std::string& nal);
	std::string generalNalConvert(const unsigned char* src, unsigned int src_len);
	void parseVPS(unsigned int nal_unit_type, unsigned int nuh_layer_id, unsigned int nuh_temporal_id_plus1, std::string nal);
	void parsePPS(unsigned int nal_unit_type, unsigned int nuh_layer_id, unsigned int nuh_temporal_id_plus1, std::string nal);
	void profile_tier_level(CBitBuffer& bitBuff, unsigned int profilePresentFlag, unsigned int maxNumSubLayersMinus1);
	void parseSPS(unsigned int nal_unit_type, unsigned int nuh_layer_id, unsigned int nuh_temporal_id_plus1, std::string nal);
};

