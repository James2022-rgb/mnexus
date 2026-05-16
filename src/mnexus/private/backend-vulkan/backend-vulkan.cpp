// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan.h"

// c++ headers ------------------------------------------
#include <cstring>

#include <vector>
#include <optional>

// external headers -------------------------------------
#if MNEXUS_HAVE_DEAR_IMGUI
# include "imgui.h"
#endif

// public project headers -------------------------------
#include "mbase/public/log.h"
#include "mbase/public/trap.h"
#include "mbase/public/tsa.h"

// project headers --------------------------------------
#include "pipeline/render_pipeline_state_tracker.h"

#include "resource_pool/generational_pool.h"

#include "impl/impl_macros.h"

#include "backend-vulkan/backend-vulkan-command_list.h"
#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/backend-vulkan-texture.h"
#include "backend-vulkan/backend-vulkan-compute_pipeline.h"
#include "backend-vulkan/command/command_encoder.h"
#include "backend-vulkan/descriptor/descriptor_set_allocator.h"
#include "backend-vulkan/object/vk-object-command_pool.h"

#include "backend-vulkan/device/vk-device.h"
#include "backend-vulkan/device/vk-instance.h"
#include "backend-vulkan/device/vk-physical_device.h"
#include "backend-vulkan/device/vk-queue.h"
#include "backend-vulkan/device/vk-staging.h"
#include "backend-vulkan/wsi/vk-wsi_surface.h"
#include "backend-vulkan/depend/vulkan_vma.h"
#include "backend-vulkan/resource/resource_storage.h"
#include "backend-vulkan/resource/types_bridge.h"

#if MNEXUS_ENABLE_VIDEO_CODING
#  include "backend-vulkan/video/vk-video_session.h"
#  include "backend-vulkan/video/vk-video_session_parameters.h"
#endif

namespace mnexus_backend::vulkan {

namespace {

#if MNEXUS_ENABLE_VIDEO_CODING
/// Translate the backend-internal `VideoDecodeH265Properties` into the
/// public `mnexus::VideoDecodeH265Capabilities` shape.
mnexus::VideoDecodeH265Capabilities ToPublicVideoDecodeH265Capabilities(
  VideoDecodeH265Properties const& props
) {
  mnexus::VideoDecodeH265Capabilities out {};

  // VkVideoCapabilitiesKHR -> VideoCommonCapabilities
  out.common.picture_access_granularity = mnexus::Extent2d {
    .width  = props.coding_capabilities.pictureAccessGranularity.width,
    .height = props.coding_capabilities.pictureAccessGranularity.height,
  };
  out.common.min_coded_extent = mnexus::Extent2d {
    .width  = props.coding_capabilities.minCodedExtent.width,
    .height = props.coding_capabilities.minCodedExtent.height,
  };
  out.common.max_coded_extent = mnexus::Extent2d {
    .width  = props.coding_capabilities.maxCodedExtent.width,
    .height = props.coding_capabilities.maxCodedExtent.height,
  };
  out.common.min_bitstream_buffer_offset_alignment = props.coding_capabilities.minBitstreamBufferOffsetAlignment;
  out.common.min_bitstream_buffer_size_alignment   = props.coding_capabilities.minBitstreamBufferSizeAlignment;
  out.common.max_dpb_slots                         = props.coding_capabilities.maxDpbSlots;
  out.common.max_active_reference_pictures         = props.coding_capabilities.maxActiveReferencePictures;
  out.common.protected_content =
    (props.coding_capabilities.flags & VK_VIDEO_CAPABILITY_PROTECTED_CONTENT_BIT_KHR) != 0
      ? MnBoolTrue : MnBoolFalse;
  out.common.separate_reference_images =
    (props.coding_capabilities.flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR) != 0
      ? MnBoolTrue : MnBoolFalse;

  // VkVideoDecodeCapabilityFlagsKHR -> VideoDecodeCommonCapabilities
  out.decode_common.dpb_and_output_coincide =
    (props.decode_flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR) != 0
      ? MnBoolTrue : MnBoolFalse;
  out.decode_common.dpb_and_output_distinct =
    (props.decode_flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR) != 0
      ? MnBoolTrue : MnBoolFalse;

  // StdVideoH265LevelIdc -> mnexus::VideoH265Level. Both are sequential
  // ordinals starting at 0 with identical entries, but cast explicitly to
  // make the assumption auditable.
  static_assert(
    static_cast<uint32_t>(mnexus::VideoH265Level::k1_0) == STD_VIDEO_H265_LEVEL_IDC_1_0 &&
    static_cast<uint32_t>(mnexus::VideoH265Level::k6_2) == STD_VIDEO_H265_LEVEL_IDC_6_2,
    "VideoH265Level enum values must match StdVideoH265LevelIdc"
  );
  out.max_level = static_cast<mnexus::VideoH265Level>(props.max_level_idc);

  // VkFormat -> mnexus::Format
  out.picture_format = FromVkFormat(props.format_properties.format);

  return out;
}

/// Pick which internal slot corresponds to the requested (profile, bit_depth).
/// Returns nullptr if the combination is unsupported (e.g. Main + 10-bit;
/// Main is always 8-bit per spec) or not probed by this device.
VideoDecodeH265Properties const* SelectDecodeH265Slot(
  VideoDecodeH265Capabilities const& caps,
  mnexus::VideoH265Profile profile,
  mnexus::VideoBitDepth bit_depth
) {
  if (profile == mnexus::VideoH265Profile::kMain && bit_depth == mnexus::VideoBitDepth::k8) {
    return caps.main.has_value() ? &*caps.main : nullptr;
  }
  if (profile == mnexus::VideoH265Profile::kMain10 && bit_depth == mnexus::VideoBitDepth::k8) {
    return caps.main10_8bit.has_value() ? &*caps.main10_8bit : nullptr;
  }
  if (profile == mnexus::VideoH265Profile::kMain10 && bit_depth == mnexus::VideoBitDepth::k10) {
    return caps.main10_10bit.has_value() ? &*caps.main10_10bit : nullptr;
  }
  // (kMain, k10) is never valid: the Main profile is 8-bit only.
  return nullptr;
}
#endif // MNEXUS_ENABLE_VIDEO_CODING

} // anonymous namespace



// ==================================================================================================
// MnexusDeviceVulkan
//

class MnexusDeviceVulkan final : public mnexus::IDevice {
public:
  explicit MnexusDeviceVulkan(
    IVulkanDevice* vk_device,
    StagingBufferPool& staging_buffer_pool,
    TransientCommandPool& transient_command_pool,
    ResourceStorage* resource_storage
  ) :
    vk_device_(vk_device),
    staging_buffer_pool_(staging_buffer_pool),
    transient_command_pool_(transient_command_pool),
    wsi_swapchain_(WsiSwapchain::Create(vk_device->instance(), vk_device)),
    resource_storage_(resource_storage)
  {
    resource_storage_->swapchain_texture_handle = EmplaceTextureResourcePoolSwapchain(resource_storage_->textures, &wsi_swapchain_);

    descriptor_set_allocator_ = IDescriptorSetAllocator::Create(vk_device);
  }
  ~MnexusDeviceVulkan() override {
    if (descriptor_set_allocator_ != nullptr) {
      descriptor_set_allocator_->Shutdown(); // Shutdown does delete this.
      descriptor_set_allocator_ = nullptr;
    }
  }

