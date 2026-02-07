#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-webgpu/include_dawn.h"

namespace mnexus_backend::webgpu {

wgpu::BufferUsage ToWgpuBufferUsage(mnexus::BufferUsageFlags usage);

wgpu::TextureUsage ToWgpuTextureUsage(mnexus::TextureUsageFlags usage);
mnexus::TextureUsageFlags FromWgpuTextureUsage(wgpu::TextureUsage usage);

wgpu::TextureDimension ToWgpuTextureDimension(mnexus::TextureDimension value);

wgpu::TextureFormat ToWgpuTextureFormat(MnFormat value);
MnFormat FromWgpuTextureFormat(wgpu::TextureFormat value);

} // namespace mnexus_backend::webgpu
