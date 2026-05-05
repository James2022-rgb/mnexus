// TU header --------------------------------------------
#include "backend-vulkan/descriptor/descriptor_set_allocator.h"

// c++ headers ------------------------------------------
#include <cstring>

#include <mutex>
#include <unordered_map>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/log.h"
#include "mbase/public/tsa.h"

// project headers --------------------------------------
#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/device/vk-device.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// DescriptorSetAllocator (implementation)
//

class DescriptorSetAllocator final : public IDescriptorSetAllocator {
public:
  DescriptorSetAllocator() = default;
  ~DescriptorSetAllocator() override = default;

  void Initialize(IVulkanDevice* device) {
    device_ = device;
  }

  void Shutdown() override MBASE_NO_THREAD_SAFETY_ANALYSIS {
    if (device_ != nullptr) {
      VkDevice vk_device = device_->handle();
      for (auto& [layout_handle, state] : per_layout_states_) {
        for (VkDescriptorPool vk_pool : state.vk_pools) {
          vkDestroyDescriptorPool(vk_device, vk_pool, nullptr);
        }
      }

      // Mark shutdown BEFORE clearing the map. The clear() chains into
      // ~VulkanDescriptorSet for each cached set, whose destroy_func
      // re-enters FreeImpl(); FreeImpl checks `device_` and short-circuits
      // so we don't modify the map while it is being destroyed.
      device_ = nullptr;
      per_layout_states_.clear();
    }

    delete this;
  }

  VulkanDescriptorSetPtr Allocate(
    VkDevice device,
    VulkanDescriptorSetLayout const& layout,
    DescriptorSetWriteDesc const& write_desc
  ) override {
    mbase::LockGuard lock(mutex_);

    PerLayoutState& state = per_layout_states_[layout.handle()];

    // Cache lookup.
    size_t const hash = write_desc.GetCachedHash();
    auto cache_it = state.cache.find(hash);
    if (cache_it != state.cache.end()) {
      CacheEntry& entry = cache_it->second;
      // Verify equality (not just hash match) to handle collisions.
      if (entry.write_desc == write_desc) {
        return entry.descriptor_set;
      }
    }

    // Cache miss -- allocate a fresh VkDescriptorSet.
    VkDescriptorSet vk_set = this->AllocateRawSet(layout);
    if (vk_set == VK_NULL_HANDLE) {
      return nullptr;
    }

    // Build VkWriteDescriptorSet from write_desc and update.
    std::span<DescriptorWriteDesc const> descs = write_desc.GetDenseDescriptorWriteDescs();

    // The buffer/image info storage backs the pBufferInfo / pImageInfo pointers
    // referenced by the writes; reserve up-front so emplaces don't invalidate them.
    mbase::SmallVector<VkWriteDescriptorSet, 4> writes;
    mbase::SmallVector<VkDescriptorBufferInfo, 4> buffer_infos;
    mbase::SmallVector<VkDescriptorImageInfo, 4> image_infos;
    writes.reserve(descs.size());
    buffer_infos.reserve(descs.size());
    image_infos.reserve(descs.size());

    for (auto const& desc : descs) {
      VkWriteDescriptorSet write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = vk_set,
        .dstBinding = desc.binding,
        .dstArrayElement = desc.array_element,
        .descriptorCount = 1,
        .descriptorType = desc.descriptor_type,
        .pImageInfo = nullptr,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
      };

      switch (desc.descriptor_type) {
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        buffer_infos.emplace_back(
          VkDescriptorBufferInfo {
            .buffer = desc.value.buffer.buffer,
            .offset = desc.value.buffer.offset,
            .range = desc.value.buffer.range,
          }
        );
        write.pBufferInfo = &buffer_infos.back();
        break;

      case VK_DESCRIPTOR_TYPE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        image_infos.emplace_back(
          VkDescriptorImageInfo {
            .sampler = desc.value.image.sampler,
            .imageView = desc.value.image.image_view,
            .imageLayout = desc.value.image.image_layout,
          }
        );
        write.pImageInfo = &image_infos.back();
        break;

      default:
        MBASE_LOG_ERROR("Unhandled VkDescriptorType {} in IDescriptorSetAllocator::Allocate", static_cast<int32_t>(desc.descriptor_type));
        continue;
      }

      writes.emplace_back(write);
    }