  // ----------------------------------------------------------------------------------------------
  // Queue
  //

  IMPL_VAPI(uint32_t, QueueGetFamilyCount) {
    STUB_NOT_IMPLEMENTED();
    return 1;
  }

  IMPL_VAPI(MnBool32, QueueGetFamilyDesc,
    uint32_t /*queue_family_index*/,
    mnexus::QueueFamilyDesc& /*out_desc*/
  ) {
    STUB_NOT_IMPLEMENTED();
    return MnBoolFalse;
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueSubmitCommandList,
    mnexus::QueueId const& queue_id,
    mnexus::ICommandList* command_list
  ) {
    return this->QueueSubmitCommandListWithWaits(queue_id, command_list, {});
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueSubmitCommandListWithWaits,
    mnexus::QueueId const& queue_id,
    mnexus::ICommandList* command_list,
    mnexus::container::ArrayProxy<mnexus::QueueWaitInfo const> waits
  ) {
    auto* cmd_list_vk = static_cast<IMnexusCommandListVulkan*>(command_list);
    VkCommandBuffer vk_cb_handle = cmd_list_vk->encoder().vk_cb_handle();

    IVulkanQueue* const queue = vk_device_->GetQueue(queue_id);

    // Resolve each (QueueId, IntraQueueSubmissionId) wait to the source
    // queue's timeline semaphore + serial. Zero-valued serials are
    // silently dropped inside SubmitSingleWithWaits.
    std::vector<VkSemaphore> wait_semaphores;
    std::vector<uint64_t>    wait_values;
    wait_semaphores.reserve(waits.size());
    wait_values.reserve(waits.size());
    for (mnexus::QueueWaitInfo const& w : waits) {
      IVulkanQueue* const src = vk_device_->GetQueue(w.queue);
      wait_semaphores.push_back(src->timeline_semaphore());
      wait_values.push_back(w.value.Get());
    }

    uint64_t const serial = queue->SubmitSingleWithWaits(
      vk_cb_handle,
      static_cast<uint32_t>(wait_semaphores.size()),
      wait_semaphores.empty() ? nullptr : wait_semaphores.data(),
      wait_values.empty()     ? nullptr : wait_values.data()
    );
    uint32_t const queue_compact_index = queue->compact_index();

    // Stamp the per-list command pool so its deferred destruction waits for GPU completion.
    cmd_list_vk->vk_command_pool().sync_stamp().Stamp(queue_compact_index, serial);

    mbase::ArrayProxy<resource_pool::ResourceHandle const> referenced = cmd_list_vk->GetReferencedResources();
    for (resource_pool::ResourceHandle const& handle : referenced) {
      resource_storage_->StampResourceUse(handle, queue_compact_index, serial);
    }

    cmd_list_vk->Shutdown();
    return mnexus::IntraQueueSubmissionId { serial };
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueWriteBuffer,
    mnexus::QueueId const& queue_id,
    mnexus::BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void const* data,
    uint32_t data_size_in_bytes
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotRefWithSharedLockGuard(pool_handle);

    IVulkanQueue* const queue = vk_device_->GetQueue(queue_id);

    if (hot.mapped_data != nullptr) {
      // Mappable buffer: direct memcpy + flush.
      std::memcpy(static_cast<uint8_t*>(hot.mapped_data) + buffer_offset, data, data_size_in_bytes);
      vmaFlushAllocation(hot.vma_allocator, hot.vma_allocation, buffer_offset, data_size_in_bytes);

      // No actual queue submit needed; data is visible after flush.
      // Advance timeline to satisfy the API contract.
      uint64_t const serial = queue->AdvanceTimeline();
      return mnexus::IntraQueueSubmissionId { serial };
    }

    // Non-mappable buffer: staging path.
    StagingBuffer* staging = staging_buffer_pool_.Acquire(data_size_in_bytes);
    if (staging == nullptr) {
      MBASE_LOG_ERROR("Failed to acquire staging buffer for QueueWriteBuffer");
      return mnexus::IntraQueueSubmissionId { 0 };
    }

    std::memcpy(staging->mapped_data, data, data_size_in_bytes);
    vmaFlushAllocation(vk_device_->vma_allocator(), staging->allocation, 0, data_size_in_bytes);

    VkCommandBuffer vk_cb_handle = transient_command_pool_.Acquire();
    VkBufferCopy region {
      .srcOffset = 0,
      .dstOffset = buffer_offset,
      .size = data_size_in_bytes,
    };
    vkCmdCopyBuffer(vk_cb_handle, staging->vk_buffer, hot.vk_buffer.handle(), 1, &region);
    vkEndCommandBuffer(vk_cb_handle);

    uint64_t const serial = queue->SubmitSingle(vk_cb_handle);

    hot.vk_buffer.sync_stamp().Stamp(queue->compact_index(), serial);

    transient_command_pool_.Release(vk_cb_handle, queue_id, serial);
    staging_buffer_pool_.Release(staging, queue_id, serial);

    return mnexus::IntraQueueSubmissionId { serial };
  }

  IMPL_VAPI(void, WriteMappedBuffer,
    mnexus::BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void const* data,
    uint32_t data_size_in_bytes
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotRefWithSharedLockGuard(pool_handle);

    MBASE_ASSERT_MSG(hot.mapped_data != nullptr,
      "WriteMappedBuffer requires a buffer created with kMappable usage.");

    std::memcpy(static_cast<uint8_t*>(hot.mapped_data) + buffer_offset, data, data_size_in_bytes);
    vmaFlushAllocation(hot.vma_allocator, hot.vma_allocation, buffer_offset, data_size_in_bytes);
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueReadBuffer,
    mnexus::QueueId const& queue_id,
    mnexus::BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void* dst,
    uint32_t size_in_bytes
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);

