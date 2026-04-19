#pragma once

// public project headers -------------------------------
#include "mbase/public/container.h"

// project headers --------------------------------------
#include "backend-vulkan/object/vk-object.h"

namespace mnexus_backend::vulkan {

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

} // namespace mnexus_backend::vulkan
