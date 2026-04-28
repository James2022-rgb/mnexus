#pragma once

// public project headers -------------------------------
#include "mbase/public/accessor.h"

// project headers --------------------------------------
#include "backend-vulkan/object/vk-object.h"

namespace mnexus_backend::vulkan {

class VulkanImage final : public TVulkanObjectBase<VkImage> {
public:
  VulkanImage() = default;
  VulkanImage(VkImage handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer, VkImageUsageFlags vk_usage_flags, VkFormat vk_format, VkExtent3D extent) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer),
    vk_usage_flags_(vk_usage_flags),
    vk_format_(vk_format),
    extent_(extent)
  {
  }

  MBASE_ACCESSOR_GETV(VkImageUsageFlags, vk_usage_flags);
  MBASE_ACCESSOR_GETV(VkFormat, vk_format);
  MBASE_ACCESSOR_GETCR(VkExtent3D, extent);

private:
  VkImageUsageFlags vk_usage_flags_ = 0;
  VkFormat vk_format_ = VK_FORMAT_UNDEFINED;
  VkExtent3D extent_{};
};

class VulkanImageView final : public TVulkanObjectBase<VkImageView> {
public:
  VulkanImageView() = default;
  VulkanImageView(VkImageView handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }
};

using VulkanImageViewPtr = std::shared_ptr<VulkanImageView>;

} // namespace mnexus_backend::vulkan
