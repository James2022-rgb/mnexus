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
#include "backend-vulkan/device/vk-instance.h"
#include "backend-vulkan/device/vk-device.h"
#include "backend-vulkan/object/vk-object-image.h"

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

struct SwapchainImage final {
  VulkanImage vk_image;
  VkSemaphore present_binary_semaphore = VK_NULL_HANDLE;
};

class WsiSwapchain final {
public:
  ~WsiSwapchain() = default;
  MBASE_DISALLOW_COPY_MOVE(WsiSwapchain);

  static WsiSwapchain Create(VulkanInstance const* vk_instance, IVulkanDevice const* vk_device);

  bool IsValid() const;
  VkSwapchainKHR GetVkSwapchainHandle() const { return vk_swapchain_handle_; }
  mnexus::TextureDesc const& GetTextureDesc() const;
  VkImageLayout GetDefaultVkImageLayout() const { return default_vk_image_layout_; }
  /// Color space the current swapchain was actually created with. Read after
  /// every (re)creation to confirm what was negotiated.
  mnexus::ColorSpace GetCurrentColorSpace() const { return current_color_space_; }

  bool OnSourceCreated(mnexus::SurfaceSourceDesc const& source_desc,
                       std::optional<VkSurfaceFormatKHR> opt_desired_surface_format = std::nullopt);
  void OnSourceDestroyed();

  /// Tears down the VkSwapchainKHR (and its semaphores / images) but leaves
  /// the underlying VkSurfaceKHR alive, then creates a new swapchain that
  /// targets `opt_desired_surface_format` (falling back to SDR if not
  /// supported on the surface's current monitor). Used for runtime HDR
  /// toggles and for monitor-change-triggered re-realizations.
  bool RecreateSwapchain(std::optional<VkSurfaceFormatKHR> opt_desired_surface_format);

  /// Snapshot of the (format, color_space) pairs the surface currently
  /// supports. Re-queries `vkGetPhysicalDeviceSurfaceFormatsKHR`, so the
  /// result reflects the monitor the window is currently on. Surface MUST
  /// be valid.
  mnexus::SurfaceCapability QuerySurfaceCapability() const;

  std::optional<std::pair<uint32_t, SwapchainImage const*>> AcquireNextImage(
    uint64_t timeout_ns,
    VkSemaphore nullable_signal_semaphore,
    VkFence nullable_signal_fence
  );

  std::optional<std::pair<uint32_t, SwapchainImage const*>> GetLastAcquiredImage() const;
  void ReturnImage(uint32_t image_index);

private:
  explicit WsiSwapchain(VulkanInstance const* vk_instance, IVulkanDevice const* vk_device)
    : vk_instance_(vk_instance), vk_device_(vk_device)
  {}

  /// Builds the VkSwapchainKHR + per-image semaphores + `texture_desc_` /
  /// `current_color_space_`. Surface MUST already be set in `surface_`.
  bool CreateSwapchainOnExistingSurface(std::optional<VkSurfaceFormatKHR> opt_desired_surface_format);

  /// Destroys the VkSwapchainKHR, per-image semaphores, and clears `images_`.
  /// Does NOT touch the surface.
  void DestroySwapchainOnly();

  VulkanInstance const* vk_instance_ = nullptr;
  IVulkanDevice const* vk_device_ = nullptr;

  std::optional<WsiSurface> surface_;
  VkSwapchainKHR vk_swapchain_handle_ = VK_NULL_HANDLE;
  std::vector<SwapchainImage> images_;
  mnexus::TextureDesc texture_desc_;
  mnexus::ColorSpace current_color_space_ = mnexus::ColorSpace::kSrgb;
  VkImageLayout default_vk_image_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;

  mbase::Lockable<std::mutex> mutable mutex_;
  std::optional<uint32_t> last_acquired_image_index_ MBASE_GUARDED_BY(mutex_);
};

} // namespace mnexus_backend::vulkan
