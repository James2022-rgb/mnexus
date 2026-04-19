// TU header --------------------------------------------
#include "backend-vulkan/wsi/vk-wsi_surface.h"

// public project headers -------------------------------
#include "mbase/public/platform.h"
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/resource/types_bridge.h"
#include "backend-vulkan/device/vk-physical_device.h"
#include "backend-vulkan/command/image_layout_tracker.h"

namespace mnexus_backend::vulkan {

namespace {

std::optional<VkSurfaceKHR> CreateVkSurface(VkInstance vk_instance_handle, mnexus::SurfaceSourceDesc const& surface_source_info) {
#if MBASE_PLATFORM_WINDOWS
  VkWin32SurfaceCreateInfoKHR create_info {
    .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .hinstance = reinterpret_cast<HINSTANCE>(surface_source_info.instance_handle),
    .hwnd = reinterpret_cast<HWND>(surface_source_info.window_handle),
  };

  VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
  VkResult const result = vkCreateWin32SurfaceKHR(vk_instance_handle, &create_info, nullptr, &vk_surface);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateWin32SurfaceKHR failed: {}", string_VkResult(result));
    return std::nullopt;
  }
  return vk_surface;
#elif MBASE_PLATFORM_LINUX
  VkXlibSurfaceCreateInfoKHR create_info {
    .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .dpy = reinterpret_cast<Display*>(surface_source_info.display_handle),
    .window = static_cast<Window>(surface_source_info.window_handle),
  };

  VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
  VkResult const result = vkCreateXlibSurfaceKHR(vk_instance_handle, &create_info, nullptr, &vk_surface);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateXlibSurfaceKHR failed: {}", string_VkResult(result));
    return std::nullopt;
  }
  return vk_surface;
#elif MBASE_PLATFORM_ANDROID
  VkAndroidSurfaceCreateInfoKHR create_info {
    .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .window = reinterpret_cast<ANativeWindow*>(surface_source_info.window_handle),
  };

  VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
  VkResult const result = vkCreateAndroidSurfaceKHR(vk_instance_handle, &create_info, nullptr, &vk_surface);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateAndroidSurfaceKHR failed: {}", string_VkResult(result));
    return std::nullopt;
  }
  return vk_surface;
#else
  MBASE_LOG_ERROR("Unsupported platform");
  return std::nullopt;
#endif
}

void DestroyVkSurface(VkInstance vk_instance_handle, VkSurfaceKHR vk_surface_handle) {
  vkDestroySurfaceKHR(vk_instance_handle, vk_surface_handle, nullptr);
}

}

PhysicalDeviceSurfaceSupportInfo PhysicalDeviceSurfaceSupportInfo::FromVkSurface(VkSurfaceKHR vk_surface_handle, VkPhysicalDevice vk_physical_device_handle) {
  VkSurfaceCapabilitiesKHR surface_capabilities;
  MBASE_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device_handle, vk_surface_handle, &surface_capabilities) == VK_SUCCESS);

  uint32_t format_count;
  MBASE_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device_handle, vk_surface_handle, &format_count, nullptr) == VK_SUCCESS);
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  MBASE_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device_handle, vk_surface_handle, &format_count, formats.data()) == VK_SUCCESS);

  uint32_t present_mode_count;
  MBASE_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physical_device_handle, vk_surface_handle, &present_mode_count, nullptr) == VK_SUCCESS);
  std::vector<VkPresentModeKHR> present_modes(present_mode_count);
  MBASE_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physical_device_handle, vk_surface_handle, &present_mode_count, present_modes.data()) == VK_SUCCESS);

  return PhysicalDeviceSurfaceSupportInfo {
    .surface_capabilities = surface_capabilities,
    .formats = std::move(formats),
    .present_modes = std::move(present_modes),
  };
}

std::optional<WsiSurface> WsiSurface::Create(VulkanInstance const* vk_instance, mnexus::SurfaceSourceDesc const& surface_source_desc) {
  std::optional<VkSurfaceKHR> opt_vk_surface_handle = CreateVkSurface(vk_instance->handle(), surface_source_desc);
  if (!opt_vk_surface_handle.has_value()) {
    return std::nullopt;
  }
  return WsiSurface(vk_instance, *opt_vk_surface_handle);
}