    IVulkanQueue* const queue = vk_device_->GetQueue(queue_id);

    if (hot.mapped_data != nullptr) {
      // Mappable buffer: direct read after invalidate.
      vmaInvalidateAllocation(hot.vma_allocator, hot.vma_allocation, buffer_offset, size_in_bytes);
      std::memcpy(dst, static_cast<uint8_t const*>(hot.mapped_data) + buffer_offset, size_in_bytes);
      uint64_t const serial = queue->AdvanceTimeline();
      return mnexus::IntraQueueSubmissionId { serial };
    }

    // Non-mappable buffer: staging + deferred copy.
    StagingBuffer* staging = staging_buffer_pool_.Acquire(size_in_bytes);
    if (staging == nullptr) {
      MBASE_LOG_ERROR("Failed to acquire staging buffer for QueueReadBuffer");
      return mnexus::IntraQueueSubmissionId { 0 };
    }

    VkCommandBuffer vk_cb_handle = transient_command_pool_.Acquire();
    VkBufferCopy region {
      .srcOffset = buffer_offset,
      .dstOffset = 0,
      .size = size_in_bytes,
    };
    vkCmdCopyBuffer(vk_cb_handle, hot.vk_buffer.handle(), staging->vk_buffer, 1, &region);
    vkEndCommandBuffer(vk_cb_handle);

    uint64_t const serial = queue->SubmitSingle(vk_cb_handle);

    transient_command_pool_.Release(vk_cb_handle, queue_id, serial);

    {
      mbase::LockGuard mtx_lock(pending_readbacks_mutex_);

      pending_readbacks_.emplace_back(
        PendingReadback {
          .dst = dst,
          .size_in_bytes = size_in_bytes,
          .staging = staging,
          .queue_id = queue_id,
          .serial = serial,
        }
      );
    }

    return mnexus::IntraQueueSubmissionId { serial };
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueGetCompletedValue,
    mnexus::QueueId const& queue_id
  ) {
    return mnexus::IntraQueueSubmissionId { vk_device_->GetQueue(queue_id)->GetCompletedValue() };
  }

  IMPL_VAPI(void, QueueWaitIdle,
    mnexus::QueueId const& queue_id,
    mnexus::IntraQueueSubmissionId value
  ) {
    vk_device_->GetQueue(queue_id)->WaitSubmitSerial(value.Get());
    this->ProcessPendingReadbacks();
  }

  // ----------------------------------------------------------------------------------------------
  // Command List
  //

  IMPL_VAPI(mnexus::ICommandList*, CreateCommandList,
    mnexus::CommandListDesc const& desc
  ) {
    return IMnexusCommandListVulkan::Create(
      vk_device_, descriptor_set_allocator_, resource_storage_,
      desc.queue_family_index
    );
  }

  IMPL_VAPI(void, DiscardCommandList,
    mnexus::ICommandList* command_list
  ) {
    // The owned VulkanCommandPool's destructor enqueues deferred destruction;
    // since it was never stamped, it'll fire immediately (used_mask == 0).
    static_cast<IMnexusCommandListVulkan*>(command_list)->Shutdown();
  }

  // ----------------------------------------------------------------------------------------------
  // Buffer
  //

  IMPL_VAPI(mnexus::BufferHandle, CreateBuffer,
    mnexus::BufferDesc const& desc
  ) {
    resource_pool::ResourceHandle const pool_handle = EmplaceBufferResourcePool(
      resource_storage_->buffers,
      *vk_device_,
      desc
    );
    if (pool_handle.IsNull()) {
      return mnexus::BufferHandle::Invalid();
    }

    return mnexus::BufferHandle { pool_handle.AsU64() };
  }

  IMPL_VAPI(void, DestroyBuffer,
    mnexus::BufferHandle buffer_handle
  ) {
    // FIXME: Should defer destruction until the GPU is done using this buffer.
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
    resource_storage_->buffers.Erase(pool_handle);
  }

  IMPL_VAPI(void, GetBufferDesc,
    mnexus::BufferHandle buffer_handle,
    mnexus::BufferDesc& out_desc
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
    auto [cold, lock] = resource_storage_->buffers.GetColdConstRefWithSharedLockGuard(pool_handle);
    out_desc = cold.desc;
  }

  // ----------------------------------------------------------------------------------------------
  // Texture
  //

  IMPL_VAPI(mnexus::TextureHandle, GetSwapchainTexture) {
    return mnexus::TextureHandle{ resource_storage_->swapchain_texture_handle.AsU64() };
  }

  IMPL_VAPI(mnexus::TextureHandle, CreateTexture,
    mnexus::TextureDesc const& desc
  ) {
    resource_pool::ResourceHandle const pool_handle = EmplaceTextureResourcePool(
      resource_storage_->textures,
      *vk_device_,
      *vk_device_->GetQueue(vk_device_->queue_selection().present_capable),
      transient_command_pool_,
      desc
    );
    if (pool_handle.IsNull()) {
      return mnexus::TextureHandle::Invalid();
    }

    return mnexus::TextureHandle { pool_handle.AsU64() };
  }

  IMPL_VAPI(void, DestroyTexture,
    mnexus::TextureHandle texture_handle
  ) {
    // FIXME: Should defer destruction until the GPU is done using this texture.
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
    resource_storage_->textures.Erase(pool_handle);
  }

  IMPL_VAPI(void, GetTextureDesc,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureDesc& out_desc
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
    auto [cold, lock] = resource_storage_->textures.GetColdConstRefWithSharedLockGuard(pool_handle);
    out_desc = cold.GetTextureDesc();
  }

  // ----------------------------------------------------------------------------------------------
  // Sampler
  //

  IMPL_VAPI(mnexus::SamplerHandle, CreateSampler,
    mnexus::SamplerDesc const& desc
  ) {
    resource_pool::ResourceHandle const pool_handle = EmplaceSamplerResourcePool(
      resource_storage_->samplers,
      *vk_device_,
      desc
    );

    if (pool_handle.IsNull()) {
      return mnexus::SamplerHandle::Invalid();
    }

    return mnexus::SamplerHandle { pool_handle.AsU64() };
  }

  IMPL_VAPI(void, DestroySampler,
    mnexus::SamplerHandle sampler_handle
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(sampler_handle.Get());
    resource_storage_->samplers.Erase(pool_handle);
  }

