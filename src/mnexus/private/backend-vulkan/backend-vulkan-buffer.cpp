// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-buffer.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

namespace mnexus_backend::vulkan {

static VkBufferUsageFlags ConvertBufferUsageFlags(mnexus::BufferUsageFlags usage) {
  VkBufferUsageFlags vk_flags = 0;
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kVertex))      { vk_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kIndex))       { vk_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kUniform))     { vk_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kStorage))     { vk_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kTransferSrc)) { vk_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kTransferDst)) { vk_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kIndirect))    { vk_flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT; }
  return vk_flags;
}

bool CreateVulkanBuffer(
  VulkanBuffer& out_vk_buffer,
  VulkanDevice const& vk_device,
  mnexus::BufferDesc const& buffer_desc
) {
  VkBufferCreateInfo info {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .size = buffer_desc.size_in_bytes,
    .usage = ConvertBufferUsageFlags(buffer_desc.usage),
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
  };

  VkBuffer vk_buffer_handle = VK_NULL_HANDLE;
  VkResult result = vkCreateBuffer(vk_device.handle(), &info, nullptr, &vk_buffer_handle);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateBuffer failed: {}", string_VkResult(result));
    return false;
  }

  // FIXME: No memory binding yet. Need VMA or manual vkAllocateMemory + vkBindBufferMemory.

  VmaAllocationCreateInfo alloc_info {
    .flags = 0,
    .usage = VMA_MEMORY_USAGE_UNKNOWN,
    .requiredFlags = 0,
    .preferredFlags = 0,
    .memoryTypeBits = 0,
    .pool = VK_NULL_HANDLE,
    .pUserData = nullptr,
    .priority = 0.0f,
  };

  VmaAllocator const vma_allocator = vk_device.vma_allocator();

  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo allocation_info {};
  {
    uint32_t memory_type_index = 0;
    vmaFindMemoryTypeIndexForBufferInfo(
      vma_allocator,
      &info,
      &alloc_info,
      &memory_type_index
    );

    alloc_info.memoryTypeBits = 1u << memory_type_index;

    result = vmaAllocateMemoryForBuffer(vma_allocator, vk_buffer_handle, &alloc_info, &allocation, &allocation_info);
    if (result != VK_SUCCESS) {
      MBASE_LOG_ERROR("vmaAllocateMemoryForBuffer failed: {}", string_VkResult(result));
      vkDestroyBuffer(vk_device.handle(), vk_buffer_handle, nullptr);
      return false;
    }
  }

  vkBindBufferMemory(vk_device.handle(), vk_buffer_handle, allocation_info.deviceMemory, allocation_info.offset);

  VkDevice vk_device_handle = vk_device.handle();
  out_vk_buffer = VulkanBuffer(
    vk_buffer_handle,
    [vk_device_handle, vk_buffer_handle, allocation, vma_allocator] {
      vkDestroyBuffer(vk_device_handle, vk_buffer_handle, nullptr);

      if (allocation != VK_NULL_HANDLE) {
        vmaFreeMemory(vma_allocator, allocation);
      }
    },
    vk_device.deferred_destroyer()
  );
  return true;
}

container::ResourceHandle EmplaceBufferResourcePool(
  BufferResourcePool& out_pool,
  VulkanDevice const& vk_device,
  mnexus::BufferDesc const& buffer_desc
) {
  VulkanBuffer vk_buffer;
  bool const success = CreateVulkanBuffer(vk_buffer, vk_device, buffer_desc);
  if (!success) {
    return container::ResourceHandle::Null();
  }

  BufferHot hot {
    .vk_buffer = std::move(vk_buffer),
  };
  BufferCold cold {
    .desc = buffer_desc,
  };

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

} // namespace mnexus_backend::vulkan
