// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-buffer.h"

// c++ headers ------------------------------------------
#include <optional>

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

struct CreateVulkanBufferResult {
  VulkanBuffer vk_buffer;
  void* mapped_data = nullptr;
  VmaAllocation vma_allocation = VK_NULL_HANDLE;
};

std::optional<CreateVulkanBufferResult> CreateVulkanBuffer(
  VulkanDevice const& vk_device,
  mnexus::BufferDesc const& buffer_desc
) {
  bool const mappable = buffer_desc.usage.HasAnyOf(mnexus::BufferUsageFlagBits::kMappable);

  VkBufferUsageFlags const vk_usage_flags = ConvertBufferUsageFlags(buffer_desc.usage) | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  VkBufferCreateInfo info {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .size = buffer_desc.size_in_bytes,
    .usage = vk_usage_flags,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
  };

  VmaAllocator const vma_allocator = vk_device.vma_allocator();

  VmaAllocationCreateInfo alloc_info {
    .flags = mappable
      ? (VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
      : static_cast<VmaAllocationCreateFlags>(0),
    .usage = VMA_MEMORY_USAGE_AUTO,
    .requiredFlags = 0,
    .preferredFlags = 0,
    .memoryTypeBits = 0,
    .pool = VK_NULL_HANDLE,
    .pUserData = nullptr,
    .priority = 0.0f,
  };

  VkBuffer vk_buffer_handle = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo allocation_info {};

  VkResult const result = vmaCreateBuffer(
    vma_allocator, &info, &alloc_info,
    &vk_buffer_handle, &allocation, &allocation_info
  );
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vmaCreateBuffer failed: {}", string_VkResult(result));
    return std::nullopt;
  }

  VkDevice vk_device_handle = vk_device.handle();
  VulkanBuffer vk_buffer(
    vk_buffer_handle,
    [vk_device_handle, vk_buffer_handle, allocation, vma_allocator] {
      vmaDestroyBuffer(vma_allocator, vk_buffer_handle, allocation);
    },
    vk_device.deferred_destroyer()
  );

  return CreateVulkanBufferResult {
    .vk_buffer = std::move(vk_buffer),
    .mapped_data = allocation_info.pMappedData,
    .vma_allocation = allocation,
  };
}

container::ResourceHandle EmplaceBufferResourcePool(
  BufferResourcePool& out_pool,
  VulkanDevice const& vk_device,
  mnexus::BufferDesc const& buffer_desc
) {
  std::optional<CreateVulkanBufferResult> opt_result = CreateVulkanBuffer(vk_device, buffer_desc);
  if (!opt_result.has_value()) {
    return container::ResourceHandle::Null();
  }

  CreateVulkanBufferResult& buf = *opt_result;

  BufferHot hot {
    .vk_buffer = std::move(buf.vk_buffer),
    .mapped_data = buf.mapped_data,
    .vma_allocation = buf.vma_allocation,
    .vma_allocator = vk_device.vma_allocator(),
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
