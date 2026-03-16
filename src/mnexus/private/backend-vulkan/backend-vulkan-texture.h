#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "container/resource_generational_pool.h"

namespace mnexus_backend::vulkan {

struct TextureHot final {
};

struct TextureCold final {
  mnexus::TextureDesc desc;
};

using TextureResourcePool = container::TResourceGenerationalPool<TextureHot, TextureCold>;

} // namespace mnexus_backend::vulkan