void WsiSurface::Destroy() {
  if (vk_surface_handle_ != VK_NULL_HANDLE) {
    DestroyVkSurface(vk_instance_->handle(), vk_surface_handle_);
    vk_surface_handle_ = VK_NULL_HANDLE;
  }
}

std::optional<VkExtent2D> WsiSurface::QuerySurfaceExtent(VkPhysicalDevice vk_physical_device_handle) const {
  VkSurfaceCapabilitiesKHR surface_capabilities;
  MBASE_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device_handle, vk_surface_handle_, &surface_capabilities) == VK_SUCCESS);
  if (surface_capabilities.currentExtent.width == UINT32_MAX && surface_capabilities.currentExtent.height == UINT32_MAX) {
    return std::nullopt;
  }
  return VkExtent2D {
    .width = surface_capabilities.currentExtent.width,
    .height = surface_capabilities.currentExtent.height
  };
}

PhysicalDeviceSurfaceSupportInfo WsiSurface::QueryPhysicalDeviceSurfaceSupportInfo(VkPhysicalDevice vk_physical_device_handle) const {
  MBASE_ASSERT(vk_surface_handle_ != VK_NULL_HANDLE);
  return PhysicalDeviceSurfaceSupportInfo::FromVkSurface(vk_surface_handle_, vk_physical_device_handle);
}

VulkanSwapchainConfiguration VulkanSwapchainConfiguration::Select(
  PhysicalDeviceSurfaceSupportInfo const& surface_support_info,
  std::optional<VkSurfaceFormatKHR> opt_desired_surface_format,
  std::optional<VkExtent2D> opt_client_area_extent,
  uint32_t queue_family_index
) {
  constexpr VkPresentModeKHR kPresentMode = VK_PRESENT_MODE_FIFO_KHR;

  // `maxImageCount == 0`` means there's no limit besides runtime memory usage.
  uint32_t min_image_count = surface_support_info.surface_capabilities.minImageCount + 1;
  if (surface_support_info.surface_capabilities.maxImageCount > 0) {
    min_image_count = std::min(min_image_count, surface_support_info.surface_capabilities.maxImageCount);
  }

  std::optional<VkSurfaceFormatKHR> opt_surface_format;

  if (opt_desired_surface_format.has_value()) {
    auto it = std::find_if(
      surface_support_info.formats.begin(),
      surface_support_info.formats.end(),
      [&opt_desired_surface_format](VkSurfaceFormatKHR const& surface_format) {
        return surface_format.format == opt_desired_surface_format.value().format && surface_format.colorSpace == opt_desired_surface_format.value().colorSpace;
      }
    );

    if (it != surface_support_info.formats.end()) {
      opt_surface_format = *it;
    }
    else {
      MBASE_LOG_WARN(
        "Desired surface format is not supported. Falling back to the default surface format. Desired: {} x {}",
        string_VkFormat(opt_desired_surface_format->format), string_VkColorSpaceKHR(opt_desired_surface_format->colorSpace)
      );
    }
  }

  if (!opt_surface_format.has_value()) {
    // Choose the surface format with R8G8B8A8_SRGB or B8G8R8A8_SRGB format and SRGB_NONLINEAR color space if available,
    // otherwise just use the first one.

    VkFormat image_format = surface_support_info.formats.front().format;
    VkColorSpaceKHR image_color_space = surface_support_info.formats.front().colorSpace;

    for (VkSurfaceFormatKHR const& surface_format : surface_support_info.formats) {
      if (surface_format.format == VK_FORMAT_R8G8B8A8_SRGB || surface_format.format == VK_FORMAT_B8G8R8A8_SRGB) {
        if (surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
          image_format = surface_format.format;
          image_color_space = surface_format.colorSpace;
          break;
        }
      }
    }

    opt_surface_format = VkSurfaceFormatKHR {
      .format = image_format,
      .colorSpace = image_color_space,
    };
  }

  VkSurfaceFormatKHR const surface_format = opt_surface_format.value();

  // `0xFFFFFFFF` indicates that the surface size will be determined by the extent of a swapchain targeting it i.e we get to choose as we like.
  VkExtent2D extent {};
  {
    VkExtent2D const& current_extent = surface_support_info.surface_capabilities.currentExtent;

    if (current_extent.width != UINT32_MAX || current_extent.height != UINT32_MAX) {
      extent = current_extent;
    }
    else {
      MBASE_ASSERT_MSG(opt_client_area_extent.has_value(), "Surface extent must be specified if the surface does not have a fixed size.");

      extent = opt_client_area_extent.value();
      extent.width  = std::clamp(extent.width, surface_support_info.surface_capabilities.minImageExtent.width, surface_support_info.surface_capabilities.maxImageExtent.width);
      extent.height = std::clamp(extent.height, surface_support_info.surface_capabilities.minImageExtent.height, surface_support_info.surface_capabilities.maxImageExtent.height);
    }
  }

  // Use the rightmost (least significant) bit.
  VkCompositeAlphaFlagBitsKHR const composite_alpha =
    static_cast<VkCompositeAlphaFlagBitsKHR>(1u << std::countr_zero(static_cast<uint32_t>(surface_support_info.surface_capabilities.supportedCompositeAlpha)));

  return VulkanSwapchainConfiguration {
    .min_image_count = min_image_count,
    .image_format = surface_format.format,
    .image_color_space = surface_format.colorSpace,
    .extent = extent,
    .composite_alpha = composite_alpha,
    .present_mode = kPresentMode,
    .queue_family_index = queue_family_index,
    .image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
  };
}

