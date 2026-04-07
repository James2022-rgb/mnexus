#pragma once

// c++ headers ------------------------------------------
#include <mutex>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/tsa.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan_vma.h"

namespace mnexus_backend::vulkan {

class IVulkanDevice;

// ----------------------------------------------------------------------------------------------------
// StagingBuffer
//
// A host-visible, persistently mapped VkBuffer used for CPU→GPU transfers.
//

struct StagingBuffer {
  VkBuffer vk_buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  void* mapped_data = nullptr;
  VkDeviceSize size = 0;
};

// ----------------------------------------------------------------------------------------------------
// StagingBufferPool
//
// Pool of staging buffers for QueueWriteBuffer (non-mappable path).
// Buffers are reused once the GPU has completed past the serial they were last used on.
// Thread-safe.
//

class StagingBufferPool final {
public:
  StagingBufferPool() = default;
  ~StagingBufferPool();
  MBASE_DISALLOW_COPY_MOVE(StagingBufferPool);

  void Initialize(IVulkanDevice* device);
  void Shutdown();

  /// Acquire a staging buffer of at least `size` bytes.
  StagingBuffer* Acquire(VkDeviceSize size);

  /// Release a staging buffer back to the pool after a queue submit.
  void Release(StagingBuffer* buffer, mnexus::QueueId const& queue_id, uint64_t serial);

private:
  struct PendingEntry {
    StagingBuffer* buffer = nullptr;
    mnexus::QueueId queue_id;
    uint64_t serial = 0;
  };

  StagingBuffer* CreateStagingBuffer(VkDeviceSize size);

  IVulkanDevice* device_ = nullptr;
  mbase::Lockable<std::mutex> mutex_;
  std::vector<PendingEntry> pending_buffers_ MBASE_GUARDED_BY(mutex_);
  std::vector<StagingBuffer*> all_buffers_ MBASE_GUARDED_BY(mutex_);
};

// ----------------------------------------------------------------------------------------------------
// TransientCommandPool
//
// Per-queue pool of single-use command buffers for internal transfers.
// Thread-safe.
//

class TransientCommandPool final {
public:
  TransientCommandPool() = default;
  ~TransientCommandPool();
  MBASE_DISALLOW_COPY_MOVE(TransientCommandPool);

  void Initialize(IVulkanDevice* device, uint32_t queue_family_index);
  void Shutdown();

  /// Allocate and begin a one-shot command buffer.
  VkCommandBuffer Acquire();

  /// Release a command buffer back to the pool after a queue submit.
  void Release(VkCommandBuffer command_buffer, mnexus::QueueId const& queue_id, uint64_t serial);

private:
  struct PendingEntry {
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    mnexus::QueueId queue_id;
    uint64_t serial = 0;
  };

  IVulkanDevice* device_ = nullptr;
  mbase::Lockable<std::mutex> mutex_;
  VkCommandPool vk_command_pool_ MBASE_GUARDED_BY(mutex_) = VK_NULL_HANDLE;
  std::vector<PendingEntry> pending_command_buffers_ MBASE_GUARDED_BY(mutex_);
};

} // namespace mnexus_backend::vulkan
