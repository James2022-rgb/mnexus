// TU header --------------------------------------------
#include "backend-vulkan/device/vk-queue.h"

// c++ headers ------------------------------------------
#include <atomic>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/device/vk-deferred_destroyer.h"

namespace mnexus_backend::vulkan {

// ====================================================================================================
// VulkanQueue
//

namespace {

class VulkanQueue final : public IVulkanQueue {
public:
  VulkanQueue(
    VkDevice vk_device_handle,
    VkQueue vk_queue,
    VkSemaphore timeline_semaphore,
    mnexus::QueueId const& queue_id,
    uint32_t compact_index,
    IVulkanDeferredDestroyer* deferred_destroyer
  ) :
    vk_device_handle_(vk_device_handle),
    vk_queue_(vk_queue),
    timeline_semaphore_(timeline_semaphore),
    queue_id_(queue_id),
    compact_index_(compact_index),
    deferred_destroyer_(deferred_destroyer)
  {}
  ~VulkanQueue() override = default;
  VulkanQueue(VulkanQueue const&) = delete;
  VulkanQueue& operator=(VulkanQueue const&) = delete;
  VulkanQueue(VulkanQueue&&) = delete;
  VulkanQueue& operator=(VulkanQueue&&) = delete;

  void Shutdown() override {
    if (timeline_semaphore_ != VK_NULL_HANDLE) {
      vkDestroySemaphore(vk_device_handle_, timeline_semaphore_, nullptr);
      timeline_semaphore_ = VK_NULL_HANDLE;
    }
  }

  mnexus::QueueId const& queue_id() const override { return queue_id_; }
  uint32_t compact_index() const override { return compact_index_; }

  uint64_t GetCompletedValue() override;
  void WaitSubmitSerial(uint64_t value) override;
  uint64_t WaitIdle() override;
  uint64_t AdvanceTimeline() override;
  uint64_t SubmitSingle(VkCommandBuffer command_buffer) override;

  uint64_t PresentSwapchainImage(
    uint32_t wait_semaphore_count,
    VkSemaphore const* wait_semaphores,
    uint64_t const* wait_values,
    VkSemaphore present_binary_semaphore,
    VkSwapchainKHR swapchain,
    uint32_t image_index
  ) override;

  uint64_t PresentSwapchainImage(
    uint64_t wait_serial,
    VkSemaphore present_binary_semaphore,
    VkSwapchainKHR swapchain,
    uint32_t image_index
  ) override;

private:
  VkDevice vk_device_handle_ = VK_NULL_HANDLE;
  VkQueue vk_queue_ = VK_NULL_HANDLE;
  VkSemaphore timeline_semaphore_ = VK_NULL_HANDLE;
  mnexus::QueueId queue_id_;
  uint32_t compact_index_ = 0;
  std::atomic<uint64_t> next_submit_serial_ {1}; // Valid serials start at 1.
  IVulkanDeferredDestroyer* deferred_destroyer_ = nullptr;
};

uint64_t VulkanQueue::GetCompletedValue() {
  uint64_t completed_value = 0;
  vkGetSemaphoreCounterValueKHR(vk_device_handle_, timeline_semaphore_, &completed_value);
  return completed_value;
}

void VulkanQueue::WaitSubmitSerial(uint64_t value) {
  if (value == 0) {
    return;
  }

  VkSemaphoreWaitInfoKHR wait_info {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .semaphoreCount = 1,
    .pSemaphores = &timeline_semaphore_,
    .pValues = &value,
  };
  vkWaitSemaphoresKHR(vk_device_handle_, &wait_info, UINT64_MAX);

  if (deferred_destroyer_ != nullptr) {
    deferred_destroyer_->Process();
  }
}

uint64_t VulkanQueue::WaitIdle() {
  uint64_t const last_submitted = next_submit_serial_.load(std::memory_order_acquire) - 1;
  this->WaitSubmitSerial(last_submitted);
  return last_submitted;
}

uint64_t VulkanQueue::AdvanceTimeline() {
  uint64_t const serial = next_submit_serial_.fetch_add(1, std::memory_order_acq_rel);

  if (deferred_destroyer_ != nullptr) {
    deferred_destroyer_->Process();
  }

  return serial;
}

uint64_t VulkanQueue::SubmitSingle(VkCommandBuffer command_buffer) {
  uint64_t const serial = next_submit_serial_.fetch_add(1, std::memory_order_acq_rel);

  VkCommandBufferSubmitInfoKHR cmd_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
    .pNext = nullptr,
    .commandBuffer = command_buffer,
    .deviceMask = 0,
  };

  VkSemaphoreSubmitInfoKHR signal_info {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
    .pNext = nullptr,
    .semaphore = timeline_semaphore_,
    .value = serial,
    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
    .deviceIndex = 0,
  };

