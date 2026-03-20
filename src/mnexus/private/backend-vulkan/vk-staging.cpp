// TU header --------------------------------------------
#include "backend-vulkan/vk-staging.h"

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/vk-device.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// StagingBufferPool
//

StagingBufferPool::~StagingBufferPool() {
  this->Shutdown();
}

void StagingBufferPool::Initialize(VulkanDevice* device) {
  device_ = device;
}

void StagingBufferPool::Shutdown() {
  if (device_ == nullptr) {
    return;
  }

  VmaAllocator vma = device_->vma_allocator();
  for (StagingBuffer* buf : all_buffers_) {
    vmaDestroyBuffer(vma, buf->vk_buffer, buf->allocation);
    delete buf;
  }
  all_buffers_.clear();
  pending_buffers_.clear();
  device_ = nullptr;
}

StagingBuffer* StagingBufferPool::Acquire(VkDeviceSize size) {
  mbase::LockGuard lock(mutex_);

  // Scan pending buffers for one that is completed and large enough.
  for (size_t i = 0; i < pending_buffers_.size(); ++i) {
    PendingEntry& entry = pending_buffers_[i];
    uint64_t const completed = device_->QueueGetCompletedValue(entry.queue_id);
    if (completed >= entry.serial && entry.buffer->size >= size) {
      StagingBuffer* buf = entry.buffer;
      pending_buffers_.erase(pending_buffers_.begin() + static_cast<ptrdiff_t>(i));
      return buf;
    }
  }

  // No suitable buffer available; create a new one.
  return this->CreateStagingBuffer(size);
}

void StagingBufferPool::Release(StagingBuffer* buffer, mnexus::QueueId const& queue_id, uint64_t serial) {
  mbase::LockGuard lock(mutex_);
  pending_buffers_.emplace_back(
    PendingEntry {
      .buffer = buffer,
      .queue_id = queue_id,
      .serial = serial,
    }
  );
}

StagingBuffer* StagingBufferPool::CreateStagingBuffer(VkDeviceSize size) {
  MBASE_ASSERT(device_ != nullptr);

  VkBufferCreateInfo buffer_info {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .size = size,
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
  };

  VmaAllocationCreateInfo alloc_info {
    .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
    .usage = VMA_MEMORY_USAGE_AUTO,
    .requiredFlags = 0,
    .preferredFlags = 0,
    .memoryTypeBits = 0,
    .pool = VK_NULL_HANDLE,
    .pUserData = nullptr,
    .priority = 0.0f,
  };

  VkBuffer vk_buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo allocation_info {};

  VkResult const result = vmaCreateBuffer(
    device_->vma_allocator(), &buffer_info, &alloc_info,
    &vk_buffer, &allocation, &allocation_info
  );
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vmaCreateBuffer (staging) failed: {}", string_VkResult(result));
    return nullptr;
  }

  auto* buf = new StagingBuffer {
    .vk_buffer = vk_buffer,
    .allocation = allocation,
    .mapped_data = allocation_info.pMappedData,
    .size = size,
  };
  all_buffers_.push_back(buf);
  return buf;
}

// ----------------------------------------------------------------------------------------------------
// TransientCommandPool
//

TransientCommandPool::~TransientCommandPool() {
  this->Shutdown();
}

void TransientCommandPool::Initialize(VulkanDevice* device, uint32_t queue_family_index) {
  device_ = device;

  VkCommandPoolCreateInfo pool_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = queue_family_index,
  };

  VkResult const result = vkCreateCommandPool(device_->handle(), &pool_info, nullptr, &vk_command_pool_);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateCommandPool failed: {}", string_VkResult(result));
  }
}

void TransientCommandPool::Shutdown() {
  if (device_ == nullptr) {
    return;
  }

  if (vk_command_pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_->handle(), vk_command_pool_, nullptr);
    vk_command_pool_ = VK_NULL_HANDLE;
  }
  pending_command_buffers_.clear();
  device_ = nullptr;
}

VkCommandBuffer TransientCommandPool::Acquire() {
  mbase::LockGuard lock(mutex_);

  VkCommandBuffer cmd = VK_NULL_HANDLE;

  // Scan pending command buffers for one that has completed.
  for (size_t i = 0; i < pending_command_buffers_.size(); ++i) {
    PendingEntry& entry = pending_command_buffers_[i];
    uint64_t const completed = device_->QueueGetCompletedValue(entry.queue_id);
    if (completed >= entry.serial) {
      cmd = entry.command_buffer;
      pending_command_buffers_.erase(pending_command_buffers_.begin() + static_cast<ptrdiff_t>(i));
      vkResetCommandBuffer(cmd, 0);
      break;
    }
  }

  if (cmd == VK_NULL_HANDLE) {
    // No completed command buffer available; allocate a new one.
    VkCommandBufferAllocateInfo alloc_info {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = vk_command_pool_,
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

void TransientCommandPool::Release(VkCommandBuffer command_buffer, mnexus::QueueId const& queue_id, uint64_t serial) {
  mbase::LockGuard lock(mutex_);
  pending_command_buffers_.emplace_back(
    PendingEntry {
      .command_buffer = command_buffer,
      .queue_id = queue_id,
      .serial = serial,
    }
  );
}

} // namespace mnexus_backend::vulkan
