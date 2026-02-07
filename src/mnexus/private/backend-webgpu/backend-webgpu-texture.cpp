// TU header --------------------------------------------
#include "backend-webgpu/backend-webgpu-texture.h"

namespace mnexus_backend::webgpu {

wgpu::TextureViewDescriptor MakeWgpuTextureViewDesc(
  wgpu::TextureFormat wgpu_texture_format,
  wgpu::TextureViewDimension wgpu_texture_view_dimension,
  mnexus::TextureSubresourceRange const& subresource_range,
  wgpu::TextureAspect wgpu_texture_aspect
) {
  return wgpu::TextureViewDescriptor {
    .format = wgpu_texture_format,
    .dimension = wgpu_texture_view_dimension,
    .baseMipLevel = subresource_range.base_mip_level,
    .mipLevelCount = subresource_range.mip_level_count,
    .baseArrayLayer = subresource_range.base_array_layer,
    .arrayLayerCount = subresource_range.array_layer_count,
    .aspect = wgpu_texture_aspect,
    .usage = {},
  };
}

} // namespace mnexus_backend::webgpu
