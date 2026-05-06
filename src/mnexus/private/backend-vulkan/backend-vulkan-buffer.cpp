// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-buffer.h"

// c++ headers ------------------------------------------
#include <optional>

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/resource/types_bridge.h"

namespace mnexus_backend::vulkan {

struct CreateVulkanBufferResult {
  VulkanBuffer vk_buffer;
  void* mapped_data = nullptr;
  VmaAllocation vma_allocation = VK_NULL_HANDLE;
};

std::optional<CreateVulkanBufferResult> CreateVulkanBuffer(
  IVulkanDevice const& vk_device,
  mnexus::BufferDesc const& buffer_desc
) {
  bool const mappable = buffer_desc.usage.HasAnyOf(mnexus::BufferUsageFlagBits::kMappable);

  // Vulkan Video usage flags require the corresponding KHR_video_* extensions
  // to have been enabled at device creation. Reject up-front with a clear log
  // instead of letting the driver surface a confusing usage validation error.
  bool const wants_video_decode = buffer_desc.usage.HasAnyOf(mnexus::BufferUsageFlagBits::kVideoDecodeSrc);
  bool const wants_video_encode = buffer_desc.usage.HasAnyOf(mnexus::BufferUsageFlagBits::kVideoEncodeDst);
  if (wants_video_decode && !vk_device.IsExtensionEnabled(VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME)) {
    MBASE_LOG_ERROR("BufferUsageFlagBits::kVideoDecodeSrc requires VK_KHR_video_decode_queue, which is not enabled on this device.");
    return std::nullopt;
  }
  if (wants_video_encode && !vk_device.IsExtensionEnabled(VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME)) {
    MBASE_LOG_ERROR("BufferUsageFlagBits::kVideoEncodeDst requires VK_KHR_video_encode_queue, which is not enabled on this device.");
    return std::nullopt;
  }

  VkBufferUsageFlags const vk_usage_flags = ToVkBufferUsageFlags(buffer_desc.usage) | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  // For video-bitstream buffers, decouple from any specific VkVideoProfileInfoKHR
  // by setting VIDEO_PROFILE_INDEPENDENT (added by VK_KHR_video_maintenance1).
  VkBufferCreateFlags const vk_create_flags = (wants_video_decode || wants_video_encode)
    ? VK_BUFFER_CREATE_VIDEO_PROFILE_INDEPENDENT_BIT_KHR
    : 0;

  VkBufferCreateInfo create_info {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = vk_create_flags,
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
    .minAlignment = 0,
  };

  VkBuffer vk_buffer_handle = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo allocation_info {};

  VkResult const result = vmaCreateBuffer(
    vma_allocator, &create_info, &alloc_info,
    &vk_buffer_handle, &allocation, &allocation_info
  );
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vmaCreateBuffer failed: {}", string_VkResult(result));
    return std::nullopt;
  }

  VulkanBuffer vk_buffer(
    vk_buffer_handle,
    [vk_buffer_handle, allocation, vma_allocator] {
      vmaDestroyBuffer(vma_allocator, vk_buffer_handle, allocation);
    },
    vk_device.GetDeferredDestroyer()
  );

  return CreateVulkanBufferResult {
    .vk_buffer = std::move(vk_buffer),
    .mapped_data = allocation_info.pMappedData,
    .vma_allocation = allocation,
  };
}

resource_pool::ResourceHandle EmplaceBufferResourcePool(
  BufferResourcePool& out_pool,
  IVulkanDevice const& vk_device,
  mnexus::BufferDesc const& buffer_desc
) {
  std::optional<CreateVulkanBufferResult> opt_result = CreateVulkanBuffer(vk_device, buffer_desc);
  if (!opt_result.has_value()) {
    return resource_pool::ResourceHandle::Null();
  }

  CreateVulkanBufferResult& result = *opt_result;

  BufferHot hot {
    .vk_buffer = std::move(result.vk_buffer),
    .mapped_data = result.mapped_data,
    .vma_allocation = result.vma_allocation,
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
