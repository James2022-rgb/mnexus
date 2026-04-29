#pragma once

// c++ headers ------------------------------------------
#include <cstdint>
#include <memory>

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan_fwd.h"

#include "backend-vulkan/device/fwd.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// IVulkanQueue
//
// Abstract interface for a single Vulkan queue. Each queue owns its own
// timeline semaphore and submit serial counter; QueueId resolution is
// performed once at acquisition time via IVulkanDevice::GetQueue.
//

class IVulkanQueue {
public:
  virtual ~IVulkanQueue() = default;

  /// Construct a fully-initialized queue. The caller has already obtained
  /// the VkQueue via vkGetDeviceQueue and created the timeline semaphore.
  /// The queue takes no ownership of `vk_device_handle`, `vk_queue`, or
  /// `deferred_destroyer`. It owns `timeline_semaphore` and destroys it
  /// in Shutdown().
  static std::unique_ptr<IVulkanQueue> Create(
    VkDevice vk_device_handle,
    VkQueue vk_queue,
    VkSemaphore timeline_semaphore,
    mnexus::QueueId const& queue_id,
    uint32_t compact_index,
    IVulkanDeferredDestroyer* deferred_destroyer
  );

  /// Destroys the timeline semaphore. Caller is responsible for ensuring
  /// the GPU is idle (vkDeviceWaitIdle) before calling Shutdown.
  virtual void Shutdown() = 0;

  // Identity.
  [[nodiscard]] virtual mnexus::QueueId const& queue_id() const = 0;
  [[nodiscard]] virtual uint32_t compact_index() const = 0;

  // Timeline semaphore queries.
  [[nodiscard]] virtual uint64_t GetCompletedValue() = 0;
  virtual void WaitSubmitSerial(uint64_t value) = 0;
  [[nodiscard]] virtual uint64_t WaitIdle() = 0;

  // Submit / advance.
  [[nodiscard]] virtual uint64_t AdvanceTimeline() = 0;
  [[nodiscard]] virtual uint64_t SubmitSingle(VkCommandBuffer command_buffer) = 0;

  // Present.
  virtual uint64_t PresentSwapchainImage(
    uint32_t wait_semaphore_count,
    VkSemaphore const* wait_semaphores,
    uint64_t const* wait_values,
    VkSemaphore present_binary_semaphore,
    VkSwapchainKHR swapchain,
    uint32_t image_index
  ) = 0;

  virtual uint64_t PresentSwapchainImage(
    uint64_t wait_serial,
    VkSemaphore present_binary_semaphore,
    VkSwapchainKHR swapchain,
    uint32_t image_index
  ) = 0;

protected:
  IVulkanQueue() = default;
};

} // namespace mnexus_backend::vulkan
