#pragma once

// c++ headers ------------------------------------------
#include <cstring>

// public project headers -------------------------------
#include "mbase/public/hash.h"

// project headers --------------------------------------
#include "cache/hash_cache.h"

#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/object/vk-object-image.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// ImageViewCacheKey
//

struct ImageViewCacheKey final {
  VkImage vk_image = VK_NULL_HANDLE;
  VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkImageSubresourceRange subresource_range {};
  /// If non-zero, the view's effective usage is restricted to these bits via
  /// `VkImageViewUsageCreateInfo`. Use this for video decode views of a
  /// SAMPLED multi-planar image to keep `VK_IMAGE_USAGE_SAMPLED_BIT` out of
  /// the view's usage so it does not require a `VkSamplerYcbcrConversion`.
  VkImageUsageFlags usage_override = 0;

  [[nodiscard]] size_t ComputeHash() const {
    mbase::HasherSizeT hasher;
    hasher.Do(reinterpret_cast<uint64_t>(vk_image));
    hasher.Do(static_cast<uint32_t>(view_type));
    hasher.Do(static_cast<uint32_t>(format));
    hasher.DoBytes(&subresource_range, sizeof(subresource_range));
    hasher.Do(static_cast<uint32_t>(usage_override));
    return hasher.Finish();
  }

  [[nodiscard]] bool operator==(ImageViewCacheKey const& other) const {
    return vk_image == other.vk_image &&
           view_type == other.view_type &&
           format == other.format &&
           std::memcmp(&subresource_range, &other.subresource_range, sizeof(subresource_range)) == 0 &&
           usage_override == other.usage_override;
  }

  struct Hasher final {
    size_t operator()(ImageViewCacheKey const& key) const {
      return key.ComputeHash();
    }
  };
};

// ----------------------------------------------------------------------------------------------------
// ImageViewCache
//

using ImageViewCache = THashCache<ImageViewCacheKey, VulkanImageViewPtr, ImageViewCacheKey::Hasher>;

} // namespace mnexus_backend::vulkan
