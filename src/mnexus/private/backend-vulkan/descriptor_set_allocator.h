#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

namespace mnexus_backend::vulkan {

class VulkanDescriptorSetLayout;
class VulkanDevice;

// ----------------------------------------------------------------------------------------------------
// IDescriptorSetAllocator
//
// Per-layout descriptor set allocation with GPU-sync-aware reuse.
// Implementation is hidden in the .cpp file.
//

class IDescriptorSetAllocator {
public:
  virtual ~IDescriptorSetAllocator() = default;

  static IDescriptorSetAllocator* Create(VulkanDevice* device);

  /// Clean up all Vulkan resources and delete this object.
  virtual void Shutdown() = 0;

  /// Allocate a VkDescriptorSet for the given layout.
  virtual VkDescriptorSet Allocate(VulkanDescriptorSetLayout const& layout) = 0;

  /// Return a descriptor set for reuse after GPU completes.
  virtual void Free(
    VulkanDescriptorSetLayout const& layout, VkDescriptorSet set,
    mnexus::QueueId const& queue_id, uint64_t serial
  ) = 0;
};

} // namespace mnexus_backend::vulkan
