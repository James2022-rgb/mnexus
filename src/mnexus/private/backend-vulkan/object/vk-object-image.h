#pragma once

// public project headers -------------------------------
#include "mbase/public/accessor.h"

// project headers --------------------------------------
#include "backend-vulkan/object/vk-object.h"

namespace mnexus_backend::vulkan {

class VulkanImage final : public TVulkanObjectBase<VkImage> {
public:
  VulkanImage() = default;
  VulkanImage(VkImage handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer, VkImageUsageFlags vk_usage_flags, VkFormat vk_format) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer),
    vk_usage_flags_(vk_usage_flags),
    vk_format_(vk_format)
  {
  }

  MBASE_ACCESSOR_GETV(VkImageUsageFlags, vk_usage_flags);
  MBASE_ACCESSOR_GETV(VkFormat, vk_format);

private:
  VkImageUsageFlags vk_usage_flags_ = 0;
  VkFormat vk_format_ = VK_FORMAT_UNDEFINED;
};

} // namespace mnexus_backend::vulkan
