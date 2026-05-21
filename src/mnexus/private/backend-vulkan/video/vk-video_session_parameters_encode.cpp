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
  // Khronos reference HEVC encoder sets this to 0; NVIDIA cross-checks
  // the general constraint indicator bits, and packed-source = 0 is the
  // working combination with progressive_source = 1.
  ptl_flags.general_non_packed_constraint_flag = 0;
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
  StdVideoH265DecPicBufMgr const& buf_mgr,
  StdVideoH265HrdParameters const& hrd_params
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
    // Khronos reference encoder always points pHrdParameters at a
    // (typically zeroed) HRD struct rather than nullptr; some drivers
    // dereference this even when vps_timing_info_present_flag = 0.
    .pHrdParameters                   = &hrd_params,
    .pProfileTierLevel                = &ptl,
  };
}

StdVideoH265ShortTermRefPicSet BuildEncodeStrps() {
  // Single STRPS describing the GoPro-style "one prev forward ref" config.
  // The slice header for P pictures uses `short_term_ref_pic_set_idx = 0`
  // to reference this set.
  StdVideoH265ShortTermRefPicSet strps{};
  strps.flags.inter_ref_pic_set_prediction_flag = 0;
  strps.flags.delta_rps_sign                    = 0;
  strps.delta_idx_minus1                        = 0;
  strps.use_delta_flag                          = 0;
  strps.abs_delta_rps_minus1                    = 0;
  strps.used_by_curr_pic_flag                   = 0;
  strps.used_by_curr_pic_s0_flag                = 1;  // bit 0 = "use the first negative ref"
  strps.used_by_curr_pic_s1_flag                = 0;
  strps.num_negative_pics                       = 1;
  strps.num_positive_pics                       = 0;
  strps.delta_poc_s0_minus1[0]                  = 0;  // POC delta = 1 (prev picture)
  return strps;
}

StdVideoH265SequenceParameterSetVui BuildEncodeVui() {
  // Minimal VUI: square pixel aspect, no overscan, no video signal type,
  // no chroma loc, no timing (the encoder doesn't need it for CQP), no
  // HRD. Some drivers (notably NVIDIA) require a non-null VUI struct
  // even with all-zero flags; the Khronos reference encoder always wires
  // one.
  StdVideoH265SequenceParameterSetVui vui{};
  vui.flags.aspect_ratio_info_present_flag           = 1;
  vui.aspect_ratio_idc                               = STD_VIDEO_H265_ASPECT_RATIO_IDC_SQUARE;
  // Reference encoder always sets these in its working path; some
  // drivers' VUI bitstream writers read the MV-length fields whether
  // or not bitstream_restriction_flag is set, and the two motion-vector
  // flags interact with the encoder's RPL-handling firmware.
  vui.flags.motion_vectors_over_pic_boundaries_flag  = 1;
  vui.flags.restricted_ref_pic_lists_flag            = 1;
  vui.log2_max_mv_length_horizontal                  = 12;
  vui.log2_max_mv_length_vertical                    = 10;
  return vui;
}

