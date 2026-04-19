#pragma once

// c++ headers ------------------------------------------
#include <map>
#include <optional>

// public project headers -------------------------------
#include "mbase/public/container.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/command/pending_pipeline_barrier.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// ImageLayoutTracker
//
// Per-command-list, per-subresource image layout tracker.
// Accumulates layout transitions and flushes them as pipeline barriers via PendingPipelineBarrier.
//
// Usage:
//   1. Call TransitionToTransferDst / TransitionToShaderRead / etc. before recording commands
//   2. Call FlushPendingTransitions(barrier) before the commands that depend on the transitions
//   3. Call TransitionAllToDefaults() + FlushPendingTransitions() at command list End()
//

class ImageLayoutTracker final {
public:
  struct Subresource final {
    uint32_t mip_level = 0;
    uint32_t array_layer = 0;
  };

  /// Register an image with its creation info. Must be called before any Transition* calls for this image.
  /// The tracker uses VkImage as the map key, and derives default state from usage + format.
  void RegisterImage(VkImage vk_image, VkImageUsageFlags usage, VkFormat format, uint32_t mip_level_count, uint32_t array_layer_count);

  /// Transition a single subresource to the given layout with the given sync scope.
  void Transition(
    VkImage vk_image,
    Subresource const& subresource,
    VkPipelineStageFlags2KHR dst_stage_mask,
    VkAccessFlags2KHR dst_access_mask,
    VkImageLayout new_layout
  );

  /// Convenience: transition to TRANSFER_DST_OPTIMAL.
  void TransitionToTransferDst(VkImage vk_image, Subresource const& subresource);

  /// Convenience: transition to TRANSFER_SRC_OPTIMAL.
  void TransitionToTransferSrc(VkImage vk_image, Subresource const& subresource);

  /// Transition all tracked subresources back to their default state (derived from usage).
  void TransitionAllToDefaults();

  /// Flush all pending transitions to the given PendingPipelineBarrier.
  void FlushPendingTransitions(PendingPipelineBarrier& barrier);

  // --- Static helpers (also used by texture creation for initial layout transition) ---

  [[nodiscard]] static SyncScope GetDefaultSyncScope(VkImageUsageFlags usage, VkFormat format);
  [[nodiscard]] static VkImageLayout GetDefaultLayout(VkImageUsageFlags usage, VkFormat format);
  [[nodiscard]] static VkImageAspectFlags GetAspectMaskFromFormat(VkFormat format);

private:
  struct ImageInfo final {
    VkImageUsageFlags usage = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t mip_level_count = 1;
    uint32_t array_layer_count = 1;
  };

  struct Entry final {
    VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    SyncScope src_scope;

    VkImageLayout new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    SyncScope dst_scope;

    bool pending = false;
  };

  static constexpr uint32_t kExpectedSubresourceCount = 4;
  using EntryContainer = mbase::SmallVector<std::optional<Entry>, kExpectedSubresourceCount>;

  struct TrackedImage final {
    ImageInfo info;
    EntryContainer entries;
  };

  [[nodiscard]] static Entry MakeDefaultEntry(VkImageUsageFlags usage, VkFormat format);

  [[nodiscard]] static uint32_t GetSubresourceIndex(uint32_t mip_level_count, Subresource const& subresource);
  [[nodiscard]] static Subresource GetSubresource(uint32_t mip_level_count, uint32_t index);

  Entry& FindOrAddDefault(VkImage vk_image, Subresource const& subresource);

  std::map<VkImage, TrackedImage> tracked_images_;
};

} // namespace mnexus_backend::vulkan
