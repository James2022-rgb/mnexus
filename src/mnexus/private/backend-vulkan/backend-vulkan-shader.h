#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"
#include "shader/reflection.h"

#include "pipeline/pipeline_layout_cache.h"

#include "backend-vulkan/device/vk-device.h"
#include "backend-vulkan/object/vk-object-shader_module.h"
#include "backend-vulkan/object/vk-object-pipeline_layout.h"

namespace mnexus_backend::vulkan {

//
// ShaderModule
//

struct ShaderModuleHot final {
  VulkanShaderModule vk_shader_module;

  void Stamp(uint32_t queue_compact_index, uint64_t serial) {
    this->vk_shader_module.sync_stamp().Stamp(queue_compact_index, serial);
  }
};

struct ShaderModuleCold final {
  shader::ShaderModuleReflection reflection;
};

using ShaderModuleResourcePool = resource_pool::TResourceGenerationalPool<ShaderModuleHot, ShaderModuleCold, mnexus::kResourceTypeShaderModule>;

resource_pool::ResourceHandle EmplaceShaderModuleResourcePool(
  ShaderModuleResourcePool& out_pool,
  IVulkanDevice const& device,
  mnexus::ShaderModuleDesc const& shader_module_desc
);

//
// Program
//

struct ProgramHot final {
  VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE; // Non-owning; kept alive by the cache.
  VulkanPipelineLayoutPtr pipeline_layout_ref;           // Shared ownership with the cache.

  void Stamp(uint32_t queue_compact_index, uint64_t serial) {
    this->pipeline_layout_ref->sync_stamp().Stamp(queue_compact_index, serial);
  }
};

struct ProgramCold final {
  mbase::SmallVector<mnexus::ShaderModuleHandle, 2> shader_module_handles;
};

using ProgramResourcePool = resource_pool::TResourceGenerationalPool<ProgramHot, ProgramCold, mnexus::kResourceTypeProgram>;

resource_pool::ResourceHandle EmplaceProgramResourcePool(
  ProgramResourcePool& out_pool,
  IVulkanDevice const& device,
  mnexus::ProgramDesc const& program_desc,
  ShaderModuleResourcePool const& shader_module_pool,
  pipeline::TPipelineLayoutCache<VulkanPipelineLayoutPtr>& pipeline_layout_cache
);

} // namespace mnexus_backend::vulkan
