// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan.h"

// c++ headers ------------------------------------------
#include <cstring>

#include <vector>
#include <optional>

// public project headers -------------------------------
#include "mbase/public/log.h"
#include "mbase/public/trap.h"
#include "mbase/public/tsa.h"

// project headers --------------------------------------
#include "resource_pool/generational_pool.h"

#include "impl/impl_macros.h"

#include "backend-vulkan/backend-vulkan-command_list.h"
#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/backend-vulkan-texture.h"
#include "backend-vulkan/backend-vulkan-compute_pipeline.h"
#include "backend-vulkan/descriptor_set_allocator.h"

#include "backend-vulkan/vk-device.h"
#include "backend-vulkan/vk-instance.h"
#include "backend-vulkan/vk-physical_device.h"
#include "backend-vulkan/vk-staging.h"
#include "backend-vulkan/vk-wsi_surface.h"
#include "backend-vulkan/thread_command_pool.h"
#include "backend-vulkan/depend/vulkan_vma.h"
#include "backend-vulkan/resource_storage.h"

namespace mnexus_backend::vulkan {


// ==================================================================================================
// MnexusDeviceVulkan
//

class MnexusDeviceVulkan final : public mnexus::IDevice {
public:
  explicit MnexusDeviceVulkan(IVulkanDevice* vk_device, ResourceStorage* resource_storage) :
    vk_device_(vk_device),
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
    auto* cmd_list_vk = static_cast<MnexusCommandListVulkan*>(command_list);
    VkCommandBuffer vk_cb_handle = cmd_list_vk->encoder().command_buffer();

    uint64_t const serial = vk_device_->QueueSubmitSingle(queue_id, vk_cb_handle);
    vk_device_->thread_command_pool_registry().FreeCommandBuffer(vk_cb_handle, queue_id, serial);

    uint32_t const queue_compact_index = *vk_device_->queue_index_map().Find(queue_id);

    mbase::ArrayProxy<resource_pool::ResourceHandle const> referenced = cmd_list_vk->GetReferencedResources();
    for (resource_pool::ResourceHandle const& handle : referenced) {
      resource_storage_->StampResourceUse(handle, queue_compact_index, serial);
    }

    delete cmd_list_vk;
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

    if (hot.mapped_data != nullptr) {
      // Mappable buffer: direct memcpy + flush.
      std::memcpy(static_cast<uint8_t*>(hot.mapped_data) + buffer_offset, data, data_size_in_bytes);
      vmaFlushAllocation(hot.vma_allocator, hot.vma_allocation, buffer_offset, data_size_in_bytes);

      // No actual queue submit needed; data is visible after flush.
      // Advance timeline to satisfy the API contract.
      uint64_t const serial = vk_device_->QueueAdvanceTimeline(queue_id);
      return mnexus::IntraQueueSubmissionId { serial };
    }

    // Non-mappable buffer: staging path.
    StagingBuffer* staging = vk_device_->staging_buffer_pool().Acquire(data_size_in_bytes);
    if (staging == nullptr) {
      MBASE_LOG_ERROR("Failed to acquire staging buffer for QueueWriteBuffer");
      return mnexus::IntraQueueSubmissionId { 0 };
    }

    std::memcpy(staging->mapped_data, data, data_size_in_bytes);
    vmaFlushAllocation(vk_device_->vma_allocator(), staging->allocation, 0, data_size_in_bytes);

    VkCommandBuffer vk_cb_handle = vk_device_->transient_command_pool().Acquire();
    VkBufferCopy region {
      .srcOffset = 0,
      .dstOffset = buffer_offset,
      .size = data_size_in_bytes,
    };
    vkCmdCopyBuffer(vk_cb_handle, staging->vk_buffer, hot.vk_buffer.handle(), 1, &region);
    vkEndCommandBuffer(vk_cb_handle);

    uint64_t const serial = vk_device_->QueueSubmitSingle(queue_id, vk_cb_handle);

    uint32_t const queue_compact_index = *vk_device_->queue_index_map().Find(queue_id);
    hot.vk_buffer.sync_stamp().Stamp(queue_compact_index, serial);

