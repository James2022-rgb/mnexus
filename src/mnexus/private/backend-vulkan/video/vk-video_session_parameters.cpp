// TU header --------------------------------------------
#include "backend-vulkan/video/vk-video_session_parameters.h"

// c++ headers ------------------------------------------
#include <cstring>
#include <utility>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/log.h"

// external headers -------------------------------------
#include "vidsynt.h"

// project headers --------------------------------------
#include "backend-vulkan/device/vk-device.h"

namespace mnexus_backend::vulkan {

namespace {

// ----------------------------------------------------------------------------------------------------
// Spec uint8 -> StdVideoH265* enum cast helpers
//

StdVideoH265ProfileIdc ToStdProfileIdc(uint8_t spec_profile_idc) {
  switch (spec_profile_idc) {
  case 1: return STD_VIDEO_H265_PROFILE_IDC_MAIN;
  case 2: return STD_VIDEO_H265_PROFILE_IDC_MAIN_10;
  case 3: return STD_VIDEO_H265_PROFILE_IDC_MAIN_STILL_PICTURE;
  case 4: return STD_VIDEO_H265_PROFILE_IDC_FORMAT_RANGE_EXTENSIONS;
  case 9: return STD_VIDEO_H265_PROFILE_IDC_SCC_EXTENSIONS;
  default: return STD_VIDEO_H265_PROFILE_IDC_INVALID;
  }
}

StdVideoH265LevelIdc ToStdLevelIdc(uint8_t spec_level_idc) {
  // spec_level_idc = level x 30
  switch (spec_level_idc) {
  case 30:  return STD_VIDEO_H265_LEVEL_IDC_1_0;
  case 60:  return STD_VIDEO_H265_LEVEL_IDC_2_0;
  case 63:  return STD_VIDEO_H265_LEVEL_IDC_2_1;
  case 90:  return STD_VIDEO_H265_LEVEL_IDC_3_0;
  case 93:  return STD_VIDEO_H265_LEVEL_IDC_3_1;
  case 120: return STD_VIDEO_H265_LEVEL_IDC_4_0;
  case 123: return STD_VIDEO_H265_LEVEL_IDC_4_1;
  case 150: return STD_VIDEO_H265_LEVEL_IDC_5_0;
  case 153: return STD_VIDEO_H265_LEVEL_IDC_5_1;
  case 156: return STD_VIDEO_H265_LEVEL_IDC_5_2;
  case 180: return STD_VIDEO_H265_LEVEL_IDC_6_0;
  case 183: return STD_VIDEO_H265_LEVEL_IDC_6_1;
  case 186: return STD_VIDEO_H265_LEVEL_IDC_6_2;
  default:  return STD_VIDEO_H265_LEVEL_IDC_INVALID;
  }
}

// ----------------------------------------------------------------------------------------------------
// Bit-pack helpers (vidsynt -> Std flag struct)
//

StdVideoH265ProfileTierLevelFlags BuildPtlFlags(VidsyntHevcProfileTierLevel const& ptl) {
  StdVideoH265ProfileTierLevelFlags flags{};
  flags.general_tier_flag                 = ptl.general_tier_flag;
  flags.general_progressive_source_flag   = ptl.general_progressive_source_flag;
  flags.general_interlaced_source_flag    = ptl.general_interlaced_source_flag;
  flags.general_non_packed_constraint_flag = ptl.general_non_packed_constraint_flag;
  flags.general_frame_only_constraint_flag = ptl.general_frame_only_constraint_flag;
  return flags;
}

StdVideoH265VpsFlags BuildVpsFlags(VidsyntHevcVideoParameterSet const& vps) {
  StdVideoH265VpsFlags flags{};
  flags.vps_temporal_id_nesting_flag             = vps.vps_temporal_id_nesting_flag;
  flags.vps_sub_layer_ordering_info_present_flag = (vps.sub_layer_ordering_info != nullptr) ? 1u : 0u;
  flags.vps_timing_info_present_flag             = (vps.timing_info != nullptr) ? 1u : 0u;
  flags.vps_poc_proportional_to_timing_flag      =
      (vps.timing_info != nullptr && vps.timing_info->vps_num_ticks_poc_diff_one_minus1 != 0xFFFFFFFFu) ? 1u : 0u;
  return flags;
}

StdVideoH265SpsFlags BuildSpsFlags(VidsyntHevcSequenceParameterSet const& sps) {
  StdVideoH265SpsFlags flags{};
  flags.sps_temporal_id_nesting_flag             = sps.sps_temporal_id_nesting_flag;
  flags.separate_colour_plane_flag               = (sps.separate_colour_plane_flag == 0xFFu) ? 0u : sps.separate_colour_plane_flag;
  flags.conformance_window_flag                  = (sps.conformance_window != nullptr) ? 1u : 0u;
  flags.sps_sub_layer_ordering_info_present_flag = (sps.sub_layer_ordering_info != nullptr) ? 1u : 0u;
  flags.scaling_list_enabled_flag                = sps.scaling_list_enabled_flag;
  flags.sps_scaling_list_data_present_flag       = 0; // vidsynt does not surface this; we never provide custom lists.
  flags.amp_enabled_flag                         = sps.amp_enabled_flag;
  flags.sample_adaptive_offset_enabled_flag      = sps.sample_adaptive_offset_enabled_flag;
  flags.pcm_enabled_flag                         = sps.pcm_enabled_flag;
  flags.pcm_loop_filter_disabled_flag            = sps.pcm_loop_filter_disabled_flag;
  flags.long_term_ref_pics_present_flag          = sps.long_term_ref_pics_present_flag;
  flags.sps_temporal_mvp_enabled_flag            = sps.sps_temporal_mvp_enabled_flag;
  flags.strong_intra_smoothing_enabled_flag      = sps.strong_intra_smoothing_enabled_flag;
  flags.vui_parameters_present_flag              = (sps.vui != nullptr) ? 1u : 0u;
  // sps_extension / range / SCC bits: vidsynt does not surface them; assume off.
  return flags;
}

StdVideoH265PpsFlags BuildPpsFlags(VidsyntHevcPictureParameterSet const& pps) {
  StdVideoH265PpsFlags flags{};
  flags.dependent_slice_segments_enabled_flag    = pps.dependent_slice_segments_enabled_flag;
  flags.output_flag_present_flag                 = pps.output_flag_present_flag;
  flags.sign_data_hiding_enabled_flag            = pps.sign_data_hiding_enabled_flag;
  flags.cabac_init_present_flag                  = pps.cabac_init_present_flag;
  flags.constrained_intra_pred_flag              = pps.constrained_intra_pred_flag;
  flags.transform_skip_enabled_flag              = pps.transform_skip_enabled_flag;
  flags.cu_qp_delta_enabled_flag                 = pps.cu_qp_delta_enabled_flag;
  flags.pps_slice_chroma_qp_offsets_present_flag = pps.pps_slice_chroma_qp_offsets_present_flag;
  flags.weighted_pred_flag                       = pps.weighted_pred_flag;
  flags.weighted_bipred_flag                     = pps.weighted_bipred_flag;
  flags.transquant_bypass_enabled_flag           = pps.transquant_bypass_enabled_flag;
  flags.tiles_enabled_flag                       = (pps.tiles != nullptr) ? 1u : 0u;
  flags.entropy_coding_sync_enabled_flag         = pps.entropy_coding_sync_enabled_flag;
  flags.uniform_spacing_flag                     = (pps.tiles != nullptr) ? pps.tiles->uniform_spacing_flag : 0u;
  flags.loop_filter_across_tiles_enabled_flag    = (pps.tiles != nullptr) ? pps.tiles->loop_filter_across_tiles_enabled_flag : 0u;
  flags.pps_loop_filter_across_slices_enabled_flag = pps.pps_loop_filter_across_slices_enabled_flag;
  flags.deblocking_filter_control_present_flag   = (pps.deblocking_filter_control != nullptr) ? 1u : 0u;
  flags.deblocking_filter_override_enabled_flag  = 0; // vidsynt does not surface this.
  flags.pps_deblocking_filter_disabled_flag      =
      (pps.deblocking_filter_control != nullptr) ? pps.deblocking_filter_control->pps_deblocking_filter_disabled_flag : 0u;
  flags.pps_scaling_list_data_present_flag       = pps.pps_scaling_list_data_present_flag;
  flags.lists_modification_present_flag          = pps.lists_modification_present_flag;
  flags.slice_segment_header_extension_present_flag = pps.slice_segment_header_extension_present_flag;
  flags.pps_extension_present_flag               = pps.pps_extension_present_flag;
  // Range / SCC extension flags: vidsynt does not surface them; assume off.
  return flags;
}

StdVideoH265ShortTermRefPicSetFlags BuildShortTermRefPicSetFlags(VidsyntHevcShortTermRefPicSetFlags const& src) {
  StdVideoH265ShortTermRefPicSetFlags flags{};
  flags.inter_ref_pic_set_prediction_flag = src.inter_ref_pic_set_prediction_flag;
  flags.delta_rps_sign                    = src.delta_rps_sign;
  return flags;
}

StdVideoH265ShortTermRefPicSet BuildShortTermRefPicSet(VidsyntHevcShortTermRefPicSet const& src) {
  StdVideoH265ShortTermRefPicSet out{};
  out.flags                    = BuildShortTermRefPicSetFlags(src.flags);
  out.delta_idx_minus1         = src.delta_idx_minus1;
  out.use_delta_flag           = src.use_delta_flag;
  out.abs_delta_rps_minus1     = src.abs_delta_rps_minus1;
  out.used_by_curr_pic_flag    = src.used_by_curr_pic_flag;
  out.used_by_curr_pic_s0_flag = src.used_by_curr_pic_s0_flag;
  out.used_by_curr_pic_s1_flag = src.used_by_curr_pic_s1_flag;
  out.num_negative_pics        = src.num_negative_pics;
  out.num_positive_pics        = src.num_positive_pics;
  static_assert(STD_VIDEO_H265_MAX_DPB_SIZE >= 16, "STD_VIDEO_H265_MAX_DPB_SIZE must accommodate vidsynt's 16-entry delta_poc arrays");
  for (int i = 0; i < 16; ++i) {
    out.delta_poc_s0_minus1[i] = src.delta_poc_s0_minus1[i];
    out.delta_poc_s1_minus1[i] = src.delta_poc_s1_minus1[i];
  }
  return out;
}

StdVideoH265SpsVuiFlags BuildSpsVuiFlags(VidsyntHevcVui const& vui) {
  StdVideoH265SpsVuiFlags flags{};
  flags.aspect_ratio_info_present_flag    = vui.aspect_ratio_info_present_flag;
  flags.video_signal_type_present_flag    = (vui.video_signal_type != nullptr) ? 1u : 0u;
  flags.video_full_range_flag             = (vui.video_signal_type != nullptr) ? vui.video_signal_type->video_full_range_flag : 0u;
  flags.colour_description_present_flag   =
      (vui.video_signal_type != nullptr && vui.video_signal_type->colour_description != nullptr) ? 1u : 0u;
  flags.chroma_loc_info_present_flag      = (vui.chroma_loc_info != nullptr) ? 1u : 0u;
  flags.neutral_chroma_indication_flag    = vui.neutral_chroma_indication_flag;
  flags.field_seq_flag                    = vui.field_seq_flag;
  flags.frame_field_info_present_flag     = vui.frame_field_info_present_flag;
  flags.vui_timing_info_present_flag      = (vui.vui_timing_info != nullptr) ? 1u : 0u;
  flags.vui_poc_proportional_to_timing_flag =
      (vui.vui_timing_info != nullptr && vui.vui_timing_info->vui_num_ticks_poc_diff_one_minus1 != 0xFFFFFFFFu) ? 1u : 0u;
  // Other VUI bits (overscan / default_display_window / hrd / bitstream_restriction / etc.): not surfaced by vidsynt.
  return flags;
}

// ----------------------------------------------------------------------------------------------------
// Sub-struct converters
//

StdVideoH265DecPicBufMgr BuildDecPicBufMgr(VidsyntHevcSubLayerOrderingInfo const& info) {
  StdVideoH265DecPicBufMgr m{};
  static_assert(STD_VIDEO_H265_SUBLAYERS_LIST_SIZE >= 7, "STD_VIDEO_H265_SUBLAYERS_LIST_SIZE must accommodate vidsynt's 7 sub-layer entries");
  for (int i = 0; i < 7; ++i) {
    m.max_latency_increase_plus1[i] = info.max_latency_increase_plus1[i];
    m.max_dec_pic_buffering_minus1[i] = info.max_dec_pic_buffering_minus1[i];
    m.max_num_reorder_pics[i] = info.max_num_reorder_pics[i];
  }
  return m;
}

StdVideoH265ProfileTierLevel BuildProfileTierLevel(
  VidsyntHevcProfileTierLevel const& src,
  StdVideoH265ProfileTierLevelFlags const& ptl_flags
) {
  StdVideoH265ProfileTierLevel out{};
  out.flags               = ptl_flags;
  out.general_profile_idc = ToStdProfileIdc(src.general_profile_idc);
  out.general_level_idc   = ToStdLevelIdc(src.general_level_idc);
  return out;
}

StdVideoH265SequenceParameterSetVui BuildSpsVui(
  VidsyntHevcVui const& src,
  StdVideoH265SpsVuiFlags const& vui_flags
) {
  StdVideoH265SequenceParameterSetVui vui{};
  vui.flags             = vui_flags;
  vui.aspect_ratio_idc  = static_cast<StdVideoH265AspectRatioIdc>(src.aspect_ratio_idc);
  vui.sar_width         = src.sar_width;
  vui.sar_height        = src.sar_height;
  if (src.video_signal_type != nullptr) {
    vui.video_format = src.video_signal_type->video_format;
    if (src.video_signal_type->colour_description != nullptr) {
      vui.colour_primaries         = src.video_signal_type->colour_description->colour_primaries;
      vui.transfer_characteristics = src.video_signal_type->colour_description->transfer_characteristics;
      vui.matrix_coeffs            = src.video_signal_type->colour_description->matrix_coeffs;
    }
  }
  if (src.chroma_loc_info != nullptr) {
    vui.chroma_sample_loc_type_top_field    = src.chroma_loc_info->chroma_sample_loc_type_top_field;
    vui.chroma_sample_loc_type_bottom_field = src.chroma_loc_info->chroma_sample_loc_type_bottom_field;
  }
  if (src.vui_timing_info != nullptr) {
    vui.vui_num_units_in_tick           = src.vui_timing_info->vui_num_units_in_tick;
    vui.vui_time_scale                  = src.vui_timing_info->vui_time_scale;
    vui.vui_num_ticks_poc_diff_one_minus1 =
        (src.vui_timing_info->vui_num_ticks_poc_diff_one_minus1 != 0xFFFFFFFFu)
        ? src.vui_timing_info->vui_num_ticks_poc_diff_one_minus1
        : 0u;
  }
  // Default display window / min_spatial_segmentation_idc / HRD / max_bytes_per_pic_denom etc.:
  // not surfaced by vidsynt; left as default-zero.
  return vui;
}

// ----------------------------------------------------------------------------------------------------
// Bundled "parsed" containers (own all storage referenced by the resulting Std* structs).
//

struct ParsedVps {
  StdVideoH265ProfileTierLevelFlags ptl_flags{};
  StdVideoH265ProfileTierLevel      ptl{};
  StdVideoH265DecPicBufMgr          dpb_mgr{};
  StdVideoH265VpsFlags              flags{};
  StdVideoH265VideoParameterSet     vps{};
};

struct ParsedSps {
  StdVideoH265ProfileTierLevelFlags ptl_flags{};
  StdVideoH265ProfileTierLevel      ptl{};
  StdVideoH265DecPicBufMgr          dpb_mgr{};
  StdVideoH265SpsVuiFlags           vui_flags{};
  StdVideoH265SequenceParameterSetVui vui{};
  StdVideoH265SpsFlags              flags{};
  StdVideoH265SequenceParameterSet  sps{};
  /// Storage backing `sps.pShortTermRefPicSet`; sized to `sps.num_short_term_ref_pic_sets`.
  std::vector<StdVideoH265ShortTermRefPicSet> short_term_ref_pic_sets;
  bool                              has_vui = false;
  bool                              has_dpb_mgr = false;
};

struct ParsedPps {
  StdVideoH265PpsFlags             flags{};
  StdVideoH265PictureParameterSet  pps{};
};

void FillVps(ParsedVps& out, VidsyntHevcVideoParameterSet const& src) {
  if (src.profile_tier_level != nullptr) {
    out.ptl_flags = BuildPtlFlags(*src.profile_tier_level);
    out.ptl       = BuildProfileTierLevel(*src.profile_tier_level, out.ptl_flags);
  }
  if (src.sub_layer_ordering_info != nullptr) {
    out.dpb_mgr = BuildDecPicBufMgr(*src.sub_layer_ordering_info);
  }
  out.flags = BuildVpsFlags(src);

  out.vps.flags                          = out.flags;
  out.vps.vps_video_parameter_set_id     = src.vps_video_parameter_set_id;
  out.vps.vps_max_sub_layers_minus1      = src.vps_max_sub_layers_minus1;
  if (src.timing_info != nullptr) {
    out.vps.vps_num_units_in_tick = src.timing_info->vps_num_units_in_tick;
    out.vps.vps_time_scale        = src.timing_info->vps_time_scale;
    out.vps.vps_num_ticks_poc_diff_one_minus1 =
        (src.timing_info->vps_num_ticks_poc_diff_one_minus1 != 0xFFFFFFFFu)
        ? src.timing_info->vps_num_ticks_poc_diff_one_minus1
        : 0u;
  }
  out.vps.pDecPicBufMgr     = (src.sub_layer_ordering_info != nullptr) ? &out.dpb_mgr : nullptr;
  out.vps.pHrdParameters    = nullptr; // vidsynt does not surface VPS HRD parameters.
  out.vps.pProfileTierLevel = (src.profile_tier_level != nullptr) ? &out.ptl : nullptr;
}

void FillSps(ParsedSps& out, VidsyntHevcSequenceParameterSet const& src) {
  if (src.profile_tier_level != nullptr) {
    out.ptl_flags = BuildPtlFlags(*src.profile_tier_level);
    out.ptl       = BuildProfileTierLevel(*src.profile_tier_level, out.ptl_flags);
  }
  if (src.sub_layer_ordering_info != nullptr) {
    out.dpb_mgr = BuildDecPicBufMgr(*src.sub_layer_ordering_info);
    out.has_dpb_mgr = true;
  }
  if (src.vui != nullptr) {
    out.vui_flags = BuildSpsVuiFlags(*src.vui);
    out.vui       = BuildSpsVui(*src.vui, out.vui_flags);
    out.has_vui   = true;
  }
  out.flags = BuildSpsFlags(src);

  out.sps.flags                                       = out.flags;
  out.sps.chroma_format_idc                           = static_cast<StdVideoH265ChromaFormatIdc>(src.chroma_format_idc);
  out.sps.pic_width_in_luma_samples                   = src.pic_width_in_luma_samples;
  out.sps.pic_height_in_luma_samples                  = src.pic_height_in_luma_samples;
  out.sps.sps_video_parameter_set_id                  = src.sps_video_parameter_set_id;
  out.sps.sps_max_sub_layers_minus1                   = src.sps_max_sub_layers_minus1;
  out.sps.sps_seq_parameter_set_id                    = src.sps_seq_parameter_set_id;
  out.sps.bit_depth_luma_minus8                       = src.bit_depth_luma_minus8;
  out.sps.bit_depth_chroma_minus8                     = src.bit_depth_chroma_minus8;
  out.sps.log2_max_pic_order_cnt_lsb_minus4           = src.log2_max_pic_order_cnt_lsb_minus4;
  out.sps.log2_min_luma_coding_block_size_minus3      = src.log2_min_luma_coding_block_size_minus3;
  out.sps.log2_diff_max_min_luma_coding_block_size    = src.log2_diff_max_min_luma_coding_block_size;
  out.sps.log2_min_luma_transform_block_size_minus2   = src.log2_min_luma_transform_block_size_minus2;
  out.sps.log2_diff_max_min_luma_transform_block_size = src.log2_diff_max_min_luma_transform_block_size;
  out.sps.max_transform_hierarchy_depth_inter         = src.max_transform_hierarchy_depth_inter;
  out.sps.max_transform_hierarchy_depth_intra         = src.max_transform_hierarchy_depth_intra;
  // Translate the SPS-stored short-term reference picture sets.
  if (src.short_term_ref_pic_sets != nullptr && src.short_term_ref_pic_set_count > 0) {
    out.short_term_ref_pic_sets.reserve(src.short_term_ref_pic_set_count);
    for (uintptr_t i = 0; i < src.short_term_ref_pic_set_count; ++i) {
      out.short_term_ref_pic_sets.push_back(BuildShortTermRefPicSet(src.short_term_ref_pic_sets[i]));
    }
  }
  out.sps.num_short_term_ref_pic_sets                 = static_cast<uint8_t>(out.short_term_ref_pic_sets.size());
  out.sps.num_long_term_ref_pics_sps                  = src.num_long_term_ref_pics_sps;
  // PCM / palette / motion vector resolution: vidsynt does not surface; default-zero.
  if (src.conformance_window != nullptr) {
    out.sps.conf_win_left_offset   = src.conformance_window->conf_win_left_offset;
    out.sps.conf_win_right_offset  = src.conformance_window->conf_win_right_offset;
    out.sps.conf_win_top_offset    = src.conformance_window->conf_win_top_offset;
    out.sps.conf_win_bottom_offset = src.conformance_window->conf_win_bottom_offset;
  }
  out.sps.pProfileTierLevel        = (src.profile_tier_level != nullptr) ? &out.ptl : nullptr;
  out.sps.pDecPicBufMgr            = out.has_dpb_mgr ? &out.dpb_mgr : nullptr;
  out.sps.pScalingLists            = nullptr;
  out.sps.pShortTermRefPicSet      = out.short_term_ref_pic_sets.empty() ? nullptr : out.short_term_ref_pic_sets.data();
  out.sps.pLongTermRefPicsSps      = nullptr; // long-term refs not supported in this path
  out.sps.pSequenceParameterSetVui = out.has_vui ? &out.vui : nullptr;
  out.sps.pPredictorPaletteEntries = nullptr;
}

void FillPps(ParsedPps& out, VidsyntHevcPictureParameterSet const& src) {
  out.flags = BuildPpsFlags(src);

  out.pps.flags                                = out.flags;
  out.pps.pps_pic_parameter_set_id             = src.pps_pic_parameter_set_id;
  out.pps.pps_seq_parameter_set_id             = src.pps_seq_parameter_set_id;
  // sps_video_parameter_set_id: not present in vidsynt PPS. Caller-supplied SPS
  // carries the VPS-id chain; leave 0 here.
  out.pps.num_extra_slice_header_bits          = src.num_extra_slice_header_bits;
  out.pps.num_ref_idx_l0_default_active_minus1 = src.num_ref_idx_l0_default_active_minus1;
  out.pps.num_ref_idx_l1_default_active_minus1 = src.num_ref_idx_l1_default_active_minus1;
  out.pps.init_qp_minus26                      = src.init_qp_minus26;
  out.pps.diff_cu_qp_delta_depth               = (src.diff_cu_qp_delta_depth == 0xFFu) ? 0u : src.diff_cu_qp_delta_depth;
  out.pps.pps_cb_qp_offset                     = src.pps_cb_qp_offset;
  out.pps.pps_cr_qp_offset                     = src.pps_cr_qp_offset;
  if (src.deblocking_filter_control != nullptr) {
    out.pps.pps_beta_offset_div2 = src.deblocking_filter_control->pps_beta_offset_div2;
    out.pps.pps_tc_offset_div2   = src.deblocking_filter_control->pps_tc_offset_div2;
  }
  out.pps.log2_parallel_merge_level_minus2     = src.log2_parallel_merge_level_minus2;
  if (src.tiles != nullptr) {
    out.pps.num_tile_columns_minus1 = src.tiles->num_tile_columns_minus1;
    out.pps.num_tile_rows_minus1    = src.tiles->num_tile_rows_minus1;
    // column_width_minus1 / row_height_minus1: only meaningful when uniform_spacing_flag is 0;
    // vidsynt does not surface them. Default-zero is acceptable when uniform_spacing_flag is set.
  }
  // Range / SCC extension fields and scaling lists / palette entries: not provided.
  out.pps.pScalingLists            = nullptr;
  out.pps.pPredictorPaletteEntries = nullptr;
}

// ----------------------------------------------------------------------------------------------------
// vidsynt parse helpers.
//

bool ParseNalu(VidsyntHevcContext* ctx, uint8_t const* data, uint32_t size, VidsyntHevcNalu const** out_nalu) {
  if (data == nullptr || size == 0) {
    MBASE_LOG_ERROR("vidsynt: empty NAL data.");
    return false;
  }
  VidsyntResult const r = vidsynt_hevc_parse_nalu_from_bytes(ctx, data, size, out_nalu);
  if (r != VidsyntResult::Success) {
    MBASE_LOG_ERROR("vidsynt_hevc_parse_nalu_from_bytes failed: {}", static_cast<int>(r));
    return false;
  }
  return true;
}

} // anonymous namespace

resource_pool::ResourceHandle EmplaceVideoSessionParametersResourcePoolDecodeH265(
  VideoSessionParametersResourcePool& out_pool,
  IVulkanDevice& vk_device,
  VideoSessionResourcePool const& session_pool,
  mnexus::VideoSessionParametersDecodeH265Desc const& desc
) {
  // Resolve the originating session.
  auto const session_pool_handle = resource_pool::ResourceHandle::FromU64(desc.session.Get());
  if (session_pool_handle.IsNull()) {
    MBASE_LOG_ERROR("CreateVideoSessionParametersDecodeH265: invalid session handle.");
    return resource_pool::ResourceHandle::Null();
  }
  auto [session_hot, session_cold, session_lock] =
    session_pool.GetConstRefWithSharedLockGuard(session_pool_handle);
  VkVideoSessionKHR const vk_session = session_hot.vk_video_session.handle();
  if (vk_session == VK_NULL_HANDLE) {
    MBASE_LOG_ERROR("CreateVideoSessionParametersDecodeH265: session VkVideoSessionKHR is null.");
    return resource_pool::ResourceHandle::Null();
  }

  // vidsynt: parse VPS / SPS / PPS NAL units. The context (and the parsed
  // structs that hang off it) is moved into the parameters' Cold storage at
  // the end so the parsed_sps / parsed_pps pointers stay valid for the
  // lifetime of the parameters object (DecodeVideoH265 needs them).
  VidsyntHevcContextPtr ctx{ vidsynt_hevc_context_new() };
  if (ctx == nullptr) {
    MBASE_LOG_ERROR("vidsynt_hevc_context_new returned null.");
    return resource_pool::ResourceHandle::Null();
  }

  VidsyntHevcNalu const* vps_nalu = nullptr;
  VidsyntHevcNalu const* sps_nalu = nullptr;
  VidsyntHevcNalu const* pps_nalu = nullptr;
  if (!ParseNalu(ctx.get(), desc.vps_data, desc.vps_size, &vps_nalu)) return resource_pool::ResourceHandle::Null();
  if (!ParseNalu(ctx.get(), desc.sps_data, desc.sps_size, &sps_nalu)) return resource_pool::ResourceHandle::Null();
  if (!ParseNalu(ctx.get(), desc.pps_data, desc.pps_size, &pps_nalu)) return resource_pool::ResourceHandle::Null();

  VidsyntHevcVideoParameterSet const* vps_in = nullptr;
  VidsyntHevcSequenceParameterSet const* sps_in = nullptr;
  VidsyntHevcPictureParameterSet const* pps_in = nullptr;
  if (vidsynt_hevc_nalu_get_vps(ctx.get(), vps_nalu, &vps_in) != VidsyntResult::Success) {
    MBASE_LOG_ERROR("vidsynt_hevc_nalu_get_vps failed.");
    return resource_pool::ResourceHandle::Null();
  }
  if (vidsynt_hevc_nalu_get_sps(ctx.get(), sps_nalu, &sps_in) != VidsyntResult::Success) {
    MBASE_LOG_ERROR("vidsynt_hevc_nalu_get_sps failed.");
    return resource_pool::ResourceHandle::Null();
  }
  if (vidsynt_hevc_nalu_get_pps(ctx.get(), pps_nalu, &pps_in) != VidsyntResult::Success) {
    MBASE_LOG_ERROR("vidsynt_hevc_nalu_get_pps failed.");
    return resource_pool::ResourceHandle::Null();
  }

  // Also seed the session's vidsynt context with the same VPS / SPS / PPS
  // NAL units. vidsynt's `set_active_sps/pps` (called from
  // `DecodeVideoH265`) looks up parameter sets by id in the called
  // context's own `sps_cache` / `pps_cache`, which is populated by
  // `parse_nalu_from_bytes`. Cross-context refs alone do not register an
  // entry in the cache, so we have to parse the NALs into the session's
  // context too.
  {
    VidsyntHevcContext* const session_ctx = session_hot.vidsynt_ctx.get();
    MBASE_ASSERT(session_ctx != nullptr);
    VidsyntHevcNalu const* seeded_vps_nalu = nullptr;
    VidsyntHevcNalu const* seeded_sps_nalu = nullptr;
    VidsyntHevcNalu const* seeded_pps_nalu = nullptr;
    if (!ParseNalu(session_ctx, desc.vps_data, desc.vps_size, &seeded_vps_nalu)
     || !ParseNalu(session_ctx, desc.sps_data, desc.sps_size, &seeded_sps_nalu)
     || !ParseNalu(session_ctx, desc.pps_data, desc.pps_size, &seeded_pps_nalu)) {
      MBASE_LOG_ERROR("CreateVideoSessionParametersDecodeH265: failed to seed session vidsynt context.");
      return resource_pool::ResourceHandle::Null();
    }
    // Also retrieve the seeded session-side SPS/PPS so we read the id values
    // out of THIS context's parsed structs (not the parameters' own ctx).
    // If session-side id != parameters-side id, parsing is non-deterministic
    // (= bug). If session-side id matches but lookup later still fails, the
    // ctx pointer is somehow different between here and DecodeVideoH265.
    VidsyntHevcSequenceParameterSet const* seeded_sps_in_session = nullptr;
    VidsyntHevcPictureParameterSet  const* seeded_pps_in_session = nullptr;
    vidsynt_hevc_nalu_get_sps(session_ctx, seeded_sps_nalu, &seeded_sps_in_session);
    vidsynt_hevc_nalu_get_pps(session_ctx, seeded_pps_nalu, &seeded_pps_in_session);

    MBASE_LOG_INFO(
      "Seeded session vidsynt ctx={}: vps_nal_type={}, sps_nal_type={}, pps_nal_type={}, "
      "params_sps_id={}, params_pps_id={}, session_sps_id={}, session_pps_id={}",
      static_cast<void const*>(session_ctx),
      static_cast<int>(vidsynt_hevc_nalu_get_type(seeded_vps_nalu)),
      static_cast<int>(vidsynt_hevc_nalu_get_type(seeded_sps_nalu)),
      static_cast<int>(vidsynt_hevc_nalu_get_type(seeded_pps_nalu)),
      static_cast<int>(sps_in->sps_seq_parameter_set_id),
      static_cast<int>(pps_in->pps_pic_parameter_set_id),
      seeded_sps_in_session != nullptr ? static_cast<int>(seeded_sps_in_session->sps_seq_parameter_set_id) : -1,
      seeded_pps_in_session != nullptr ? static_cast<int>(seeded_pps_in_session->pps_pic_parameter_set_id) : -1
    );

    // Sanity check: try set_active_sps/pps right here in the seed step so
    // we catch the failure at parameters creation rather than at decode time.
    if (VidsyntResult const r = vidsynt_hevc_context_set_active_sps(session_ctx, seeded_sps_in_session); r != VidsyntResult::Success) {
      MBASE_LOG_ERROR("Seed-time set_active_sps on session ctx FAILED: {}", static_cast<int>(r));
    } else {
      MBASE_LOG_INFO("Seed-time set_active_sps on session ctx succeeded.");
    }
  }

  // Translate vidsynt -> Std structs. All Std* objects (and the things they
  // point at) live on the stack frame here; vkCreateVideoSessionParametersKHR
  // copies what it needs synchronously.
  ParsedVps parsed_vps{};
  ParsedSps parsed_sps{};
  ParsedPps parsed_pps{};
  FillVps(parsed_vps, *vps_in);
  FillSps(parsed_sps, *sps_in);
  FillPps(parsed_pps, *pps_in);

  VkVideoDecodeH265SessionParametersAddInfoKHR add_info{
    .sType        = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR,
    .pNext        = nullptr,
    .stdVPSCount  = 1,
    .pStdVPSs     = &parsed_vps.vps,
    .stdSPSCount  = 1,
    .pStdSPSs     = &parsed_sps.sps,
    .stdPPSCount  = 1,
    .pStdPPSs     = &parsed_pps.pps,
  };

  VkVideoDecodeH265SessionParametersCreateInfoKHR h265_create_info{
    .sType              = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR,
    .pNext              = nullptr,
    .maxStdVPSCount     = 1,
    .maxStdSPSCount     = 1,
    .maxStdPPSCount     = 1,
    .pParametersAddInfo = &add_info,
  };

  VkVideoSessionParametersCreateInfoKHR create_info{
    .sType                            = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR,
    .pNext                            = &h265_create_info,
    .flags                            = 0,
    .videoSessionParametersTemplate   = VK_NULL_HANDLE,
    .videoSession                     = vk_session,
  };

  VkDevice const vk_device_handle = vk_device.handle();
  VkVideoSessionParametersKHR vk_params = VK_NULL_HANDLE;
  VkResult const result = vkCreateVideoSessionParametersKHR(vk_device_handle, &create_info, nullptr, &vk_params);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateVideoSessionParametersKHR failed: {}", string_VkResult(result));
    return resource_pool::ResourceHandle::Null();
  }

  VulkanVideoSessionParameters vk_video_params(
    vk_params,
    [vk_device_handle, vk_params] {
      vkDestroyVideoSessionParametersKHR(vk_device_handle, vk_params, nullptr);
    },
    vk_device.GetDeferredDestroyer()
  );

  VideoSessionParametersHot hot{ .vk_video_session_parameters = std::move(vk_video_params) };
  VideoSessionParametersCold cold{
    .session_handle = session_pool_handle,
    .vidsynt_ctx    = std::move(ctx),
    .parsed_sps     = sps_in,
    .parsed_pps     = pps_in,
  };

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

} // namespace mnexus_backend::vulkan
