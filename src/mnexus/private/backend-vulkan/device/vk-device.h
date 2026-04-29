#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <memory>

// public project headers -------------------------------
#include "mbase/public/array_proxy.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan_fwd.h"

#include "backend-vulkan/device/fwd.h"

// Forward declarations ---------------------------------
struct VmaAllocator_T;
typedef VmaAllocator_T* VmaAllocator;

namespace mnexus_backend::vulkan {

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
  // Queue access.

  /// Returns the queue for the given QueueId, or nullptr if the QueueId is
  /// not part of this device's QueueSelection.
  [[nodiscard]] virtual IVulkanQueue* GetQueue(mnexus::QueueId const& queue_id) = 0;

protected:
  IVulkanDevice() = default;
};

} // namespace mnexus_backend::vulkan
