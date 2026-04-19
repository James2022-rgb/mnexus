// TU header --------------------------------------------
#include "backend-vulkan/command/image_layout_tracker.h"

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/log.h"
#include "mbase/public/trap.h"

// external headers -------------------------------------
#include "vulkan/utility/vk_format_utils.h"

namespace mnexus_backend::vulkan {

// ====================================================================================================
// Default state helpers
//

SyncScope ImageLayoutTracker::GetDefaultSyncScope(VkImageUsageFlags usage, VkFormat format) {
  if (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
    if (vkuFormatIsDepthOrStencil(format)) {
      return {
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR
      };
    }
    return {
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR
    };
  }
  if (usage & VK_IMAGE_USAGE_STORAGE_BIT) {
    return {
      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR
    };
  }
  if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
    return {
      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      VK_ACCESS_2_SHADER_READ_BIT_KHR
    };
  }
  if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
    return {
      VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
      VK_ACCESS_2_TRANSFER_READ_BIT_KHR
    };
  }
  MBASE_LOG_ERROR("ImageLayoutTracker: cannot determine default sync scope for usage {}", usage);
  return {};
}

VkImageLayout ImageLayoutTracker::GetDefaultLayout(VkImageUsageFlags usage, VkFormat format) {
  if (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
    return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
  }
  if (usage & VK_IMAGE_USAGE_STORAGE_BIT) {
    return VK_IMAGE_LAYOUT_GENERAL;
  }
  if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
    return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;
  }
  if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  }
  MBASE_LOG_ERROR("ImageLayoutTracker: cannot determine default layout for usage {}", usage);
  return VK_IMAGE_LAYOUT_UNDEFINED;
}

ImageLayoutTracker::Entry ImageLayoutTracker::MakeDefaultEntry(VkImageUsageFlags usage, VkFormat format) {
  return Entry {
    .old_layout = GetDefaultLayout(usage, format),
    .src_scope = GetDefaultSyncScope(usage, format),
  };
}

uint32_t ImageLayoutTracker::GetSubresourceIndex(uint32_t mip_level_count, Subresource const& subresource) {
  return mip_level_count * subresource.array_layer + subresource.mip_level;
}

ImageLayoutTracker::Subresource ImageLayoutTracker::GetSubresource(uint32_t mip_level_count, uint32_t index) {
  return {
    .mip_level = index % mip_level_count,
    .array_layer = index / mip_level_count,
  };
}

VkImageAspectFlags ImageLayoutTracker::GetAspectMaskFromFormat(VkFormat format) {
  if (vkuFormatIsDepthOnly(format)) {
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  if (vkuFormatIsStencilOnly(format)) {
    return VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  if (vkuFormatIsDepthOrStencil(format)) {
    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  return VK_IMAGE_ASPECT_COLOR_BIT;
}

// ====================================================================================================
// Public API
//

void ImageLayoutTracker::RegisterImage(
  VkImage vk_image,
  VkImageUsageFlags usage,
  VkFormat format,
  uint32_t mip_level_count,
  uint32_t array_layer_count
) {
  if (tracked_images_.contains(vk_image)) {
    return; // Already registered.
  }
  tracked_images_.emplace(vk_image, TrackedImage {
    .info = ImageInfo {
      .usage = usage,
      .format = format,
      .mip_level_count = mip_level_count,
      .array_layer_count = array_layer_count,
    },
    .entries = {},
  });
}

ImageLayoutTracker::Entry& ImageLayoutTracker::FindOrAddDefault(
  VkImage vk_image,
  Subresource const& subresource
) {
  auto it = tracked_images_.find(vk_image);
  MBASE_ASSERT_MSG(it != tracked_images_.end(), "ImageLayoutTracker: image not registered");

  auto& tracked = it->second;
  uint32_t const index = GetSubresourceIndex(tracked.info.mip_level_count, subresource);

  if (tracked.entries.size() <= index) {
    tracked.entries.resize(index + 1u);
  }

  if (!tracked.entries[index].has_value()) {
    tracked.entries[index] = MakeDefaultEntry(tracked.info.usage, tracked.info.format);
  }

  return tracked.entries[index].value();
}

void ImageLayoutTracker::Transition(
  VkImage vk_image,
  Subresource const& subresource,
  VkPipelineStageFlags2KHR dst_stage_mask,
  VkAccessFlags2KHR dst_access_mask,
  VkImageLayout new_layout
) {
  Entry& entry = this->FindOrAddDefault(vk_image, subresource);

  entry.new_layout = new_layout;
  entry.dst_scope = { dst_stage_mask, dst_access_mask };
  entry.pending = true;
}

void ImageLayoutTracker::TransitionToTransferDst(VkImage vk_image, Subresource const& subresource) {
  this->Transition(
    vk_image, subresource,
    VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
    VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
  );
}

void ImageLayoutTracker::TransitionToTransferSrc(VkImage vk_image, Subresource const& subresource) {
  this->Transition(
    vk_image, subresource,
    VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
    VK_ACCESS_2_TRANSFER_READ_BIT_KHR,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  );
}

void ImageLayoutTracker::TransitionAllToDefaults() {
  for (auto& [vk_image, tracked] : tracked_images_) {
    for (uint32_t i = 0; i < tracked.entries.size(); ++i) {
      if (!tracked.entries[i].has_value()) {
        continue;
      }

      Entry& entry = tracked.entries[i].value();
      VkImageLayout const default_layout = GetDefaultLayout(tracked.info.usage, tracked.info.format);
      SyncScope const default_scope = GetDefaultSyncScope(tracked.info.usage, tracked.info.format);

      // Skip if already in default state.
      if (!entry.pending && entry.old_layout == default_layout) {
        continue;
      }

      entry.new_layout = default_layout;
      entry.dst_scope = default_scope;
      entry.pending = true;
    }
  }
}

void ImageLayoutTracker::FlushPendingTransitions(PendingPipelineBarrier& barrier) {
  for (auto& [vk_image, tracked] : tracked_images_) {
    VkImageAspectFlags const aspect_mask = GetAspectMaskFromFormat(tracked.info.format);

    for (uint32_t i = 0; i < tracked.entries.size(); ++i) {
      if (!tracked.entries[i].has_value()) {
        continue;
      }

      Entry& entry = tracked.entries[i].value();

      if (!entry.pending) {
        continue;
      }

      Subresource const subresource = GetSubresource(tracked.info.mip_level_count, i);

      barrier.AddImageMemoryBarrier(
        vk_image,
        VkImageSubresourceRange {
          .aspectMask = aspect_mask,
          .baseMipLevel = subresource.mip_level,
          .levelCount = 1,
          .baseArrayLayer = subresource.array_layer,
          .layerCount = 1,
        },
        entry.src_scope.stage_mask,
        entry.src_scope.access_mask,
        entry.dst_scope.stage_mask,
        entry.dst_scope.access_mask,
        entry.old_layout,
        entry.new_layout
      );

      // Advance state: the transition's destination becomes the new source.
      entry.old_layout = entry.new_layout;
      entry.src_scope = entry.dst_scope;

      entry.new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
      entry.dst_scope = {};
      entry.pending = false;
    }
  }
}

} // namespace mnexus_backend::vulkan