    vk_device_->transient_command_pool().Release(vk_cb_handle, queue_id, serial);
    vk_device_->staging_buffer_pool().Release(staging, queue_id, serial);

    return mnexus::IntraQueueSubmissionId { serial };
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

    if (hot.mapped_data != nullptr) {
      // Mappable buffer: direct read after invalidate.
      vmaInvalidateAllocation(hot.vma_allocator, hot.vma_allocation, buffer_offset, size_in_bytes);
      std::memcpy(dst, static_cast<uint8_t const*>(hot.mapped_data) + buffer_offset, size_in_bytes);
      uint64_t const serial = vk_device_->QueueAdvanceTimeline(queue_id);
      return mnexus::IntraQueueSubmissionId { serial };
    }

    // Non-mappable buffer: staging + deferred copy.
    StagingBuffer* staging = vk_device_->staging_buffer_pool().Acquire(size_in_bytes);
    if (staging == nullptr) {
      MBASE_LOG_ERROR("Failed to acquire staging buffer for QueueReadBuffer");
      return mnexus::IntraQueueSubmissionId { 0 };
    }

    VkCommandBuffer vk_cb_handle = vk_device_->transient_command_pool().Acquire();
    VkBufferCopy region {
      .srcOffset = buffer_offset,
      .dstOffset = 0,
      .size = size_in_bytes,
    };
    vkCmdCopyBuffer(vk_cb_handle, hot.vk_buffer.handle(), staging->vk_buffer, 1, &region);
    vkEndCommandBuffer(vk_cb_handle);

    uint64_t const serial = vk_device_->QueueSubmitSingle(queue_id, vk_cb_handle);

    vk_device_->transient_command_pool().Release(vk_cb_handle, queue_id, serial);

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
    return mnexus::IntraQueueSubmissionId { vk_device_->QueueGetCompletedValue(queue_id) };
  }

  IMPL_VAPI(void, QueueWaitIdle,
    mnexus::QueueId const& queue_id,
    mnexus::IntraQueueSubmissionId value
  ) {
    vk_device_->QueueWaitSubmitSerial(queue_id, value.Get());
    this->ProcessPendingReadbacks();
  }

  // ----------------------------------------------------------------------------------------------
  // Command List
  //

  IMPL_VAPI(mnexus::ICommandList*, CreateCommandList,
    mnexus::CommandListDesc const& /*desc*/
  ) {
    VkCommandBuffer vk_cb_handle = vk_device_->thread_command_pool_registry().AllocateCommandBuffer();
    return new MnexusCommandListVulkan(
      CommandEncoder(vk_cb_handle, vk_device_->handle(), descriptor_set_allocator_, resource_storage_),
      resource_storage_
    );
  }

