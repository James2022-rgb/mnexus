// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/backend-vulkan-command_list.h"

#include "backend-vulkan/vk-device.h"

namespace mnexus_backend::vulkan {

// ==================================================================================================
// MnexusDeviceVulkan
//

class MnexusDeviceVulkan final : public mnexus::IDevice {
public:
  MnexusDeviceVulkan() = default;
  ~MnexusDeviceVulkan() override = default;

  // ----------------------------------------------------------------------------------------------
  // Queue
  //

  MNEXUS_NO_THROW uint32_t MNEXUS_CALL QueueGetFamilyCount() override {
    MBASE_LOG_WARN("Vulkan backend: QueueGetFamilyCount() not implemented");
    return 1;
  }

  MNEXUS_NO_THROW MnBool32 MNEXUS_CALL QueueGetFamilyDesc(
    uint32_t /*queue_family_index*/,
    mnexus::QueueFamilyDesc& /*out_desc*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: QueueGetFamilyDesc() not implemented");
    return MnBoolFalse;
  }

  MNEXUS_NO_THROW mnexus::IntraQueueSubmissionId MNEXUS_CALL QueueSubmitCommandList(
    mnexus::QueueId const& /*queue_id*/,
    mnexus::ICommandList* command_list
  ) override {
    MBASE_LOG_WARN("Vulkan backend: QueueSubmitCommandList() not implemented");
    delete command_list;
    return mnexus::IntraQueueSubmissionId{0};
  }

  MNEXUS_NO_THROW mnexus::IntraQueueSubmissionId MNEXUS_CALL QueueWriteBuffer(
    mnexus::QueueId const& /*queue_id*/,
    mnexus::BufferHandle /*buffer_handle*/,
    uint32_t /*buffer_offset*/,
    void const* /*data*/,
    uint32_t /*data_size_in_bytes*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: QueueWriteBuffer() not implemented");
    return mnexus::IntraQueueSubmissionId{0};
  }

  MNEXUS_NO_THROW mnexus::IntraQueueSubmissionId MNEXUS_CALL QueueReadBuffer(
    mnexus::QueueId const& /*queue_id*/,
    mnexus::BufferHandle /*buffer_handle*/,
    uint32_t /*buffer_offset*/,
    void* /*dst*/,
    uint32_t /*size_in_bytes*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: QueueReadBuffer() not implemented");
    return mnexus::IntraQueueSubmissionId{0};
  }

  MNEXUS_NO_THROW mnexus::IntraQueueSubmissionId MNEXUS_CALL QueueGetCompletedValue(
    mnexus::QueueId const& /*queue_id*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: QueueGetCompletedValue() not implemented");
    return mnexus::IntraQueueSubmissionId{0};
  }

  MNEXUS_NO_THROW void MNEXUS_CALL QueueWaitIdle(
    mnexus::QueueId const& /*queue_id*/,
    mnexus::IntraQueueSubmissionId /*value*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: QueueWaitIdle() not implemented");
  }

  // ----------------------------------------------------------------------------------------------
  // Command List
  //

  MNEXUS_NO_THROW mnexus::ICommandList* MNEXUS_CALL CreateCommandList(
    mnexus::CommandListDesc const& /*desc*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: CreateCommandList() not implemented");
    return new MnexusCommandListVulkan();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL DiscardCommandList(
    mnexus::ICommandList* command_list
  ) override {
    delete command_list;
  }

  // ----------------------------------------------------------------------------------------------
  // Buffer
  //

  MNEXUS_NO_THROW mnexus::BufferHandle MNEXUS_CALL CreateBuffer(
    mnexus::BufferDesc const& /*desc*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: CreateBuffer() not implemented");
    return mnexus::BufferHandle::Invalid();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL DestroyBuffer(
    mnexus::BufferHandle /*buffer_handle*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: DestroyBuffer() not implemented");
  }

  MNEXUS_NO_THROW void MNEXUS_CALL GetBufferDesc(
    mnexus::BufferHandle /*buffer_handle*/,
    mnexus::BufferDesc& /*out_desc*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: GetBufferDesc() not implemented");
  }

  // ----------------------------------------------------------------------------------------------
  // Texture
  //

  MNEXUS_NO_THROW mnexus::TextureHandle MNEXUS_CALL GetSwapchainTexture() override {
    MBASE_LOG_WARN("Vulkan backend: GetSwapchainTexture() not implemented");
    return mnexus::TextureHandle::Invalid();
  }

  MNEXUS_NO_THROW mnexus::TextureHandle MNEXUS_CALL CreateTexture(
    mnexus::TextureDesc const& /*desc*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: CreateTexture() not implemented");
    return mnexus::TextureHandle::Invalid();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL DestroyTexture(
    mnexus::TextureHandle /*texture_handle*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: DestroyTexture() not implemented");
  }

  MNEXUS_NO_THROW void MNEXUS_CALL GetTextureDesc(
    mnexus::TextureHandle /*texture_handle*/,
    mnexus::TextureDesc& /*out_desc*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: GetTextureDesc() not implemented");
  }

  // ----------------------------------------------------------------------------------------------
  // Sampler
  //

  MNEXUS_NO_THROW mnexus::SamplerHandle MNEXUS_CALL CreateSampler(
    mnexus::SamplerDesc const& /*desc*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: CreateSampler() not implemented");
    return mnexus::SamplerHandle::Invalid();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL DestroySampler(
    mnexus::SamplerHandle /*sampler_handle*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: DestroySampler() not implemented");
  }

  // ----------------------------------------------------------------------------------------------
  // ShaderModule
  //

  MNEXUS_NO_THROW mnexus::ShaderModuleHandle MNEXUS_CALL CreateShaderModule(
    mnexus::ShaderModuleDesc const& /*desc*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: CreateShaderModule() not implemented");
    return mnexus::ShaderModuleHandle::Invalid();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL DestroyShaderModule(
    mnexus::ShaderModuleHandle /*shader_module_handle*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: DestroyShaderModule() not implemented");
  }

  // ----------------------------------------------------------------------------------------------
  // Program
  //

  MNEXUS_NO_THROW mnexus::ProgramHandle MNEXUS_CALL CreateProgram(
    mnexus::ProgramDesc const& /*desc*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: CreateProgram() not implemented");
    return mnexus::ProgramHandle::Invalid();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL DestroyProgram(
    mnexus::ProgramHandle /*program_handle*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: DestroyProgram() not implemented");
  }

  // ----------------------------------------------------------------------------------------------
  // ComputePipeline
  //

  MNEXUS_NO_THROW mnexus::ComputePipelineHandle MNEXUS_CALL CreateComputePipeline(
    mnexus::ComputePipelineDesc const& /*desc*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: CreateComputePipeline() not implemented");
    return mnexus::ComputePipelineHandle::Invalid();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL DestroyComputePipeline(
    mnexus::ComputePipelineHandle /*compute_pipeline_handle*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: DestroyComputePipeline() not implemented");
  }

  // ----------------------------------------------------------------------------------------------
  // RenderPipeline
  //

  MNEXUS_NO_THROW mnexus::RenderPipelineHandle MNEXUS_CALL CreateRenderPipeline(
    mnexus::RenderPipelineDesc const& /*desc*/
  ) override {
    MBASE_LOG_WARN("Vulkan backend: CreateRenderPipeline() not implemented");
    return mnexus::RenderPipelineHandle::Invalid();
  }