StdVideoH265SequenceParameterSet BuildEncodeSps(
  uint32_t coded_width,
  uint32_t coded_height,
  StdVideoH265ProfileTierLevel const& ptl,
  StdVideoH265DecPicBufMgr const& buf_mgr,
  StdVideoH265ShortTermRefPicSet const& strps,
  StdVideoH265SequenceParameterSetVui const& vui
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
  // sps_temporal_mvp_enabled_flag: disabled on the conservative side.
  // Enabling it requires matching picture-level / slice-level flags +
  // collocated picture book-keeping; some drivers reject the encoded
  // parameters when the implicit slice-level flag is left at 0.
  sps_flags.sps_temporal_mvp_enabled_flag            = 0;
  // strong_intra_smoothing_enabled_flag: 0 to match the Khronos reference
  // encoder's working SPS for Main 8-bit CQP encoding. Enabling it gives
  // marginal quality improvement on intra blocks but interacts with the
  // intra prediction path in ways some encoders do not like.
  sps_flags.strong_intra_smoothing_enabled_flag      = 0;
  // vui_parameters_present_flag: 1 with a populated VUI. Required by the
  // working reference encoder path; NVIDIA's driver rejects SPS emission
  // when the flag is 1 with a null VUI pointer (or, observed here, when
  // it is 0 and no VUI is provided).
  sps_flags.vui_parameters_present_flag              = 1;
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
    // CTB = 32, min CB = 8. NVIDIA's HEVC encoder on some hardware /
    // driver combinations rejects CTB = 64 with OOM at parameters
    // emission time even though the spec allows it.
    .log2_min_luma_coding_block_size_minus3         = 0,
    .log2_diff_max_min_luma_coding_block_size       = 2,
    .log2_min_luma_transform_block_size_minus2      = 0,
    .log2_diff_max_min_luma_transform_block_size    = 3,
    .max_transform_hierarchy_depth_inter            = 2,
    .max_transform_hierarchy_depth_intra            = 2,
    .num_short_term_ref_pic_sets                    = 1,
    .num_long_term_ref_pics_sps                     = 0,
    // PCM fields are set to spec-defined defaults even when
    // pcm_enabled_flag = 0; NVIDIA's encoder reads them unconditionally
    // and rejects all-zero values (vkGetEncodedVideoSessionParametersKHR
    // surfaces this as VK_ERROR_OUT_OF_HOST_MEMORY).
    .pcm_sample_bit_depth_luma_minus1               = 7,
    .pcm_sample_bit_depth_chroma_minus1             = 7,
    .log2_min_pcm_luma_coding_block_size_minus3     = 0,
    .log2_diff_max_min_pcm_luma_coding_block_size   = 2,  // mirrors log2_diff_max_min_luma_coding_block_size
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
    .pShortTermRefPicSet                            = &strps,
    .pLongTermRefPicsSps                            = nullptr,
    .pSequenceParameterSetVui                       = &vui,
    .pPredictorPaletteEntries                       = nullptr,
  };
}

