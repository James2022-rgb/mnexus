#pragma once

// c++ headers ------------------------------------------
#include <optional>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/container.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// SyncScope
//
// Pipeline stage + access flags pair for synchronization2.
//

struct SyncScope final {
  VkPipelineStageFlags2KHR stage_mask = 0;
  VkAccessFlags2KHR access_mask = 0;
};

// ----------------------------------------------------------------------------------------------------
// PendingPipelineBarrier
//
// Accumulates image memory barriers and an optional global memory barrier,
// then flushes them all via a single vkCmdPipelineBarrier2KHR call.
//

class PendingPipelineBarrier final {
public:
  PendingPipelineBarrier() = default;
  ~PendingPipelineBarrier() = default;
  MBASE_DISALLOW_COPY_MOVE(PendingPipelineBarrier);

  struct ImageBarrier final {
    VkImage vk_image = VK_NULL_HANDLE;
    VkImageSubresourceRange subresource_range {};
    VkPipelineStageFlags2KHR src_stage_mask = 0;
    VkAccessFlags2KHR src_access_mask = 0;
    VkPipelineStageFlags2KHR dst_stage_mask = 0;
    VkAccessFlags2KHR dst_access_mask = 0;
    VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };

  struct GlobalBarrier final {
    VkPipelineStageFlags2KHR src_stage_mask = 0;
    VkAccessFlags2KHR src_access_mask = 0;
    VkPipelineStageFlags2KHR dst_stage_mask = 0;
    VkAccessFlags2KHR dst_access_mask = 0;
  };

  void AddImageMemoryBarrier(
    VkImage vk_image,
    VkImageSubresourceRange const& subresource_range,
    VkPipelineStageFlags2KHR src_stage_mask,
    VkAccessFlags2KHR src_access_mask,
    VkPipelineStageFlags2KHR dst_stage_mask,
    VkAccessFlags2KHR dst_access_mask,
    VkImageLayout old_layout,
    VkImageLayout new_layout
  );

  void AddGlobalMemoryBarrier(
    VkPipelineStageFlags2KHR src_stage_mask,
    VkAccessFlags2KHR src_access_mask,
    VkPipelineStageFlags2KHR dst_stage_mask,
    VkAccessFlags2KHR dst_access_mask
  );

  /// Emits vkCmdPipelineBarrier2KHR and clears accumulated barriers.
  /// No-op if no barriers are pending.
  void FlushAndClear(VkCommandBuffer command_buffer);

  [[nodiscard]] bool IsEmpty() const;

private:
  mbase::SmallVector<ImageBarrier, 4> image_barriers_;
  std::optional<GlobalBarrier> global_barrier_;
};

} // namespace mnexus_backend::vulkan
