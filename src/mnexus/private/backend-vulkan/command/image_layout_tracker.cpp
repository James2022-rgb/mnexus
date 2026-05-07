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
  // Access bits use the wildcard VK_ACCESS_2_MEMORY_READ/WRITE_BIT pair so the
  // default scope is valid regardless of the queue family the command list
  // runs on. Specific bits like VK_ACCESS_2_SHADER_READ_BIT have stage
  // compatibility constraints (e.g. VUID-VkImageMemoryBarrier2-srcAccessMask-07454)
  // that fail when ALL_COMMANDS expands to stages like VIDEO_DECODE that
  // do not support SHADER_READ; MEMORY_READ/WRITE have no such restriction.
  if (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
    if (vkuFormatIsDepthOrStencil(format)) {
      return {
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR,
        VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR
      };
    }
    return {
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR
    };
  }
  if (usage & VK_IMAGE_USAGE_STORAGE_BIT) {
    return {
      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR
    };
  }
  if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
    return {
      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      VK_ACCESS_2_MEMORY_READ_BIT_KHR
    };
  }
  if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
    return {
      VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
      VK_ACCESS_2_MEMORY_READ_BIT_KHR
    };
  }
  MBASE_LOG_ERROR("ImageLayoutTracker: cannot determine default sync scope for usage {}", usage);
  return {};
}

VkImageLayout ImageLayoutTracker::GetDefaultLayout(VkImageUsageFlags usage, VkFormat /*format*/) {
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
  mbase::Trap();
  return VK_IMAGE_LAYOUT_UNDEFINED;
}

ImageLayoutTracker::Entry ImageLayoutTracker::MakeDefaultEntry(VkImageUsageFlags usage, VkFormat format) {
  return Entry {
    .old_layout = GetDefaultLayout(usage, format),
    .src_scope = GetDefaultSyncScope(usage, format),
    .dst_scope = GetDefaultSyncScope(usage, format),
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
  if (vkuFormatIsColor(format)) {
    return VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if (vkuFormatPlaneCount(format) == 2) {
    return VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT;
  }
  MBASE_LOG_ERROR("ImageLayoutTracker: cannot determine aspect mask for format {}", string_VkFormat(format));
  return 0;
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

void ImageLayoutTracker::TransitionRelease(
  VkImage vk_image,
  Subresource const& subresource,
  VkImageLayout new_layout,
  uint32_t current_queue_family_index,
  uint32_t dst_queue_family_index
) {
  Entry& entry = this->FindOrAddDefault(vk_image, subresource);

  // QFOT release: src sync = whatever the image was last accessed with
  // (already in entry.src_scope), dst sync = NONE/0 (the destination queue
  // handles the dst side via Acquire). The layout fields declare the
  // transition that the QFOT performs; per Vulkan spec, release and
  // acquire halves must declare the same `oldLayout` / `newLayout`.
  entry.new_layout = new_layout;
  entry.dst_scope = SyncScope { 0, 0 };
  entry.src_queue_family_index = current_queue_family_index;
  entry.dst_queue_family_index = dst_queue_family_index;
  entry.pending = true;
}

void ImageLayoutTracker::TransitionAcquire(
  VkImage vk_image,
  Subresource const& subresource,
  VkPipelineStageFlags2KHR dst_stage_mask,
  VkAccessFlags2KHR dst_access_mask,
  VkImageLayout new_layout,
  uint32_t src_queue_family_index,
  uint32_t current_queue_family_index
) {
  // Acquire creates the entry as if the image just appeared in `new_layout`
  // on this queue, with no prior access scope to wait for. Force-overwrite
  // any pre-existing default entry; this barrier IS the first authoritative
  // state on this queue.
  auto it = tracked_images_.find(vk_image);
  MBASE_ASSERT_MSG(it != tracked_images_.end(), "ImageLayoutTracker: image not registered");

  auto& tracked = it->second;
  uint32_t const index = GetSubresourceIndex(tracked.info.mip_level_count, subresource);
  if (tracked.entries.size() <= index) {
    tracked.entries.resize(index + 1u);
  }

  tracked.entries[index] = Entry {
    .old_layout = new_layout,
    .src_scope  = SyncScope { 0, 0 },
    .new_layout = new_layout,
    .dst_scope  = SyncScope { dst_stage_mask, dst_access_mask },
    .src_queue_family_index = src_queue_family_index,
    .dst_queue_family_index = current_queue_family_index,
    .pending = true,
  };
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
        entry.new_layout,
        entry.src_queue_family_index,
        entry.dst_queue_family_index
      );

      // Advance state: the transition's destination becomes the new source.
      entry.old_layout = entry.new_layout;
      entry.src_scope = entry.dst_scope;

      entry.new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
      entry.dst_scope = {};
      // QFOT was a one-shot for this barrier; subsequent same-queue
      // transitions revert to non-QFOT (IGNORED / IGNORED).
      entry.src_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
      entry.dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
      entry.pending = false;
    }
  }
}

} // namespace mnexus_backend::vulkan
