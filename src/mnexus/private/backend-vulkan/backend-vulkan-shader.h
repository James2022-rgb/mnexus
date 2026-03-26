#pragma once

// c++ headers ------------------------------------------
#include <functional>
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

class VulkanShaderModule final : public TVulkanObjectBase<VkShaderModule> {
public:
  VulkanShaderModule() = default;
  VulkanShaderModule(VkShaderModule handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }
};

struct ShaderModuleHot final {
  VulkanShaderModule vk_shader_module;

  void Stamp(uint32_t queue_compact_index, uint64_t serial) {
    vk_shader_module.sync_stamp().Stamp(queue_compact_index, serial);
  }
};

struct ShaderModuleCold final {
  shader::ShaderModuleReflection reflection;
};

using ShaderModuleResourcePool = container::TResourceGenerationalPool<ShaderModuleHot, ShaderModuleCold, mnexus::kResourceTypeShaderModule>;

container::ResourceHandle EmplaceShaderModuleResourcePool(
  ShaderModuleResourcePool& out_pool,
  VulkanDevice const& device,
  mnexus::ShaderModuleDesc const& shader_module_desc
);

//
// VulkanDescriptorSetLayout
//
// Wraps VkDescriptorSetLayout + stores its binding composition.
// Needed by DescriptorSetAllocator to compute VkDescriptorPoolSize for pool creation.
//

class VulkanDescriptorSetLayout final : public TVulkanObjectBase<VkDescriptorSetLayout> {
public:
  VulkanDescriptorSetLayout() = default;
  VulkanDescriptorSetLayout(
    VkDescriptorSetLayout handle,
    std::function<void()> destroy_func,
    IVulkanDeferredDestroyer* deferred_destroyer,
    mbase::SmallVector<VkDescriptorSetLayoutBinding, 4> bindings
  ) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer),
    bindings(std::move(bindings))
  {
  }

  mbase::SmallVector<VkDescriptorSetLayoutBinding, 4> bindings;
};

//
// VulkanPipelineLayout
//
// Bundles a VkPipelineLayout and its VkDescriptorSetLayouts.
// Not RAII -- deferred destruction handles cleanup via destroy_func.
//

class VulkanPipelineLayout final : public TVulkanObjectBase<VkPipelineLayout> {
public:
  VulkanPipelineLayout() = default;
  VulkanPipelineLayout(VkPipelineLayout handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }

  mbase::SmallVector<VulkanDescriptorSetLayout, 4> descriptor_set_layouts;
};

using VulkanPipelineLayoutPtr = std::shared_ptr<VulkanPipelineLayout>;

//
// Program
//

struct ProgramHot final {
  VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE; // Non-owning; kept alive by the cache.
  VulkanPipelineLayoutPtr pipeline_layout_ref;           // Shared ownership with the cache.

  void Stamp(uint32_t queue_compact_index, uint64_t serial) {
    pipeline_layout_ref->sync_stamp().Stamp(queue_compact_index, serial);
  }
};

struct ProgramCold final {
  mbase::SmallVector<mnexus::ShaderModuleHandle, 2> shader_module_handles;
};

using ProgramResourcePool = container::TResourceGenerationalPool<ProgramHot, ProgramCold, mnexus::kResourceTypeProgram>;

container::ResourceHandle EmplaceProgramResourcePool(
  ProgramResourcePool& out_pool,
  VulkanDevice const& device,
  mnexus::ProgramDesc const& program_desc,
  ShaderModuleResourcePool const& shader_module_pool,
  pipeline::TPipelineLayoutCache<VulkanPipelineLayoutPtr>& pipeline_layout_cache
);

} // namespace mnexus_backend::vulkan
