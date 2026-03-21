// TU header --------------------------------------------
#include "backend-vulkan/descriptor_set_allocator.h"

// c++ headers ------------------------------------------
#include <mutex>
#include <unordered_map>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/log.h"
#include "mbase/public/tsa.h"

// project headers --------------------------------------
#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/vk-device.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// DescriptorSetAllocator (implementation)
//

class DescriptorSetAllocator final : public IDescriptorSetAllocator {
public:
  DescriptorSetAllocator() = default;
  ~DescriptorSetAllocator() override = default;

  void Initialize(VulkanDevice* device) {
    device_ = device;
  }

  void Shutdown() override {
    if (device_ != nullptr) {
      VkDevice vk_device = device_->handle();
      for (auto& [layout, pool] : pools_) {
        for (VkDescriptorPool vk_pool : pool.vk_pools) {
          vkDestroyDescriptorPool(vk_device, vk_pool, nullptr);
        }
      }
      pools_.clear();
      device_ = nullptr;
    }

    delete this;
  }

  VkDescriptorSet Allocate(VulkanDescriptorSetLayout const& layout) override {
    mbase::LockGuard lock(mutex_);

    PerLayoutPool& pool = pools_[layout.handle()];

    // Try to reclaim a completed pending set.
    for (size_t i = 0; i < pool.pending_sets.size(); ++i) {
      PendingEntry& entry = pool.pending_sets[i];
      uint64_t const completed = device_->QueueGetCompletedValue(entry.queue_id);
      if (completed >= entry.serial) {
        VkDescriptorSet set = entry.set;
        pool.pending_sets.erase(pool.pending_sets.begin() + static_cast<ptrdiff_t>(i));
        return set;
      }
    }

    // Try from free list.
    if (!pool.free_sets.empty()) {
      VkDescriptorSet set = pool.free_sets.back();
      pool.free_sets.pop_back();
      return set;
    }

    // Allocate a new batch from a new pool.
    VkDescriptorPool vk_pool = this->CreatePool(layout);
    pool.vk_pools.push_back(vk_pool);

    VkDescriptorSetLayout vk_layout = layout.handle();
    std::vector<VkDescriptorSetLayout> layouts(kSetsPerPool, vk_layout);

    VkDescriptorSetAllocateInfo alloc_info {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = nullptr,
      .descriptorPool = vk_pool,
      .descriptorSetCount = kSetsPerPool,
      .pSetLayouts = layouts.data(),
    };

    std::vector<VkDescriptorSet> sets(kSetsPerPool);
    VkResult const result = vkAllocateDescriptorSets(device_->handle(), &alloc_info, sets.data());
    if (result != VK_SUCCESS) {
      MBASE_LOG_ERROR("vkAllocateDescriptorSets failed: {}", string_VkResult(result));
      return VK_NULL_HANDLE;
    }

    // Return the first set, push the rest to free list.
    for (uint32_t i = 1; i < kSetsPerPool; ++i) {
      pool.free_sets.push_back(sets[i]);
    }
    return sets[0];
  }

  void Free(
    VulkanDescriptorSetLayout const& layout,
    VkDescriptorSet set,
    mnexus::QueueId const& queue_id,
    uint64_t serial
  ) override {
    mbase::LockGuard lock(mutex_);

    PerLayoutPool& pool = pools_[layout.handle()];
    if (serial == 0) {
      pool.free_sets.push_back(set);
    } else {
      pool.pending_sets.emplace_back(
        PendingEntry {
          .set = set,
          .queue_id = queue_id,
          .serial = serial,
        }
      );
    }
  }

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

  VkDescriptorPool CreatePool(VulkanDescriptorSetLayout const& layout) {
    std::unordered_map<VkDescriptorType, uint32_t> type_counts;
    for (auto const& binding : layout.bindings) {
      type_counts[binding.descriptorType] += binding.descriptorCount;
    }

    std::vector<VkDescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(type_counts.size());
    for (auto const& [type, count] : type_counts) {
      pool_sizes.emplace_back(
        VkDescriptorPoolSize {
          .type = type,
          .descriptorCount = count * kSetsPerPool,
        }
      );
    }

    VkDescriptorPoolCreateInfo pool_info {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .maxSets = kSetsPerPool,
      .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
      .pPoolSizes = pool_sizes.data(),
    };

    VkDescriptorPool vk_pool = VK_NULL_HANDLE;
    VkResult const result = vkCreateDescriptorPool(device_->handle(), &pool_info, nullptr, &vk_pool);
    if (result != VK_SUCCESS) {
      MBASE_LOG_ERROR("vkCreateDescriptorPool failed: {}", string_VkResult(result));
      return VK_NULL_HANDLE;
    }
    return vk_pool;
  }

  static constexpr uint32_t kSetsPerPool = 16;

  VulkanDevice* device_ = nullptr;
  mbase::Lockable<std::mutex> mutex_;
  std::unordered_map<VkDescriptorSetLayout, PerLayoutPool> pools_ MBASE_GUARDED_BY(mutex_);
};

// ----------------------------------------------------------------------------------------------------
// IDescriptorSetAllocator::Create
//

IDescriptorSetAllocator* IDescriptorSetAllocator::Create(VulkanDevice* device) {
  auto* allocator = new DescriptorSetAllocator();
  allocator->Initialize(device);
  return allocator;
}

} // namespace mnexus_backend::vulkan
