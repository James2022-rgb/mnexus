// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-command_list.h"

// c++ headers ------------------------------------------
#include <array>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/log.h"

#include "mnexus/public/render_state_event_log.h"

// project headers --------------------------------------
#include "impl/impl_macros.h"

#include "pipeline/render_pipeline_state_tracker.h"

#include "backend-vulkan/backend-vulkan-render_pipeline.h"
#include "backend-vulkan/command/command_encoder.h"
#include "backend-vulkan/command/image_layout_tracker.h"
#include "backend-vulkan/object/vk-object-command_pool.h"
#include "backend-vulkan/object/vk-object-render_pipeline.h"

#include "backend-vulkan/device/vk-device.h"
#include "backend-vulkan/resource/resource_storage.h"
#include "backend-vulkan/resource/types_bridge.h"

#if MNEXUS_ENABLE_VIDEO_CODING
#  include "backend-vulkan/video/vk-video_session.h"
#  include "backend-vulkan/video/vk-video_session_parameters.h"
#endif

namespace mnexus_backend::vulkan {

namespace {

VkCommandPool CreateCommandPool(IVulkanDevice* vk_device, uint32_t queue_family_index) {
  VkCommandPoolCreateInfo pool_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
    .queueFamilyIndex = queue_family_index,
  };
  VkCommandPool vk_pool = VK_NULL_HANDLE;
  VkResult const result = vkCreateCommandPool(vk_device->handle(), &pool_info, nullptr, &vk_pool);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateCommandPool failed: {}", string_VkResult(result));
  }
  return vk_pool;
}

VkCommandBuffer AllocateAndBeginCommandBuffer(IVulkanDevice* vk_device, VkCommandPool vk_pool) {
  VkCommandBufferAllocateInfo alloc_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,
    .commandPool = vk_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  VkCommandBuffer vk_cb = VK_NULL_HANDLE;
  VkResult const alloc_result = vkAllocateCommandBuffers(vk_device->handle(), &alloc_info, &vk_cb);
  if (alloc_result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkAllocateCommandBuffers failed: {}", string_VkResult(alloc_result));
    return VK_NULL_HANDLE;
  }

  VkCommandBufferBeginInfo begin_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = nullptr,
  };
  VkResult const begin_result = vkBeginCommandBuffer(vk_cb, &begin_info);
  if (begin_result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkBeginCommandBuffer failed: {}", string_VkResult(begin_result));
  }
  return vk_cb;
}

VulkanCommandPool MakeVulkanCommandPool(IVulkanDevice* vk_device, VkCommandPool vk_pool) {
  VkDevice const vk_device_handle = vk_device->handle();
  return VulkanCommandPool(
    vk_pool,
    [vk_device_handle, vk_pool] { vkDestroyCommandPool(vk_device_handle, vk_pool, nullptr); },
    vk_device->GetDeferredDestroyer()
  );
}

class MnexusCommandListVulkan final : public IMnexusCommandListVulkan {
public:
  MnexusCommandListVulkan(
    IVulkanDevice* vk_device,
    IDescriptorSetAllocator* ds_allocator,
    ResourceStorage* resource_storage,
    uint32_t queue_family_index
  ) :
    vk_device_(vk_device),
    queue_family_index_(queue_family_index),
    vk_command_pool_(MakeVulkanCommandPool(vk_device, CreateCommandPool(vk_device, queue_family_index))),
    encoder_(ICommandEncoder::Create(CommandEncoderDesc {
      .vk_cb_handle = AllocateAndBeginCommandBuffer(vk_device, vk_command_pool_.handle()),
      .vk_device = vk_device,
      .ds_allocator = ds_allocator,
      .resource_storage = resource_storage,
    })),
    resource_storage_(resource_storage)
  {
    render_pipeline_state_tracker_.SetEventLog(&render_state_event_log_);
  }
  ~MnexusCommandListVulkan() override {
    if (encoder_ != nullptr) {
      encoder_->Shutdown();
      encoder_ = nullptr;
    }
  }
  MBASE_DISALLOW_COPY_MOVE(MnexusCommandListVulkan);

  void Shutdown() override {
    delete this;
  }

  ICommandEncoder& encoder() override { return *encoder_; }
  VulkanCommandPool& vk_command_pool() override { return vk_command_pool_; }
  mbase::ArrayProxy<resource_pool::ResourceHandle const> GetReferencedResources() const override {
    return referenced_resources_;
  }

  // --------------------------------------------------------------------------------------------------
  // mnexus::ICommandList implementation
  //

  IMPL_VAPI(void, End) {
    // Transition all tracked images back to their default layouts before finalizing.
    image_layout_tracker_.TransitionAllToDefaults();
    this->FlushPipelineBarrier();

    encoder_->End();
  }

  //
  // Diagnostics
  //

  IMPL_VAPI(mnexus::RenderStateEventLog&, GetStateEventLog) {
    return render_state_event_log_;
  }

  //
  // Debug Markers
  //

