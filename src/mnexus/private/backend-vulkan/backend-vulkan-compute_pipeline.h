#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "container/resource_generational_pool.h"

#include "backend-vulkan/backend-vulkan-shader.h"

namespace mnexus_backend::vulkan {

class VulkanComputePipeline final : public TVulkanObjectBase<VkPipeline> {
public:
  VulkanComputePipeline() = default;
  VulkanComputePipeline(VkPipeline handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }
};

struct ComputePipelineHot final {
  VulkanComputePipeline vk_compute_pipeline;
};

struct ComputePipelineCold final {
};

using ComputePipelineResourcePool = container::TResourceGenerationalPool<ComputePipelineHot, ComputePipelineCold>;

container::ResourceHandle EmplaceComputePipelineResourcePool(
  ComputePipelineResourcePool& out_pool,
  VulkanDevice const& vk_device,
  ProgramHot const& program_hot,
  ProgramCold const& program_cold,
  ShaderModuleResourcePool const& shader_module_pool
);

} // namespace mnexus_backend::vulkan
