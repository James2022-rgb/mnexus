#pragma once

// c++ headers ------------------------------------------
#include <vector>

// project headers --------------------------------------
#include "sync/resource_sync.h"

#include "backend-vulkan/depend/vulkan_vma.h"

namespace mnexus_backend::vulkan {

class VulkanDevice;

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
  ResourceSyncStamp sync_stamp;
};

// ----------------------------------------------------------------------------------------------------
// StagingBufferPool
//
// Pool of staging buffers for QueueWriteBuffer (non-mappable path).
// Buffers are reused once the GPU has completed past their sync stamp.
//

class StagingBufferPool final {
public:
  StagingBufferPool() = default;
  ~StagingBufferPool();
  MBASE_DISALLOW_COPY_MOVE(StagingBufferPool);

  void Initialize(VulkanDevice* device);
  void Shutdown();

  /// Acquire a staging buffer of at least `size` bytes.
  /// The returned buffer is persistently mapped.
  StagingBuffer* Acquire(VkDeviceSize size);

  /// Release a staging buffer back to the pool.
  void Release(StagingBuffer* buffer);

private:
  StagingBuffer* CreateStagingBuffer(VkDeviceSize size);

  VulkanDevice* device_ = nullptr;
  std::vector<StagingBuffer*> free_buffers_;
  std::vector<StagingBuffer*> all_buffers_; // Owns all allocated buffers.
};

// ----------------------------------------------------------------------------------------------------
// TransientCommandPool
//
// Per-queue pool of single-use command buffers for internal transfers.
//

class TransientCommandPool final {
public:
  TransientCommandPool() = default;
  ~TransientCommandPool();
  MBASE_DISALLOW_COPY_MOVE(TransientCommandPool);

  void Initialize(VulkanDevice* device, uint32_t queue_family_index);
  void Shutdown();

  /// Allocate and begin a one-shot command buffer.
  VkCommandBuffer Acquire();

  /// End and return the command buffer to the pool.
  void Release(VkCommandBuffer command_buffer);

private:
  VulkanDevice* device_ = nullptr;
  VkCommandPool vk_command_pool_ = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> free_command_buffers_;
};

} // namespace mnexus_backend::vulkan
