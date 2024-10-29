#include "HvccParser.h"

void HvCCParser::reset() {
	strVPS = "";
	strSPS = "";
	strPPS = "";
}

std::string HvCCParser::generalNalConvert(std::string& nal) {
	if (nal.length() < 4) return nal;
	return generalNalConvert((unsigned char*)nal.c_str(), (unsigned int)nal.length());
}

std::string HvCCParser::generalNalConvert(const unsigned char* src, unsigned int src_len) {
	auto* des = new unsigned char[src_len];
	memset(des, 0, src_len);
	unsigned int src_pos = 0;
	unsigned int des_pos = 0;
	while (src_pos < src_len) {
		if (src_pos < src_len - 3 && src[src_pos] == 0 && src[src_pos + 1] == 0 &&
			src[src_pos + 2] == 3) {
			des_pos += 2;
			src_pos += 3;
		}
		else
			des[des_pos++] = src[src_pos++];
	}
	std::string ret = std::string((char*)des, des_pos);
	delete[] des;
	return ret;
}

void HvCCParser::parseVPS(unsigned int nal_unit_type, unsigned int nuh_layer_id,
	unsigned int nuh_temporal_id_plus1, std::string nal) {
	if (nuh_layer_id != 0) return;
	if (nal == strVPS) return;
	strVPS = nal;
	std::string generalNal = generalNalConvert(nal);
	CBitBuffer bitBuff((unsigned char*)generalNal.c_str());
	unsigned int vps_video_parameter_set_id = bitBuff.get(4);
	unsigned int vps_base_layer_internal_flag = bitBuff.get(1);
	unsigned int vps_base_layer_available_flag = bitBuff.get(1);
	unsigned int vps_max_layers_minus1 = bitBuff.get(6);
	unsigned int vps_max_sub_layers_minus1 = bitBuff.get(3);
	unsigned int vps_temporal_id_nesting_flag = bitBuff.get(1);
}

void HvCCParser::parsePPS(unsigned int nal_unit_type, unsigned int nuh_layer_id,
	unsigned int nuh_temporal_id_plus1, std::string nal) {
	if (nuh_layer_id != 0) return;
	if (nal == strPPS) return;
	strPPS = nal;
	std::string generalNal = generalNalConvert(nal);

	//printBinary("nal", (unsigned char*)nal.c_str(), (unsigned int)nal.length());
	//printBinary("generalNal", (unsigned char*)generalNal.c_str(), (unsigned int)generalNal.length());

	dependent_slice_segments_enabled_flag = 0;
	output_flag_present_flag = 0;
	num_extra_slice_header_bits = 0;

	CBitBuffer bitBuff((unsigned char*)generalNal.c_str());
	unsigned int pps_pic_parameter_set_id = bitBuff.get_golomb();
	unsigned int pps_seq_parameter_set_id = bitBuff.get_golomb();
	dependent_slice_segments_enabled_flag = bitBuff.get(1);
	output_flag_present_flag = bitBuff.get(1);
	num_extra_slice_header_bits = bitBuff.get(3);
	printf("%d\n", num_extra_slice_header_bits);
}