  // ----------------------------------------------------------------------------------------------
  // ShaderModule
  //

  IMPL_VAPI(mnexus::ShaderModuleHandle, CreateShaderModule,
    mnexus::ShaderModuleDesc const& desc
  ) {
    resource_pool::ResourceHandle pool_handle = EmplaceShaderModuleResourcePool(
      resource_storage_->shader_modules,
      *vk_device_,
      desc
    );

    if (pool_handle.IsNull()) {
      return mnexus::ShaderModuleHandle::Invalid();
    }

    return mnexus::ShaderModuleHandle { pool_handle.AsU64() };
  }

  IMPL_VAPI(void, DestroyShaderModule,
    mnexus::ShaderModuleHandle shader_module_handle
  ) {
    auto pool_handle = resource_pool::ResourceHandle::FromU64(shader_module_handle.Get());
    resource_storage_->shader_modules.Erase(pool_handle);
  }

  // ----------------------------------------------------------------------------------------------
  // Program
  //

  IMPL_VAPI(mnexus::ProgramHandle, CreateProgram,
    mnexus::ProgramDesc const& desc
  ) {
    resource_pool::ResourceHandle const pool_handle = EmplaceProgramResourcePool(
      resource_storage_->programs,
      *vk_device_,
      desc,
      resource_storage_->shader_modules,
      resource_storage_->pipeline_layout_cache
    );

    if (pool_handle.IsNull()) {
      return mnexus::ProgramHandle::Invalid();
    }

    return mnexus::ProgramHandle { pool_handle.AsU64() };
  }

  IMPL_VAPI(void, DestroyProgram,
    mnexus::ProgramHandle program_handle
  ) {
    // FIXME: Should defer destruction until the GPU is done using this program.
    auto pool_handle = resource_pool::ResourceHandle::FromU64(program_handle.Get());
    resource_storage_->programs.Erase(pool_handle);
  }

  // ----------------------------------------------------------------------------------------------
  // ComputePipeline
  //

  IMPL_VAPI(mnexus::ComputePipelineHandle, CreateComputePipeline,
    mnexus::ComputePipelineDesc const& desc
  ) {
    resource_pool::ResourceHandle const pool_handle = EmplaceComputePipelineResourcePool(
      resource_storage_->compute_pipelines,
      *vk_device_,
      desc.program,
      resource_storage_->programs,
      resource_storage_->shader_modules
    );

    if (pool_handle.IsNull()) {
      return mnexus::ComputePipelineHandle::Invalid();
    }

    return mnexus::ComputePipelineHandle{ pool_handle.AsU64() };
  }

  IMPL_VAPI(void, DestroyComputePipeline,
    mnexus::ComputePipelineHandle compute_pipeline_handle
  ) {
    // FIXME: Should defer destruction until the GPU is done using this program.
    auto pool_handle = resource_pool::ResourceHandle::FromU64(compute_pipeline_handle.Get());
    resource_storage_->compute_pipelines.Erase(pool_handle);
  }

  // ----------------------------------------------------------------------------------------------
  // RenderPipeline
  //

  IMPL_VAPI(mnexus::RenderPipelineHandle, CreateRenderPipeline,
    mnexus::RenderPipelineDesc const& /*desc*/
  ) {
    STUB_NOT_IMPLEMENTED();
    return mnexus::RenderPipelineHandle::Invalid();
  }

  // ----------------------------------------------------------------------------------------------
  // Device Capability
  //

  IMPL_VAPI(mnexus::AdapterCapability, GetAdapterCapability) {
    return mnexus::AdapterCapability {
      .vertex_shader_storage_write = MnBoolTrue, // Vulkan guarantees this.
      .polygon_mode_line = MnBoolTrue,
      .polygon_mode_point = MnBoolTrue,
      .buffer_mappable = MnBoolTrue,
    };
  }

  IMPL_VAPI(mnexus::ClipSpaceConvention, GetClipSpaceConvention) {
    return mnexus::ClipSpaceConvention {
      .y_direction = mnexus::ClipSpaceYDirection::kDown,
      .depth_range = mnexus::ClipSpaceDepthRange::kZeroToOne,
    };
  }

  IMPL_VAPI(void, GetQueueSelection, mnexus::QueueSelection& out) {
    out = vk_device_->queue_selection();
  }

  IMPL_VAPI(void, GetAdapterInfo, mnexus::AdapterInfo& out_info) {
    STUB_NOT_IMPLEMENTED();
    out_info = {};
  }

  // ----------------------------------------------------------------------------------------------
  // Video coding
  //

  IMPL_VAPI(MnBool32, QueryVideoDecodeH265Capabilities,
    mnexus::VideoH265Profile profile,
    mnexus::VideoBitDepth    bit_depth,
    mnexus::VideoDecodeH265Capabilities& out_caps
  ) {
#if MNEXUS_ENABLE_VIDEO_CODING
    auto const& opt_caps = vk_device_->physical_device_desc().video_coding_capabilities();
    if (!opt_caps.has_value()) {
      return MnBoolFalse;
    }

    VideoDecodeH265Properties const* slot = SelectDecodeH265Slot(opt_caps->decode_h265, profile, bit_depth);
    if (slot == nullptr) {
      return MnBoolFalse;
    }

    out_caps = ToPublicVideoDecodeH265Capabilities(*slot);
    return MnBoolTrue;
#else
    (void)profile; (void)bit_depth; (void)out_caps;
    MBASE_LOG_ERROR("QueryVideoDecodeH265Capabilities called but mnexus was built without MNEXUS_ENABLE_VIDEO_CODING");
    return MnBoolFalse;
#endif
  }

  IMPL_VAPI(mnexus::VideoSessionHandle, CreateVideoSessionDecodeH265,
    mnexus::VideoSessionDecodeH265Desc const& desc
  ) {
#if MNEXUS_ENABLE_VIDEO_CODING
    resource_pool::ResourceHandle const pool_handle = EmplaceVideoSessionResourcePoolDecodeH265(
      resource_storage_->video_sessions,
      *vk_device_,
      desc
    );
    if (pool_handle.IsNull()) {
      return mnexus::VideoSessionHandle::Invalid();
    }
    return mnexus::VideoSessionHandle { pool_handle.AsU64() };
#else
    (void)desc;
    MBASE_LOG_ERROR("CreateVideoSessionDecodeH265 called but mnexus was built without MNEXUS_ENABLE_VIDEO_CODING");
    return mnexus::VideoSessionHandle::Invalid();
#endif
  }

