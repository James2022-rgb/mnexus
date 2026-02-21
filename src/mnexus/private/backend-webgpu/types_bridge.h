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

// Sampler conversions.
wgpu::FilterMode ToWgpuFilterMode(mnexus::Filter value);
wgpu::MipmapFilterMode ToWgpuMipmapFilterMode(mnexus::Filter value);
wgpu::AddressMode ToWgpuAddressMode(mnexus::AddressMode value);

// Render state conversions.
wgpu::PrimitiveTopology ToWgpuPrimitiveTopology(mnexus::PrimitiveTopology value);
wgpu::CullMode ToWgpuCullMode(mnexus::CullMode value);
wgpu::FrontFace ToWgpuFrontFace(mnexus::FrontFace value);
wgpu::IndexFormat ToWgpuIndexFormat(mnexus::IndexType value);
wgpu::VertexStepMode ToWgpuVertexStepMode(mnexus::VertexStepMode value);
wgpu::VertexFormat ToWgpuVertexFormat(MnFormat value);
wgpu::LoadOp ToWgpuLoadOp(mnexus::LoadOp value);
wgpu::StoreOp ToWgpuStoreOp(mnexus::StoreOp value);
wgpu::CompareFunction ToWgpuCompareFunction(mnexus::CompareOp value);
wgpu::BlendFactor ToWgpuBlendFactor(mnexus::BlendFactor value);
wgpu::BlendOperation ToWgpuBlendOperation(mnexus::BlendOp value);
wgpu::ColorWriteMask ToWgpuColorWriteMask(mnexus::ColorWriteMask value);

} // namespace mnexus_backend::webgpu
