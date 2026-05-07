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

  /// Release queue family ownership: emits a barrier with the current src
  /// sync as wait scope, no dst sync (per QFOT release semantics), and the
  /// (current_qf -> dst_queue_family_index) ownership transfer. The new
  /// layout becomes the layout the receiving queue must specify in its
  /// matching Acquire.
  void TransitionRelease(
    VkImage vk_image,
    Subresource const& subresource,
    VkImageLayout new_layout,
    uint32_t current_queue_family_index,
    uint32_t dst_queue_family_index
  );

  /// Acquire queue family ownership: emits a barrier with no src sync (per
  /// QFOT acquire semantics) and the given dst sync, and the (src_queue_
  /// family_index -> current_qf) ownership transfer.
  ///
  /// The QFOT acquire half must declare exactly the same `oldLayout` and
  /// `newLayout` as the matching release half (Vulkan QFOT contract):
  /// - `pre_qfot_layout`  = release barrier's `oldLayout` (the layout the
  ///   image was in on the source queue immediately before its release).
  /// - `post_qfot_layout` = release barrier's `newLayout` (the layout the
  ///   image is in after the QFOT completes on this queue).
  void TransitionAcquire(
    VkImage vk_image,
    Subresource const& subresource,
    VkPipelineStageFlags2KHR dst_stage_mask,
    VkAccessFlags2KHR dst_access_mask,
    VkImageLayout pre_qfot_layout,
    VkImageLayout post_qfot_layout,
    uint32_t src_queue_family_index,
    uint32_t current_queue_family_index
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

    /// Queue family ownership transfer for the pending barrier (if any).
    /// Both default to `VK_QUEUE_FAMILY_IGNORED`; set to actual indices
    /// when the pending barrier is one half of a QFOT (release/acquire).
    /// Reset to IGNORED after flush.
    uint32_t src_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
    uint32_t dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED;

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
