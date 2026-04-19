#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/depend/vulkan_vma.h"
#include "backend-vulkan/device/vk-device.h"
#include "backend-vulkan/object/vk-object-buffer.h"

namespace mnexus_backend::vulkan {

struct BufferHot final {
  VulkanBuffer vk_buffer;
  void* mapped_data = nullptr;                     // Non-null if mappable.
  VmaAllocation vma_allocation = VK_NULL_HANDLE;   // For flush.
  VmaAllocator vma_allocator = VK_NULL_HANDLE;     // For flush.

  void Stamp(uint32_t queue_compact_index, uint64_t serial) {
    this->vk_buffer.sync_stamp().Stamp(queue_compact_index, serial);
  }
};

struct BufferCold final {
  mnexus::BufferDesc desc;
};

using BufferResourcePool = resource_pool::TResourceGenerationalPool<BufferHot, BufferCold, mnexus::kResourceTypeBuffer>;

resource_pool::ResourceHandle EmplaceBufferResourcePool(
  BufferResourcePool& out_pool,
  IVulkanDevice const& vk_device,
  mnexus::BufferDesc const& buffer_desc
);

} // namespace mnexus_backend::vulkan