    vkUpdateDescriptorSets(
      device,
      static_cast<uint32_t>(writes.size()),
      writes.data(),
      0,
      nullptr
    );

    // Wrap in VulkanDescriptorSet with destroy_func that calls Free.
    VkDescriptorSetLayout vk_layout = layout.handle();
    auto result = std::make_shared<VulkanDescriptorSet>(
      vk_set,
      [this, vk_layout, vk_set] { this->FreeImpl(vk_layout, vk_set); },
      device_->GetDeferredDestroyer()
    );

    // Cache the set.
    state.cache[hash] = CacheEntry {
      .write_desc = write_desc,
      .descriptor_set = result,
    };

    return result;
  }

  void Free(
    VulkanDescriptorSetLayout const& layout,
    VkDescriptorSet set
  ) override {
    this->FreeImpl(layout.handle(), set);
  }

private:
  void FreeImpl(VkDescriptorSetLayout layout_handle, VkDescriptorSet set) {
    mbase::LockGuard lock(mutex_);

    // After Shutdown() begins, `device_` is nullptr; the descriptor pools
    // are destroyed and `per_layout_states_` is mid-clear. Skip cleanly.
    if (device_ == nullptr) {
      return;
    }

    auto it = per_layout_states_.find(layout_handle);
    if (it != per_layout_states_.end()) {
      it->second.free_sets.push_back(set);

      // Remove from cache if present.
      for (auto cache_it = it->second.cache.begin(); cache_it != it->second.cache.end(); ++cache_it) {
        if (cache_it->second.descriptor_set && cache_it->second.descriptor_set->handle() == set) {
          it->second.cache.erase(cache_it);
          break;
        }
      }
    }
  }

  VkDescriptorSet AllocateRawSet(VulkanDescriptorSetLayout const& layout) MBASE_REQUIRES(mutex_) {
    PerLayoutState& state = per_layout_states_[layout.handle()];

    // Try from free list.
    if (!state.free_sets.empty()) {
      VkDescriptorSet set = state.free_sets.back();
      state.free_sets.pop_back();
      return set;
    }

    // Allocate a new batch.
    VkDescriptorPool vk_pool = this->CreatePool(layout);
    state.vk_pools.push_back(vk_pool);

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

    for (uint32_t i = 1; i < kSetsPerPool; ++i) {
      state.free_sets.push_back(sets[i]);
    }
    return sets[0];
  }

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

  struct CacheEntry {
    DescriptorSetWriteDesc write_desc;
    VulkanDescriptorSetPtr descriptor_set;
  };

  struct PerLayoutState {
    std::vector<VkDescriptorPool> vk_pools;
    std::vector<VkDescriptorSet> free_sets;
    std::unordered_map<size_t, CacheEntry> cache; // hash -> entry
  };

  static constexpr uint32_t kSetsPerPool = 16;

  IVulkanDevice* device_ = nullptr;
  mbase::Lockable<std::mutex> mutex_;
  std::unordered_map<VkDescriptorSetLayout, PerLayoutState> per_layout_states_ MBASE_GUARDED_BY(mutex_);
};

// ----------------------------------------------------------------------------------------------------
// IDescriptorSetAllocator::Create
//

IDescriptorSetAllocator* IDescriptorSetAllocator::Create(IVulkanDevice* device) {
  auto* allocator = new DescriptorSetAllocator();
  allocator->Initialize(device);
  return allocator;
}

} // namespace mnexus_backend::vulkan
