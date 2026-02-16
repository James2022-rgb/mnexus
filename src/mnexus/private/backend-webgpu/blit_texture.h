#pragma once

// project headers --------------------------------------
#include "backend-webgpu/include_dawn.h"
#include "mnexus/public/types.h"

#include <cstdint>

namespace mnexus_backend::webgpu::blit_texture {

void Initialize(wgpu::Device const& wgpu_device);
void Shutdown();

void BlitTexture2D(
  wgpu::Device const& wgpu_device,
  wgpu::CommandEncoder& command_encoder,
  wgpu::Texture const& src_texture,
  wgpu::TextureFormat src_format,
  mnexus::TextureSubresourceRange const& src_subresource,
  uint32_t src_offset_x, uint32_t src_offset_y,
  uint32_t src_extent_w, uint32_t src_extent_h,
  wgpu::Texture const& dst_texture,
  wgpu::TextureFormat dst_format,
  mnexus::TextureSubresourceRange const& dst_subresource,
  uint32_t dst_offset_x, uint32_t dst_offset_y,
  uint32_t dst_extent_w, uint32_t dst_extent_h,
  mnexus::Filter filter
);

} // namespace mnexus_backend::webgpu::blit_texture