  IMPL_VAPI(void, DestroyVideoSession, mnexus::VideoSessionHandle session) {
#if MNEXUS_ENABLE_VIDEO_CODING
    // FIXME: Should defer destruction until the GPU is done using this session
    // (matches the existing FIXME in DestroyBuffer / DestroyTexture).
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(session.Get());
    resource_storage_->video_sessions.Erase(pool_handle);
#else
    (void)session;
#endif
  }

  IMPL_VAPI(mnexus::VideoSessionParametersHandle, CreateVideoSessionParametersDecodeH265,
    mnexus::VideoSessionParametersDecodeH265Desc const& desc
  ) {
#if MNEXUS_ENABLE_VIDEO_CODING
    resource_pool::ResourceHandle const pool_handle = EmplaceVideoSessionParametersResourcePoolDecodeH265(
      resource_storage_->video_session_parameters,
      *vk_device_,
      resource_storage_->video_sessions,
      desc
    );
    if (pool_handle.IsNull()) {
      return mnexus::VideoSessionParametersHandle::Invalid();
    }
    return mnexus::VideoSessionParametersHandle { pool_handle.AsU64() };
#else
    (void)desc;
    MBASE_LOG_ERROR("CreateVideoSessionParametersDecodeH265 called but mnexus was built without MNEXUS_ENABLE_VIDEO_CODING");
    return mnexus::VideoSessionParametersHandle::Invalid();
#endif
  }

  IMPL_VAPI(void, DestroyVideoSessionParameters, mnexus::VideoSessionParametersHandle params) {
#if MNEXUS_ENABLE_VIDEO_CODING
    // FIXME: Should defer destruction until the GPU is done using these
    // parameters (matches the existing FIXME on Destroy{Buffer,Texture,VideoSession}).
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(params.Get());
    resource_storage_->video_session_parameters.Erase(pool_handle);
#else
    (void)params;
#endif
  }

  // ----------------------------------------------------------------------------------------------
  // Timestamp queries
  //

  IMPL_VAPI(mnexus::QueryPoolHandle, CreateTimestampQueryPool, uint32_t query_count) {
    resource_pool::ResourceHandle const pool_handle = EmplaceTimestampQueryPool(
      resource_storage_->query_pools, *vk_device_, query_count);
    if (pool_handle.IsNull()) {
      return mnexus::QueryPoolHandle::Invalid();
    }
    return mnexus::QueryPoolHandle { pool_handle.AsU64() };
  }

  IMPL_VAPI(void, DestroyQueryPool, mnexus::QueryPoolHandle pool) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(pool.Get());
    if (pool_handle.IsNull()) return;
    {
      auto [hot, cold, lock] =
        resource_storage_->query_pools.GetConstRefWithSharedLockGuard(pool_handle);
      if (hot.vk_query_pool != VK_NULL_HANDLE) {
        // FIXME: defer destruction in case the pool is still being
        // touched by an in-flight CL (rare for non-blocking timestamp
        // reads, but the same issue exists for buffers / textures and
        // is tracked by the FIXME on Destroy{Buffer,Texture}).
        vkDestroyQueryPool(vk_device_->handle(), hot.vk_query_pool, nullptr);
      }
    }
    resource_storage_->query_pools.Erase(pool_handle);
  }

  IMPL_VAPI(uint32_t, GetTimestampQueryResults,
    mnexus::QueryPoolHandle pool,
    uint32_t                first_query,
    uint32_t                count,
    uint64_t*               out_timestamps_ns
  ) {
    if (out_timestamps_ns == nullptr || count == 0) return 0;

    auto const pool_handle = resource_pool::ResourceHandle::FromU64(pool.Get());
    if (pool_handle.IsNull()) return 0;

    auto [hot, cold, lock] =
      resource_storage_->query_pools.GetConstRefWithSharedLockGuard(pool_handle);
    if (hot.vk_query_pool == VK_NULL_HANDLE || cold.query_count == 0) return 0;
    if (first_query + count > cold.query_count) {
      // Clamp range to the pool size.
      count = (first_query >= cold.query_count) ? 0u : (cold.query_count - first_query);
      if (count == 0) return 0;
    }

    // Pull (timestamp_u64, availability_u64) per slot, non-blocking.
    // Only entries whose `availability != 0` get converted into
    // `out_timestamps_ns`. Untouched entries are NOT cleared (it's
    // the caller's job to track which slots they've consumed).
    struct TimestampPair {
      uint64_t timestamp;
      uint64_t availability;
    };
    std::vector<TimestampPair> raw(count);
    VkResult const r = vkGetQueryPoolResults(
      vk_device_->handle(),
      hot.vk_query_pool,
      first_query, count,
      sizeof(TimestampPair) * count,
      raw.data(),
      sizeof(TimestampPair),
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
    );
    // VK_NOT_READY is expected and normal for non-blocking polling --
    // it just means "not all queries are ready", and per-slot availability
    // tells us which ones are. VK_SUCCESS means all queries are ready.
    if (r != VK_SUCCESS && r != VK_NOT_READY) {
      MBASE_LOG_ERROR("vkGetQueryPoolResults (timestamp) failed: {}", string_VkResult(r));
      return 0;
    }

    uint32_t completed = 0;
    for (uint32_t i = 0; i < count; ++i) {
      if (raw[i].availability != 0) {
        out_timestamps_ns[i] = static_cast<uint64_t>(
          static_cast<double>(raw[i].timestamp) * static_cast<double>(cold.timestamp_period_ns));
        ++completed;
      }
    }
    return completed;
  }

  // ----------------------------------------------------------------------------------------------
  // Diagnostics
  //

  IMPL_VAPI(mnexus::RenderPipelineCacheSnapshot, GetRenderPipelineCacheSnapshot) {
    mnexus::RenderPipelineCacheSnapshot snapshot;

    auto diag = resource_storage_->render_pipeline_cache.GetDiagnostics();
    snapshot.diagnostics.total_lookups = diag.total_lookups;
    snapshot.diagnostics.cache_hits = diag.cache_hits;
    snapshot.diagnostics.cache_misses = diag.cache_misses;
    snapshot.diagnostics.cached_pipeline_count = diag.cached_pipeline_count;

    resource_storage_->render_pipeline_cache.ForEachEntry(
      [&snapshot](pipeline::RenderPipelineCacheKey const& key) {
        snapshot.entries.push_back({
          .hash = key.ComputeHash(),
          .state = pipeline::RenderPipelineStateTracker::SnapshotFromCacheKey(key),
        });
      }
    );

    return snapshot;
  }

  // ----------------------------------------------------------------------------------------------
  // Local

  void OnSurfaceDestroyed() {
    vkDeviceWaitIdle(vk_device_->handle());

    wsi_swapchain_.OnSourceDestroyed();
  }

  void OnSurfaceRecreated(mnexus::SurfaceSourceDesc const& surface_source_desc,
                          std::optional<VkSurfaceFormatKHR> opt_desired_surface_format) {
    wsi_swapchain_.OnSourceCreated(surface_source_desc, opt_desired_surface_format);
  }

  bool RecreateSwapchain(std::optional<VkSurfaceFormatKHR> opt_desired_surface_format) {
    return wsi_swapchain_.RecreateSwapchain(opt_desired_surface_format);
  }

  mnexus::SurfaceCapability QuerySurfaceCapability() const {
    return wsi_swapchain_.QuerySurfaceCapability();
  }

  mnexus::ColorSpace GetSwapchainColorSpace() const {
    return wsi_swapchain_.GetCurrentColorSpace();
  }

  mnexus::Format GetSwapchainFormat() const {
    return wsi_swapchain_.GetTextureDesc().format;
  }

  std::optional<uint32_t> AcquireNextSwapchainTexture(
    VkFence nullable_signal_fence
  ) {
    auto opt_acquired = wsi_swapchain_.AcquireNextImage(
      std::numeric_limits<uint64_t>::max(),
      VK_NULL_HANDLE, // signal semaphore
      nullable_signal_fence
    );
    if (!opt_acquired.has_value()) {
      return std::nullopt;
    }

    auto [image_index, swapchain_image] = opt_acquired.value();
    return image_index;
  }

  bool QueueSwapchainTexturePresent(mnexus::QueueId const& queue_id, uint64_t wait_serial) {
    auto opt_last_acquired = wsi_swapchain_.GetLastAcquiredImage();
    if (!opt_last_acquired.has_value()) {
      MBASE_LOG_ERROR("No swapchain image has been acquired for presentation.");
      return false;
    }
    auto [image_index, swapchain_image] = opt_last_acquired.value();
    
    vk_device_->GetQueue(queue_id)->PresentSwapchainImage(wait_serial, swapchain_image->present_binary_semaphore, wsi_swapchain_.GetVkSwapchainHandle(), image_index);
    wsi_swapchain_.ReturnImage(image_index);

    return true;
  }