  // ----------------------------------------------------------------------------------------------
  // Device Capability
  //

  MNEXUS_NO_THROW mnexus::AdapterCapability MNEXUS_CALL GetAdapterCapability() override {
    MBASE_LOG_WARN("Vulkan backend: GetAdapterCapability() not implemented");
    return {};
  }

  MNEXUS_NO_THROW mnexus::ClipSpaceConvention MNEXUS_CALL GetClipSpaceConvention() override {
    return mnexus::ClipSpaceConvention {
      .y_direction = mnexus::ClipSpaceYDirection::kDown,
      .depth_range = mnexus::ClipSpaceDepthRange::kZeroToOne,
    };
  }

  MNEXUS_NO_THROW void MNEXUS_CALL GetAdapterInfo(mnexus::AdapterInfo& out_info) override {
    MBASE_LOG_WARN("Vulkan backend: GetAdapterInfo() not implemented");
    out_info = {};
  }

  // ----------------------------------------------------------------------------------------------
  // Diagnostics
  //

  MNEXUS_NO_THROW mnexus::RenderPipelineCacheSnapshot MNEXUS_CALL GetRenderPipelineCacheSnapshot() override {
    MBASE_LOG_WARN("Vulkan backend: GetRenderPipelineCacheSnapshot() not implemented");
    return {};
  }
};

// ==================================================================================================
// BackendVulkan
//

class BackendVulkan final : public IBackendVulkan {
public:
  explicit BackendVulkan(VulkanInstance instance, VulkanDevice vk_device) :
    vk_instance_(std::move(instance)),
    vk_device_(std::move(vk_device))
  {}
  ~BackendVulkan() override = default;

  // ----------------------------------------------------------------------------------------------
  // Surface lifecycle.

  void OnDisplayChanged() override {
    MBASE_LOG_WARN("Vulkan backend: OnDisplayChanged() not implemented");
  }

  void OnSurfaceDestroyed() override {
    MBASE_LOG_WARN("Vulkan backend: OnSurfaceDestroyed() not implemented");
  }

  void OnSurfaceRecreated(mnexus::SurfaceSourceDesc const& /*surface_source_desc*/) override {
    MBASE_LOG_WARN("Vulkan backend: OnSurfaceRecreated() not implemented");
  }

  // ----------------------------------------------------------------------------------------------
  // Presentation.

  void OnPresentPrologue() override {
    MBASE_LOG_WARN("Vulkan backend: OnPresentPrologue() not implemented");
  }

  void OnPresentEpilogue() override {
    MBASE_LOG_WARN("Vulkan backend: OnPresentEpilogue() not implemented");
  }

  // ----------------------------------------------------------------------------------------------
  // Device.

  mnexus::IDevice* GetDevice() override {
    return &device_;
  }

  // ----------------------------------------------------------------------------------------------
  // Local.

  void Shutdown() {
    MBASE_LOG_WARN("Vulkan backend: Shutdown() not implemented");

    vk_device_.Shutdown();

    vk_instance_.Shutdown();
    VulkanInstance::ShutdownVolk();
  }

private:
  VulkanInstance vk_instance_;
  VulkanDevice vk_device_;
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

  std::optional<VulkanDevice> opt_device = VulkanDevice::Create(
    instance,
    device_desc
  );
  if (!opt_device.has_value()) {
    MBASE_LOG_ERROR("Failed to create Vulkan device.");
    return nullptr;
  }
  VulkanDevice device = std::move(opt_device.value());

  return std::make_unique<BackendVulkan>(std::move(instance), std::move(device));
}

} // namespace mnexus_backend::vulkan
