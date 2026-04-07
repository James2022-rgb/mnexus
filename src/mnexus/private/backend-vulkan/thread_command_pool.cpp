// TU header --------------------------------------------
#include "backend-vulkan/thread_command_pool.h"

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/vk-device.h"

namespace mnexus_backend::vulkan {

ThreadCommandPoolRegistry::~ThreadCommandPoolRegistry() {
  this->Shutdown();
}

void ThreadCommandPoolRegistry::Initialize(IVulkanDevice* device, uint32_t queue_family_index) {
  device_ = device;
  queue_family_index_ = queue_family_index;
}

void ThreadCommandPoolRegistry::Shutdown() {
  if (device_ == nullptr) {
    return;
  }

  for (auto& [thread_id, pool] : pools_) {
    if (pool.vk_command_pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device_->handle(), pool.vk_command_pool, nullptr);
    }
  }
  pools_.clear();
  device_ = nullptr;
}

ThreadCommandPoolRegistry::PerThreadPool& ThreadCommandPoolRegistry::GetOrCreatePool() {
  std::thread::id const tid = std::this_thread::get_id();

  mbase::LockGuard lock(mutex_);

  auto it = pools_.find(tid);
  if (it != pools_.end()) {
    return it->second;
  }

  // Create a new command pool for this thread.
  VkCommandPoolCreateInfo pool_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = queue_family_index_,
  };

  VkCommandPool vk_pool = VK_NULL_HANDLE;
  VkResult const result = vkCreateCommandPool(device_->handle(), &pool_info, nullptr, &vk_pool);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateCommandPool failed: {}", string_VkResult(result));
  }

  auto [inserted_it, _] = pools_.emplace(tid, PerThreadPool { .vk_command_pool = vk_pool });
  return inserted_it->second;
}

VkCommandBuffer ThreadCommandPoolRegistry::AllocateCommandBuffer() {
  PerThreadPool& pool = this->GetOrCreatePool();

  VkCommandBuffer cmd = VK_NULL_HANDLE;

  // Try to reclaim a completed pending command buffer.
  for (size_t i = 0; i < pool.pending.size(); ++i) {
    PendingEntry& entry = pool.pending[i];
    // serial == 0 means immediately reusable (discarded, never submitted).
    bool const reusable = (entry.serial == 0)
      || (device_->QueueGetCompletedValue(entry.queue_id) >= entry.serial);
    if (reusable) {
      cmd = entry.command_buffer;
      pool.pending.erase(pool.pending.begin() + static_cast<ptrdiff_t>(i));
      vkResetCommandBuffer(cmd, 0);
      break;
    }
  }

  if (cmd == VK_NULL_HANDLE) {
    VkCommandBufferAllocateInfo alloc_info {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = pool.vk_command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(device_->handle(), &alloc_info, &cmd);
  }

  VkCommandBufferBeginInfo begin_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = nullptr,
  };
  vkBeginCommandBuffer(cmd, &begin_info);

  return cmd;
}

void ThreadCommandPoolRegistry::FreeCommandBuffer(
  VkCommandBuffer cmd,
  mnexus::QueueId const& queue_id,
  uint64_t serial
) {
  PerThreadPool& pool = this->GetOrCreatePool();
  pool.pending.emplace_back(
    PendingEntry {
      .command_buffer = cmd,
      .queue_id = queue_id,
      .serial = serial,
    }
  );
}

} // namespace mnexus_backend::vulkan