// 7.3.3 Profile, tierand level syntax
void HvCCParser::profile_tier_level(CBitBuffer& bitBuff, unsigned int profilePresentFlag,
	unsigned int maxNumSubLayersMinus1) {
	if (profilePresentFlag) {
		unsigned int general_profile_space = bitBuff.get(2);
		unsigned int general_tier_flag = bitBuff.get(1);
		unsigned int general_profile_idc = bitBuff.get(5);
		unsigned int general_profile_compatibility_flag[32];
		for (unsigned int& j : general_profile_compatibility_flag)
			j = bitBuff.get(1);
		unsigned int general_progressive_source_flag = bitBuff.get(1);
		unsigned int general_interlaced_source_flag = bitBuff.get(1);
		unsigned int general_non_packed_constraint_flag = bitBuff.get(1);
		unsigned int general_frame_only_constraint_flag = bitBuff.get(1);
		bitBuff.get(43); // general_reserved_zero_43bits
		bitBuff.get(1); // general_reserved_zero_bit
	}
	unsigned int general_level_idc = bitBuff.get(8); // general_level_idc
	//bitBuff.get(2* sps_max_sub_layers_minus1);
	unsigned int sub_layer_profile_present_flag[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned int sub_layer_level_present_flag[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	if (maxNumSubLayersMinus1 > 0) {
		for (unsigned int i = 0; i < maxNumSubLayersMinus1; i++) {
			sub_layer_profile_present_flag[i] = bitBuff.get(1);
			sub_layer_level_present_flag[i] = bitBuff.get(1);
		}
	}
	if (maxNumSubLayersMinus1 > 0) {
		for (unsigned int i = maxNumSubLayersMinus1; i < 8; i++) {
			unsigned int reserved_zero_2bits = bitBuff.get(2);
		}
	}
	for (unsigned int i = 0; i < maxNumSubLayersMinus1; i++) {
		if (sub_layer_profile_present_flag[i]) {
			bitBuff.get(44);
			bitBuff.get(43); // sub_layer_reserved_zero_43bits
			bitBuff.get(1); // sub_layer_reserved_zero_bit
			unsigned int sub_layer_level_idc = bitBuff.get(8);
		}
	}
}

// F.7.3.2.2.1 General sequence parameter set RBSP syntax
void HvCCParser::parseSPS(unsigned int nal_unit_type, unsigned int nuh_layer_id,
	unsigned int nuh_temporal_id_plus1, std::string nal) {
	if (nuh_layer_id != 0) return;
	if (nal == strSPS) return;
	strSPS = nal;
	std::string generalNal = generalNalConvert(nal);
	//printBinary("nal", (unsigned char*)nal.c_str(), (unsigned int)nal.length());
	//printBinary("generalNal", (unsigned char*)generalNal.c_str(), (unsigned int)generalNal.length());

	// 외부에서 사용할 변수 초기화
	separate_colour_plane_flag = 0;
	log2_max_pic_order_cnt_lsb_minus4 = 0;
	MaxPicOrderCntLsb = 0;
	bit_depth_chroma_minus8 = 0;
	vui_num_units_in_tick = 0;
	vui_time_scale = 0;
	sps_max_dec_pic_buffering_minus1 = 0;
	sps_max_num_reorder_pics = 0;
	sps_max_latency_increase_plus1 = 0;

	CBitBuffer bitBuff((unsigned char*)generalNal.c_str());
	unsigned int sps_video_parameter_set_id = bitBuff.get(4);
	unsigned int sps_max_sub_layers_minus1 = bitBuff.get(3);
	unsigned int sps_temporal_id_nesting_flag = bitBuff.get(1);
	profile_tier_level(bitBuff, 1, sps_max_sub_layers_minus1);

	unsigned int sps_seq_parameter_set_id = bitBuff.get_golomb();
	unsigned int chroma_format_idc = bitBuff.get_golomb();
	if (chroma_format_idc == 3) {
		separate_colour_plane_flag = bitBuff.get(1);
	}
	unsigned int pic_width_in_luma_samples = bitBuff.get_golomb();
	unsigned int pic_height_in_luma_samples = bitBuff.get_golomb();
	unsigned int conformance_window_flag = bitBuff.get(1);
	if (conformance_window_flag == 1) {
		unsigned int conf_win_left_offset = bitBuff.get_golomb();
		unsigned int conf_win_right_offset = bitBuff.get_golomb();
		unsigned int conf_win_top_offset = bitBuff.get_golomb();
		unsigned int conf_win_bottom_offset = bitBuff.get_golomb();
	}
	unsigned int bit_depth_luma_minus8 = bitBuff.get_golomb();
	bit_depth_chroma_minus8 = bitBuff.get_golomb();
	log2_max_pic_order_cnt_lsb_minus4 = bitBuff.get_golomb();
	MaxPicOrderCntLsb = (unsigned int)std::pow(2, log2_max_pic_order_cnt_lsb_minus4 + 4);
	unsigned int sps_sub_layer_ordering_info_present_flag = bitBuff.get(1);
	//printf("sps_sub_layer_ordering_info_present_flag=%d\n", sps_sub_layer_ordering_info_present_flag);
	for (unsigned int k = sps_sub_layer_ordering_info_present_flag ? 0
		: sps_max_sub_layers_minus1;
		k <= sps_max_sub_layers_minus1; k++) {
		sps_max_dec_pic_buffering_minus1 = bitBuff.get_golomb();
		sps_max_num_reorder_pics = bitBuff.get_golomb();
		sps_max_latency_increase_plus1 = bitBuff.get_golomb();
	}
	unsigned int log2_min_luma_coding_block_size_minus3 = bitBuff.get_golomb();
	unsigned int log2_diff_max_min_luma_coding_block_size = bitBuff.get_golomb();
	unsigned int log2_min_luma_transform_block_size_minus2 = bitBuff.get_golomb();
	unsigned int log2_diff_max_min_luma_transform_block_size = bitBuff.get_golomb();
	unsigned int max_transform_hierarchy_depth_inter = bitBuff.get_golomb();
	unsigned int max_transform_hierarchy_depth_intra = bitBuff.get_golomb();
	unsigned int scaling_list_enabled_flag = bitBuff.get(1);
	//printf("log2_min_luma_coding_block_size_minus3=%d\n", log2_min_luma_coding_block_size_minus3);
	//printf("log2_diff_max_min_luma_coding_block_size=%d\n", log2_diff_max_min_luma_coding_block_size);
	//printf("log2_min_luma_transform_block_size_minus2=%d\n", log2_min_luma_transform_block_size_minus2);
	//printf("log2_diff_max_min_luma_transform_block_size=%d\n", log2_diff_max_min_luma_transform_block_size);
	//printf("max_transform_hierarchy_depth_inter=%d\n", max_transform_hierarchy_depth_inter);
	//printf("max_transform_hierarchy_depth_intra=%d\n", max_transform_hierarchy_depth_intra);
	//printf("scaling_list_enabled_flag=%d\n", scaling_list_enabled_flag);
	if (scaling_list_enabled_flag) {
		unsigned int sps_infer_scaling_list_flag = 0;
		//if (MultiLayerExtSpsFlag) {
		//	sps_infer_scaling_list_flag = bitBuff.get(1);
		//printf("sps_infer_scaling_list_flag=%d\n", sps_infer_scaling_list_flag);
		//}
		if (sps_infer_scaling_list_flag) {
			unsigned int sps_scaling_list_ref_layer_id = bitBuff.get(6);
			//printf("sps_scaling_list_ref_layer_id=%d\n", sps_scaling_list_ref_layer_id);
		}
		else {
			unsigned int sps_scaling_list_data_present_flag = bitBuff.get(1);
			//printf("sps_scaling_list_data_present_flag=%d\n", sps_scaling_list_data_present_flag);
			if (sps_scaling_list_data_present_flag) {
				// scaling_list_data( ) Page65
				for (unsigned int sizeID = 0; sizeID < 4; sizeID++) {
					for (unsigned int matrixId = 0;
						matrixId < 6; matrixId += (sizeID == 3) ? 3 : 1) {
						unsigned int scaling_list_pred_mode_flag = bitBuff.get(1);
						if (scaling_list_pred_mode_flag == 0) {
							unsigned int scaling_list_pred_matrix_id_delta = bitBuff.get_golomb();
						}
						else {
							unsigned int coefNum = 1 << (sizeID * 2 + 4);
							if (coefNum > 64)
								coefNum = 64;
							if (sizeID > 1) {
								unsigned int scaling_list_dc_coef_minus8 = bitBuff.get_golomb();
							}
							for (unsigned int k = 0; k < coefNum; k++) {
								unsigned int scaling_list_delta_coef = bitBuff.get_golomb();
							}
						}
					}
				}
			}
		}
	}
	unsigned int amp_enabled_flag = bitBuff.get(1);
	unsigned int sample_adaptive_offset_enabled_flag = bitBuff.get(1);
	unsigned int pcm_enabled_flag = bitBuff.get(1);
	//printf("amp_enabled_flag=%d\n", amp_enabled_flag);
	//printf("sample_adaptive_offset_enabled_flag=%d\n", sample_adaptive_offset_enabled_flag);
	//printf("pcm_enabled_flag=%d\n", pcm_enabled_flag);
	if (pcm_enabled_flag) {
		unsigned int pcm_sample_bit_depth_luma_minus1 = bitBuff.get(4);
		unsigned int pcm_sample_bit_depth_chroma_minus1 = bitBuff.get(4);
		unsigned int log2_min_pcm_luma_coding_block_size_minus3 = bitBuff.get_golomb();
		unsigned int log2_diff_max_min_pcm_luma_coding_block_size = bitBuff.get_golomb();
		unsigned int pcm_loop_filter_disabled_flag = bitBuff.get(1);
		//printf("pcm_sample_bit_depth_luma_minus1=%d\n", pcm_sample_bit_depth_luma_minus1);
		//printf("pcm_sample_bit_depth_chroma_minus1=%d\n", pcm_sample_bit_depth_chroma_minus1);
		//printf("log2_min_pcm_luma_coding_block_size_minus3=%d\n", log2_min_pcm_luma_coding_block_size_minus3);
		//printf("log2_diff_max_min_pcm_luma_coding_block_size=%d\n", log2_diff_max_min_pcm_luma_coding_block_size);
		//printf("pcm_loop_filter_disabled_flag=%d\n", pcm_loop_filter_disabled_flag);
	}
	unsigned int num_short_term_ref_pic_sets = bitBuff.get_golomb();
	//printf("num_short_term_ref_pic_sets=%d\n", num_short_term_ref_pic_sets);
	for (unsigned int stRpsIdx = 0; stRpsIdx < num_short_term_ref_pic_sets; stRpsIdx++) {
		// st_ref_pic_set( i ) page70
		unsigned int inter_ref_pic_set_prediction_flag = 0;
		if (stRpsIdx != 0) {
			inter_ref_pic_set_prediction_flag = bitBuff.get(1);
		}
		if (inter_ref_pic_set_prediction_flag) {
			long delta_idx_minus1 = -1;
			if (stRpsIdx == num_short_term_ref_pic_sets) {
				delta_idx_minus1 = (long)bitBuff.get_golomb();
			}
			unsigned int delta_rps_sign = bitBuff.get(1);
			unsigned int abs_delta_rps_minus1 = bitBuff.get_golomb();
			for (unsigned int k = 0; k <= abs_delta_rps_minus1; k++) // ??
			{
				unsigned int used_by_curr_pic_flag = bitBuff.get(1);
				if (used_by_curr_pic_flag) {
					unsigned int use_delta_flag = bitBuff.get(1);
				}
			}
		}
		else {
			unsigned int num_negative_pics = bitBuff.get_golomb();
			unsigned int num_positive_pics = bitBuff.get_golomb();
			for (unsigned int k = 0; k < num_negative_pics; k++) {
				unsigned int delta_poc_s0_minus1 = bitBuff.get_golomb();
				unsigned int used_by_curr_pic_s0_flag = bitBuff.get(1);
			}
			for (unsigned int k = 0; k < num_positive_pics; k++) {
				unsigned int delta_poc_s1_minus1 = bitBuff.get_golomb();
				unsigned int used_by_curr_pic_s1_flag = bitBuff.get(1);
			}
		}
	}
	unsigned int long_term_ref_pics_present_flag = bitBuff.get(1);
	//printf("long_term_ref_pics_present_flag=%d\n", long_term_ref_pics_present_flag);
	if (long_term_ref_pics_present_flag) {
		unsigned int num_long_term_ref_pics_sps = bitBuff.get_golomb();
		//printf("num_long_term_ref_pics_sps=%d\n", num_long_term_ref_pics_sps);
		for (unsigned int k = 0; k < num_long_term_ref_pics_sps; k++) {
			unsigned int lt_ref_pic_poc_lsb_sps = bitBuff.get_golomb(); // u(v) ??
			unsigned int used_by_curr_pic_lt_sps_flag = bitBuff.get(1);
			//printf("lt_ref_pic_poc_lsb_sps[%d]=%d\n", k, lt_ref_pic_poc_lsb_sps);
			//printf("used_by_curr_pic_lt_sps_flag[%d]=%d\n", k, used_by_curr_pic_lt_sps_flag);
		}
	}
	unsigned int sps_temporal_mvp_enabled_flag = bitBuff.get(1);
	unsigned int strong_intra_smoothing_enabled_flag = bitBuff.get(1);
	unsigned int vui_parameters_present_flag = bitBuff.get(1);
	//printf("sps_temporal_mvp_enabled_flag=%d\n", sps_temporal_mvp_enabled_flag);
	//printf("strong_intra_smoothing_enabled_flag=%d\n", strong_intra_smoothing_enabled_flag);
	//printf("vui_parameters_present_flag=%d\n", vui_parameters_present_flag);
	if (vui_parameters_present_flag) {
		//vui_parameters(); Page 363
		unsigned int aspect_ratio_info_present_flag = bitBuff.get(1);
		if (aspect_ratio_info_present_flag) {
			unsigned int aspect_ratio_idc = bitBuff.get(8);
			if (aspect_ratio_idc == 255 /*EXTENDED_SAR*/) {
				unsigned int sar_width = bitBuff.get(16);
				unsigned int sar_height = bitBuff.get(16);
			}
		}
		unsigned int overscan_info_present_flag = bitBuff.get(1);
		if (overscan_info_present_flag) {
			unsigned int overscan_appropriate_flag = bitBuff.get(1);
		}
		unsigned int video_signal_type_present_flag = bitBuff.get(1);
		if (video_signal_type_present_flag) {
			unsigned int video_format = bitBuff.get(3);
			unsigned int video_full_range_flag = bitBuff.get(1);
			unsigned int colour_description_present_flag = bitBuff.get(1);
			if (colour_description_present_flag) {
				unsigned int colour_primaries = bitBuff.get(8);
				unsigned int transfer_characteristics = bitBuff.get(8);
				std::string strHdrMode = "Unknown";
				if (transfer_characteristics == 1) strHdrMode = "BT.709";
				if (transfer_characteristics == 18) strHdrMode = "HLG BT.2100";
				if (transfer_characteristics == 14) strHdrMode = "HLG BT.2020";
				if (transfer_characteristics == 16) strHdrMode = "PQ ST.2084";
				printf("transfer_characteristics:%s(%d)\n", strHdrMode.c_str(),
					transfer_characteristics);
				unsigned int matrix_coeffs = bitBuff.get(8);
			}
		}
		unsigned int chroma_loc_info_present_flag = bitBuff.get(1);
		if (chroma_loc_info_present_flag) {
			unsigned int chroma_sample_loc_type_top_field = bitBuff.get_golomb();
			unsigned int chroma_sample_loc_type_bottom_field = bitBuff.get_golomb();
		}
		unsigned int neutral_chroma_indication_flag = bitBuff.get(1);
		unsigned int field_seq_flag = bitBuff.get(1);
		unsigned int frame_field_info_present_flag = bitBuff.get(1);
		unsigned int default_display_window_flag = bitBuff.get(1);
		if (default_display_window_flag) {
			unsigned int def_disp_win_left_offset = bitBuff.get_golomb();
			unsigned int def_disp_win_right_offset = bitBuff.get_golomb();
			unsigned int def_disp_win_top_offset = bitBuff.get_golomb();
			unsigned int def_disp_win_bottom_offset = bitBuff.get_golomb();
		}
		unsigned int vui_timing_info_present_flag = bitBuff.get(1);
		if (vui_timing_info_present_flag) {
			vui_num_units_in_tick = bitBuff.get(32);
			vui_time_scale = bitBuff.get(32);
			//printf("vui_num_units_in_tick=%d\n", vui_num_units_in_tick);
			//printf("vui_time_scale=%d\n", vui_time_scale);
			// 추가 구현은 필요시
		}
	}
}