  IMPL_VAPI(void, DiscardCommandList,
    mnexus::ICommandList* command_list
  ) {
    auto* cmd_list_vk = static_cast<MnexusCommandListVulkan*>(command_list);
    VkCommandBuffer vk_cb_handle = cmd_list_vk->encoder().command_buffer();
    vk_device_->thread_command_pool_registry().FreeCommandBuffer(vk_cb_handle, {}, 0);
    delete cmd_list_vk;
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

  IMPL_VAPI(void, GetAdapterInfo, mnexus::AdapterInfo& out_info) {
    STUB_NOT_IMPLEMENTED();
    out_info = {};
  }

  // ----------------------------------------------------------------------------------------------
  // Diagnostics
  //

  IMPL_VAPI(mnexus::RenderPipelineCacheSnapshot, GetRenderPipelineCacheSnapshot) {
    STUB_NOT_IMPLEMENTED();
    return {};
  }

  // ----------------------------------------------------------------------------------------------
  // Local

  void OnSurfaceDestroyed() {
    wsi_swapchain_.OnSourceDestroyed();
  }

  void OnSurfaceRecreated(mnexus::SurfaceSourceDesc const& surface_source_desc) {
    wsi_swapchain_.OnSourceCreated(surface_source_desc);
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

    auto [image_index, vk_image] = opt_acquired.value();
    return image_index;
  }

  bool QueueSwapchainTexturePresent(mnexus::QueueId const& queue_id, uint64_t wait_serial) {
    auto opt_last_acquired = wsi_swapchain_.GetLastAcquiredImage();
    if (!opt_last_acquired.has_value()) {
      MBASE_LOG_ERROR("No swapchain image has been acquired for presentation.");
      return false;
    }
    auto [image_index, vk_image] = opt_last_acquired.value();
    
    uint64_t const serial = vk_device_->QueuePresentSwapchainImage(queue_id, wait_serial, wsi_swapchain_.GetVkSwapchainHandle(), image_index);
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
      uint64_t const completed = vk_device_->QueueGetCompletedValue(rb.queue_id);
      if (completed >= rb.serial) {
        vmaInvalidateAllocation(vk_device_->vma_allocator(), rb.staging->allocation, 0, rb.size_in_bytes);
        std::memcpy(rb.dst, rb.staging->mapped_data, rb.size_in_bytes);
        vk_device_->staging_buffer_pool().Release(rb.staging, rb.queue_id, rb.serial);
        pending_readbacks_.erase(pending_readbacks_.begin() + static_cast<ptrdiff_t>(i));
      } else {
        ++i;
      }
    }
  }

  IVulkanDevice* vk_device_ = nullptr;
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
    device_(vk_device_.get(), &resource_storage_)
  {
    
  }
  ~BackendVulkan() override = default;
  MBASE_DISALLOW_COPY_MOVE(BackendVulkan);

  // ----------------------------------------------------------------------------------------------
  // Surface lifecycle.

  void OnDisplayChanged() override {
    STUB_NOT_IMPLEMENTED();
  }

  void OnSurfaceDestroyed() override {
    device_.OnSurfaceDestroyed();
  }

  void OnSurfaceRecreated(mnexus::SurfaceSourceDesc const& surface_source_desc) override {
    device_.OnSurfaceRecreated(surface_source_desc);
  }

  // ----------------------------------------------------------------------------------------------
  // Presentation.

  void OnPresentPrologue() override {
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

      VkCommandBuffer vk_cb_handle = vk_device_->transient_command_pool().Acquire();
      vkCmdPipelineBarrier2KHR(vk_cb_handle, &dependency_info);
      vkEndCommandBuffer(vk_cb_handle);

      uint64_t const serial = vk_device_->QueueSubmitSingle(queue_id, vk_cb_handle);
      vk_device_->transient_command_pool().Release(vk_cb_handle, queue_id, serial);
    }

    // Wait for the swapchain image acquire to complete.
    vkWaitForFences(vk_device_->handle(), 1, &signal_fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(vk_device_->handle(), signal_fence, nullptr);
  }

  void OnPresentEpilogue() override {
    mnexus::QueueId const queue_id = vk_device_->queue_selection().present_capable;
    uint32_t const queue_compact_index = *vk_device_->queue_index_map().Find(queue_id);

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

      VkCommandBuffer vk_cb_handle = vk_device_->transient_command_pool().Acquire();
      vkCmdPipelineBarrier2KHR(vk_cb_handle, &dependency_info);
      vkEndCommandBuffer(vk_cb_handle);

      serial = vk_device_->QueueSubmitSingle(queue_id, vk_cb_handle);
      vk_device_->transient_command_pool().Release(vk_cb_handle, queue_id, serial);
    }

    device_.QueueSwapchainTexturePresent(queue_id, serial);
  }

  // ----------------------------------------------------------------------------------------------
  // Device.

  mnexus::IDevice* GetDevice() override {
    return &device_;
  }

  // ----------------------------------------------------------------------------------------------
  // Local.

  void Shutdown() {
    vk_device_->Shutdown();
  }

private:
  std::unique_ptr<IVulkanDevice> vk_device_;

  ResourceStorage resource_storage_;
  MnexusDeviceVulkan device_;
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

  return std::make_unique<BackendVulkan>(std::move(vk_device));
}

} // namespace mnexus_backend::vulkan
