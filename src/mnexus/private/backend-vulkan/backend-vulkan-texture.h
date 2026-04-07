#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/vk-object.h"

namespace mnexus_backend::vulkan {

struct TextureHot final {
};

struct TextureCold final {
  mnexus::TextureDesc desc;
};

using TextureResourcePool = resource_pool::TResourceGenerationalPool<TextureHot, TextureCold, mnexus::kResourceTypeTexture>;

} // namespace mnexus_backend::vulkan
