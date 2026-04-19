#pragma once

// project headers --------------------------------------
#include "backend-vulkan/object/vk-object.h"

namespace mnexus_backend::vulkan {

class VulkanBuffer final : public TVulkanObjectBase<VkBuffer> {
public:
  VulkanBuffer() = default;
  VulkanBuffer(VkBuffer handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }
};

} // namespace mnexus_backend::vulkan
