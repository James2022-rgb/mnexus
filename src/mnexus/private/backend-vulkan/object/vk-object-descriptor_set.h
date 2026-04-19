#pragma once

// c++ headers ------------------------------------------
#include <memory>

// project headers --------------------------------------
#include "backend-vulkan/object/vk-object.h"

namespace mnexus_backend::vulkan {

class VulkanDescriptorSet final : public TVulkanObjectBase<VkDescriptorSet> {
public:
  VulkanDescriptorSet() = default;
  VulkanDescriptorSet(VkDescriptorSet handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }
};

using VulkanDescriptorSetPtr = std::shared_ptr<VulkanDescriptorSet>;

} // namespace mnexus_backend::vulkan