WsiSwapchain WsiSwapchain::Create(VulkanInstance const* vk_instance, IVulkanDevice const* vk_device) {
  return WsiSwapchain(vk_instance, vk_device);
}

bool WsiSwapchain::IsValid() const {
  MBASE_ASSERT_MSG(surface_.has_value() == (vk_swapchain_handle_ != VK_NULL_HANDLE), "Surface and swapchain handle state mismatch");
  return surface_.has_value();
}

mnexus::TextureDesc const& WsiSwapchain::GetTextureDesc() const {
  MBASE_ASSERT_MSG(this->IsValid(), "Swapchain is not valid");
  return texture_desc_;
}

bool WsiSwapchain::OnSourceCreated(mnexus::SurfaceSourceDesc const& source_desc) {
  MBASE_ASSERT_MSG(!surface_.has_value(), "Surface already created");

  std::optional<WsiSurface> opt_surface = WsiSurface::Create(vk_instance_, source_desc);
  if (!opt_surface.has_value()) {
    MBASE_LOG_ERROR("Failed to create WsiSurface for the swapchain.");
    return false;
  }

  surface_ = std::move(opt_surface.value());

  PhysicalDeviceSurfaceSupportInfo const support_info = surface_->QueryPhysicalDeviceSurfaceSupportInfo(vk_device_->physical_device_desc().handle());

  std::optional<VkExtent2D> const opt_client_area_extent = surface_->QuerySurfaceExtent(vk_device_->physical_device_desc().handle());

  VulkanSwapchainConfiguration configuration = VulkanSwapchainConfiguration::Select(
    support_info,
    /*opt_desired_surface_format=*/std::nullopt,
    opt_client_area_extent,
    vk_device_->queue_selection().present_capable.queue_family_index
  );

  VkSwapchainCreateInfoKHR create_info {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .surface = surface_->vk_surface_handle(),
    .minImageCount = configuration.min_image_count,
    .imageFormat = configuration.image_format,
    .imageColorSpace = configuration.image_color_space,
    .imageExtent = configuration.extent,
    .imageArrayLayers = 1,
    .imageUsage = configuration.image_usage,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    .compositeAlpha = configuration.composite_alpha,
    .presentMode = configuration.present_mode,
    .clipped = VK_TRUE,
    .oldSwapchain = VK_NULL_HANDLE,
  };

  VkResult result = vkCreateSwapchainKHR(vk_device_->handle(), &create_info, nullptr, &vk_swapchain_handle_);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateSwapchainKHR failed: {}", string_VkResult(result));
    vk_swapchain_handle_ = VK_NULL_HANDLE;
    return false;
  }

  std::vector<VkImage> vk_image_handles(configuration.min_image_count);
  result = vkGetSwapchainImagesKHR(vk_device_->handle(), vk_swapchain_handle_, &configuration.min_image_count, vk_image_handles.data());
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkGetSwapchainImagesKHR failed: {}", string_VkResult(result));
    vkDestroySwapchainKHR(vk_device_->handle(), vk_swapchain_handle_, nullptr);
    vk_swapchain_handle_ = VK_NULL_HANDLE;
    vk_images_.clear();
    return false;
  }

  vk_images_.reserve(configuration.min_image_count);
  for (VkImage vk_image_handle : vk_image_handles) {
    vk_images_.emplace_back(
      vk_image_handle,
      []() { /* Swapchain images are implicitly destroyed with the swapchain, so no explicit destruction needed. */ },
      nullptr,
      configuration.image_usage,
      configuration.image_format
    );
  }

  mnexus::TextureUsageFlags usage_flags;
  if (configuration.image_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    usage_flags |= mnexus::TextureUsageFlagBits::kAttachment;
  }
  if (configuration.image_usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
    usage_flags |= mnexus::TextureUsageFlagBits::kSampled;
  }
  if (configuration.image_usage & VK_IMAGE_USAGE_STORAGE_BIT) {
    usage_flags |= mnexus::TextureUsageFlagBits::kUnorderedAccess;
  }

  texture_desc_ = mnexus::TextureDesc {
    .usage = usage_flags,
    .format = FromVkFormat(configuration.image_format),
    .dimension = mnexus::TextureDimension::k2D,
    .width = configuration.extent.width,
    .height = configuration.extent.height,
    .depth = 1,
    .mip_level_count = 1,
    .array_layer_count = 1,
  };

  default_vk_image_layout_ = ImageLayoutTracker::GetDefaultLayout(configuration.image_usage, configuration.image_format);

  return true;
}

