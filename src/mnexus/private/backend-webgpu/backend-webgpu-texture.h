#pragma once

// c++ headers ------------------------------------------
#include <mutex>

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"
#include "backend-webgpu/include_dawn.h"

namespace mnexus_backend::webgpu {

struct TextureHot final {
  // Can be null for the texture object representing the swapchain.
  wgpu::Texture wgpu_texture;
};
struct TextureCold final {
  mnexus::TextureDesc desc;
};

using TextureResourcePool = resource_pool::TResourceGenerationalPool<TextureHot, TextureCold, mnexus::kResourceTypeTexture>;

wgpu::TextureViewDescriptor MakeWgpuTextureViewDesc(
  wgpu::TextureFormat wgpu_texture_format,
  wgpu::TextureViewDimension wgpu_texture_view_dimension,
  mnexus::TextureSubresourceRange const& subresource_range,
  wgpu::TextureAspect wgpu_texture_aspect
);

} // namespace mnexus_backend::webgpu
