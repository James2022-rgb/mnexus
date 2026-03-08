#pragma once

// c++ headers ------------------------------------------
#include <memory>

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/vk-physical_device.h"

namespace mnexus_backend::vulkan {

struct VulkanDeviceDesc final {
  PhysicalDeviceDesc const* physical_device_desc = nullptr;
  bool headless = false;
};

class VulkanDevice final {
public:
  ~VulkanDevice() = default;
  MBASE_DISALLOW_COPY_DEFAULT_MOVE(VulkanDevice);

  static std::optional<VulkanDevice> Create(
    VulkanInstance const& instance,
    VulkanDeviceDesc const& desc
  );

  void Shutdown();

private:
  explicit VulkanDevice(
    VulkanInstance const* instance,
    PhysicalDeviceDesc const& physical_device_desc,
    VkDevice handle,
    mnexus::QueueSelection queue_selection
  ) :
    instance_(instance),
    physical_device_desc_(std::make_unique<PhysicalDeviceDesc>(physical_device_desc)),
    queue_selection_(queue_selection),
    handle_(handle)
  {
  }

  VulkanInstance const* instance_ = nullptr;
  std::unique_ptr<PhysicalDeviceDesc> physical_device_desc_; // Holding a copy would make this class too large and pollute the cache, so we store it on the heap and keep a unique_ptr to it.
  VkDevice handle_ = VK_NULL_HANDLE;
  mnexus::QueueSelection queue_selection_;
};

} // namespace mnexus_backend::vulkan
