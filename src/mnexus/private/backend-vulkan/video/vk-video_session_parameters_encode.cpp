// TU header --------------------------------------------
#include "backend-vulkan/video/vk-video_session_parameters.h"

// c++ headers ------------------------------------------
#include <cstring>
#include <utility>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/device/vk-device.h"

namespace mnexus_backend::vulkan {

namespace {

// ----------------------------------------------------------------------------------------------------
// Local helpers
//

StdVideoH265ProfileIdc ToStdVideoH265ProfileIdcLocal(mnexus::VideoH265Profile profile) {
  switch (profile) {
  case mnexus::VideoH265Profile::kMain:   return STD_VIDEO_H265_PROFILE_IDC_MAIN;
  case mnexus::VideoH265Profile::kMain10: return STD_VIDEO_H265_PROFILE_IDC_MAIN_10;
  }
  return STD_VIDEO_H265_PROFILE_IDC_INVALID;
}

StdVideoH265LevelIdc ToStdVideoH265LevelIdcLocal(mnexus::VideoH265Level level) {
  return static_cast<StdVideoH265LevelIdc>(level);
}

// ----------------------------------------------------------------------------------------------------
// Std H.265 syntax builders (encode side)
//
// Build a Main (8-bit 4:2:0) VPS / SPS / PPS suitable for an
// IDR + N x P (no B-frame) constant-QP encode. Many fields default to 0;
// the comments below explain the non-zero choices.
//

StdVideoH265ProfileTierLevel BuildEncodeProfileTierLevel(
  mnexus::VideoH265Profile profile,
  mnexus::VideoH265Level level
) {
  StdVideoH265ProfileTierLevelFlags ptl_flags{};
  ptl_flags.general_tier_flag                  = 0;
  ptl_flags.general_progressive_source_flag    = 1;
  ptl_flags.general_interlaced_source_flag     = 0;
  ptl_flags.general_non_packed_constraint_flag = 1;
  ptl_flags.general_frame_only_constraint_flag = 1;

  return StdVideoH265ProfileTierLevel {
    .flags               = ptl_flags,
    .general_profile_idc = ToStdVideoH265ProfileIdcLocal(profile),
    .general_level_idc   = ToStdVideoH265LevelIdcLocal(level),
  };
}

StdVideoH265DecPicBufMgr BuildEncodeDecPicBufMgr(
  uint8_t num_ref_frames,
  uint8_t max_num_reorder_pics
) {
  StdVideoH265DecPicBufMgr buf_mgr{};
  // Sub-layer 0 only (no temporal scalability). For an IDR + N x P GOP
  // with one reference, max_dec_pic_buffering_minus1[0] = num_ref_frames
  // (= the at-most-one P reference plus possibly itself, but `_minus1`
  // already accounts for that).
  buf_mgr.max_latency_increase_plus1[0]   = 0;
  buf_mgr.max_dec_pic_buffering_minus1[0] = num_ref_frames;
  buf_mgr.max_num_reorder_pics[0]         = max_num_reorder_pics;
  return buf_mgr;
}

StdVideoH265VideoParameterSet BuildEncodeVps(
  StdVideoH265ProfileTierLevel const& ptl,
  StdVideoH265DecPicBufMgr const& buf_mgr
) {
  StdVideoH265VpsFlags vps_flags{};
  vps_flags.vps_temporal_id_nesting_flag             = 1;
  vps_flags.vps_sub_layer_ordering_info_present_flag = 1;
  vps_flags.vps_timing_info_present_flag             = 0;
  vps_flags.vps_poc_proportional_to_timing_flag      = 0;

  return StdVideoH265VideoParameterSet {
    .flags                            = vps_flags,
    .vps_video_parameter_set_id       = 0,
    .vps_max_sub_layers_minus1        = 0,
    .reserved1                        = 0,
    .reserved2                        = 0,
    .vps_num_units_in_tick            = 0,
    .vps_time_scale                   = 0,
    .vps_num_ticks_poc_diff_one_minus1= 0,
    .reserved3                        = 0,
    .pDecPicBufMgr                    = &buf_mgr,
    .pHrdParameters                   = nullptr,
    .pProfileTierLevel                = &ptl,
  };
}

StdVideoH265SequenceParameterSet BuildEncodeSps(
  uint32_t coded_width,
  uint32_t coded_height,
  StdVideoH265ProfileTierLevel const& ptl,
  StdVideoH265DecPicBufMgr const& buf_mgr
) {
  StdVideoH265SpsFlags sps_flags{};
  sps_flags.sps_temporal_id_nesting_flag             = 1;
  sps_flags.separate_colour_plane_flag               = 0;
  sps_flags.conformance_window_flag                  = 0;  // assume coded == display
  sps_flags.sps_sub_layer_ordering_info_present_flag = 1;
  sps_flags.scaling_list_enabled_flag                = 0;
  sps_flags.sps_scaling_list_data_present_flag       = 0;
  sps_flags.amp_enabled_flag                         = 1;
  sps_flags.sample_adaptive_offset_enabled_flag      = 1;
  sps_flags.pcm_enabled_flag                         = 0;
  sps_flags.long_term_ref_pics_present_flag          = 0;
  sps_flags.sps_temporal_mvp_enabled_flag            = 1;
  sps_flags.strong_intra_smoothing_enabled_flag      = 1;
  sps_flags.vui_parameters_present_flag              = 0;
  sps_flags.sps_extension_present_flag               = 0;

  // Coding block sizes -- targets common encoder defaults:
  //   min CB = 2^(3+0) = 8 px
  //   max CB = 2^(3+3) = 64 px (CTB)
  //   min TB = 2^(2+0) = 4 px
  //   max TB = 2^(2+3) = 32 px
  // max_transform_hierarchy_depth = 3 (also common).
  return StdVideoH265SequenceParameterSet {
    .flags                                          = sps_flags,
    .chroma_format_idc                              = STD_VIDEO_H265_CHROMA_FORMAT_IDC_420,
    .pic_width_in_luma_samples                      = coded_width,
    .pic_height_in_luma_samples                     = coded_height,
    .sps_video_parameter_set_id                     = 0,
    .sps_max_sub_layers_minus1                      = 0,
    .sps_seq_parameter_set_id                       = 0,
    .bit_depth_luma_minus8                          = 0,
    .bit_depth_chroma_minus8                        = 0,
    .log2_max_pic_order_cnt_lsb_minus4              = 4,    // 8-bit LSB -- enough for any GOP up to 256
    .log2_min_luma_coding_block_size_minus3         = 0,
    .log2_diff_max_min_luma_coding_block_size       = 3,
    .log2_min_luma_transform_block_size_minus2      = 0,
    .log2_diff_max_min_luma_transform_block_size    = 3,
    .max_transform_hierarchy_depth_inter            = 3,
    .max_transform_hierarchy_depth_intra            = 3,
    .num_short_term_ref_pic_sets                    = 0,
    .num_long_term_ref_pics_sps                     = 0,
    .pcm_sample_bit_depth_luma_minus1               = 0,
    .pcm_sample_bit_depth_chroma_minus1             = 0,
    .log2_min_pcm_luma_coding_block_size_minus3     = 0,
    .log2_diff_max_min_pcm_luma_coding_block_size   = 0,
    .reserved1                                      = 0,
    .reserved2                                      = 0,
    .palette_max_size                               = 0,
    .delta_palette_max_predictor_size               = 0,
    .motion_vector_resolution_control_idc           = 0,
    .sps_num_palette_predictor_initializers_minus1  = 0,
    .conf_win_left_offset                           = 0,
    .conf_win_right_offset                          = 0,
    .conf_win_top_offset                            = 0,
    .conf_win_bottom_offset                         = 0,
    .pProfileTierLevel                              = &ptl,
    .pDecPicBufMgr                                  = &buf_mgr,
    .pScalingLists                                  = nullptr,
    .pShortTermRefPicSet                            = nullptr,
    .pLongTermRefPicsSps                            = nullptr,
    .pSequenceParameterSetVui                       = nullptr,
    .pPredictorPaletteEntries                       = nullptr,
  };
}

StdVideoH265PictureParameterSet BuildEncodePps(int8_t qp) {
  StdVideoH265PpsFlags pps_flags{};
  pps_flags.dependent_slice_segments_enabled_flag         = 0;
  pps_flags.output_flag_present_flag                      = 0;
  pps_flags.sign_data_hiding_enabled_flag                 = 0;
  pps_flags.cabac_init_present_flag                       = 0;
  pps_flags.constrained_intra_pred_flag                   = 0;
  pps_flags.transform_skip_enabled_flag                   = 0;
  pps_flags.cu_qp_delta_enabled_flag                      = 0;
  pps_flags.pps_slice_chroma_qp_offsets_present_flag      = 0;
  pps_flags.weighted_pred_flag                            = 0;
  pps_flags.weighted_bipred_flag                          = 0;
  pps_flags.transquant_bypass_enabled_flag                = 0;
  pps_flags.tiles_enabled_flag                            = 0;
  pps_flags.entropy_coding_sync_enabled_flag              = 0;
  pps_flags.uniform_spacing_flag                          = 0;
  pps_flags.loop_filter_across_tiles_enabled_flag         = 0;
  pps_flags.pps_loop_filter_across_slices_enabled_flag    = 1;
  pps_flags.deblocking_filter_control_present_flag        = 0;
  pps_flags.deblocking_filter_override_enabled_flag       = 0;
  pps_flags.pps_deblocking_filter_disabled_flag           = 0;
  pps_flags.pps_scaling_list_data_present_flag            = 0;
  pps_flags.lists_modification_present_flag               = 1;  // slice header can rewrite RPL
  pps_flags.slice_segment_header_extension_present_flag   = 0;
  pps_flags.pps_extension_present_flag                    = 0;

  StdVideoH265PictureParameterSet pps {};
  pps.flags                                = pps_flags;
  pps.pps_pic_parameter_set_id             = 0;
  pps.pps_seq_parameter_set_id             = 0;
  pps.sps_video_parameter_set_id           = 0;
  pps.num_extra_slice_header_bits          = 0;
  pps.num_ref_idx_l0_default_active_minus1 = 0;
  pps.num_ref_idx_l1_default_active_minus1 = 0;
  pps.init_qp_minus26                      = static_cast<int8_t>(qp - 26);
  pps.diff_cu_qp_delta_depth               = 0;
  pps.pps_cb_qp_offset                     = 0;
  pps.pps_cr_qp_offset                     = 0;
  pps.num_tile_columns_minus1              = 0;
  pps.num_tile_rows_minus1                 = 0;
  pps.pps_beta_offset_div2                 = 0;
  pps.pps_tc_offset_div2                   = 0;
  pps.log2_parallel_merge_level_minus2     = 0;
  return pps;
}

// ----------------------------------------------------------------------------------------------------
// Readback
//
// Two-call vkGetEncodedVideoSessionParametersKHR pattern: nullptr pData
// to learn the byte count, then allocate, then call again. Asks for all
// three of VPS / SPS / PPS in one shot (the driver concatenates them
// into the output buffer in that order, each Annex B framed with a 4-byte
// start code).
//

bool ReadEncodedParametersBytes(
  VkDevice vk_device_handle,
  VkVideoSessionParametersKHR vk_params,
  std::vector<uint8_t>& out_bytes
) {
  VkVideoEncodeH265SessionParametersGetInfoKHR h265_get_info {
    .sType        = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_GET_INFO_KHR,
    .pNext        = nullptr,
    .writeStdVPS  = VK_TRUE,
    .writeStdSPS  = VK_TRUE,
    .writeStdPPS  = VK_TRUE,
    .stdVPSId     = 0,
    .stdSPSId     = 0,
    .stdPPSId     = 0,
  };
  VkVideoEncodeSessionParametersGetInfoKHR get_info {
    .sType                  = VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_GET_INFO_KHR,
    .pNext                  = &h265_get_info,
    .videoSessionParameters = vk_params,
  };

  size_t data_size = 0;
  VkResult r = vkGetEncodedVideoSessionParametersKHR(
    vk_device_handle, &get_info, nullptr, &data_size, nullptr);
  if (r != VK_SUCCESS || data_size == 0) {
    MBASE_LOG_ERROR("vkGetEncodedVideoSessionParametersKHR (size query) failed or returned 0 bytes: {}", string_VkResult(r));
    return false;
  }

  out_bytes.assign(data_size, 0);
  r = vkGetEncodedVideoSessionParametersKHR(
    vk_device_handle, &get_info, nullptr, &data_size, out_bytes.data());
  if (r != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkGetEncodedVideoSessionParametersKHR (data fetch) failed: {}", string_VkResult(r));
    return false;
  }
  out_bytes.resize(data_size);
  return true;
}

} // anonymous namespace

resource_pool::ResourceHandle EmplaceVideoSessionParametersResourcePoolEncodeH265(
  VideoSessionParametersResourcePool& out_pool,
  IVulkanDevice& vk_device,
  VideoSessionResourcePool const& session_pool,
  mnexus::VideoSessionParametersEncodeH265Desc const& desc
) {
  if (!vk_device.IsExtensionEnabled(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME)
   || !vk_device.IsExtensionEnabled(VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME)
   || !vk_device.IsExtensionEnabled(VK_KHR_VIDEO_ENCODE_H265_EXTENSION_NAME)) {
    MBASE_LOG_ERROR("CreateVideoSessionParametersEncodeH265: required Vulkan Video encode extensions not enabled.");
    return resource_pool::ResourceHandle::Null();
  }

  // Look up the session this parameters object will be bound to.
  auto const session_pool_handle = resource_pool::ResourceHandle::FromU64(desc.session.Get());
  if (session_pool_handle.IsNull()) {
    MBASE_LOG_ERROR("CreateVideoSessionParametersEncodeH265: invalid session handle.");
    return resource_pool::ResourceHandle::Null();
  }
  auto [session_hot, session_cold, session_lock] =
    session_pool.GetConstRefWithSharedLockGuard(session_pool_handle);
  VkVideoSessionKHR const vk_session = session_hot.vk_video_session.handle();
  if (vk_session == VK_NULL_HANDLE) {
    MBASE_LOG_ERROR("CreateVideoSessionParametersEncodeH265: session VkVideoSessionKHR is null.");
    return resource_pool::ResourceHandle::Null();
  }
  if (!session_cold.encode_desc.has_value()) {
    MBASE_LOG_ERROR("CreateVideoSessionParametersEncodeH265: session handle is not an encode session.");
    return resource_pool::ResourceHandle::Null();
  }
  auto const& session_desc = *session_cold.encode_desc;

  // Build the Std H.265 syntax structs. Storage MUST live until after
  // vkCreateVideoSessionParametersKHR returns (the driver reads through
  // pointers in the AddInfo chain), so they are stack-locals here.
  StdVideoH265ProfileTierLevel const ptl =
    BuildEncodeProfileTierLevel(session_desc.profile, desc.level);
  StdVideoH265DecPicBufMgr const buf_mgr =
    BuildEncodeDecPicBufMgr(desc.num_ref_frames, desc.max_num_reorder_pics);
  StdVideoH265VideoParameterSet const std_vps = BuildEncodeVps(ptl, buf_mgr);
  StdVideoH265SequenceParameterSet const std_sps =
    BuildEncodeSps(desc.coded_width, desc.coded_height, ptl, buf_mgr);
  // For PPS we need a configurable init_qp_minus26; without a per-frame
  // QP override path yet, pick the midpoint of the spec range so the
  // encoder defaults to "reasonable quality". Per-frame
  // EncodeVideoH265PictureInfo::constant_qp supersedes this via the
  // slice-header delta path.
  StdVideoH265PictureParameterSet const std_pps = BuildEncodePps(28);

  VkVideoEncodeH265SessionParametersAddInfoKHR add_info {
    .sType       = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR,
    .pNext       = nullptr,
    .stdVPSCount = 1,
    .pStdVPSs    = &std_vps,
    .stdSPSCount = 1,
    .pStdSPSs    = &std_sps,
    .stdPPSCount = 1,
    .pStdPPSs    = &std_pps,
  };
  VkVideoEncodeH265SessionParametersCreateInfoKHR encode_params_create_info {
    .sType              = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR,
    .pNext              = nullptr,
    .maxStdVPSCount     = 1,
    .maxStdSPSCount     = 1,
    .maxStdPPSCount     = 1,
    .pParametersAddInfo = &add_info,
  };
  VkVideoSessionParametersCreateInfoKHR create_info {
    .sType                              = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR,
    .pNext                              = &encode_params_create_info,
    .flags                              = 0,
    .videoSessionParametersTemplate     = VK_NULL_HANDLE,
    .videoSession                       = session_hot.vk_video_session.handle(),
  };

  VkDevice const vk_device_handle = vk_device.handle();
  VkVideoSessionParametersKHR vk_params = VK_NULL_HANDLE;
  VkResult const r = vkCreateVideoSessionParametersKHR(vk_device_handle, &create_info, nullptr, &vk_params);
  if (r != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateVideoSessionParametersKHR (encode) failed: {}", string_VkResult(r));
    return resource_pool::ResourceHandle::Null();
  }

  // Read back the driver-encoded VPS / SPS / PPS NALs.
  std::vector<uint8_t> encoded_bytes;
  if (!ReadEncodedParametersBytes(vk_device_handle, vk_params, encoded_bytes)) {
    vkDestroyVideoSessionParametersKHR(vk_device_handle, vk_params, nullptr);
    return resource_pool::ResourceHandle::Null();
  }

  VulkanVideoSessionParameters vk_video_session_parameters(
    vk_params,
    [vk_device_handle, vk_params] {
      vkDestroyVideoSessionParametersKHR(vk_device_handle, vk_params, nullptr);
    },
    vk_device.GetDeferredDestroyer()
  );

  VideoSessionParametersHot hot {
    .vk_video_session_parameters = std::move(vk_video_session_parameters),
  };
  VideoSessionParametersCold cold {
    .session_handle             = session_pool_handle,
    .encoded_vps_sps_pps_bytes  = std::move(encoded_bytes),
  };

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

} // namespace mnexus_backend::vulkan
