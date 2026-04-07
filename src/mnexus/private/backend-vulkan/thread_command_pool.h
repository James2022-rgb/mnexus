#pragma once

// c++ headers ------------------------------------------
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/tsa.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

namespace mnexus_backend::vulkan {

class IVulkanDevice;

// ----------------------------------------------------------------------------------------------------
// ThreadCommandPoolRegistry
//
// Per-thread VkCommandPool registry for user-facing command list recording.
// Ensures that command buffers from the same VkCommandPool are only used
// by the thread that created them (Vulkan external synchronization requirement).
//

class ThreadCommandPoolRegistry final {
public:
  ThreadCommandPoolRegistry() = default;
  ~ThreadCommandPoolRegistry();
  MBASE_DISALLOW_COPY_MOVE(ThreadCommandPoolRegistry);

  void Initialize(IVulkanDevice* device, uint32_t queue_family_index);
  void Shutdown();

  /// Allocate and begin a VkCommandBuffer for the calling thread.
  VkCommandBuffer AllocateCommandBuffer();

  /// Free a command buffer after submission (with queue_id + serial for deferred reuse)
  /// or after discard (serial = 0 for immediate reuse).
  void FreeCommandBuffer(VkCommandBuffer cmd, mnexus::QueueId const& queue_id, uint64_t serial);

private:
  struct PendingEntry {
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    mnexus::QueueId queue_id;
    uint64_t serial = 0;
  };

  struct PerThreadPool {
    VkCommandPool vk_command_pool = VK_NULL_HANDLE;
    std::vector<PendingEntry> pending;
  };

  PerThreadPool& GetOrCreatePool();

  IVulkanDevice* device_ = nullptr;
  uint32_t queue_family_index_ = 0;
  mbase::Lockable<std::mutex> mutex_;
  std::unordered_map<std::thread::id, PerThreadPool> pools_ MBASE_GUARDED_BY(mutex_);
};

} // namespace mnexus_backend::vulkan
