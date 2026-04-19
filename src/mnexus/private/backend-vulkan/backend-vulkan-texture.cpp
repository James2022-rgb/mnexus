// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-texture.h"

// c++ headers ------------------------------------------
#include <optional>

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/command/image_layout_tracker.h"
#include "backend-vulkan/resource/types_bridge.h"
#include "backend-vulkan/device/vk-staging.h"
#include "backend-vulkan/wsi/vk-wsi_surface.h"

namespace mnexus_backend::vulkan {

// ====================================================================================================
// Texture
//

VulkanImage const& TextureHot::GetVkImage() const {
  struct Visitor {
    VulkanImage const& operator()(TextureHotRegular const& regular) const {
      return regular.vk_image;
    }
    VulkanImage const& operator()(TextureHotSwapchain const& swapchain) const {
      auto opt_last_acquired = swapchain.swapchain->GetLastAcquiredImage();
      if (!opt_last_acquired.has_value()) {
        MBASE_LOG_ERROR("Attempted to get VkImage from swapchain texture, but no image is currently acquired");
        static VulkanImage dummy_image;
        return dummy_image;
      }

      return *(opt_last_acquired->second);
    }
  };
  return std::visit(
    Visitor{},
    content_
  );
}

void TextureHot::Stamp(uint32_t queue_compact_index, uint64_t serial) {
  struct Visitor {
    uint32_t queue_compact_index;
    uint64_t serial;

    void operator()(TextureHotRegular& regular) const {
      regular.vk_image.sync_stamp().Stamp(queue_compact_index, serial);
    }
    void operator()(TextureHotSwapchain& swapchain) const {
      // Swapchain image stamping is no-op since we don't track swapchain image usage with sync stamps.
    }
  };

  std::visit(
    Visitor{queue_compact_index, serial},
    content_
  );
}

mnexus::TextureDesc const& TextureCold::GetTextureDesc() const {
  struct Visitor {
    mnexus::TextureDesc const& operator()(TextureColdRegular const& regular) const {
      return regular.desc;
    }
    mnexus::TextureDesc const& operator()(TextureColdSwapchain const& swapchain) const {
      MBASE_ASSERT(swapchain.swapchain->IsValid());
      return swapchain.swapchain->GetTextureDesc();
    }
  };
  return std::visit(
    Visitor{},
    content_
  );
}

void TextureCold::GetDefaultState(VkImageLayout& out_layout) const {
  struct Visitor {
    VkImageLayout* out_layout;
    void operator()(TextureColdRegular const& regular) const {
      // No-op.
      mbase::Trap();
    }
    void operator()(TextureColdSwapchain const& swapchain) const {
      *out_layout = swapchain.swapchain->GetDefaultVkImageLayout();
    }
  };
  return std::visit(
    Visitor{&out_layout},
    content_
  );
}

namespace {

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
    vk_device.GetDeferredDestroyer(),
    create_info.usage,
    create_info.format
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

} // namespace

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

  VkImageUsageFlags const vk_usage_flags = result.vk_image.vk_usage_flags();
  VkFormat const vk_format = result.vk_image.vk_format();

  VkImageLayout const default_layout = ImageLayoutTracker::GetDefaultLayout(vk_usage_flags, vk_format);
  SyncScope const default_scope = ImageLayoutTracker::GetDefaultSyncScope(vk_usage_flags, vk_format);
  VkImageAspectFlags const aspect_mask = ImageLayoutTracker::GetAspectMaskFromFormat(vk_format);

  TextureHot hot(TextureHotRegular{ .vk_image = std::move(result.vk_image) });
  TextureCold cold(TextureColdRegular{ .desc = texture_desc });

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

resource_pool::ResourceHandle EmplaceTextureResourcePoolSwapchain(
  TextureResourcePool& out_pool,
  WsiSwapchain const* swapchain
) {
  TextureHot hot(TextureHotSwapchain{ .swapchain = swapchain });
  TextureCold cold(TextureColdSwapchain{ .swapchain = swapchain });

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

// ====================================================================================================
// Sampler
//

resource_pool::ResourceHandle EmplaceSamplerResourcePool(
  SamplerResourcePool& out_pool,
  IVulkanDevice const& vk_device,
  mnexus::SamplerDesc const& sampler_desc
) {
  VkSamplerCreateInfo create_info {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .magFilter = ToVkFilter(sampler_desc.mag_filter),
    .minFilter = ToVkFilter(sampler_desc.min_filter),
    .mipmapMode = ToVkSamplerMipmapMode(sampler_desc.mipmap_filter),
    .addressModeU = ToVkSamplerAddressMode(sampler_desc.address_mode_u),
    .addressModeV = ToVkSamplerAddressMode(sampler_desc.address_mode_v),
    .addressModeW = ToVkSamplerAddressMode(sampler_desc.address_mode_w),
    .mipLodBias = 0.0f,
    .anisotropyEnable = VK_FALSE,
    .maxAnisotropy = 1.0f,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .minLod = 0.0f,
    .maxLod = VK_LOD_CLAMP_NONE,
    .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
  };

  VkSampler vk_sampler_handle = VK_NULL_HANDLE;
  VkResult const result = vkCreateSampler(vk_device.handle(), &create_info, nullptr, &vk_sampler_handle);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateSampler failed: {}", string_VkResult(result));
    return resource_pool::ResourceHandle::Null();
  }

  VkDevice vk_device_handle = vk_device.handle();
  VulkanSampler vk_sampler(
    vk_sampler_handle,
    [vk_device_handle, vk_sampler_handle] {
      vkDestroySampler(vk_device_handle, vk_sampler_handle, nullptr);
    },
    vk_device.GetDeferredDestroyer()
  );

  SamplerHot hot { .vk_sampler = std::move(vk_sampler) };
  SamplerCold cold { .desc = sampler_desc };

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

} // namespace mnexus_backend::vulkan
