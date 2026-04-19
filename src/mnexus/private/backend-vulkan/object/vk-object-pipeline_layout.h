#pragma once

// c++ headers ------------------------------------------
#include <memory>

// project headers --------------------------------------
#include "backend-vulkan/object/vk-object-descriptor_set_layout.h"

namespace mnexus_backend::vulkan {

//
// VulkanPipelineLayout
//
// Bundles a VkPipelineLayout and its VkDescriptorSetLayouts.
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

} // namespace mnexus_backend::vulkan