  VkSubmitInfo2KHR submit_info {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
    .pNext = nullptr,
    .flags = 0,
    .waitSemaphoreInfoCount = 0,
    .pWaitSemaphoreInfos = nullptr,
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &cmd_info,
    .signalSemaphoreInfoCount = 1,
    .pSignalSemaphoreInfos = &signal_info,
  };

  VkResult const result = vkQueueSubmit2KHR(vk_queue_, 1, &submit_info, VK_NULL_HANDLE);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkQueueSubmit2KHR failed: {}", string_VkResult(result));
  }

  if (deferred_destroyer_ != nullptr) {
    deferred_destroyer_->Process();
  }

  return serial;
}

uint64_t VulkanQueue::PresentSwapchainImage(
  uint32_t wait_semaphore_count,
  VkSemaphore const* wait_semaphores,
  uint64_t const* wait_values,
  VkSemaphore present_binary_semaphore,
  VkSwapchainKHR swapchain,
  uint32_t image_index
) {
  uint64_t const serial = next_submit_serial_.fetch_add(1, std::memory_order_acq_rel);

  // vkQueuePresentKHR does not support timeline semaphores. To advance the
  // queue timeline we insert a command-less vkQueueSubmit2KHR that:
  //   - waits on the caller's timeline semaphores
  //   - signals the timeline semaphore with the new serial
  //   - signals a binary semaphore for present to wait on
  //
  // See https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html for why a single binary semaphore is NOT sufficient.

  // Build wait infos from the caller's timeline semaphores.
  std::vector<VkSemaphoreSubmitInfoKHR> wait_infos(wait_semaphore_count);
  for (uint32_t i = 0; i < wait_semaphore_count; ++i) {
    wait_infos[i] = {
      .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
      .pNext     = nullptr,
      .semaphore = wait_semaphores[i],
      .value     = wait_values[i],
      .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .deviceIndex = 0,
    };
  }

  // Signal both the timeline (serial tracking) and binary (for present).
  VkSemaphoreSubmitInfoKHR signal_infos[2] {
    {
      .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
      .pNext     = nullptr,
      .semaphore = timeline_semaphore_,
      .value     = serial,
      .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .deviceIndex = 0,
    },
    {
      .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
      .pNext     = nullptr,
      .semaphore = present_binary_semaphore,
      .value     = 0, // binary semaphore
      .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .deviceIndex = 0,
    },
  };

  VkSubmitInfo2KHR submit_info {
    .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
    .pNext                    = nullptr,
    .flags                    = 0,
    .waitSemaphoreInfoCount   = wait_semaphore_count,
    .pWaitSemaphoreInfos      = wait_infos.data(),
    .commandBufferInfoCount   = 0,
    .pCommandBufferInfos      = nullptr,
    .signalSemaphoreInfoCount = 2,
    .pSignalSemaphoreInfos    = signal_infos,
  };

  VkResult result = vkQueueSubmit2KHR(vk_queue_, 1, &submit_info, VK_NULL_HANDLE);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkQueueSubmit2KHR (pre-present) failed: {}", string_VkResult(result));
    return 0;
  }

  // Present, waiting on the binary semaphore.
  VkPresentInfoKHR present_info {
    .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext              = nullptr,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores    = &present_binary_semaphore,
    .swapchainCount     = 1,
    .pSwapchains        = &swapchain,
    .pImageIndices      = &image_index,
    .pResults           = nullptr,
  };

  result = vkQueuePresentKHR(vk_queue_, &present_info);
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    MBASE_LOG_ERROR("vkQueuePresentKHR failed: {}", string_VkResult(result));
  }

  if (deferred_destroyer_ != nullptr) {
    deferred_destroyer_->Process();
  }

  return serial;
}

uint64_t VulkanQueue::PresentSwapchainImage(
  uint64_t wait_serial,
  VkSemaphore present_binary_semaphore,
  VkSwapchainKHR swapchain,
  uint32_t image_index
) {
  return this->PresentSwapchainImage(
    1, &timeline_semaphore_, &wait_serial, present_binary_semaphore, swapchain, image_index
  );
}

} // namespace

// ----------------------------------------------------------------------------------------------------
// IVulkanQueue::Create
//

std::unique_ptr<IVulkanQueue> IVulkanQueue::Create(
  VkDevice vk_device_handle,
  VkQueue vk_queue,
  VkSemaphore timeline_semaphore,
  mnexus::QueueId const& queue_id,
  uint32_t compact_index,
  IVulkanDeferredDestroyer* deferred_destroyer
) {
  return std::make_unique<VulkanQueue>(
    vk_device_handle,
    vk_queue,
    timeline_semaphore,
    queue_id,
    compact_index,
    deferred_destroyer
  );
}

} // namespace mnexus_backend::vulkan
