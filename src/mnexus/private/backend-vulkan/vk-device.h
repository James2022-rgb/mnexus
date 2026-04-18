#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <memory>

// public project headers -------------------------------
#include "mbase/public/array_proxy.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan_fwd.h"

// Forward declarations ---------------------------------
struct VmaAllocator_T;
typedef VmaAllocator_T* VmaAllocator;

namespace mnexus_backend {
class QueueIndexMap;
}

namespace mnexus_backend::vulkan {

class IVulkanDeferredDestroyer;
class StagingBufferPool;
class TransientCommandPool;
class ThreadCommandPoolRegistry;
class VulkanInstance;
class PhysicalDeviceDesc;

struct VulkanDeviceDesc final {
  PhysicalDeviceDesc const* physical_device_desc = nullptr;
  bool headless = false;
};

// ----------------------------------------------------------------------------------------------------
// IVulkanDevice
//
// Abstract interface for the Vulkan logical device.
//

class IVulkanDevice {
public:
  virtual ~IVulkanDevice() = default;

  static std::unique_ptr<IVulkanDevice> Create(
    VulkanInstance instance,
    VulkanDeviceDesc const& desc
  );

  virtual void Shutdown() = 0;

  // ----------------------------------------------------------------------------------------------
  // Accessors.

  [[nodiscard]] virtual VulkanInstance const* instance() const = 0;
  [[nodiscard]] virtual PhysicalDeviceDesc const& physical_device_desc() const = 0;
  [[nodiscard]] virtual VkDevice handle() const = 0;
  [[nodiscard]] virtual mnexus::QueueSelection const& queue_selection() const = 0;
  [[nodiscard]] virtual VmaAllocator vma_allocator() const = 0;

  [[nodiscard]] virtual bool IsExtensionEnabled(char const* extension_name) const = 0;

  /// Returns the deferred destroyer for enqueuing GPU resource cleanup.
  [[nodiscard]] virtual IVulkanDeferredDestroyer* GetDeferredDestroyer() const = 0;

  // ----------------------------------------------------------------------------------------------
  // Queue operations.

  /// Returns the highest completed serial on the given queue.
  [[nodiscard]] virtual uint64_t QueueGetCompletedValue(mnexus::QueueId const& queue_id) = 0;

  /// Blocks until the given serial has completed on the given queue.
  virtual void QueueWaitSubmitSerial(mnexus::QueueId const& queue_id, uint64_t value) = 0;

  /// Blocks until all submitted work on the given queue has completed.
  /// Returns the last submitted serial (0 if nothing has been submitted).
  [[nodiscard]] virtual uint64_t QueueWaitIdle(mnexus::QueueId const& queue_id) = 0;

  /// Advances the queue timeline without an actual GPU submit.
  /// Returns the new serial. Used for mappable buffer writes where
  /// the data is visible immediately after a host flush.
  [[nodiscard]] virtual uint64_t QueueAdvanceTimeline(mnexus::QueueId const& queue_id) = 0;

  /// Submits a command buffer to the given queue, signaling the timeline semaphore.
  /// Returns the new serial.
  [[nodiscard]] virtual uint64_t QueueSubmitSingle(mnexus::QueueId const& queue_id, VkCommandBuffer command_buffer) = 0;

  [[nodiscard]] virtual uint64_t QueuePresentSwapchainImage(
    mnexus::QueueId const& queue_id,
    uint32_t wait_semaphore_count,
    VkSemaphore const* wait_semaphores,
    uint64_t const* wait_values,
    VkSwapchainKHR swapchain,
    uint32_t image_index
  ) = 0;

  [[nodiscard]] virtual uint64_t QueuePresentSwapchainImage(
    mnexus::QueueId const& queue_id,
    uint64_t wait_serial,
    VkSwapchainKHR swapchain,
    uint32_t image_index
  ) = 0;

  // ----------------------------------------------------------------------------------------------
  // Sub-system accessors.

  [[nodiscard]] virtual StagingBufferPool& staging_buffer_pool() = 0;
  [[nodiscard]] virtual TransientCommandPool& transient_command_pool() = 0;
  [[nodiscard]] virtual ThreadCommandPoolRegistry& thread_command_pool_registry() = 0;
  [[nodiscard]] virtual QueueIndexMap const& queue_index_map() const = 0;

protected:
  IVulkanDevice() = default;
};

} // namespace mnexus_backend::vulkan
