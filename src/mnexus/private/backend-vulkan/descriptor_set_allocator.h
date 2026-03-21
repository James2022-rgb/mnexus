#pragma once

// c++ headers ------------------------------------------
#include <mutex>
#include <unordered_map>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/tsa.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

namespace mnexus_backend::vulkan {

class VulkanDescriptorSetLayout;
class VulkanDevice;

// ----------------------------------------------------------------------------------------------------
// DescriptorSetAllocator
//
// Per-layout descriptor set allocation with GPU-sync-aware reuse.
//

class DescriptorSetAllocator final {
public:
  DescriptorSetAllocator() = default;
  ~DescriptorSetAllocator();
  MBASE_DISALLOW_COPY_MOVE(DescriptorSetAllocator);

  void Initialize(VulkanDevice* device);
  void Shutdown();

  /// Allocate a VkDescriptorSet for the given layout.
  VkDescriptorSet Allocate(VulkanDescriptorSetLayout const& layout);

  /// Return a descriptor set for reuse after GPU completes.
  void Free(VulkanDescriptorSetLayout const& layout, VkDescriptorSet set,
            mnexus::QueueId const& queue_id, uint64_t serial);

private:
  struct PendingEntry {
    VkDescriptorSet set = VK_NULL_HANDLE;
    mnexus::QueueId queue_id;
    uint64_t serial = 0;
  };

  struct PerLayoutPool {
    std::vector<VkDescriptorPool> vk_pools;
    std::vector<VkDescriptorSet> free_sets;
    std::vector<PendingEntry> pending_sets;
  };

  VkDescriptorPool CreatePool(VulkanDescriptorSetLayout const& layout);

  static constexpr uint32_t kSetsPerPool = 16;

  VulkanDevice* device_ = nullptr;
  mbase::Lockable<std::mutex> mutex_;
  std::unordered_map<VkDescriptorSetLayout, PerLayoutPool> pools_ MBASE_GUARDED_BY(mutex_);
};

} // namespace mnexus_backend::vulkan
