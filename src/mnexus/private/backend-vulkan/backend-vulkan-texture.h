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

// ----------------------------------------------------------------------------------------------------
// Sampler
//

class VulkanSampler final : public TVulkanObjectBase<VkSampler> {
public:
  VulkanSampler() = default;
  VulkanSampler(VkSampler handle, std::function<void()> destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {
  }
};

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