private:
  struct PendingReadback {
    void* dst;
    uint32_t size_in_bytes;
    StagingBuffer* staging;
    mnexus::QueueId queue_id;
    uint64_t serial;
  };

  void ProcessPendingReadbacks() {
    mbase::LockGuard lock(pending_readbacks_mutex_);

    for (uint32_t i = 0; i < pending_readbacks_.size();) {
      PendingReadback& rb = pending_readbacks_[i];
      uint64_t const completed = vk_device_->GetQueue(rb.queue_id)->GetCompletedValue();
      if (completed >= rb.serial) {
        vmaInvalidateAllocation(vk_device_->vma_allocator(), rb.staging->allocation, 0, rb.size_in_bytes);
        std::memcpy(rb.dst, rb.staging->mapped_data, rb.size_in_bytes);
        staging_buffer_pool_.Release(rb.staging, rb.queue_id, rb.serial);
        pending_readbacks_.erase(pending_readbacks_.begin() + static_cast<ptrdiff_t>(i));
      } else {
        ++i;
      }
    }
  }

  IVulkanDevice* vk_device_ = nullptr;
  StagingBufferPool& staging_buffer_pool_;
  TransientCommandPool& transient_command_pool_;
  WsiSwapchain wsi_swapchain_;
  ResourceStorage* resource_storage_ = nullptr;
  IDescriptorSetAllocator* descriptor_set_allocator_ = nullptr;
  std::vector<PendingReadback> pending_readbacks_;
  mbase::Lockable<std::mutex> pending_readbacks_mutex_;
};

// ==================================================================================================
// BackendVulkan
//

class BackendVulkan final : public IBackendVulkan {
public:
  explicit BackendVulkan(std::unique_ptr<IVulkanDevice> vk_device) :
    vk_device_(std::move(vk_device)),
    device_(
      vk_device_.get(),
      staging_buffer_pool_,
      transient_command_pool_,
      &resource_storage_
    )
  {
    staging_buffer_pool_.Initialize(vk_device_.get());
    transient_command_pool_.Initialize(
      vk_device_.get(),
      vk_device_->queue_selection().present_capable.queue_family_index
    );
  }
  ~BackendVulkan() override = default;
  MBASE_DISALLOW_COPY_MOVE(BackendVulkan);

  // ----------------------------------------------------------------------------------------------
  // Surface lifecycle.

  void OnDisplayChanged() override {
    // The current monitor's HDR availability and surface format list may
    // have changed (e.g. window dragged to a different display, OS HDR
    // toggled). We can't safely re-realize mid-frame, so just flag a
    // recreation to be picked up at the next OnPresentPrologue. The
    // recreation re-runs format selection against the now-updated
    // surface formats list, so a still-applicable kHdr request is
    // honored on the new monitor (if it supports HDR), and falls back
    // to SDR otherwise.
    pending_swapchain_recreation_ = true;
  }

  void OnSurfaceDestroyed() override {
    device_.OnSurfaceDestroyed();
  }

  void OnSurfaceRecreated(mnexus::SurfaceSourceDesc const& surface_source_desc) override {
    device_.OnSurfaceRecreated(surface_source_desc, DesiredSurfaceFormat());
    // The new swapchain already reflects the current request, so clear
    // any previously-pending recreation flag.
    pending_swapchain_recreation_ = false;
  }

  // ----------------------------------------------------------------------------------------------
  // Surface / Swapchain Capability

  mnexus::SurfaceCapability GetSurfaceCapability() override {
    return device_.QuerySurfaceCapability();
  }

  void RequestSwapchainRecreation(mnexus::SwapchainRecreateDesc const& desc) override {
    requested_swapchain_flags_      = desc.flags;
    pending_swapchain_recreation_   = true;
  }

  mnexus::ColorSpace GetSwapchainSurfaceColorSpace() override {
    return device_.GetSwapchainColorSpace();
  }

  mnexus::Format GetSwapchainSurfaceFormat() override {
    return device_.GetSwapchainFormat();
  }

  // ----------------------------------------------------------------------------------------------
  // Presentation.

