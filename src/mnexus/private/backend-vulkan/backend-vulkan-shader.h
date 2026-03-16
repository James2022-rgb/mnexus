#pragma once

// c++ headers ------------------------------------------
#include <memory>

// public project headers -------------------------------
#include "mbase/public/container.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "container/resource_generational_pool.h"
#include "shader/reflection.h"

#include "pipeline/pipeline_layout_cache.h"

#include "backend-vulkan/vk-device.h"
#include "backend-vulkan/vk-object.h"

namespace mnexus_backend::vulkan {

//
// ShaderModule
//

struct ShaderModuleHot final {
  TVulkanObject<VkShaderModule> vk_shader_module;
};

struct ShaderModuleCold final {
  shader::ShaderModuleReflection reflection;
};

using ShaderModuleResourcePool = container::TResourceGenerationalPool<ShaderModuleHot, ShaderModuleCold>;

container::ResourceHandle EmplaceShaderModuleResourcePool(
  ShaderModuleResourcePool& out_pool,
  VulkanDevice const& device,
  mnexus::ShaderModuleDesc const& shader_module_desc
);

//
// VulkanPipelineLayout
//
// Bundles a VkPipelineLayout and its VkDescriptorSetLayouts.
// Not RAII -- deferred destruction handles cleanup via destroy_func.
//

struct VulkanPipelineLayout final {
  TVulkanObject<VkPipelineLayout> layout;
  mbase::SmallVector<VkDescriptorSetLayout, 4> descriptor_set_layouts;

  VulkanPipelineLayout() = default;
  ~VulkanPipelineLayout() = default;
  MBASE_DISALLOW_COPY(VulkanPipelineLayout);
  VulkanPipelineLayout(VulkanPipelineLayout&&) noexcept = default;
  VulkanPipelineLayout& operator=(VulkanPipelineLayout&&) noexcept = default;
};

using VulkanPipelineLayoutPtr = std::shared_ptr<VulkanPipelineLayout>;

//
// Program
//

struct ProgramHot final {
  VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE; // Non-owning; kept alive by the cache.
  VulkanPipelineLayoutPtr pipeline_layout_ref;           // Shared ownership with the cache.
  ResourceSyncStamp sync_stamp;
};

struct ProgramCold final {
  mbase::SmallVector<mnexus::ShaderModuleHandle, 2> shader_module_handles;
};

using ProgramResourcePool = container::TResourceGenerationalPool<ProgramHot, ProgramCold>;

container::ResourceHandle EmplaceProgramResourcePool(
  ProgramResourcePool& out_pool,
  VulkanDevice const& device,
  mnexus::ProgramDesc const& program_desc,
  ShaderModuleResourcePool const& shader_module_pool,
  pipeline::TPipelineLayoutCache<VulkanPipelineLayoutPtr>& pipeline_layout_cache
);

} // namespace mnexus_backend::vulkan