  IMPL_VAPI(void, PushDebugGroup,
    mnexus::container::ArrayProxy<char const> name, float const* color
  ) {
    if (vkCmdBeginDebugUtilsLabelEXT != nullptr) {
      // `VkDebugUtilsLabelEXT` expects a null-terminated string, but `name` may not be null-terminated. Create a temporary null-terminated string for this call.
      std::string name_str(name.data(), name.size());

      VkDebugUtilsLabelEXT label_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = nullptr,
        .pLabelName = name_str.c_str(),
        .color = { 0.f, 0.f, 0.f, 0.f }
      };

      std::array<float, 4> default_color = { 0.f, 0.f, 0.f, 1.f };
      if (color != nullptr) {
        std::copy_n(color, 4, label_info.color);
      } else {
        std::copy_n(default_color.data(), 4, label_info.color);
      }

      vkCmdBeginDebugUtilsLabelEXT(encoder_->vk_cb_handle(), &label_info);
    }
  }

  IMPL_VAPI(void, PopDebugGroup) {
    if (vkCmdEndDebugUtilsLabelEXT != nullptr) {
      vkCmdEndDebugUtilsLabelEXT(encoder_->vk_cb_handle());
    }
  }

  //
  // Pipeline Barriers
  //

  IMPL_VAPI(void, TextureBarrier,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ResourceBarrierStageFlags dst_stage_flags,
    mnexus::ResourceBarrierState dst_state
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    VulkanImage const& vk_image = hot.GetVkImage();
    VkImage const vk_image_handle = vk_image.handle();
    mnexus::TextureDesc const& desc = cold.GetTextureDesc();
    VkFormat const vk_format = ToVkFormat(desc.format);

    image_layout_tracker_.RegisterImage(
      vk_image_handle,
      ToVkImageUsageFlags(desc.usage, vk_format),
      vk_format,
      desc.mip_level_count,
      desc.array_layer_count
    );

    VkPipelineStageFlags2KHR const vk_stage_mask = ToVkPipelineStageFlags2(dst_stage_flags);
    VkAccessFlags2KHR const vk_access_mask = ToVkAccessFlags2(dst_state, dst_stage_flags);
    VkImageLayout const vk_layout = ToVkImageLayout(dst_state);

    for (uint32_t mip = 0; mip < subresource_range.mip_level_count; ++mip) {
      for (uint32_t layer = 0; layer < subresource_range.array_layer_count; ++layer) {
        image_layout_tracker_.Transition(
          vk_image_handle,
          { .mip_level = subresource_range.base_mip_level + mip,
            .array_layer = subresource_range.base_array_layer + layer },
          vk_stage_mask,
          vk_access_mask,
          vk_layout
        );
      }
    }

    referenced_resources_.push_back(pool_handle);
  }

  IMPL_VAPI(void, TextureBarrierRelease,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ResourceBarrierState release_state,
    mnexus::QueueId dst_queue_id
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    VulkanImage const& vk_image = hot.GetVkImage();
    VkImage const vk_image_handle = vk_image.handle();
    mnexus::TextureDesc const& desc = cold.GetTextureDesc();
    VkFormat const vk_format = ToVkFormat(desc.format);

    image_layout_tracker_.RegisterImage(
      vk_image_handle,
      ToVkImageUsageFlags(desc.usage, vk_format),
      vk_format,
      desc.mip_level_count,
      desc.array_layer_count
    );

    VkImageLayout const vk_layout = ToVkImageLayout(release_state);

    for (uint32_t mip = 0; mip < subresource_range.mip_level_count; ++mip) {
      for (uint32_t layer = 0; layer < subresource_range.array_layer_count; ++layer) {
        image_layout_tracker_.TransitionRelease(
          vk_image_handle,
          { .mip_level = subresource_range.base_mip_level + mip,
            .array_layer = subresource_range.base_array_layer + layer },
          vk_layout,
          queue_family_index_,
          dst_queue_id.queue_family_index
        );
      }
    }

    referenced_resources_.push_back(pool_handle);
  }

  IMPL_VAPI(void, TextureBarrierAcquire,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ResourceBarrierStageFlags dst_stage_flags,
    mnexus::ResourceBarrierState released_from_state,
    mnexus::ResourceBarrierState acquire_state,
    mnexus::QueueId src_queue_id
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    VulkanImage const& vk_image = hot.GetVkImage();
    VkImage const vk_image_handle = vk_image.handle();
    mnexus::TextureDesc const& desc = cold.GetTextureDesc();
    VkFormat const vk_format = ToVkFormat(desc.format);

    image_layout_tracker_.RegisterImage(
      vk_image_handle,
      ToVkImageUsageFlags(desc.usage, vk_format),
      vk_format,
      desc.mip_level_count,
      desc.array_layer_count
    );

    VkPipelineStageFlags2KHR const vk_stage_mask = ToVkPipelineStageFlags2(dst_stage_flags);
    VkAccessFlags2KHR const vk_access_mask = ToVkAccessFlags2(acquire_state, dst_stage_flags);
    VkImageLayout const vk_pre_qfot_layout  = ToVkImageLayout(released_from_state);
    VkImageLayout const vk_post_qfot_layout = ToVkImageLayout(acquire_state);

    for (uint32_t mip = 0; mip < subresource_range.mip_level_count; ++mip) {
      for (uint32_t layer = 0; layer < subresource_range.array_layer_count; ++layer) {
        image_layout_tracker_.TransitionAcquire(
          vk_image_handle,
          { .mip_level = subresource_range.base_mip_level + mip,
            .array_layer = subresource_range.base_array_layer + layer },
          vk_stage_mask,
          vk_access_mask,
          vk_pre_qfot_layout,
          vk_post_qfot_layout,
          src_queue_id.queue_family_index,
          queue_family_index_
        );
      }
    }

    referenced_resources_.push_back(pool_handle);
  }

  //
  // Video coding
  //

  IMPL_VAPI(void, BeginVideoCoding, mnexus::BeginVideoCodingDesc const& desc) {
#if MNEXUS_ENABLE_VIDEO_CODING
    // Resolve session.
    auto const session_pool_handle = resource_pool::ResourceHandle::FromU64(desc.session.Get());
    auto [session_hot, session_cold, session_lock] =
      resource_storage_->video_sessions.GetConstRefWithSharedLockGuard(session_pool_handle);
    VkVideoSessionKHR const vk_session = session_hot.vk_video_session.handle();

    // Diagnostic: if a previous DecodeVideoH265 has a pending result_status
    // query, read it back (synchronously waits for prior decode submission
    // to complete) and log it before resetting the pool for this frame.
    // The reset itself is recorded into the command buffer; it MUST happen
    // outside the video coding scope, so we issue it before
    // vkCmdBeginVideoCodingKHR below.
    if (session_hot.result_status_query_pool != VK_NULL_HANDLE) {
      if (session_hot.result_status_pending) {
        VkQueryResultStatusKHR status = VK_QUERY_RESULT_STATUS_NOT_READY_KHR;
        VkResult const r = vkGetQueryPoolResults(
          this->vk_device_->handle(),
          session_hot.result_status_query_pool,
          0, 1,
          sizeof(status),
          &status,
          sizeof(status),
          VK_QUERY_RESULT_WITH_STATUS_BIT_KHR | VK_QUERY_RESULT_WAIT_BIT
        );
        if (r == VK_SUCCESS) {
          // Successful decodes are the common case -- only surface the
          // status when it is not COMPLETE so per-frame redecodes do not
          // spam the log.
          if (status != VK_QUERY_RESULT_STATUS_COMPLETE_KHR) {
            MBASE_LOG_WARN("Decode result_status: {} (raw, non-COMPLETE)", static_cast<int32_t>(status));
          }
        } else {
          MBASE_LOG_ERROR("vkGetQueryPoolResults (decode result_status) failed: {}", string_VkResult(r));
        }
        session_hot.result_status_pending = false;
      }
      vkCmdResetQueryPool(encoder_->vk_cb_handle(), session_hot.result_status_query_pool, 0, 1);
    }

    // Resolve parameters.
    auto const params_pool_handle = resource_pool::ResourceHandle::FromU64(desc.parameters.Get());
    auto [params_hot, params_cold, params_lock] =
      resource_storage_->video_session_parameters.GetConstRefWithSharedLockGuard(params_pool_handle);
    VkVideoSessionParametersKHR const vk_params = params_hot.vk_video_session_parameters.handle();

    // Build per-slot Vulkan structs. Each slot needs (in order):
    //   - per-array-layer VkImageView (cached)
    //   - VkVideoPictureResourceInfoKHR pointing at that view
    //   - StdVideoDecodeH265ReferenceInfo (POC + flags)
    //   - VkVideoDecodeH265DpbSlotInfoKHR pointing at the std reference info
    //   - VkVideoReferenceSlotInfoKHR pointing at the picture resource and the codec slot info via pNext
    // Reserve the SmallVectors up-front so address-of-back() stays valid.
    uint32_t const slot_count = desc.bound_reference_slots.size();
    mbase::SmallVector<VkVideoPictureResourceInfoKHR, 8>     picture_resources;
    mbase::SmallVector<StdVideoDecodeH265ReferenceInfo, 8>   std_ref_infos;
    mbase::SmallVector<VkVideoDecodeH265DpbSlotInfoKHR, 8>   dpb_slot_infos;
    mbase::SmallVector<VkVideoReferenceSlotInfoKHR, 8>       slot_infos;
    picture_resources.reserve(slot_count);
    std_ref_infos.reserve(slot_count);
    dpb_slot_infos.reserve(slot_count);
    slot_infos.reserve(slot_count);

    auto make_image_view = [this](ImageViewCacheKey const& key) {
      // For video decode views of a SAMPLED multi-planar image we restrict
      // the view's effective usage to exclude SAMPLED, since otherwise the
      // view would require a VkSamplerYcbcrConversion in pNext (which we
      // cannot specify here without coupling decode to a particular sampler
      // configuration).
      VkImageViewUsageCreateInfo const usage_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
        .pNext = nullptr,
        .usage = key.usage_override,
      };
      VkImageViewCreateInfo const create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = (key.usage_override != 0) ? &usage_info : nullptr,
        .flags = 0,
        .image = key.vk_image,
        .viewType = key.view_type,
        .format = key.format,
        .components = VkComponentMapping {
          .r = VK_COMPONENT_SWIZZLE_IDENTITY,
          .g = VK_COMPONENT_SWIZZLE_IDENTITY,
          .b = VK_COMPONENT_SWIZZLE_IDENTITY,
          .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = key.subresource_range,
      };
      VkImageView vk_image_view_handle = VK_NULL_HANDLE;
      VkResult const result = vkCreateImageView(this->vk_device_->handle(), &create_info, nullptr, &vk_image_view_handle);
      MBASE_ASSERT(result == VK_SUCCESS);
      return std::make_shared<VulkanImageView>(
        vk_image_view_handle,
        [vk_device = this->vk_device_, vk_image_view_handle]() {
          vkDestroyImageView(vk_device->handle(), vk_image_view_handle, nullptr);
        },
        this->vk_device_->GetDeferredDestroyer()
      );
    };

    for (auto const& slot : desc.bound_reference_slots) {
      auto const tex_pool_handle = resource_pool::ResourceHandle::FromU64(slot.picture.Get());
      auto [tex_hot, tex_cold, tex_lock] =
        resource_storage_->textures.GetConstRefWithSharedLockGuard(tex_pool_handle);
      VkImage const vk_image = tex_hot.GetVkImage().handle();
      mnexus::TextureDesc const& tex_desc = tex_cold.GetTextureDesc();
      VkFormat const vk_format = ToVkFormat(tex_desc.format);

      VkImageSubresourceRange const sub_range{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = slot.array_layer,
        .layerCount = 1,
      };

      VulkanImageViewPtr const vk_image_view = resource_storage_->image_view_cache.FindOrInsert(
        ImageViewCacheKey {
          .vk_image = vk_image,
          .view_type = VK_IMAGE_VIEW_TYPE_2D,
          .format = vk_format,
          .subresource_range = sub_range,
          // Restrict the view to video-decode-only usage so it does not
          // require a YcbcrConversion when the image's format is a multi-
          // planar YCbCr one (NV12 / P010 etc.).
          .usage_override = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR
                          | VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,
        },
        make_image_view
      );

      picture_resources.emplace_back(VkVideoPictureResourceInfoKHR {
        .sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR,
        .pNext = nullptr,
        .codedOffset = VkOffset2D { 0, 0 },
        .codedExtent = VkExtent2D { tex_desc.width, tex_desc.height },
        .baseArrayLayer = 0, // image view already targets the right layer
        .imageViewBinding = vk_image_view->handle(),
      });

      StdVideoDecodeH265ReferenceInfo std_ref{};
      std_ref.PicOrderCntVal = slot.pic_order_cnt_val;
      std_ref_infos.push_back(std_ref);

      dpb_slot_infos.emplace_back(VkVideoDecodeH265DpbSlotInfoKHR {
        .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR,
        .pNext = nullptr,
        .pStdReferenceInfo = &std_ref_infos.back(),
      });

      slot_infos.emplace_back(VkVideoReferenceSlotInfoKHR {
        .sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR,
        .pNext = &dpb_slot_infos.back(),
        .slotIndex = slot.slot_index,
        .pPictureResource = &picture_resources.back(),
      });

      referenced_resources_.push_back(tex_pool_handle);
    }

    // Flush any pending barriers; Vulkan disallows barriers inside a video coding scope.
    this->FlushPipelineBarrier();

    VkVideoBeginCodingInfoKHR const begin_info{
      .sType = VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .videoSession = vk_session,
      .videoSessionParameters = vk_params,
      .referenceSlotCount = static_cast<uint32_t>(slot_infos.size()),
      .pReferenceSlots = slot_infos.empty() ? nullptr : slot_infos.data(),
    };
    vkCmdBeginVideoCodingKHR(encoder_->vk_cb_handle(), &begin_info);

    referenced_resources_.push_back(session_pool_handle);
    referenced_resources_.push_back(params_pool_handle);

    // Remember the active scope so DecodeVideoH265 can look up the session's
    // POC computer / vidsynt context and the parameters' parsed SPS/PPS.
    current_video_session_pool_handle_            = session_pool_handle;
    current_video_session_parameters_pool_handle_ = params_pool_handle;