  void OnPresentPrologue() override {
    if (pending_swapchain_recreation_) {
      device_.RecreateSwapchain(DesiredSurfaceFormat());
      pending_swapchain_recreation_ = false;
    }

    VkFence signal_fence = VK_NULL_HANDLE;
    {
      VkFenceCreateInfo fence_info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
      };
      vkCreateFence(vk_device_->handle(), &fence_info, nullptr, &signal_fence);
    }

    std::optional<uint32_t> opt_acquired_index = device_.AcquireNextSwapchainTexture(signal_fence);
    if (!opt_acquired_index.has_value()) {
      MBASE_LOG_ERROR("Failed to acquire next swapchain image for presentation.");
      vkDestroyFence(vk_device_->handle(), signal_fence, nullptr);
      return;
    }

    mnexus::QueueId const queue_id = vk_device_->queue_selection().present_capable;

    //
    // Every `ICommandList` MUST see the swapchain texture as being in the "default" state as its `ImageLayoutTracker` expects.
    // (They also have to return it into the default state at the end)
    // We submit a commmand buffer that transitions the image to the default layout.
    //
    {
      auto [hot, cold, lock] = resource_storage_.textures.GetConstRefWithSharedLockGuard(resource_storage_.swapchain_texture_handle);

      VkImageLayout default_layout = VK_IMAGE_LAYOUT_UNDEFINED;
      cold.GetDefaultState(default_layout);
      VkImageAspectFlags const aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;

      VkImageMemoryBarrier2KHR barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
        .pNext = nullptr,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
        .dstAccessMask = 0, // We'll use a release barrier in the command list to make the image available, so no dst access flags needed here.
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = default_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = hot.GetVkImage().handle(),
        .subresourceRange = VkImageSubresourceRange {
          .aspectMask = aspect_mask,
          .baseMipLevel = 0,
          .levelCount = VK_REMAINING_MIP_LEVELS,
          .baseArrayLayer = 0,
          .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
      };

      VkDependencyInfoKHR dependency_info {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
      };

      VkCommandBuffer vk_cb_handle = transient_command_pool_.Acquire();
      vkCmdPipelineBarrier2KHR(vk_cb_handle, &dependency_info);
      vkEndCommandBuffer(vk_cb_handle);

      uint64_t const serial = vk_device_->GetQueue(queue_id)->SubmitSingle(vk_cb_handle);
      transient_command_pool_.Release(vk_cb_handle, queue_id, serial);
    }

    // Wait for the swapchain image acquire to complete.
    vkWaitForFences(vk_device_->handle(), 1, &signal_fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(vk_device_->handle(), signal_fence, nullptr);
  }

  void OnPresentEpilogue() override {
    mnexus::QueueId const queue_id = vk_device_->queue_selection().present_capable;

    //
    // Every` ICommandList` that have touched the swapchain texture MUST have transitioned it back to the default layout at the end, so we can assume it's in the default layout here.
    //
    uint64_t serial = 0;
    {
      auto [hot, cold, lock] = resource_storage_.textures.GetConstRefWithSharedLockGuard(resource_storage_.swapchain_texture_handle);

      VkImageLayout default_layout = VK_IMAGE_LAYOUT_UNDEFINED;
      cold.GetDefaultState(default_layout);
      VkImageAspectFlags const aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;

      VkImageMemoryBarrier2KHR barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
        .pNext = nullptr,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
        .oldLayout = default_layout,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = hot.GetVkImage().handle(),
        .subresourceRange = VkImageSubresourceRange {
          .aspectMask = aspect_mask,
          .baseMipLevel = 0,
          .levelCount = VK_REMAINING_MIP_LEVELS,
          .baseArrayLayer = 0,
          .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
      };

      VkDependencyInfoKHR dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
      };

      VkCommandBuffer vk_cb_handle = transient_command_pool_.Acquire();
      vkCmdPipelineBarrier2KHR(vk_cb_handle, &dependency_info);
      vkEndCommandBuffer(vk_cb_handle);

      serial = vk_device_->GetQueue(queue_id)->SubmitSingle(vk_cb_handle);
      transient_command_pool_.Release(vk_cb_handle, queue_id, serial);
    }

    device_.QueueSwapchainTexturePresent(queue_id, serial);
  }

  // ----------------------------------------------------------------------------------------------
  // Device.

  mnexus::IDevice* GetDevice() override {
    return &device_;
  }

  // ----------------------------------------------------------------------------------------------
  // Debug UI

  void ShowDebugUi() override {
#if MNEXUS_HAVE_DEAR_IMGUI
    if (ImGui::CollapsingHeader("Surface / Swapchain")) {
      mnexus::Format     const cur_format = device_.GetSwapchainFormat();
      mnexus::ColorSpace const cur_cs     = device_.GetSwapchainColorSpace();
      auto const fmt_str = mnexus::ToString(static_cast<MnFormat>(cur_format));
      auto const cs_str  = mnexus::ToString(cur_cs);
      ImGui::Text("Format: %.*s",      static_cast<int>(fmt_str.size()), fmt_str.data());
      ImGui::Text("Color space: %.*s", static_cast<int>(cs_str.size()),  cs_str.data());

      mnexus::SurfaceCapability const cap = device_.QuerySurfaceCapability();
      bool const hdr_supported = (cap.GetHdr10ColorFormat() != nullptr);
      ImGui::Text("HDR10 ST.2084 supported on this monitor: %s",
                  hdr_supported ? "yes" : "no");

      bool requested_hdr =
        requested_swapchain_flags_.HasAnyOf(mnexus::SwapchainCreateFlagBits::kHdr);
      bool const previous = requested_hdr;
      ImGui::BeginDisabled(!hdr_supported && !requested_hdr);
      if (ImGui::Checkbox("Request HDR10", &requested_hdr) && requested_hdr != previous) {
        mnexus::SwapchainRecreateDesc desc;
        desc.flags = requested_hdr
          ? mnexus::SwapchainCreateFlags(mnexus::SwapchainCreateFlagBits::kHdr)
          : mnexus::SwapchainCreateFlags(mnexus::SwapchainCreateFlagBits::kNone);
        RequestSwapchainRecreation(desc);
      }
      ImGui::EndDisabled();
    }
    if (ImGui::CollapsingHeader("Physical Device")) {
      vk_device_->physical_device_desc().ShowDebugUi();
    }
    if (ImGui::CollapsingHeader("Logical Device")) {
      vk_device_->ShowDebugUi();
    }
    if (ImGui::CollapsingHeader("Resource Storage")) {
      resource_storage_.ShowDebugUi();
    }
#endif
  }

  // ----------------------------------------------------------------------------------------------
  // Local.

  void Shutdown() {
    transient_command_pool_.Shutdown();
    staging_buffer_pool_.Shutdown();
    vk_device_->Shutdown();
  }

