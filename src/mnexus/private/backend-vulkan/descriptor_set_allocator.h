#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/descriptor_set_write.h"
#include "backend-vulkan/vk-object.h"

namespace mnexus_backend::vulkan {

class VulkanDescriptorSetLayout;
class IVulkanDevice;

// ----------------------------------------------------------------------------------------------------
// VulkanDescriptorSet
//

class VulkanDescriptorSet final : public TVulkanObjectBase<VkDescriptorSet> {
public:
  VulkanDescriptorSet() = default;
  VulkanDescriptorSet(VkDescriptorSet handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }
};

using VulkanDescriptorSetPtr = std::shared_ptr<VulkanDescriptorSet>;

// ----------------------------------------------------------------------------------------------------
// IDescriptorSetAllocator
//
// Per-layout descriptor set allocation with hash-and-cache.
// Implementation is hidden in the .cpp file.
//

class IDescriptorSetAllocator {
public:
  virtual ~IDescriptorSetAllocator() = default;

  static IDescriptorSetAllocator* Create(IVulkanDevice* device);

  /// Clean up all Vulkan resources and delete this object.
  virtual void Shutdown() = 0;

  /// Allocate or return a cached VkDescriptorSet for the given layout + write desc.
  /// Cache hit: return existing set (no alloc, no vkUpdateDescriptorSets).
  /// Cache miss: allocate from pool, vkUpdateDescriptorSets, cache, return.
  virtual VulkanDescriptorSetPtr Allocate(
    VkDevice device,
    VulkanDescriptorSetLayout const& layout,
    DescriptorSetWriteDesc const& write_desc
  ) = 0;

  /// Return a VkDescriptorSet to the free pool for reuse with new values.
  /// Called by VulkanDescriptorSet's destroy_func via deferred destruction
  /// (GPU has already completed by the time this is called).
  virtual void Free(
    VulkanDescriptorSetLayout const& layout,
    VkDescriptorSet set
  ) = 0;
};

} // namespace mnexus_backend::vulkan