#else
    (void)desc;
    MBASE_LOG_ERROR("BeginVideoCoding called but mnexus was built without MNEXUS_ENABLE_VIDEO_CODING");
#endif
  }

  IMPL_VAPI(void, EndVideoCoding) {
#if MNEXUS_ENABLE_VIDEO_CODING
    VkVideoEndCodingInfoKHR const end_info{
      .sType = VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
    };
    vkCmdEndVideoCodingKHR(encoder_->vk_cb_handle(), &end_info);

    current_video_session_pool_handle_            = resource_pool::ResourceHandle::Null();
    current_video_session_parameters_pool_handle_ = resource_pool::ResourceHandle::Null();
#else
    MBASE_LOG_ERROR("EndVideoCoding called but mnexus was built without MNEXUS_ENABLE_VIDEO_CODING");
#endif
  }

  IMPL_VAPI(void, ControlVideoCodingReset) {
#if MNEXUS_ENABLE_VIDEO_CODING
    VkVideoCodingControlInfoKHR const control_info{
      .sType = VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR,
      .pNext = nullptr,
      .flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR,
    };
    vkCmdControlVideoCodingKHR(encoder_->vk_cb_handle(), &control_info);
#else
    MBASE_LOG_ERROR("ControlVideoCodingReset called but mnexus was built without MNEXUS_ENABLE_VIDEO_CODING");
#endif
  }

  IMPL_VAPI(void, DecodeVideoH265, mnexus::DecodeVideoH265Desc const& desc) {
#if MNEXUS_ENABLE_VIDEO_CODING
    if (current_video_session_pool_handle_.IsNull()
     || current_video_session_parameters_pool_handle_.IsNull()) {
      MBASE_LOG_ERROR("DecodeVideoH265 must be called inside a BeginVideoCoding / EndVideoCoding scope.");
      return;
    }

    // Resolve the active session for the result_status query pool used to
    // wrap the decode call below (diagnostic). Slice header / POC / RPS
    // info comes from `desc.picture_info`, not from a session-side parser.
    auto [session_hot, session_cold, session_lock] =
      resource_storage_->video_sessions.GetConstRefWithSharedLockGuard(current_video_session_pool_handle_);

    // ----- Build StdVideoDecodeH265PictureInfo from the caller's `picture_info`.
    // Slice-header parsing, POC computation, and DPB slot tracking are the
    // caller's responsibility (so callers that re-decode the same picture
    // multiple times do not drift the POC computer state).
    auto const& pic_info = desc.picture_info;
    StdVideoDecodeH265PictureInfo std_picture_info{};
    std_picture_info.flags.IdrPicFlag                      = pic_info.idr_pic_flag                    ? 1 : 0;
    std_picture_info.flags.IrapPicFlag                     = pic_info.irap_pic_flag                   ? 1 : 0;
    std_picture_info.flags.IsReference                     = pic_info.is_reference                    ? 1 : 0;
    std_picture_info.flags.short_term_ref_pic_set_sps_flag = pic_info.short_term_ref_pic_set_sps_flag ? 1 : 0;
    std_picture_info.sps_video_parameter_set_id            = pic_info.sps_video_parameter_set_id;
    std_picture_info.pps_seq_parameter_set_id              = pic_info.pps_seq_parameter_set_id;
    std_picture_info.pps_pic_parameter_set_id              = pic_info.pps_pic_parameter_set_id;
    std_picture_info.PicOrderCntVal                        = pic_info.pic_order_cnt_val;
    std_picture_info.NumDeltaPocsOfRefRpsIdx               = pic_info.num_delta_pocs_of_ref_rps_idx;
    std_picture_info.NumBitsForSTRefPicSetInSlice          = pic_info.num_bits_for_st_ref_pic_set_in_slice;
    static_assert(sizeof(std_picture_info.RefPicSetStCurrBefore) == sizeof(pic_info.ref_pic_set_st_curr_before));
    static_assert(sizeof(std_picture_info.RefPicSetStCurrAfter)  == sizeof(pic_info.ref_pic_set_st_curr_after));
    static_assert(sizeof(std_picture_info.RefPicSetLtCurr)       == sizeof(pic_info.ref_pic_set_lt_curr));
    std::memcpy(std_picture_info.RefPicSetStCurrBefore, pic_info.ref_pic_set_st_curr_before, sizeof(pic_info.ref_pic_set_st_curr_before));
    std::memcpy(std_picture_info.RefPicSetStCurrAfter,  pic_info.ref_pic_set_st_curr_after,  sizeof(pic_info.ref_pic_set_st_curr_after));
    std::memcpy(std_picture_info.RefPicSetLtCurr,       pic_info.ref_pic_set_lt_curr,        sizeof(pic_info.ref_pic_set_lt_curr));

    // ----- Build setup_reference picture resource + DPB slot info -----
    auto const setup_tex_pool_handle = resource_pool::ResourceHandle::FromU64(desc.setup_reference.picture.Get());
    auto [setup_hot, setup_cold, setup_lock] =
      resource_storage_->textures.GetConstRefWithSharedLockGuard(setup_tex_pool_handle);
    VkImage const setup_vk_image = setup_hot.GetVkImage().handle();
    mnexus::TextureDesc const& setup_tex_desc = setup_cold.GetTextureDesc();
    VkFormat const setup_vk_format = ToVkFormat(setup_tex_desc.format);

    auto make_video_decode_image_view = [this](ImageViewCacheKey const& key) {
      VkImageViewUsageCreateInfo const usage_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
        .pNext = nullptr,
        .usage = key.usage_override,
      };
      VkImageViewCreateInfo const create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = (key.usage_override != 0) ? &usage_info : nullptr,
        .flags = 0,
        .image = key.vk_image,
        .viewType = key.view_type,
        .format = key.format,
        .components = VkComponentMapping {
          .r = VK_COMPONENT_SWIZZLE_IDENTITY,
          .g = VK_COMPONENT_SWIZZLE_IDENTITY,
          .b = VK_COMPONENT_SWIZZLE_IDENTITY,
          .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = key.subresource_range,
      };
      VkImageView vk_image_view_handle = VK_NULL_HANDLE;
      VkResult const result = vkCreateImageView(this->vk_device_->handle(), &create_info, nullptr, &vk_image_view_handle);
      MBASE_ASSERT(result == VK_SUCCESS);
      return std::make_shared<VulkanImageView>(
        vk_image_view_handle,
        [vk_device = this->vk_device_, vk_image_view_handle]() {
          vkDestroyImageView(vk_device->handle(), vk_image_view_handle, nullptr);
        },
        this->vk_device_->GetDeferredDestroyer()
      );
    };

    VkImageSubresourceRange const setup_sub_range{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = desc.setup_reference.array_layer,
      .layerCount = 1,
    };
    VulkanImageViewPtr const setup_image_view = resource_storage_->image_view_cache.FindOrInsert(
      ImageViewCacheKey {
        .vk_image = setup_vk_image,
        .view_type = VK_IMAGE_VIEW_TYPE_2D,
        .format = setup_vk_format,
        .subresource_range = setup_sub_range,
        .usage_override = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR
                        | VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,
      },
      make_video_decode_image_view
    );

    VkVideoPictureResourceInfoKHR const setup_picture_resource{
      .sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR,
      .pNext = nullptr,
      .codedOffset = VkOffset2D { 0, 0 },
      .codedExtent = VkExtent2D { setup_tex_desc.width, setup_tex_desc.height },
      .baseArrayLayer = 0, // image view targets the right layer already
      .imageViewBinding = setup_image_view->handle(),
    };

    StdVideoDecodeH265ReferenceInfo std_setup_ref{};
    std_setup_ref.PicOrderCntVal                    = pic_info.pic_order_cnt_val;
    std_setup_ref.flags.unused_for_reference        = pic_info.is_reference ? 0 : 1;
    std_setup_ref.flags.used_for_long_term_reference = 0;

    VkVideoDecodeH265DpbSlotInfoKHR const setup_dpb_slot_info{
      .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR,
      .pNext = nullptr,
      .pStdReferenceInfo = &std_setup_ref,
    };
    VkVideoReferenceSlotInfoKHR const setup_slot_info{
      .sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR,
      .pNext = &setup_dpb_slot_info,
      .slotIndex = desc.setup_reference.slot_index,
      .pPictureResource = &setup_picture_resource,
    };

    referenced_resources_.push_back(setup_tex_pool_handle);

    // ----- Build per-active-reference slot info arrays -----
    uint32_t const ref_count = desc.active_references.size();
    mbase::SmallVector<VkVideoPictureResourceInfoKHR, 8>     ref_picture_resources;
    mbase::SmallVector<StdVideoDecodeH265ReferenceInfo, 8>   ref_std_infos;
    mbase::SmallVector<VkVideoDecodeH265DpbSlotInfoKHR, 8>   ref_dpb_slot_infos;
    mbase::SmallVector<VkVideoReferenceSlotInfoKHR, 8>       ref_slot_infos;
    ref_picture_resources.reserve(ref_count);
    ref_std_infos.reserve(ref_count);
    ref_dpb_slot_infos.reserve(ref_count);
    ref_slot_infos.reserve(ref_count);

    for (auto const& ref : desc.active_references) {
      auto const ref_tex_pool_handle = resource_pool::ResourceHandle::FromU64(ref.picture.Get());
      auto [ref_hot, ref_cold, ref_lock] =
        resource_storage_->textures.GetConstRefWithSharedLockGuard(ref_tex_pool_handle);
      VkImage const ref_vk_image = ref_hot.GetVkImage().handle();
      mnexus::TextureDesc const& ref_tex_desc = ref_cold.GetTextureDesc();
      VkFormat const ref_vk_format = ToVkFormat(ref_tex_desc.format);

      VkImageSubresourceRange const ref_sub_range{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = ref.array_layer,
        .layerCount = 1,
      };
      VulkanImageViewPtr const ref_image_view = resource_storage_->image_view_cache.FindOrInsert(
        ImageViewCacheKey {
          .vk_image = ref_vk_image,
          .view_type = VK_IMAGE_VIEW_TYPE_2D,
          .format = ref_vk_format,
          .subresource_range = ref_sub_range,
          .usage_override = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR
                          | VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,
        },
        make_video_decode_image_view
      );

      ref_picture_resources.emplace_back(VkVideoPictureResourceInfoKHR {
        .sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR,
        .pNext = nullptr,
        .codedOffset = VkOffset2D { 0, 0 },
        .codedExtent = VkExtent2D { ref_tex_desc.width, ref_tex_desc.height },
        .baseArrayLayer = 0,
        .imageViewBinding = ref_image_view->handle(),
      });

      StdVideoDecodeH265ReferenceInfo ref_std{};
      ref_std.PicOrderCntVal = ref.pic_order_cnt_val;
      ref_std_infos.push_back(ref_std);

      ref_dpb_slot_infos.emplace_back(VkVideoDecodeH265DpbSlotInfoKHR {
        .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR,
        .pNext = nullptr,
        .pStdReferenceInfo = &ref_std_infos.back(),
      });

      ref_slot_infos.emplace_back(VkVideoReferenceSlotInfoKHR {
        .sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR,
        .pNext = &ref_dpb_slot_infos.back(),
        .slotIndex = ref.slot_index,
        .pPictureResource = &ref_picture_resources.back(),
      });

      referenced_resources_.push_back(ref_tex_pool_handle);
    }

    // ----- Resolve src bitstream buffer -----
    auto const src_buffer_pool_handle = resource_pool::ResourceHandle::FromU64(desc.src_buffer.Get());
    auto [src_buf_hot, src_buf_lock] =
      resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(src_buffer_pool_handle);
    VkBuffer const vk_src_buffer = src_buf_hot.vk_buffer.handle();
    referenced_resources_.push_back(src_buffer_pool_handle);

    // ----- Build slice segment offsets array (uint64 -> uint32) -----
    mbase::SmallVector<uint32_t, 8> slice_segment_offsets_u32;
    slice_segment_offsets_u32.reserve(desc.slice_segment_offsets.size());
    for (uint64_t off : desc.slice_segment_offsets) {
      slice_segment_offsets_u32.push_back(static_cast<uint32_t>(off));
    }

    // ----- Build VkVideoDecodeH265PictureInfoKHR + VkVideoDecodeInfoKHR -----
    VkVideoDecodeH265PictureInfoKHR const h265_picture_info{
      .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PICTURE_INFO_KHR,
      .pNext = nullptr,
      .pStdPictureInfo = &std_picture_info,
      .sliceSegmentCount = static_cast<uint32_t>(slice_segment_offsets_u32.size()),
      .pSliceSegmentOffsets = slice_segment_offsets_u32.empty() ? nullptr : slice_segment_offsets_u32.data(),
    };

    // pSetupReferenceSlot is optional. Per VUID-VkVideoDecodeInfoKHR-
    // pSetupReferenceSlot-07168, if it is non-NULL its slotIndex MUST be
    // >= 0. Callers express "decode but do not register the reconstructed
    // picture in a DPB slot" by passing setup_reference.slot_index < 0;
    // the destination image is still written via dstPictureResource.
    bool const has_dpb_setup = desc.setup_reference.slot_index >= 0;
    VkVideoDecodeInfoKHR const decode_info{
      .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR,
      .pNext = &h265_picture_info,
      .flags = 0,
      .srcBuffer = vk_src_buffer,
      .srcBufferOffset = desc.src_buffer_offset,
      .srcBufferRange = desc.src_buffer_range,
      .dstPictureResource = setup_picture_resource,
      .pSetupReferenceSlot = has_dpb_setup ? &setup_slot_info : nullptr,
      .referenceSlotCount = static_cast<uint32_t>(ref_slot_infos.size()),
      .pReferenceSlots = ref_slot_infos.empty() ? nullptr : ref_slot_infos.data(),
    };

    // Wrap the decode in a result_status query so the next BeginVideoCoding
    // can read back and log the per-decode status (diagnostic only). The
    // pool was reset in BeginVideoCoding above, before the coding scope.
    if (session_hot.result_status_query_pool != VK_NULL_HANDLE) {
      vkCmdBeginQuery(encoder_->vk_cb_handle(), session_hot.result_status_query_pool, 0, 0);
    }

    vkCmdDecodeVideoKHR(encoder_->vk_cb_handle(), &decode_info);

    if (session_hot.result_status_query_pool != VK_NULL_HANDLE) {
      vkCmdEndQuery(encoder_->vk_cb_handle(), session_hot.result_status_query_pool, 0);
      session_hot.result_status_pending = true;
    }