private:
  /// Translate `requested_swapchain_flags_` into a Vulkan desired surface
  /// format. Returns `nullopt` for "no preference" (= SDR fallback path
  /// in `Select`). For `kHdr`, returns `(A2B10G10R10_UNORM_PACK32,
  /// HDR10_ST2084_EXT)`; the surface-format Selector probes this as an
  /// exact match and silently falls back to the default sRGB pick if
  /// the current monitor doesn't expose it.
  std::optional<VkSurfaceFormatKHR> DesiredSurfaceFormat() const {
    if (requested_swapchain_flags_.HasAnyOf(mnexus::SwapchainCreateFlagBits::kHdr)) {
      return VkSurfaceFormatKHR {
        .format     = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        .colorSpace = VK_COLOR_SPACE_HDR10_ST2084_EXT,
      };
    }
    return std::nullopt;
  }

  std::unique_ptr<IVulkanDevice> vk_device_;

  ResourceStorage resource_storage_;
  StagingBufferPool staging_buffer_pool_;
  TransientCommandPool transient_command_pool_;
  MnexusDeviceVulkan device_;

  /// Currently-active app request (e.g. Dear ImGui's "Request HDR10"
  /// checkbox). Persisted across monitor changes so a still-applicable
  /// HDR request is reattempted when the window moves to an
  /// HDR-capable display.
  mnexus::SwapchainCreateFlags requested_swapchain_flags_  = mnexus::SwapchainCreateFlagBits::kNone;
  /// Set by `RequestSwapchainRecreation` and `OnDisplayChanged`. Picked
  /// up at the start of the next `OnPresentPrologue`, which runs the
  /// Vulkan-side `RecreateSwapchain` (which does its own
  /// `vkDeviceWaitIdle`).
  bool                          pending_swapchain_recreation_ = false;
};

// ==================================================================================================
// Factory
//

std::unique_ptr<IBackendVulkan> IBackendVulkan::Create(BackendVulkanCreateDesc const& desc) {
  MBASE_LOG_WARN("Vulkan backend: creating stub backend");

  constexpr uint32_t kVulkanApiVersion = VK_API_VERSION_1_1;

  VulkanInstance::InitializeVolk();

  VulkanInstance instance;
  instance.CheckCapabilities();

  std::vector<std::string> instance_extensions;
  if (!desc.headless) {
    instance_extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if MBASE_PLATFORM_WINDOWS
    instance_extensions.emplace_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif MBASE_PLATFORM_ANDROID
    instance_extensions.emplace_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#else
# error "Unsupported platform"
#endif
  }

  {
    auto AddExtensionIfAvailable = [&](std::string_view extension_name) {
      if (instance.QueryExtensionSupport(extension_name) != nullptr) {
        instance_extensions.emplace_back(extension_name);
      } else {
        MBASE_LOG_WARN("Vulkan instance extension '{}' is not available", extension_name);
      }
    };

    AddExtensionIfAvailable(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    if (!desc.headless) {
      // Required to enumerate / select non-sRGB swapchain color spaces
      // (HDR10 ST.2084, scRGB, etc). Without it `vkGetPhysicalDevice
      // SurfaceFormatsKHR` only reports the legacy SRGB_NONLINEAR_KHR
      // entry and `vkCreateSwapchainKHR` rejects every other space.
      AddExtensionIfAvailable(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
    }
  }

  mbase::ArrayProxy<VkValidationFeatureEnableEXT const> enabled_validation_features;
  mbase::ArrayProxy<VkValidationFeatureDisableEXT const> disabled_validation_features;

  bool result = instance.Initialize(
    desc.app_name,
    kVulkanApiVersion,
    {},
    instance_extensions,
    enabled_validation_features,
    disabled_validation_features
  );
  if (!result) {
    MBASE_LOG_ERROR("Failed to create Vulkan instance.");
    VulkanInstance::ShutdownVolk();
    return nullptr;
  }

  std::vector<std::string> mandatory_device_extensions;
  if (!desc.headless) {
    mandatory_device_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }

  std::optional<PhysicalDeviceDesc> opt_physical_device_desc = SelectPhysicalDevice(
    instance,
    mandatory_device_extensions,
    mbase::ArrayProxy<char const* const>{}
  );
  if (!opt_physical_device_desc.has_value()) {
    MBASE_LOG_ERROR("Failed to select a physical device for Vulkan backend.");
    return nullptr;
  }
  PhysicalDeviceDesc physical_device_desc = std::move(opt_physical_device_desc.value());

#if 0
  std::vector<std::string> device_extensions;
  {
    auto AddExtensionIfAvailable = [&](std::string_view extension_name) {
      if (physical_device_desc.QueryExtensionSupport(extension_name) != nullptr) {
        device_extensions.emplace_back(std::string(extension_name));
      } else {
        MBASE_LOG_WARN("Vulkan device extension '{}' is not available", extension_name);
      }
    };
    AddExtensionIfAvailable(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    AddExtensionIfAvailable(VK_KHR_MULTIVIEW_EXTENSION_NAME);
    AddExtensionIfAvailable(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
    AddExtensionIfAvailable(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME);
    AddExtensionIfAvailable(VK_KHR_16BIT_STORAGE_EXTENSION_NAME);
    AddExtensionIfAvailable(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    AddExtensionIfAvailable(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    AddExtensionIfAvailable(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    AddExtensionIfAvailable(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    AddExtensionIfAvailable(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    AddExtensionIfAvailable(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    AddExtensionIfAvailable(VK_KHR_RAY_QUERY_EXTENSION_NAME);
  }
#endif

  VulkanDeviceDesc device_desc {
    .physical_device_desc = &physical_device_desc,
    .headless = desc.headless,
  };

  std::unique_ptr<IVulkanDevice> vk_device = IVulkanDevice::Create(
    std::move(instance),
    device_desc
  );
  if (!vk_device) {
    MBASE_LOG_ERROR("Failed to create Vulkan device.");
    return nullptr;
  }

  volkLoadDevice(vk_device->handle());

  return std::make_unique<BackendVulkan>(std::move(vk_device));
}

} // namespace mnexus_backend::vulkan
