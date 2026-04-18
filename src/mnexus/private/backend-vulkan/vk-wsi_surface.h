#pragma once

// c++ headers ------------------------------------------
#include <optional>
#include <vector>
#include <mutex>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/tsa.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/vk-instance.h"
#include "backend-vulkan/vk-device.h"
#include "backend-vulkan/vk-object-image.h"

namespace mnexus_backend::vulkan {

struct PhysicalDeviceSurfaceSupportInfo final {
  VkSurfaceCapabilitiesKHR surface_capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;

  static PhysicalDeviceSurfaceSupportInfo FromVkSurface(VkSurfaceKHR vk_surface_handle, VkPhysicalDevice vk_physical_device_handle);
};

class WsiSurface final {
public:
  explicit WsiSurface(VulkanInstance const* vk_instance, VkSurfaceKHR vk_surface_handle) :
    vk_instance_(vk_instance),
    vk_surface_handle_(vk_surface_handle)
  {
  }
  ~WsiSurface() = default;
  MBASE_DISALLOW_COPY_DEFAULT_MOVE(WsiSurface);

  static std::optional<WsiSurface> Create(VulkanInstance const* vk_instance, mnexus::SurfaceSourceDesc const& surface_source_desc);

  void Destroy();

  MBASE_ACCESSOR_GETV(VkSurfaceKHR, vk_surface_handle);

  std::optional<VkExtent2D> QuerySurfaceExtent(VkPhysicalDevice vk_physical_device_handle) const;

  PhysicalDeviceSurfaceSupportInfo QueryPhysicalDeviceSurfaceSupportInfo(VkPhysicalDevice vk_physical_device_handle) const;

private:
  VulkanInstance const* vk_instance_ = nullptr;
  VkSurfaceKHR vk_surface_handle_ = VK_NULL_HANDLE;
};

struct VulkanSwapchainConfiguration final {
  uint32_t min_image_count = 0;
  VkFormat image_format = VK_FORMAT_UNDEFINED;
  VkColorSpaceKHR image_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  VkExtent2D extent {};
  VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
  uint32_t queue_family_index = 0;
  VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  static VulkanSwapchainConfiguration Select(
    PhysicalDeviceSurfaceSupportInfo const& surface_support_info,
    std::optional<VkSurfaceFormatKHR> opt_desired_surface_format,
    std::optional<VkExtent2D> opt_client_area_extent,
    uint32_t queue_family_index
  );
};

class WsiSwapchain final {
public:
  ~WsiSwapchain() = default;
  MBASE_DISALLOW_COPY_DEFAULT_MOVE(WsiSwapchain);

  static WsiSwapchain Create(VulkanInstance const* vk_instance, IVulkanDevice const* vk_device);

  bool IsValid() const;
  VkSwapchainKHR GetVkSwapchainHandle() const { return vk_swapchain_handle_; }
  mnexus::TextureDesc const& GetTextureDesc() const;
  VkImageLayout GetDefaultVkImageLayout() const { return default_vk_image_layout_; }

  bool OnSourceCreated(mnexus::SurfaceSourceDesc const& source_desc);
  void OnSourceDestroyed();

  std::optional<std::pair<uint32_t, VulkanImage const*>> AcquireNextImage(
    uint64_t timeout_ns,
    VkSemaphore nullable_signal_semaphore,
    VkFence nullable_signal_fence
  );

  std::optional<std::pair<uint32_t, VulkanImage const*>> GetLastAcquiredImage() const;
  void ReturnImage(uint32_t image_index);

private:
  explicit WsiSwapchain(VulkanInstance const* vk_instance, IVulkanDevice const* vk_device)
    : vk_instance_(vk_instance), vk_device_(vk_device)
  {}

  VulkanInstance const* vk_instance_ = nullptr;
  IVulkanDevice const* vk_device_ = nullptr;

  std::optional<WsiSurface> surface_;
  VkSwapchainKHR vk_swapchain_handle_ = VK_NULL_HANDLE;
  std::vector<VulkanImage> vk_images_;
  mnexus::TextureDesc texture_desc_;
  VkImageLayout default_vk_image_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;

  mbase::Lockable<std::mutex> mutable mutex_;
  std::optional<uint32_t> last_acquired_image_index_ MBASE_GUARDED_BY(mutex_);
};

} // namespace mnexus_backend::vulkan