#else
    (void)desc;
    MBASE_LOG_ERROR("DecodeVideoH265 called but mnexus was built without MNEXUS_ENABLE_VIDEO_CODING");
#endif
  }

  IMPL_VAPI(void, EncodeVideoH265, mnexus::EncodeVideoH265Desc const& /*desc*/) {
    MBASE_LOG_ERROR("EncodeVideoH265 is not yet implemented on the Vulkan backend.");
  }

  //
  // Timestamp queries
  //

  IMPL_VAPI(void, ResetQueries,
    mnexus::QueryPoolHandle pool, uint32_t first_query, uint32_t count
  ) {
    if (count == 0) return;
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(pool.Get());
    if (pool_handle.IsNull()) return;

    auto [hot, cold, lock] =
      resource_storage_->query_pools.GetConstRefWithSharedLockGuard(pool_handle);
    if (hot.vk_query_pool == VK_NULL_HANDLE) return;
    // Pipeline barriers may not be flushed mid-render-pass; ResetQueries
    // is required outside one anyway, so just emit the cmd directly.
    vkCmdResetQueryPool(encoder_->vk_cb_handle(), hot.vk_query_pool, first_query, count);
  }

  IMPL_VAPI(void, WriteTimestamp,
    mnexus::QueryPoolHandle pool, uint32_t query_index,
    mnexus::ResourceBarrierStageFlagBits stage
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(pool.Get());
    if (pool_handle.IsNull()) return;

    auto [hot, cold, lock] =
      resource_storage_->query_pools.GetConstRefWithSharedLockGuard(pool_handle);
    if (hot.vk_query_pool == VK_NULL_HANDLE) return;

    // Translate the abstract stage to its Vulkan synchronization2 mask
    // and write the timestamp at the END of that stage.
    VkPipelineStageFlags2KHR const vk_stage = ToVkPipelineStageFlags2(
      mnexus::ResourceBarrierStageFlags{stage});
    vkCmdWriteTimestamp2KHR(encoder_->vk_cb_handle(), vk_stage, hot.vk_query_pool, query_index);
  }

  //
  // Transfer
  //

  IMPL_VAPI(void, ClearTexture,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ClearValue const& clear_value
  ) {
    // The caller is responsible for putting the target subresource into
    // ResourceBarrierState::kTransferDst beforehand via TextureBarrier.
    this->FlushPipelineBarrier();

    auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    encoder_->CmdClearImageSubresourceRange(hot.GetVkImage(), subresource_range, clear_value);

    referenced_resources_.push_back(pool_handle);
  }

  IMPL_VAPI(void, CopyBufferToTexture,
    mnexus::BufferHandle src_buffer_handle,
    uint32_t src_buffer_offset,
    mnexus::TextureHandle dst_texture_handle,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Extent3d const& copy_extent
  ) {
    MBASE_ASSERT(dst_subresource_range.mip_level_count == 1);

    // The caller is responsible for putting the destination subresource
    // into ResourceBarrierState::kTransferDst beforehand via TextureBarrier.
    this->FlushPipelineBarrier();

    auto const src_pool_handle = resource_pool::ResourceHandle::FromU64(src_buffer_handle.Get());
    auto [src_hot, src_lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(src_pool_handle);

    auto const dst_pool_handle = resource_pool::ResourceHandle::FromU64(dst_texture_handle.Get());
    auto [dst_hot, dst_cold, dst_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(dst_pool_handle);

    encoder_->CmdCopyBufferToImageSubresource(
      src_hot.vk_buffer,
      src_buffer_offset,
      dst_hot.GetVkImage(),
      dst_subresource_range,
      copy_extent
    );

    referenced_resources_.push_back(src_pool_handle);
    referenced_resources_.push_back(dst_pool_handle);
  }

  IMPL_VAPI(void, CopyTextureToBuffer,
    mnexus::TextureHandle src_texture_handle,
    mnexus::TextureSubresourceRange const& src_subresource_range,
    mnexus::BufferHandle dst_buffer_handle,
    uint32_t dst_buffer_offset,
    mnexus::Extent3d const& copy_extent
  ) {
    MBASE_ASSERT(src_subresource_range.mip_level_count == 1);

    // Caller is responsible for transitioning the source subresource into
    // ResourceBarrierState::kTransferSrc beforehand via TextureBarrier.
    this->FlushPipelineBarrier();

    auto const src_pool_handle = resource_pool::ResourceHandle::FromU64(src_texture_handle.Get());
    auto [src_hot, src_cold, src_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(src_pool_handle);

    auto const dst_pool_handle = resource_pool::ResourceHandle::FromU64(dst_buffer_handle.Get());
    auto [dst_hot, dst_lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(dst_pool_handle);

    encoder_->CmdCopyImageSubresourceToBuffer(
      src_hot.GetVkImage(),
      src_subresource_range,
      dst_hot.vk_buffer,
      dst_buffer_offset,
      copy_extent
    );

    referenced_resources_.push_back(src_pool_handle);
    referenced_resources_.push_back(dst_pool_handle);
  }

  IMPL_VAPI(void, CopyTextureToTexture,
    mnexus::TextureHandle src_texture_handle,
    mnexus::TextureSubresourceRange const& src_subresource_range,
    mnexus::TextureHandle dst_texture_handle,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Extent3d const& copy_extent
  ) {
    MBASE_ASSERT(src_subresource_range.mip_level_count == 1);
    MBASE_ASSERT(dst_subresource_range.mip_level_count == 1);

    // Caller is responsible for transitioning src into kTransferSrc and
    // dst into kTransferDst (stage kTransfer) beforehand via TextureBarrier.
    this->FlushPipelineBarrier();

    auto const src_pool_handle = resource_pool::ResourceHandle::FromU64(src_texture_handle.Get());
    auto [src_hot, src_cold, src_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(src_pool_handle);

    auto const dst_pool_handle = resource_pool::ResourceHandle::FromU64(dst_texture_handle.Get());
    auto [dst_hot, dst_cold, dst_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(dst_pool_handle);

    encoder_->CmdCopyImageSubresourceToImageSubresource(
      src_hot.GetVkImage(),
      src_subresource_range,
      dst_hot.GetVkImage(),
      dst_subresource_range,
      copy_extent
    );

    referenced_resources_.push_back(src_pool_handle);
    referenced_resources_.push_back(dst_pool_handle);
  }

  IMPL_VAPI(void, BlitTexture,
    mnexus::TextureHandle src_texture_handle,
    mnexus::TextureSubresourceRange const& src_subresource_range,
    mnexus::Offset3d const& src_offset,
    mnexus::Extent3d const& src_extent,
    mnexus::TextureHandle dst_texture_handle,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Offset3d const& dst_offset,
    mnexus::Extent3d const& dst_extent,
    mnexus::Filter filter
  ) {
    // Caller is required to have transitioned src into kTransferSrc and dst
    // into kTransferDst via TextureBarrier.
    this->FlushPipelineBarrier();

    auto const src_pool_handle = resource_pool::ResourceHandle::FromU64(src_texture_handle.Get());
    auto [src_hot, src_cold, src_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(src_pool_handle);
    VkImage const src_vk_image = src_hot.GetVkImage().handle();

    auto const dst_pool_handle = resource_pool::ResourceHandle::FromU64(dst_texture_handle.Get());
    auto [dst_hot, dst_cold, dst_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(dst_pool_handle);
    VkImage const dst_vk_image = dst_hot.GetVkImage().handle();

    VkImageBlit const region {
      .srcSubresource = ToVkImageSubresourceLayers(src_subresource_range),
      .srcOffsets = {
        VkOffset3D {
          static_cast<int32_t>(src_offset.x),
          static_cast<int32_t>(src_offset.y),
          static_cast<int32_t>(src_offset.z),
        },
        VkOffset3D {
          static_cast<int32_t>(src_offset.x + src_extent.width),
          static_cast<int32_t>(src_offset.y + src_extent.height),
          static_cast<int32_t>(src_offset.z + src_extent.depth),
        },
      },
      .dstSubresource = ToVkImageSubresourceLayers(dst_subresource_range),
      .dstOffsets = {
        VkOffset3D {
          static_cast<int32_t>(dst_offset.x),
          static_cast<int32_t>(dst_offset.y),
          static_cast<int32_t>(dst_offset.z),
        },
        VkOffset3D {
          static_cast<int32_t>(dst_offset.x + dst_extent.width),
          static_cast<int32_t>(dst_offset.y + dst_extent.height),
          static_cast<int32_t>(dst_offset.z + dst_extent.depth),
        },
      },
    };

    vkCmdBlitImage(
      encoder_->vk_cb_handle(),
      src_vk_image,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      dst_vk_image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region,
      ToVkFilter(filter)
    );

    referenced_resources_.push_back(src_pool_handle);
    referenced_resources_.push_back(dst_pool_handle);
  }

  //
  // Compute
  //

  IMPL_VAPI(void, BindExplicitComputePipeline,
    mnexus::ComputePipelineHandle compute_pipeline_handle
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(compute_pipeline_handle.Get());
    auto [hot, cold, lock] = resource_storage_->compute_pipelines.GetConstRefWithSharedLockGuard(pool_handle);

    VulkanPipelineLayoutPtr const& pipeline_layout_ref = hot.pipeline_layout_ref();

    encoder_->BindComputePipeline(
      hot.vk_compute_pipeline().handle(),
      pipeline_layout_ref->handle(),
      pipeline_layout_ref->descriptor_set_layouts.data(),
      static_cast<uint32_t>(pipeline_layout_ref->descriptor_set_layouts.size())
    );

    // Track referenced resources for submit-time stamping.
    referenced_resources_.push_back(pool_handle);
    referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(cold.program_handle().Get()));
    referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(cold.shader_module_handle().Get()));
  }

  IMPL_VAPI(void, DispatchCompute,
    uint32_t workgroup_count_x,
    uint32_t workgroup_count_y,
    uint32_t workgroup_count_z
  ) {
    this->FlushPipelineBarrier();
    encoder_->CmdDispatchCompute(workgroup_count_x, workgroup_count_y, workgroup_count_z);
  }

  //
  // Resource Binding
  //

  IMPL_VAPI(void, BindUniformBuffer,
    mnexus::BindingId const& id,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);
    encoder_->BindBuffer(
      id.group, id.binding, id.array_element,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      buffer_handle.Get(), hot.vk_buffer.handle(), offset, size
    );
    referenced_resources_.push_back(pool_handle);
  }

  IMPL_VAPI(void, BindStorageBuffer,
    mnexus::BindingId const& id,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);
    encoder_->BindBuffer(
      id.group, id.binding, id.array_element,
      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      buffer_handle.Get(), hot.vk_buffer.handle(), offset, size
    );
    referenced_resources_.push_back(pool_handle);
  }

  IMPL_VAPI(void, BindSampledTexture,
    mnexus::BindingId const& id,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::Format view_format
  ) {
    // Pre-condition: the texture's subresource range MUST already be in
    // VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR. machina/folgos-style: this
    // call only writes the descriptor and does not queue a layout
    // transition (pipeline barriers cannot be issued inside a dynamic
    // rendering instance). The default layout for SAMPLED textures is
    // READ_ONLY_OPTIMAL_KHR, so freshly-created textures are fine; for
    // textures that may be in another layout (e.g. just after a copy)
    // the caller is responsible via a future explicit barrier API.
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    VulkanImage const& vk_image = hot.GetVkImage();
    VkImage const vk_image_handle = vk_image.handle();
    mnexus::TextureDesc const& desc = cold.GetTextureDesc();
    // The image view's format defaults to the texture's format. Callers
    // can override (typically when sampling an individual plane of a
    // multi-planar texture, e.g. R8 for plane 0 of NV12).
    VkFormat const vk_format = ToVkFormat(
      view_format == mnexus::Format::kUndefined ? desc.format : view_format
    );
    VkImageSubresourceRange const vk_subresource_range = ToVkImageSubresourceRange(subresource_range);

    constexpr VkImageLayout kShaderReadLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;

    // Get/create a VkImageView via the cache.
    auto make_vk_image_view = [this](ImageViewCacheKey const& key) {
      VkImageViewCreateInfo const create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = key.vk_image,
        .viewType = key.view_type,
        .format = key.format,
        .components = VkComponentMapping {
          .r = VK_COMPONENT_SWIZZLE_IDENTITY,
          .g = VK_COMPONENT_SWIZZLE_IDENTITY,
          .b = VK_COMPONENT_SWIZZLE_IDENTITY,
          .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = key.subresource_range,
      };

      VkImageView vk_image_view_handle = VK_NULL_HANDLE;
      VkResult const result = vkCreateImageView(this->vk_device_->handle(), &create_info, nullptr, &vk_image_view_handle);
      MBASE_ASSERT(result == VK_SUCCESS);

      return std::make_shared<VulkanImageView>(
        vk_image_view_handle,
        [vk_device = this->vk_device_, vk_image_view_handle]() {
          vkDestroyImageView(vk_device->handle(), vk_image_view_handle, nullptr);
        },
        this->vk_device_->GetDeferredDestroyer()
      );
    };

    // TODO: Image view type should be derived from the texture's TextureDimension.
    constexpr VkImageViewType kImageViewType = VK_IMAGE_VIEW_TYPE_2D;

    VulkanImageViewPtr vk_image_view = resource_storage_->image_view_cache.FindOrInsert(
      ImageViewCacheKey {
        .vk_image = vk_image_handle,
        .view_type = kImageViewType,
        .format = vk_format,
        .subresource_range = vk_subresource_range,
      },
      // SAFETY: Executed immediately.
      make_vk_image_view
    );

    encoder_->BindSampledImage(
      id.group, id.binding, id.array_element,
      reinterpret_cast<uint64_t>(vk_image_view->handle()),
      vk_image_view->handle(),
      kShaderReadLayout
    );

    referenced_resources_.push_back(pool_handle);
  }

  IMPL_VAPI(void, BindSampler,
    mnexus::BindingId const& id,
    mnexus::SamplerHandle sampler_handle
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(sampler_handle.Get());
    auto [hot, lock] = resource_storage_->samplers.GetHotConstRefWithSharedLockGuard(pool_handle);

    encoder_->BindSampler(
      id.group, id.binding, id.array_element,
      sampler_handle.Get(),
      hot.vk_sampler.handle()
    );

    referenced_resources_.push_back(pool_handle);
  }

  //
  // Explicit Pipeline Binding
  //

  IMPL_VAPI(void, BindExplicitRenderPipeline,
    mnexus::RenderPipelineHandle /*render_pipeline_handle*/
  ) {
    STUB_NOT_IMPLEMENTED();
  }

  //
  // Render Pass
  //

  IMPL_VAPI(void, BeginRenderPass,
    mnexus::RenderPassDesc const& desc
  ) {
    // Flush any pending pipeline barriers before entering the render pass:
    // vkCmdPipelineBarrier2 is not allowed inside a dynamic rendering instance.
    this->FlushPipelineBarrier();

    DynamicRenderPassDesc dyn_rp_desc{};

    mbase::SmallVector<mnexus::Format, 4> color_formats;
    mnexus::Format depth_stencil_format = mnexus::Format::kUndefined;
    VkExtent2D render_area { 0, 0 };

    for (mnexus::ColorAttachmentDesc const& attachment_desc : desc.color_attachments) {
      auto const pool_handle = resource_pool::ResourceHandle::FromU64(attachment_desc.texture.Get());
      referenced_resources_.push_back(pool_handle);

      auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

      VulkanImage const& vk_image = hot.GetVkImage();
      VkExtent3D const& extent = vk_image.extent();
      render_area.width  = std::max(render_area.width, extent.width);
      render_area.height = std::max(render_area.height, extent.height);

      color_formats.emplace_back(FromVkFormat(vk_image.vk_format()));

      RenderTargetDesc& rt_desc = dyn_rp_desc.color_attachments.emplace_back();
      rt_desc.vk_image = &vk_image;
      rt_desc.subresource_range = attachment_desc.subresource_range;
      if (attachment_desc.load_op == mnexus::LoadOp::kClear) {
        rt_desc.clear_value = RenderTargetClearValue {
          .f32 = {
            attachment_desc.clear_value.color.r,
            attachment_desc.clear_value.color.g,
            attachment_desc.clear_value.color.b,
            attachment_desc.clear_value.color.a,
          },
        };
      }
    }

    if (desc.depth_stencil_attachment != nullptr && desc.depth_stencil_attachment->texture.IsValid()) {
      auto const pool_handle = resource_pool::ResourceHandle::FromU64(desc.depth_stencil_attachment->texture.Get());
      referenced_resources_.push_back(pool_handle);

      auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

      VulkanImage const& vk_image = hot.GetVkImage();
      VkExtent3D const& extent = vk_image.extent();
      render_area.width  = std::max(render_area.width, extent.width);
      render_area.height = std::max(render_area.height, extent.height);

      depth_stencil_format = FromVkFormat(vk_image.vk_format());

      RenderTargetDesc& rt_desc = dyn_rp_desc.depth_stencil_attachment.emplace();
      rt_desc.vk_image = &vk_image;
      rt_desc.subresource_range = desc.depth_stencil_attachment->subresource_range;
      if (desc.depth_stencil_attachment->depth_load_op == mnexus::LoadOp::kClear) {
        rt_desc.clear_value = RenderTargetClearValue {
          .f32 = {
            desc.depth_stencil_attachment->depth_clear_value,
          },
        };
      }
      if (desc.depth_stencil_attachment->stencil_load_op == mnexus::LoadOp::kClear) {
        rt_desc.stencil_clear_value = RenderTargetClearValue {
            .u32 = {
              desc.depth_stencil_attachment->stencil_clear_value,
            },
        };
      }
    }

    encoder_->CmdBeginRendering(dyn_rp_desc);

    // Configure state tracker with render target info; the next Draw will
    // build the cache key using this.
    render_pipeline_state_tracker_.SetRenderTargetConfig(
      std::move(color_formats),
      depth_stencil_format,
      1 // sample_count (always 1 for now)
    );

    // Push the default viewport / scissor matching the render area. Callers
    // can overwrite later via SetViewport / SetScissor before Draw.
    encoder_->CmdSetViewport(VkViewport {
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(render_area.width),
      .height = static_cast<float>(render_area.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
    });
    encoder_->CmdSetScissor(VkRect2D {
      .offset = VkOffset2D { .x = 0, .y = 0 },
      .extent = render_area,
    });

    if (render_state_event_log_.IsEnabled()) {
      render_state_event_log_.Record(
        mnexus::RenderStateEventTag::kBeginRenderPass,
        render_pipeline_state_tracker_.BuildSnapshot());
    }
  }

  IMPL_VAPI(void, EndRenderPass) {
    if (render_state_event_log_.IsEnabled()) {
      render_state_event_log_.Record(
        mnexus::RenderStateEventTag::kEndRenderPass,
        render_pipeline_state_tracker_.BuildSnapshot());
    }
    encoder_->CmdEndRendering();
  }

  //
  // Render State (auto-generation path)
  //

  IMPL_VAPI(void, BindRenderProgram,
    mnexus::ProgramHandle program_handle
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(program_handle.Get());
    referenced_resources_.push_back(pool_handle);
    render_pipeline_state_tracker_.SetProgram(program_handle);

    // Inform the descriptor-set binder of the program's pipeline layout so
    // that subsequent Bind*Buffer / BindSampledTexture / BindSampler calls
    // can target the correct layout. The actual VkPipeline is not bound
    // until Draw flush time (cache lookup).
    auto [program_hot, program_cold, program_lock] =
      resource_storage_->programs.GetConstRefWithSharedLockGuard(pool_handle);

    VulkanPipelineLayoutPtr const& pipeline_layout_ref = program_hot.pipeline_layout_ref;
    encoder_->AssumeRenderPipelineLayout(
      pipeline_layout_ref->handle(),
      pipeline_layout_ref->descriptor_set_layouts.data(),
      static_cast<uint32_t>(pipeline_layout_ref->descriptor_set_layouts.size())
    );
  }

  IMPL_VAPI(void, SetVertexInputLayout,
    mnexus::container::ArrayProxy<mnexus::VertexInputBindingDesc const> bindings,
    mnexus::container::ArrayProxy<mnexus::VertexInputAttributeDesc const> attributes
  ) {
    mbase::SmallVector<mnexus::VertexInputBindingDesc, 4> bindings_vec;
    bindings_vec.reserve(bindings.size());
    for (uint32_t i = 0; i < bindings.size(); ++i) {
      bindings_vec.emplace_back(bindings[i]);
    }

    mbase::SmallVector<mnexus::VertexInputAttributeDesc, 8> attributes_vec;
    attributes_vec.reserve(attributes.size());
    for (uint32_t i = 0; i < attributes.size(); ++i) {
      attributes_vec.emplace_back(attributes[i]);
    }

    render_pipeline_state_tracker_.SetVertexInputLayout(
      std::move(bindings_vec),
      std::move(attributes_vec)
    );
  }

  IMPL_VAPI(void, BindVertexBuffer,
    uint32_t binding,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset
  ) {
    if (binding >= bound_vertex_buffers_.size()) {
      bound_vertex_buffers_.resize(binding + 1);
    }
    bound_vertex_buffers_[binding] = BoundVertexBuffer {
      .handle = buffer_handle,
      .offset = offset,
    };
    referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(buffer_handle.Get()));
  }

  IMPL_VAPI(void, BindIndexBuffer,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    mnexus::IndexType index_type
  ) {
    bound_index_buffer_ = BoundIndexBuffer {
      .handle = buffer_handle,
      .offset = offset,
      .index_type = index_type,
    };
    referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(buffer_handle.Get()));
  }

  IMPL_VAPI(void, SetPrimitiveTopology,
    mnexus::PrimitiveTopology topology
  ) {
    render_pipeline_state_tracker_.SetPrimitiveTopology(topology);
  }

  IMPL_VAPI(void, SetPolygonMode,
    mnexus::PolygonMode mode
  ) {
    render_pipeline_state_tracker_.SetPolygonMode(mode);
  }

  IMPL_VAPI(void, SetCullMode,
    mnexus::CullMode cull_mode
  ) {
    render_pipeline_state_tracker_.SetCullMode(cull_mode);
  }

  IMPL_VAPI(void, SetFrontFace,
    mnexus::FrontFace front_face
  ) {
    render_pipeline_state_tracker_.SetFrontFace(front_face);
  }

  // Depth

  IMPL_VAPI(void, SetDepthTestEnabled,bool enabled) {
    render_pipeline_state_tracker_.SetDepthTestEnabled(enabled);
  }

  IMPL_VAPI(void, SetDepthWriteEnabled,bool enabled) {
    render_pipeline_state_tracker_.SetDepthWriteEnabled(enabled);
  }

  IMPL_VAPI(void, SetDepthCompareOp,
    mnexus::CompareOp op
  ) {
    render_pipeline_state_tracker_.SetDepthCompareOp(op);
  }

  // Stencil

  IMPL_VAPI(void, SetStencilTestEnabled,bool enabled) {
    render_pipeline_state_tracker_.SetStencilTestEnabled(enabled);
  }

  IMPL_VAPI(void, SetStencilFrontOps,
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  ) {
    render_pipeline_state_tracker_.SetStencilFrontOps(fail, pass, depth_fail, compare);
  }

  IMPL_VAPI(void, SetStencilBackOps,
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  ) {
    render_pipeline_state_tracker_.SetStencilBackOps(fail, pass, depth_fail, compare);
  }

  // Per-attachment blend

  IMPL_VAPI(void, SetBlendEnabled,
    uint32_t attachment, bool enabled
  ) {
    render_pipeline_state_tracker_.SetBlendEnabled(attachment, enabled);
  }

  IMPL_VAPI(void, SetBlendFactors,
    uint32_t attachment,
    mnexus::BlendFactor src_color, mnexus::BlendFactor dst_color, mnexus::BlendOp color_op,
    mnexus::BlendFactor src_alpha, mnexus::BlendFactor dst_alpha, mnexus::BlendOp alpha_op
  ) {
    render_pipeline_state_tracker_.SetBlendFactors(
      attachment, src_color, dst_color, color_op,
      src_alpha, dst_alpha, alpha_op
    );
  }

  IMPL_VAPI(void, SetColorWriteMask,
    uint32_t attachment, mnexus::ColorWriteMask mask
  ) {
    render_pipeline_state_tracker_.SetColorWriteMask(attachment, mask);
  }

  //
  // Draw
  //

  IMPL_VAPI(void, Draw,
    uint32_t vertex_count, uint32_t instance_count,
    uint32_t first_vertex, uint32_t first_instance
  ) {
    this->FlushRenderPipeline();
    this->FlushVertexBuffers();

    if (render_state_event_log_.IsEnabled()) {
      render_state_event_log_.Record(
        mnexus::RenderStateEventTag::kDraw,
        render_pipeline_state_tracker_.BuildSnapshot());
    }

    encoder_->CmdDraw(vertex_count, instance_count, first_vertex, first_instance);
  }

  IMPL_VAPI(void, DrawIndexed,
    uint32_t index_count, uint32_t instance_count,
    uint32_t first_index, int32_t vertex_offset, uint32_t first_instance
  ) {
    this->FlushRenderPipeline();
    this->FlushVertexBuffers();
    this->FlushIndexBuffer();

    if (render_state_event_log_.IsEnabled()) {
      render_state_event_log_.Record(
        mnexus::RenderStateEventTag::kDrawIndexed,
        render_pipeline_state_tracker_.BuildSnapshot());
    }

    encoder_->CmdDrawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
  }

  //
  // Viewport / Scissor
  //

  IMPL_VAPI(void, SetViewport,
    float x, float y, float width, float height,
    float min_depth, float max_depth
  ) {
    encoder_->CmdSetViewport(VkViewport {
      .x = x,
      .y = y,
      .width = width,
      .height = height,
      .minDepth = min_depth,
      .maxDepth = max_depth,
    });
  }

  IMPL_VAPI(void, SetScissor,
    int32_t x, int32_t y, uint32_t width, uint32_t height
  ) {
    encoder_->CmdSetScissor(VkRect2D {
      .offset = VkOffset2D { .x = x, .y = y },
      .extent = VkExtent2D { .width = width, .height = height },
    });
  }

private:
  struct BoundVertexBuffer {
    mnexus::BufferHandle handle = mnexus::BufferHandle::Invalid();
    uint64_t offset = 0;
  };

  struct BoundIndexBuffer {
    mnexus::BufferHandle handle = mnexus::BufferHandle::Invalid();
    uint64_t offset = 0;
    mnexus::IndexType index_type = mnexus::IndexType::kUint16;
  };

  /// Resolves the dirty render pipeline state to a VkPipeline (via cache),
  /// then binds it on the encoder. No-op if not dirty.
  void FlushRenderPipeline() {
    if (!render_pipeline_state_tracker_.IsDirty()) {
      return;
    }
    pipeline::RenderPipelineCacheKey key = render_pipeline_state_tracker_.BuildCacheKey();
    render_pipeline_state_tracker_.MarkClean();

    bool cache_hit = false;
    VulkanRenderPipelinePtr pipeline = resource_storage_->render_pipeline_cache.FindOrInsert(
      key,
      [this](pipeline::RenderPipelineCacheKey const& k) -> VulkanRenderPipelinePtr {
        return CreateVulkanRenderPipelineFromCacheKey(
          *vk_device_,
          k,
          resource_storage_->programs,
          resource_storage_->shader_modules
        );
      },
      &cache_hit
    );
    if (pipeline == nullptr) {
      MBASE_LOG_ERROR("FlushRenderPipeline: failed to acquire VkPipeline");
      return;
    }

    if (render_state_event_log_.IsEnabled()) {
      render_state_event_log_.RecordPso(
        render_pipeline_state_tracker_.BuildSnapshot(),
        key.ComputeHash(),
        cache_hit);
    }

    // Look up the program's pipeline layout (lives next to the program in the pool).
    auto const program_pool_handle = resource_pool::ResourceHandle::FromU64(key.program.Get());
    auto [program_hot, program_cold, program_lock] =
      resource_storage_->programs.GetConstRefWithSharedLockGuard(program_pool_handle);

    VulkanPipelineLayoutPtr const& pipeline_layout_ref = program_hot.pipeline_layout_ref;

    encoder_->BindRenderPipeline(
      pipeline->handle(),
      pipeline_layout_ref->handle(),
      pipeline_layout_ref->descriptor_set_layouts.data(),
      static_cast<uint32_t>(pipeline_layout_ref->descriptor_set_layouts.size())
    );
  }

  /// Pushes all currently-bound vertex buffers to the encoder.
  void FlushVertexBuffers() {
    for (uint32_t i = 0; i < bound_vertex_buffers_.size(); ++i) {
      BoundVertexBuffer const& bvb = bound_vertex_buffers_[i];
      if (!bvb.handle.IsValid()) {
        continue;
      }
      auto const buffer_pool_handle = resource_pool::ResourceHandle::FromU64(bvb.handle.Get());
      auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(buffer_pool_handle);
      encoder_->CmdBindVertexBuffer(i, hot.vk_buffer.handle(), bvb.offset);
    }
  }

  /// Emits queued image-layout transitions (TextureBarrier) to the GPU.
  /// Must be called at the start of each op that requires the GPU state to
  /// match what the user has queued via TextureBarrier (i.e. before transfer
  /// ops, dispatch, and BeginRenderPass; never inside a render pass).
  void FlushPipelineBarrier() {
    image_layout_tracker_.FlushPendingTransitions(pending_pipeline_barrier_);
    pending_pipeline_barrier_.FlushAndClear(encoder_->vk_cb_handle());
  }

  /// Pushes the currently-bound index buffer to the encoder, if any.
  void FlushIndexBuffer() {
    if (!bound_index_buffer_.has_value() || !bound_index_buffer_->handle.IsValid()) {
      return;
    }
    auto const buffer_pool_handle = resource_pool::ResourceHandle::FromU64(bound_index_buffer_->handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(buffer_pool_handle);
    encoder_->CmdBindIndexBuffer(
      hot.vk_buffer.handle(),
      bound_index_buffer_->offset,
      ToVkIndexType(bound_index_buffer_->index_type)
    );
  }

  IVulkanDevice* vk_device_ = nullptr;
  uint32_t queue_family_index_ = 0;
  VulkanCommandPool vk_command_pool_;
  ICommandEncoder* encoder_ = nullptr;
  ResourceStorage* resource_storage_ = nullptr;
  std::vector<resource_pool::ResourceHandle> referenced_resources_;
  ImageLayoutTracker image_layout_tracker_;
  PendingPipelineBarrier pending_pipeline_barrier_;
  mnexus::RenderStateEventLog render_state_event_log_;

  /// Active video coding scope. Set by `BeginVideoCoding`, used by
  /// `DecodeVideoH265` to look up the session's POC computer + parameters'
  /// parsed SPS/PPS, cleared by `EndVideoCoding`.
  resource_pool::ResourceHandle current_video_session_pool_handle_ = resource_pool::ResourceHandle::Null();
  resource_pool::ResourceHandle current_video_session_parameters_pool_handle_ = resource_pool::ResourceHandle::Null();

  pipeline::RenderPipelineStateTracker render_pipeline_state_tracker_;
  mbase::SmallVector<BoundVertexBuffer, 4> bound_vertex_buffers_;
  std::optional<BoundIndexBuffer> bound_index_buffer_;
};

} // namespace

// ----------------------------------------------------------------------------------------------------
// IMnexusCommandListVulkan::Create
//

IMnexusCommandListVulkan* IMnexusCommandListVulkan::Create(
  IVulkanDevice* vk_device,
  IDescriptorSetAllocator* ds_allocator,
  ResourceStorage* resource_storage,
  uint32_t queue_family_index
) {
  return new MnexusCommandListVulkan(vk_device, ds_allocator, resource_storage, queue_family_index);
}

} // namespace mnexus_backend::vulkan
