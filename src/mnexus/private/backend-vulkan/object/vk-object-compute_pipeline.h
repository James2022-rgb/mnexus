#pragma once

// project headers --------------------------------------
#include "backend-vulkan/object/vk-object.h"

namespace mnexus_backend::vulkan {

class VulkanComputePipeline final : public TVulkanObjectBase<VkPipeline> {
public:
  VulkanComputePipeline() = default;
  VulkanComputePipeline(VkPipeline handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }
};

} // namespace mnexus_backend::vulkan
