// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-texture.h"

// c++ headers ------------------------------------------
#include <optional>

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/image_layout_tracker.h"
#include "backend-vulkan/types_bridge.h"
#include "backend-vulkan/vk-staging.h"

namespace mnexus_backend::vulkan {

struct CreateVulkanImageResult {
  VulkanImage vk_image;
  VmaAllocation vma_allocation = VK_NULL_HANDLE;
};

std::optional<CreateVulkanImageResult> CreateVulkanImage(
  IVulkanDevice& vk_device,
  mnexus::TextureDesc const& texture_desc
) {
  VkFormat const vk_format = ToVkFormat(texture_desc.format);

  VkImageType const vk_image_type = ToVkImageType(texture_desc.dimension);

  VkImageCreateFlags flags = 0;
  if (texture_desc.dimension == mnexus::TextureDimension::kCube) {
    flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  }

  VkImageCreateInfo create_info {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = flags,
    .imageType = vk_image_type,
    .format = vk_format,
    .extent = VkExtent3D {
      .width = texture_desc.width,
      .height = texture_desc.height,
      .depth = texture_desc.depth,
    },
    .mipLevels = texture_desc.mip_level_count,
    .arrayLayers = texture_desc.array_layer_count,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = ToVkImageUsageFlags(texture_desc.usage, vk_format),
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
  };

  VmaAllocator const vma_allocator = vk_device.vma_allocator();

  VmaAllocationCreateInfo alloc_info {
    .flags = 0,
    .usage = VMA_MEMORY_USAGE_AUTO,
    .requiredFlags = 0,
    .preferredFlags = 0,
    .memoryTypeBits = 0,
    .pool = VK_NULL_HANDLE,
    .pUserData = nullptr,
    .priority = 0.0f,
  };

  VkImage vk_image_handle = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo allocation_info {};

  VkResult const result = vmaCreateImage(
    vma_allocator, &create_info, &alloc_info,
    &vk_image_handle, &allocation, &allocation_info
  );
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vmaCreateImage failed: {}", string_VkResult(result));
    return std::nullopt;
  }

  VkDevice vk_device_handle = vk_device.handle();
  VulkanImage vk_image(
    vk_image_handle,
    [vk_device_handle, vk_image_handle, allocation, vma_allocator] {
      vmaDestroyImage(vma_allocator, vk_image_handle, allocation);
    },
    vk_device.GetDeferredDestroyer()
  );

  // Transition from UNDEFINED to the default layout for this image's usage.
  // This must happen before any command list uses the image, because the
  // ImageLayoutTracker assumes images start in their default layout.
  {
    VkImageLayout const default_layout = ImageLayoutTracker::GetDefaultLayout(create_info.usage, vk_format);
    SyncScope const default_scope = ImageLayoutTracker::GetDefaultSyncScope(create_info.usage, vk_format);
    VkImageAspectFlags const aspect_mask = ImageLayoutTracker::GetAspectMaskFromFormat(vk_format);

    VkCommandBuffer cb = vk_device.transient_command_pool().Acquire();

    VkImageMemoryBarrier2KHR barrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
      .pNext = nullptr,
      .srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR,
      .srcAccessMask = VK_ACCESS_2_NONE_KHR,
      .dstStageMask = default_scope.stage_mask,
      .dstAccessMask = default_scope.access_mask,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = default_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = vk_image_handle,
      .subresourceRange = {
        .aspectMask = aspect_mask,
        .baseMipLevel = 0,
        .levelCount = texture_desc.mip_level_count,
        .baseArrayLayer = 0,
        .layerCount = texture_desc.array_layer_count,
      },
    };

    VkDependencyInfoKHR dep {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
      .pNext = nullptr,
      .dependencyFlags = 0,
      .memoryBarrierCount = 0,
      .pMemoryBarriers = nullptr,
      .bufferMemoryBarrierCount = 0,
      .pBufferMemoryBarriers = nullptr,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2KHR(cb, &dep);
    vkEndCommandBuffer(cb);

    mnexus::QueueId const queue_id = vk_device.queue_selection().present_capable;
    uint64_t const serial = vk_device.QueueSubmitSingle(queue_id, cb);
    vk_device.transient_command_pool().Release(cb, queue_id, serial);

    // FIXME: Blocking wait. Replace with cross-queue timeline semaphore waits
    // (SubmitWaitAnotherQueueSubmissionId) to avoid stalling the CPU.
    vk_device.QueueWaitSubmitSerial(queue_id, serial);
  }

  return CreateVulkanImageResult {
    .vk_image = std::move(vk_image),
    .vma_allocation = allocation,
  };
}

resource_pool::ResourceHandle EmplaceTextureResourcePool(
  TextureResourcePool& out_pool,
  IVulkanDevice& vk_device,
  mnexus::TextureDesc const& texture_desc
) {
  std::optional<CreateVulkanImageResult> opt_result = CreateVulkanImage(vk_device, texture_desc);
  if (!opt_result.has_value()) {
    return resource_pool::ResourceHandle::Null();
  }

  CreateVulkanImageResult result = std::move(opt_result.value());

  TextureHot hot {
    .vk_image = std::move(result.vk_image),
  };
  TextureCold cold {
    .desc = texture_desc,
  };

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

} // namespace mnexus_backend::vulkan
