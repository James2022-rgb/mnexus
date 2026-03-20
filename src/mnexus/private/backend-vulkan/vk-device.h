#pragma once

// c++ headers ------------------------------------------
#include <atomic>

#include <memory>

// public project headers -------------------------------
#include "mbase/public/accessor.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "sync/resource_sync.h"

#include "backend-vulkan/depend/vulkan_vma.h"
#include "backend-vulkan/vk-deferred_destroyer.h"
#include "backend-vulkan/vk-physical_device.h"

namespace mnexus_backend::vulkan {

struct VulkanDeviceDesc final {
  PhysicalDeviceDesc const* physical_device_desc = nullptr;
  bool headless = false;
};

struct VulkanQueueState final {
  VkQueue vk_queue = VK_NULL_HANDLE;
  VkSemaphore timeline_semaphore = VK_NULL_HANDLE;
  std::atomic<uint64_t> next_submit_serial {1}; // Valid serials start at 1.
};

class VulkanDevice final : public IVulkanDeferredDestroyer {
public:
  ~VulkanDevice() = default;
  MBASE_DISALLOW_COPY_MOVE(VulkanDevice);

  static std::unique_ptr<VulkanDevice> Create(
    VulkanInstance const& instance,
    VulkanDeviceDesc const& desc
  );

  void Shutdown();

  MBASE_ACCESSOR_GETV(VkDevice, handle);
  MBASE_ACCESSOR_GETCR(mnexus::QueueSelection, queue_selection);
  MBASE_ACCESSOR_GETV(VmaAllocator, vma_allocator);

  /// Returns the highest completed serial on the given queue.
  uint64_t QueueGetCompletedValue(mnexus::QueueId const& queue_id);

  /// Blocks until the given serial has completed on the given queue.
  void QueueWaitSubmitSerial(mnexus::QueueId const& queue_id, uint64_t value);

  /// Blocks until all submitted work on the given queue has completed.
  /// Returns the last submitted serial (0 if nothing has been submitted).
  uint64_t QueueWaitIdle(mnexus::QueueId const& queue_id);

  // IVulkanDeferredDestroyer
  void EnqueueDestroy(
    std::function<void()> destroy_func,
    ResourceSyncStamp::Snapshot snapshot
  ) override;

  // The returned pointer is non-const; the destroyer mutates internal state.
  // Safe to call on a const VulkanDevice because the interface is logically separate.
  IVulkanDeferredDestroyer* deferred_destroyer() const { return const_cast<VulkanDevice*>(this); }

private:
  explicit VulkanDevice(
    VulkanInstance const* instance,
    PhysicalDeviceDesc const& physical_device_desc,
    VkDevice handle,
    mnexus::QueueSelection queue_selection,
    QueueIndexMap queue_index_map,
    VulkanQueueState const* queue_states,
    uint32_t queue_count,
    VmaAllocator vma_allocator
  ) :
    instance_(instance),
    physical_device_desc_(std::make_unique<PhysicalDeviceDesc>(physical_device_desc)),
    queue_selection_(queue_selection),
    handle_(handle),
    queue_index_map_(queue_index_map),
    vma_allocator_(vma_allocator)
  {
    for (uint32_t i = 0; i < queue_count; ++i) {
      queue_states_[i].vk_queue = queue_states[i].vk_queue;
      queue_states_[i].timeline_semaphore = queue_states[i].timeline_semaphore;
    }
  }

  VulkanInstance const* instance_ = nullptr;
  std::unique_ptr<PhysicalDeviceDesc> physical_device_desc_; // Holding a copy would make this class too large and pollute the cache, so we store it on the heap and keep a unique_ptr to it.
  VkDevice handle_ = VK_NULL_HANDLE;
  mnexus::QueueSelection queue_selection_;
  QueueIndexMap queue_index_map_;
  VulkanQueueState queue_states_[kMaxQueues] {};
  VmaAllocator vma_allocator_ = VK_NULL_HANDLE;
};

} // namespace mnexus_backend::vulkan