void WsiSwapchain::OnSourceDestroyed() {
  MBASE_ASSERT_MSG(surface_.has_value(), "Surface not created");

  vkDestroySwapchainKHR(vk_device_->handle(), vk_swapchain_handle_, nullptr);
  vk_swapchain_handle_ = VK_NULL_HANDLE;
  vk_images_.clear();

  surface_->Destroy();
  surface_ = std::nullopt;
}

std::optional<std::pair<uint32_t, VulkanImage const*>> WsiSwapchain::AcquireNextImage(
  uint64_t timeout_ns,
  VkSemaphore nullable_signal_semaphore,
  VkFence nullable_signal_fence
) {
  MBASE_ASSERT_MSG(this->IsValid(), "Swapchain is not valid");

  mbase::LockGuard lock(mutex_);

  VkAcquireNextImageInfoKHR const acquire_info {
    .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
    .pNext = nullptr,
    .swapchain = vk_swapchain_handle_,
    .timeout = timeout_ns,
    .semaphore = nullable_signal_semaphore,
    .fence = nullable_signal_fence,
    .deviceMask = 1,
  };

  uint32_t image_index = 0;
  VkResult const result = vkAcquireNextImage2KHR(vk_device_->handle(), &acquire_info, &image_index);
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    MBASE_LOG_ERROR("vkAcquireNextImage2KHR failed: {}", string_VkResult(result));
    return std::nullopt;
  }

  if (result == VK_SUBOPTIMAL_KHR) {
    MBASE_LOG_WARN("vkAcquireNextImage2KHR returned VK_SUBOPTIMAL_KHR");
  }

  last_acquired_image_index_ = image_index;

  return std::make_pair(image_index, &vk_images_[image_index]);
}

void WsiSwapchain::ReturnImage(uint32_t image_index) {
  mbase::LockGuard lock(mutex_);

  MBASE_ASSERT(last_acquired_image_index_.has_value() && last_acquired_image_index_.value() == image_index);

  last_acquired_image_index_ = std::nullopt;
}

std::optional<std::pair<uint32_t, VulkanImage const*>> WsiSwapchain::GetLastAcquiredImage() const {
  mbase::LockGuard lock(mutex_);
  if (!last_acquired_image_index_.has_value()) {
    return std::nullopt;
  }
  uint32_t image_index = last_acquired_image_index_.value();
  return std::make_pair(image_index, &vk_images_[image_index]);
}

} // namespace mnexus_backend::vulkan