StdVideoH265PictureParameterSet BuildEncodePps(int8_t qp) {
  StdVideoH265PpsFlags pps_flags{};
  pps_flags.dependent_slice_segments_enabled_flag         = 0;
  pps_flags.output_flag_present_flag                      = 0;
  pps_flags.sign_data_hiding_enabled_flag                 = 0;
  // The four flags set to 1 below match the Khronos reference encoder's
  // working PPS for Main 8-bit CQP single-slice. NVIDIA's encoder
  // firmware reads cu_qp_delta_enabled_flag during rate-control setup
  // (even at CQP) and rejects parameters NAL emission when it is 0.
  pps_flags.cabac_init_present_flag                       = 1;
  pps_flags.constrained_intra_pred_flag                   = 0;
  pps_flags.transform_skip_enabled_flag                   = 1;
  pps_flags.cu_qp_delta_enabled_flag                      = 1;
  pps_flags.pps_slice_chroma_qp_offsets_present_flag      = 0;
  pps_flags.weighted_pred_flag                            = 0;
  pps_flags.weighted_bipred_flag                          = 0;
  pps_flags.transquant_bypass_enabled_flag                = 0;
  pps_flags.tiles_enabled_flag                            = 0;
  pps_flags.entropy_coding_sync_enabled_flag              = 0;
  pps_flags.uniform_spacing_flag                          = 0;
  pps_flags.loop_filter_across_tiles_enabled_flag         = 0;
  pps_flags.pps_loop_filter_across_slices_enabled_flag    = 1;
  pps_flags.deblocking_filter_control_present_flag        = 1;
  pps_flags.deblocking_filter_override_enabled_flag       = 0;
  pps_flags.pps_deblocking_filter_disabled_flag           = 0;
  pps_flags.pps_scaling_list_data_present_flag            = 0;
  // lists_modification_present_flag: 0 since the IDR + N x P GOP we author
  // never needs the slice header to override the default RPL order.
  // Setting it to 1 forces ref_pic_list_modification() into every slice
  // header; some drivers fail the parameters NAL emission when no actual
  // modification is needed but the flag is set.
  pps_flags.lists_modification_present_flag               = 0;
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

bool TryFetchOneNal(
  VkDevice vk_device_handle,
  VkVideoSessionParametersKHR vk_params,
  bool wantVps, bool wantSps, bool wantPps,
  std::vector<uint8_t>& out_bytes
) {
  VkVideoEncodeH265SessionParametersGetInfoKHR h265_get_info {
    .sType        = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_GET_INFO_KHR,
    .pNext        = nullptr,
    .writeStdVPS  = wantVps ? VK_TRUE : VK_FALSE,
    .writeStdSPS  = wantSps ? VK_TRUE : VK_FALSE,
    .writeStdPPS  = wantPps ? VK_TRUE : VK_FALSE,
    .stdVPSId     = 0,
    .stdSPSId     = 0,
    .stdPPSId     = 0,
  };
  VkVideoEncodeSessionParametersGetInfoKHR get_info {
    .sType                  = VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_GET_INFO_KHR,
    .pNext                  = &h265_get_info,
    .videoSessionParameters = vk_params,
  };
  out_bytes.assign(8192, 0);
  size_t data_size = out_bytes.size();
  VkResult r = vkGetEncodedVideoSessionParametersKHR(
    vk_device_handle, &get_info, nullptr, &data_size, out_bytes.data());
  if (r == VK_INCOMPLETE) {
    out_bytes.assign(data_size, 0);
    r = vkGetEncodedVideoSessionParametersKHR(
      vk_device_handle, &get_info, nullptr, &data_size, out_bytes.data());
  }
  if (r != VK_SUCCESS) return false;
  if (data_size == 0)  return false;
  out_bytes.resize(data_size);
  return true;
}

bool ReadEncodedParametersBytes(
  VkDevice vk_device_handle,
  VkVideoSessionParametersKHR vk_params,
  std::vector<uint8_t>& out_bytes
) {
  // First attempt: ask for VPS + SPS + PPS in a single call.
  if (TryFetchOneNal(vk_device_handle, vk_params, true, true, true, out_bytes)) {
    return true;
  }

  // Diagnostic fallback: try each NAL separately so the log narrows down
  // which one trips the driver, then re-concatenate on success. This path
  // also exercises the workaround used by some apps that fetch the three
  // NALs individually.
  MBASE_LOG_WARN("Combined VPS+SPS+PPS fetch failed; trying per-NAL fallback.");

  std::vector<uint8_t> vps_bytes, sps_bytes, pps_bytes;
  bool const ok_vps = TryFetchOneNal(vk_device_handle, vk_params, true,  false, false, vps_bytes);
  bool const ok_sps = TryFetchOneNal(vk_device_handle, vk_params, false, true,  false, sps_bytes);
  bool const ok_pps = TryFetchOneNal(vk_device_handle, vk_params, false, false, true,  pps_bytes);
  MBASE_LOG_ERROR("Per-NAL fetch results: VPS={}({}B), SPS={}({}B), PPS={}({}B)",
    ok_vps, vps_bytes.size(), ok_sps, sps_bytes.size(), ok_pps, pps_bytes.size());

  if (!ok_vps || !ok_sps || !ok_pps) {
    return false;
  }
  out_bytes.clear();
  out_bytes.insert(out_bytes.end(), vps_bytes.begin(), vps_bytes.end());
  out_bytes.insert(out_bytes.end(), sps_bytes.begin(), sps_bytes.end());
  out_bytes.insert(out_bytes.end(), pps_bytes.begin(), pps_bytes.end());
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
  StdVideoH265ShortTermRefPicSet const strps = BuildEncodeStrps();
  StdVideoH265SequenceParameterSetVui const vui = BuildEncodeVui();
  StdVideoH265HrdParameters const hrd_params{};  // zero-init; VPS dereferences it
  StdVideoH265VideoParameterSet const std_vps = BuildEncodeVps(ptl, buf_mgr, hrd_params);
  StdVideoH265SequenceParameterSet const std_sps =
    BuildEncodeSps(desc.coded_width, desc.coded_height, ptl, buf_mgr, strps, vui);
  // PPS init_qp_minus26 = 0 matches the Khronos reference encoder.
  // Per-frame `EncodeVideoH265PictureInfo::constant_qp` arrives in the
  // bitstream as slice_qp_delta = constant_qp - 26.
  StdVideoH265PictureParameterSet const std_pps = BuildEncodePps(26);

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
  // Bind quality level 0 (fastest preset) to the parameters object.
  // NVIDIA's encoder firmware validates the std params against the bound
  // quality level's preferred values at parameters emission time; the
  // implicit "no quality level set" path is observed to OOM during
  // vkGetEncodedVideoSessionParametersKHR for the SPS.
  VkVideoEncodeQualityLevelInfoKHR quality_level_info {
    .sType        = VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR,
    .pNext        = &encode_params_create_info,
    .qualityLevel = 0,
  };
  VkVideoSessionParametersCreateInfoKHR create_info {
    .sType                              = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR,
    .pNext                              = &quality_level_info,
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
