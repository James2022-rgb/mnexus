#pragma once

// project headers --------------------------------------
#include "backend-vulkan/object/vk-object.h"

namespace mnexus_backend::vulkan {

class VulkanSampler final : public TVulkanObjectBase<VkSampler> {
public:
  VulkanSampler() = default;
  VulkanSampler(VkSampler handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }
};

} // namespace mnexus_backend::vulkan
