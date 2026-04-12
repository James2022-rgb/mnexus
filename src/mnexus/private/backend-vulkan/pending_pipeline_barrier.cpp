// TU header --------------------------------------------
#include "backend-vulkan/pending_pipeline_barrier.h"

namespace mnexus_backend::vulkan {

void PendingPipelineBarrier::AddImageMemoryBarrier(
  VkImage vk_image,
  VkImageSubresourceRange const& subresource_range,
  VkPipelineStageFlags2KHR src_stage_mask,
  VkAccessFlags2KHR src_access_mask,
  VkPipelineStageFlags2KHR dst_stage_mask,
  VkAccessFlags2KHR dst_access_mask,
  VkImageLayout old_layout,
  VkImageLayout new_layout
) {
  image_barriers_.emplace_back(ImageBarrier {
    .vk_image = vk_image,
    .subresource_range = subresource_range,
    .src_stage_mask = src_stage_mask,
    .src_access_mask = src_access_mask,
    .dst_stage_mask = dst_stage_mask,
    .dst_access_mask = dst_access_mask,
    .old_layout = old_layout,
    .new_layout = new_layout,
  });
}

void PendingPipelineBarrier::AddGlobalMemoryBarrier(
  VkPipelineStageFlags2KHR src_stage_mask,
  VkAccessFlags2KHR src_access_mask,
  VkPipelineStageFlags2KHR dst_stage_mask,
  VkAccessFlags2KHR dst_access_mask
) {
  if (!global_barrier_.has_value()) {
    global_barrier_.emplace();
  }
  global_barrier_->src_stage_mask |= src_stage_mask;
  global_barrier_->src_access_mask |= src_access_mask;
  global_barrier_->dst_stage_mask |= dst_stage_mask;
  global_barrier_->dst_access_mask |= dst_access_mask;
}

void PendingPipelineBarrier::FlushAndClear(VkCommandBuffer command_buffer) {
  if (this->IsEmpty()) {
    return;
  }

  // Build VkImageMemoryBarrier2KHR array.
  mbase::SmallVector<VkImageMemoryBarrier2KHR, 4> vk_image_barriers;
  vk_image_barriers.reserve(image_barriers_.size());

  for (auto const& b : image_barriers_) {
    vk_image_barriers.emplace_back(VkImageMemoryBarrier2KHR {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
      .pNext = nullptr,
      .srcStageMask = b.src_stage_mask,
      .srcAccessMask = b.src_access_mask,
      .dstStageMask = b.dst_stage_mask,
      .dstAccessMask = b.dst_access_mask,
      .oldLayout = b.old_layout,
      .newLayout = b.new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = b.vk_image,
      .subresourceRange = b.subresource_range,
    });
  }

  // Build optional VkMemoryBarrier2KHR.
  VkMemoryBarrier2KHR vk_global_barrier {};
  if (global_barrier_.has_value()) {
    vk_global_barrier = VkMemoryBarrier2KHR {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
      .pNext = nullptr,
      .srcStageMask = global_barrier_->src_stage_mask,
      .srcAccessMask = global_barrier_->src_access_mask,
      .dstStageMask = global_barrier_->dst_stage_mask,
      .dstAccessMask = global_barrier_->dst_access_mask,
    };
  }

  VkDependencyInfoKHR dependency_info {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
    .pNext = nullptr,
    .dependencyFlags = 0,
    .memoryBarrierCount = global_barrier_.has_value() ? 1u : 0u,
    .pMemoryBarriers = global_barrier_.has_value() ? &vk_global_barrier : nullptr,
    .bufferMemoryBarrierCount = 0,
    .pBufferMemoryBarriers = nullptr,
    .imageMemoryBarrierCount = static_cast<uint32_t>(vk_image_barriers.size()),
    .pImageMemoryBarriers = vk_image_barriers.data(),
  };

  vkCmdPipelineBarrier2KHR(command_buffer, &dependency_info);

  // Clear.
  image_barriers_.clear();
  global_barrier_.reset();
}

bool PendingPipelineBarrier::IsEmpty() const {
  return image_barriers_.empty() && !global_barrier_.has_value();
}

} // namespace mnexus_backend::vulkan
