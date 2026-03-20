// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan.h"

// c++ headers ------------------------------------------
#include <cstring>

// public project headers -------------------------------
#include "mbase/public/log.h"
#include "mbase/public/trap.h"

// project headers --------------------------------------
#include "container/generational_pool.h"

#include "impl/impl_macros.h"

#include "backend-vulkan/backend-vulkan-command_list.h"
#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/backend-vulkan-compute_pipeline.h"

#include "backend-vulkan/vk-device.h"
#include "backend-vulkan/resource_storage.h"

namespace mnexus_backend::vulkan {

#define STUB_NOT_IMPLEMENTED() \
  do { MBASE_LOG_ERROR("Vulkan backend: {}() not implemented", __func__); mbase::Trap(); } while (0)

// ==================================================================================================
// MnexusDeviceVulkan
//

class MnexusDeviceVulkan final : public mnexus::IDevice {
public:
  explicit MnexusDeviceVulkan(VulkanDevice* vk_device, ResourceStorage* resource_storage) :
    vk_device_(vk_device),
    resource_storage_(resource_storage)
  {}
  ~MnexusDeviceVulkan() override = default;

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
    mnexus::QueueId const& /*queue_id*/,
    mnexus::ICommandList* command_list
  ) {
    STUB_NOT_IMPLEMENTED();
    delete command_list;
    return mnexus::IntraQueueSubmissionId{0};
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueWriteBuffer,
    mnexus::QueueId const& queue_id,
    mnexus::BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void const* data,
    uint32_t data_size_in_bytes
  ) {
    auto const pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);

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
      return mnexus::IntraQueueSubmissionId{0};
    }

    std::memcpy(staging->mapped_data, data, data_size_in_bytes);
    vmaFlushAllocation(vk_device_->vma_allocator(), staging->allocation, 0, data_size_in_bytes);

    VkCommandBuffer cmd = vk_device_->transient_command_pool().Acquire();
    VkBufferCopy region {
      .srcOffset = 0,
      .dstOffset = buffer_offset,
      .size = data_size_in_bytes,
    };
    vkCmdCopyBuffer(cmd, staging->vk_buffer, hot.vk_buffer.handle(), 1, &region);
    vkEndCommandBuffer(cmd);

    uint64_t const serial = vk_device_->QueueSubmitSingle(queue_id, cmd);

    vk_device_->transient_command_pool().Release(cmd, queue_id, serial);
    vk_device_->staging_buffer_pool().Release(staging, queue_id, serial);

    return mnexus::IntraQueueSubmissionId { serial };
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueReadBuffer,
    mnexus::QueueId const& /*queue_id*/,
    mnexus::BufferHandle /*buffer_handle*/,
    uint32_t /*buffer_offset*/,
    void* /*dst*/,
    uint32_t /*size_in_bytes*/
  ) {
    STUB_NOT_IMPLEMENTED();
    return mnexus::IntraQueueSubmissionId{0};
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
  }

  // ----------------------------------------------------------------------------------------------
  // Command List
  //

  IMPL_VAPI(mnexus::ICommandList*, CreateCommandList,
    mnexus::CommandListDesc const& /*desc*/
  ) {
    STUB_NOT_IMPLEMENTED();
    return new MnexusCommandListVulkan();
  }

  IMPL_VAPI(void, DiscardCommandList,
    mnexus::ICommandList* command_list
  ) {
    delete command_list;
  }

  // ----------------------------------------------------------------------------------------------
  // Buffer
  //

  IMPL_VAPI(mnexus::BufferHandle, CreateBuffer,
    mnexus::BufferDesc const& desc
  ) {
    container::ResourceHandle const pool_handle = EmplaceBufferResourcePool(
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
    auto const pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());
    resource_storage_->buffers.Erase(pool_handle);
  }

  IMPL_VAPI(void, GetBufferDesc,
    mnexus::BufferHandle buffer_handle,
    mnexus::BufferDesc& out_desc
  ) {
    auto const pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());
    auto [cold, lock] = resource_storage_->buffers.GetColdConstRefWithSharedLockGuard(pool_handle);
    out_desc = cold.desc;
  }

  // ----------------------------------------------------------------------------------------------
  // Texture
  //

  IMPL_VAPI(mnexus::TextureHandle, GetSwapchainTexture) {
    STUB_NOT_IMPLEMENTED();
    return mnexus::TextureHandle::Invalid();
  }

  IMPL_VAPI(mnexus::TextureHandle, CreateTexture,
    mnexus::TextureDesc const& /*desc*/
  ) {
    STUB_NOT_IMPLEMENTED();
    return mnexus::TextureHandle::Invalid();
  }

  IMPL_VAPI(void, DestroyTexture,
    mnexus::TextureHandle /*texture_handle*/
  ) {
    STUB_NOT_IMPLEMENTED();
  }

  IMPL_VAPI(void, GetTextureDesc,
    mnexus::TextureHandle /*texture_handle*/,
    mnexus::TextureDesc& /*out_desc*/
  ) {
    STUB_NOT_IMPLEMENTED();
  }

  // ----------------------------------------------------------------------------------------------
  // Sampler
  //

  IMPL_VAPI(mnexus::SamplerHandle, CreateSampler,
    mnexus::SamplerDesc const& /*desc*/
  ) {
    STUB_NOT_IMPLEMENTED();
    return mnexus::SamplerHandle::Invalid();
  }

  IMPL_VAPI(void, DestroySampler,
    mnexus::SamplerHandle /*sampler_handle*/
  ) {
    STUB_NOT_IMPLEMENTED();
  }

  // ----------------------------------------------------------------------------------------------
  // ShaderModule
  //

  IMPL_VAPI(mnexus::ShaderModuleHandle, CreateShaderModule,
    mnexus::ShaderModuleDesc const& desc
  ) {
    container::ResourceHandle pool_handle = EmplaceShaderModuleResourcePool(
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
    auto pool_handle = container::ResourceHandle::FromU64(shader_module_handle.Get());
    resource_storage_->shader_modules.Erase(pool_handle);
  }

  // ----------------------------------------------------------------------------------------------
  // Program
  //

  IMPL_VAPI(mnexus::ProgramHandle, CreateProgram,
    mnexus::ProgramDesc const& desc
  ) {
    container::ResourceHandle const pool_handle = EmplaceProgramResourcePool(
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
    auto pool_handle = container::ResourceHandle::FromU64(program_handle.Get());
    resource_storage_->programs.Erase(pool_handle);
  }

  // ----------------------------------------------------------------------------------------------
  // ComputePipeline
  //

  IMPL_VAPI(mnexus::ComputePipelineHandle, CreateComputePipeline,
    mnexus::ComputePipelineDesc const& desc
  ) {
    auto program_pool_handle = container::ResourceHandle::FromU64(desc.program.Get());

    auto [program_hot, program_cold, lock] = resource_storage_->programs.GetConstRefWithSharedLockGuard(
      program_pool_handle
    );

    container::ResourceHandle const pool_handle = EmplaceComputePipelineResourcePool(
      resource_storage_->compute_pipelines,
      *vk_device_,
      program_hot,
      program_cold,
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
    auto pool_handle = container::ResourceHandle::FromU64(compute_pipeline_handle.Get());
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

private:
  VulkanDevice* vk_device_ = nullptr;
  ResourceStorage* resource_storage_ = nullptr;
};

// ==================================================================================================
// BackendVulkan
//

class BackendVulkan final : public IBackendVulkan {
public:
  explicit BackendVulkan(VulkanInstance instance, std::unique_ptr<VulkanDevice> vk_device) :
    vk_instance_(std::move(instance)),
    vk_device_(std::move(vk_device)),
    device_(vk_device_.get(), &resource_storage_)
  {}
  ~BackendVulkan() override = default;
  MBASE_DISALLOW_COPY_MOVE(BackendVulkan);

  // ----------------------------------------------------------------------------------------------
  // Surface lifecycle.

  void OnDisplayChanged() override {
    STUB_NOT_IMPLEMENTED();
  }

  void OnSurfaceDestroyed() override {
    STUB_NOT_IMPLEMENTED();
  }

  void OnSurfaceRecreated(mnexus::SurfaceSourceDesc const& /*surface_source_desc*/) override {
    STUB_NOT_IMPLEMENTED();
  }

  // ----------------------------------------------------------------------------------------------
  // Presentation.

  void OnPresentPrologue() override {
    STUB_NOT_IMPLEMENTED();
  }

  void OnPresentEpilogue() override {
    STUB_NOT_IMPLEMENTED();
  }

  // ----------------------------------------------------------------------------------------------
  // Device.

  mnexus::IDevice* GetDevice() override {
    return &device_;
  }

  // ----------------------------------------------------------------------------------------------
  // Local.

  void Shutdown() {
    STUB_NOT_IMPLEMENTED();

    vk_device_->Shutdown();

    vk_instance_.Shutdown();
    VulkanInstance::ShutdownVolk();
  }

private:
  VulkanInstance vk_instance_;
  std::unique_ptr<VulkanDevice> vk_device_;

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

#if 0
  {
    auto AddExtensionIfAvailable = [&](std::string_view extension_name) {
      if (instance.QueryExtensionSupport(extension_name) != nullptr) {
        device_extensions.emplace_back(std::string(extension_name));
      } else {
        MBASE_LOG_WARN("Vulkan instance extension '{}' is not available", extension_name);
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

  VulkanDeviceDesc device_desc {
    .physical_device_desc = &physical_device_desc,
    .headless = desc.headless,
  };

  std::unique_ptr<VulkanDevice> vk_device = VulkanDevice::Create(
    instance,
    device_desc
  );
  if (!vk_device) {
    MBASE_LOG_ERROR("Failed to create Vulkan device.");
    return nullptr;
  }

  return std::make_unique<BackendVulkan>(std::move(instance), std::move(vk_device));
}

} // namespace mnexus_backend::vulkan
