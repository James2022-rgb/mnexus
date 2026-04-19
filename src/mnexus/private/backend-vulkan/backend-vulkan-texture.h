#pragma once

// c++ headers ------------------------------------------
#include <variant>

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "impl/impl_macros.h"

#include "backend-vulkan/depend/vulkan_vma.h"
#include "backend-vulkan/device/vk-device.h"
#include "backend-vulkan/object/vk-object-image.h"
#include "backend-vulkan/object/vk-object-sampler.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// Texture
//

// vk-wsi_surface.h
class WsiSwapchain;

struct TextureHotRegular final {
  VulkanImage vk_image;
};

struct TextureHotSwapchain final {
  WsiSwapchain const* swapchain = nullptr;
};

class TextureHot final {
public:
  explicit TextureHot(TextureHotRegular regular) : content_(std::move(regular)) {}
  explicit TextureHot(TextureHotSwapchain swapchain) : content_(std::move(swapchain)) {}
  ~TextureHot() = default;
  MBASE_DISALLOW_COPY_DEFAULT_MOVE(TextureHot);

  VulkanImage const& GetVkImage() const;

  void Stamp(uint32_t queue_compact_index, uint64_t serial);
private:
  std::variant<TextureHotRegular, TextureHotSwapchain> content_;
};

struct TextureColdRegular final {
  mnexus::TextureDesc desc;
};

struct TextureColdSwapchain final {
  /// The `TextureDesc` for swapchain textures is not stored in the `TextureColdSwapchain` since it's not meaningful until the swapchain becomes valid.
  /// Instead, we store a pointer to the swapchain, which can be used to query the surface format and extent when needed.
  WsiSwapchain const* swapchain = nullptr;
};

class TextureCold final {
public:
  explicit TextureCold(TextureColdRegular regular) : content_(std::move(regular)) {}
  explicit TextureCold(TextureColdSwapchain swapchain) : content_(std::move(swapchain)) {}
  ~TextureCold() = default;
  MBASE_DISALLOW_COPY_DEFAULT_MOVE(TextureCold);

  mnexus::TextureDesc const& GetTextureDesc() const;

  void GetDefaultState(VkImageLayout&out_layout) const;

private:
  std::variant<TextureColdRegular, TextureColdSwapchain> content_;
};

using TextureResourcePool = resource_pool::TResourceGenerationalPool<TextureHot, TextureCold, mnexus::kResourceTypeTexture>;

resource_pool::ResourceHandle EmplaceTextureResourcePool(
  TextureResourcePool& out_pool,
  IVulkanDevice& vk_device,
  mnexus::TextureDesc const& texture_desc
);

resource_pool::ResourceHandle EmplaceTextureResourcePoolSwapchain(
  TextureResourcePool& out_pool,
  WsiSwapchain const* swapchain
);

// ----------------------------------------------------------------------------------------------------
// Sampler
//

struct SamplerHot final {
  VulkanSampler vk_sampler;

  void Stamp(uint32_t queue_compact_index, uint64_t serial) {
    this->vk_sampler.sync_stamp().Stamp(queue_compact_index, serial);
  }
};

struct SamplerCold final {
  mnexus::SamplerDesc desc;
};

using SamplerResourcePool = resource_pool::TResourceGenerationalPool<SamplerHot, SamplerCold, mnexus::kResourceTypeSampler>;

resource_pool::ResourceHandle EmplaceSamplerResourcePool(
  SamplerResourcePool& out_pool,
  IVulkanDevice const& vk_device,
  mnexus::SamplerDesc const& sampler_desc
);

} // namespace mnexus_backend::vulkan
