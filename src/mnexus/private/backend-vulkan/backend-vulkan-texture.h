#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/depend/vulkan_vma.h"
#include "backend-vulkan/vk-device.h"
#include "backend-vulkan/vk-object.h"

namespace mnexus_backend::vulkan {

enum class TextureClass : uint32_t {
  kRegular,
  kSwapchain,
};

class VulkanImage final : public TVulkanObjectBase<VkImage> {
public:
  VulkanImage() = default;
  VulkanImage(VkImage handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }
};

struct TextureHot final {
  TextureClass texture_class = TextureClass::kRegular;
  VulkanImage vk_image;

  void Stamp(uint32_t queue_compact_index, uint64_t serial) {
    this->vk_image.sync_stamp().Stamp(queue_compact_index, serial);
  }
};

struct TextureCold final {
  mnexus::TextureDesc desc;
};

using TextureResourcePool = resource_pool::TResourceGenerationalPool<TextureHot, TextureCold, mnexus::kResourceTypeTexture>;

resource_pool::ResourceHandle EmplaceTextureResourcePool(
  TextureResourcePool& out_pool,
  IVulkanDevice& vk_device,
  mnexus::TextureDesc const& texture_desc
);

} // namespace mnexus_backend::vulkan
