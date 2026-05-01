#pragma once

// c++ headers ------------------------------------------
#include <memory>

// project headers --------------------------------------
#include "backend-vulkan/object/vk-object.h"

namespace mnexus_backend::vulkan {

class VulkanRenderPipeline final : public TVulkanObjectBase<VkPipeline> {
public:
  VulkanRenderPipeline() = default;
  VulkanRenderPipeline(VkPipeline handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }
};

using VulkanRenderPipelinePtr = std::shared_ptr<VulkanRenderPipeline>;

} // namespace mnexus_backend::vulkan
