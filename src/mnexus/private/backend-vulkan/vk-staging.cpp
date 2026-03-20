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
  free_buffers_.clear();
  device_ = nullptr;
}

StagingBuffer* StagingBufferPool::Acquire(VkDeviceSize size) {
  // Try to find a free buffer that's large enough.
  for (size_t i = 0; i < free_buffers_.size(); ++i) {
    if (free_buffers_[i]->size >= size) {
      StagingBuffer* buf = free_buffers_[i];
      free_buffers_.erase(free_buffers_.begin() + static_cast<ptrdiff_t>(i));
      buf->sync_stamp.Reset();
      return buf;
    }
  }

  // No suitable free buffer; create a new one.
  return this->CreateStagingBuffer(size);
}

void StagingBufferPool::Release(StagingBuffer* buffer) {
  free_buffers_.push_back(buffer);
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
    .sync_stamp {},
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
  free_command_buffers_.clear();
  device_ = nullptr;
}

VkCommandBuffer TransientCommandPool::Acquire() {
  VkCommandBuffer cmd = VK_NULL_HANDLE;

  if (!free_command_buffers_.empty()) {
    cmd = free_command_buffers_.back();
    free_command_buffers_.pop_back();
    vkResetCommandBuffer(cmd, 0);
  } else {
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

void TransientCommandPool::Release(VkCommandBuffer command_buffer) {
  free_command_buffers_.push_back(command_buffer);
}

} // namespace mnexus_backend::vulkan
