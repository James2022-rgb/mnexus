// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/backend-vulkan-command_list.h"

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
  BackendVulkan() = default;
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

private:
  MnexusDeviceVulkan device_;
};

// ==================================================================================================
// Factory
//

std::unique_ptr<IBackendVulkan> IBackendVulkan::Create() {
  MBASE_LOG_WARN("Vulkan backend: creating stub backend (no Vulkan calls)");
  return std::make_unique<BackendVulkan>();
}

} // namespace mnexus_backend::vulkan
