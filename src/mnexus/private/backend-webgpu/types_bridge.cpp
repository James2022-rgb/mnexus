// TU header --------------------------------------------
#include "backend-webgpu/types_bridge.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-webgpu/webgpu_format.h"

namespace mnexus_backend::webgpu {

wgpu::BufferUsage ToWgpuBufferUsage(mnexus::BufferUsageFlags usage) {
  wgpu::BufferUsage result = wgpu::BufferUsage::None;

  if (usage & mnexus::BufferUsageFlagBits::kVertex) {
    result |= wgpu::BufferUsage::Vertex;
  }
  if (usage & mnexus::BufferUsageFlagBits::kIndex) {
    result |= wgpu::BufferUsage::Index;
  }
  if (usage & mnexus::BufferUsageFlagBits::kUniform) {
    result |= wgpu::BufferUsage::Uniform;
  }
  if (usage & mnexus::BufferUsageFlagBits::kStorage) {
    result |= wgpu::BufferUsage::Storage;
  }
  if (usage & mnexus::BufferUsageFlagBits::kTransferSrc) {
    result |= wgpu::BufferUsage::CopySrc;
  }
  if (usage & mnexus::BufferUsageFlagBits::kTransferDst) {
    result |= wgpu::BufferUsage::CopyDst;
  }
  if (usage & mnexus::BufferUsageFlagBits::kIndirect) {
    result |= wgpu::BufferUsage::Indirect;
  }

  // Auto-add CopyDst for GPU buffers to enable WriteBuffer updates
  if (usage & (mnexus::BufferUsageFlagBits::kUniform |
               mnexus::BufferUsageFlagBits::kStorage |
               mnexus::BufferUsageFlagBits::kVertex |
               mnexus::BufferUsageFlagBits::kIndex)) {
    result |= wgpu::BufferUsage::CopyDst;
  }

  // Auto-add CopySrc for Storage buffers to enable ReadBuffer.
  if (usage & mnexus::BufferUsageFlagBits::kStorage) {
    result |= wgpu::BufferUsage::CopySrc;
  }

  // Auto-add Storage for TransferSrc buffers to enable internal compute-based row repacking.
  if (usage & mnexus::BufferUsageFlagBits::kTransferSrc) {
    result |= wgpu::BufferUsage::Storage;
  }

  return result;
}

wgpu::TextureUsage ToWgpuTextureUsage(mnexus::TextureUsageFlags usage) {
  wgpu::TextureUsage result = wgpu::TextureUsage::None;
  if (usage & mnexus::TextureUsageFlagBits::kAttachment) {
    result |= wgpu::TextureUsage::RenderAttachment;
  }
  if (usage & mnexus::TextureUsageFlagBits::kSampled) {
    result |= wgpu::TextureUsage::TextureBinding;
  }
  if (usage & mnexus::TextureUsageFlagBits::kUnorderedAccess) {
    result |= wgpu::TextureUsage::StorageBinding;
  }
  if (usage & mnexus::TextureUsageFlagBits::kTransferSrc) {
    result |= wgpu::TextureUsage::CopySrc;
  }
  if (usage & mnexus::TextureUsageFlagBits::kTransferDst) {
    result |= wgpu::TextureUsage::CopyDst;
  }

  // Auto-add TextureBinding for TransferSrc textures to enable internal blit sampling.
  if (usage & mnexus::TextureUsageFlagBits::kTransferSrc) {
    result |= wgpu::TextureUsage::TextureBinding;
  }

  // Auto-add RenderAttachment for TransferDst textures to enable render-pipeline-based BlitTexture.
  if (usage & mnexus::TextureUsageFlagBits::kTransferDst) {
    result |= wgpu::TextureUsage::RenderAttachment;
  }

  return result;
}

mnexus::TextureUsageFlags FromWgpuTextureUsage(wgpu::TextureUsage usage) {
  mnexus::TextureUsageFlags result;
  if (usage & wgpu::TextureUsage::RenderAttachment) {
    result |= mnexus::TextureUsageFlagBits::kAttachment;
  }
  if (usage & wgpu::TextureUsage::TextureBinding) {
    result |= mnexus::TextureUsageFlagBits::kSampled;
  }
  if (usage & wgpu::TextureUsage::StorageBinding) {
    result |= mnexus::TextureUsageFlagBits::kUnorderedAccess;
  }
  if (usage & wgpu::TextureUsage::CopySrc) {
    result |= mnexus::TextureUsageFlagBits::kTransferSrc;
  }
  if (usage & wgpu::TextureUsage::CopyDst) {
    result |= mnexus::TextureUsageFlagBits::kTransferDst;
  }
  return result;
}

wgpu::TextureDimension ToWgpuTextureDimension(mnexus::TextureDimension value) {
  switch (value) {
  case mnexus::TextureDimension::k1D: return wgpu::TextureDimension::e1D;
  case mnexus::TextureDimension::k2D: return wgpu::TextureDimension::e2D;
  case mnexus::TextureDimension::k3D: return wgpu::TextureDimension::e3D;
  case mnexus::TextureDimension::kCube: return wgpu::TextureDimension::e2D; // WebGPU does not have a cube dimension; cubes are represented as 2D arrays.
  default:
    MBASE_LOG_ERROR("Unknown mnexus::TextureDimension value.");
    return wgpu::TextureDimension::e2D;
  }
}

wgpu::TextureFormat ToWgpuTextureFormat(MnFormat value) {
  switch (value) {
  case MnFormatUndefined: return wgpu::TextureFormat::Undefined;
  // ...
  case MnFormatR8_UNORM: return wgpu::TextureFormat::R8Unorm;
  case MnFormatR8G8_UNORM: return wgpu::TextureFormat::RG8Unorm;
  // ...
  case MnFormatR8G8B8A8_UNORM: return wgpu::TextureFormat::RGBA8Unorm;
  case MnFormatR8G8B8A8_SRGB: return wgpu::TextureFormat::RGBA8UnormSrgb;
  case MnFormatB8G8R8A8_UNORM: return wgpu::TextureFormat::BGRA8Unorm;
  case MnFormatB8G8R8A8_SRGB: return wgpu::TextureFormat::BGRA8UnormSrgb;
  case MnFormatR16_SFLOAT: return wgpu::TextureFormat::R16Float;
  case MnFormatR16G16_SFLOAT: return wgpu::TextureFormat::RG16Float;
  // ...
  case MnFormatR16G16B16A16_UNORM: return wgpu::TextureFormat::RGBA16Unorm;
  case MnFormatR16G16B16A16_UINT: return wgpu::TextureFormat::RGBA16Uint;
  case MnFormatR16G16B16A16_SFLOAT: return wgpu::TextureFormat::RGBA16Float;
  case MnFormatR32_SFLOAT: return wgpu::TextureFormat::R32Float;
  case MnFormatR32G32_SFLOAT: return wgpu::TextureFormat::RG32Float;
  // ...
  case MnFormatR32G32B32A32_UINT: return wgpu::TextureFormat::RGBA32Uint;
  case MnFormatR32G32B32A32_SFLOAT: return wgpu::TextureFormat::RGBA32Float;
  case MnFormatA2R10G10B10_UNORM_PACK32: return wgpu::TextureFormat::RGB10A2Unorm;
  // ...
  case MnFormatD16_UNORM: return wgpu::TextureFormat::Depth16Unorm;
  case MnFormatD32_SFLOAT: return wgpu::TextureFormat::Depth32Float;
  case MnFormatD16_UNORM_S8_UINT: return wgpu::TextureFormat::Depth24PlusStencil8;
  case MnFormatD32_SFLOAT_S8_UINT: return wgpu::TextureFormat::Depth32FloatStencil8;
  case MnFormatBC1_RGB_UNORM_BLOCK: return wgpu::TextureFormat::BC1RGBAUnorm;
  case MnFormatBC1_RGB_SRGB_BLOCK: return wgpu::TextureFormat::BC1RGBAUnormSrgb;
  case MnFormatBC2_UNORM_BLOCK: return wgpu::TextureFormat::BC2RGBAUnorm;
  case MnFormatBC2_SRGB_BLOCK: return wgpu::TextureFormat::BC2RGBAUnormSrgb;
  case MnFormatBC3_UNORM_BLOCK: return wgpu::TextureFormat::BC3RGBAUnorm;
  case MnFormatBC3_SRGB_BLOCK: return wgpu::TextureFormat::BC3RGBAUnormSrgb;
  case MnFormatBC4_UNORM_BLOCK: return wgpu::TextureFormat::BC4RUnorm;
  case MnFormatBC4_SNORM_BLOCK: return wgpu::TextureFormat::BC4RSnorm;
  case MnFormatBC5_UNORM_BLOCK: return wgpu::TextureFormat::BC5RGUnorm;
  case MnFormatBC5_SNORM_BLOCK: return wgpu::TextureFormat::BC5RGSnorm;
  case MnFormatETC2_R8G8B8_UNORM_BLOCK: return wgpu::TextureFormat::ETC2RGB8Unorm;
  case MnFormatETC2_R8G8B8_SRGB_BLOCK: return wgpu::TextureFormat::ETC2RGB8UnormSrgb;
  case MnFormatETC2_R8G8B8A1_UNORM_BLOCK: return wgpu::TextureFormat::ETC2RGBA8Unorm;
  case MnFormatETC2_R8G8B8A1_SRGB_BLOCK: return wgpu::TextureFormat::ETC2RGBA8UnormSrgb;
  case MnFormatETC2_R8G8B8A8_UNORM_BLOCK: return wgpu::TextureFormat::ETC2RGBA8Unorm;
  case MnFormatETC2_R8G8B8A8_SRGB_BLOCK: return wgpu::TextureFormat::ETC2RGBA8UnormSrgb;
  case MnFormatEAC_R11_UNORM_BLOCK: return wgpu::TextureFormat::EACR11Unorm;
  case MnFormatEAC_R11_SNORM_BLOCK: return wgpu::TextureFormat::EACR11Snorm;
  case MnFormatEAC_R11G11_UNORM_BLOCK: return wgpu::TextureFormat::EACRG11Unorm;
  case MnFormatEAC_R11G11_SNORM_BLOCK: return wgpu::TextureFormat::EACRG11Snorm;
  case MnFormatASTC_4x4_UNORM_BLOCK: return wgpu::TextureFormat::ASTC4x4Unorm;
  case MnFormatASTC_4x4_SRGB_BLOCK: return wgpu::TextureFormat::ASTC4x4UnormSrgb;
  case MnFormatASTC_5x4_UNORM_BLOCK: return wgpu::TextureFormat::ASTC5x4Unorm;
  case MnFormatASTC_5x4_SRGB_BLOCK: return wgpu::TextureFormat::ASTC5x4UnormSrgb;
  case MnFormatASTC_5x5_UNORM_BLOCK: return wgpu::TextureFormat::ASTC5x5Unorm;
  case MnFormatASTC_5x5_SRGB_BLOCK: return wgpu::TextureFormat::ASTC5x5UnormSrgb;
  case MnFormatASTC_6x5_UNORM_BLOCK: return wgpu::TextureFormat::ASTC6x5Unorm;
  case MnFormatASTC_6x5_SRGB_BLOCK: return wgpu::TextureFormat::ASTC6x5UnormSrgb;
  case MnFormatASTC_6x6_UNORM_BLOCK: return wgpu::TextureFormat::ASTC6x6Unorm;
  case MnFormatASTC_6x6_SRGB_BLOCK: return wgpu::TextureFormat::ASTC6x6UnormSrgb;
  case MnFormatASTC_8x5_UNORM_BLOCK: return wgpu::TextureFormat::ASTC8x5Unorm;
  case MnFormatASTC_8x5_SRGB_BLOCK: return wgpu::TextureFormat::ASTC8x5UnormSrgb;
  case MnFormatASTC_8x6_UNORM_BLOCK: return wgpu::TextureFormat::ASTC8x6Unorm;
  case MnFormatASTC_8x6_SRGB_BLOCK: return wgpu::TextureFormat::ASTC8x6UnormSrgb;
  case MnFormatASTC_8x8_UNORM_BLOCK: return wgpu::TextureFormat::ASTC8x8Unorm;
  case MnFormatASTC_8x8_SRGB_BLOCK: return wgpu::TextureFormat::ASTC8x8UnormSrgb;
  case MnFormatASTC_10x5_UNORM_BLOCK: return wgpu::TextureFormat::ASTC10x5Unorm;
  case MnFormatASTC_10x5_SRGB_BLOCK: return wgpu::TextureFormat::ASTC10x5UnormSrgb;
  case MnFormatASTC_10x6_UNORM_BLOCK: return wgpu::TextureFormat::ASTC10x6Unorm;
  case MnFormatASTC_10x6_SRGB_BLOCK: return wgpu::TextureFormat::ASTC10x6UnormSrgb;
  case MnFormatASTC_10x8_UNORM_BLOCK: return wgpu::TextureFormat::ASTC10x8Unorm;
  case MnFormatASTC_10x8_SRGB_BLOCK: return wgpu::TextureFormat::ASTC10x8UnormSrgb;
  case MnFormatASTC_10x10_UNORM_BLOCK: return wgpu::TextureFormat::ASTC10x10Unorm;
  case MnFormatASTC_10x10_SRGB_BLOCK: return wgpu::TextureFormat::ASTC10x10UnormSrgb;
  case MnFormatASTC_12x10_UNORM_BLOCK: return wgpu::TextureFormat::ASTC12x10Unorm;
  case MnFormatASTC_12x10_SRGB_BLOCK: return wgpu::TextureFormat::ASTC12x10UnormSrgb;
  case MnFormatASTC_12x12_UNORM_BLOCK: return wgpu::TextureFormat::ASTC12x12Unorm;
  case MnFormatASTC_12x12_SRGB_BLOCK: return wgpu::TextureFormat::ASTC12x12UnormSrgb;

  //
  // Unsupported formats in WebGPU.
  //

  case MnFormatR5G6B5_UNORM_PACK16:
  case MnFormatR5G5B5A1_UNORM_PACK16:
  case MnFormatR8G8B8_UNORM:
  case MnFormatR16G16B16_SFLOAT:
  case MnFormatR32G32B32_SFLOAT:
  case MnFormatA2R10G10B10_SNORM_PACK32:
  case MnFormatA2R10G10B10_USCALED_PACK32:
  case MnFormatA2R10G10B10_SSCALED_PACK32:
  case MnFormatA2R10G10B10_UINT_PACK32:
  case MnFormatA2R10G10B10_SINT_PACK32:
  case MnFormatA2B10G10R10_UNORM_PACK32:
  case MnFormatA2B10G10R10_SNORM_PACK32:
  case MnFormatA2B10G10R10_USCALED_PACK32:
  case MnFormatA2B10G10R10_SSCALED_PACK32:
  case MnFormatA2B10G10R10_UINT_PACK32:
  case MnFormatA2B10G10R10_SINT_PACK32:
    MBASE_LOG_ERROR("MnFormat value {} is not supported in WebGPU backend", mnexus::ToString(value));
    return wgpu::TextureFormat::Undefined;

  //
  // Unknown format.
  //

  default:
    MBASE_LOG_ERROR("Unknown MnFormat value {}", static_cast<uint32_t>(value));
    return wgpu::TextureFormat::Undefined;
  }
}

MnFormat FromWgpuTextureFormat(wgpu::TextureFormat value) {
  switch (value) {
  case wgpu::TextureFormat::Undefined: return MnFormatUndefined;
  // ...
  case wgpu::TextureFormat::R8Unorm: return MnFormatR8_UNORM;
  case wgpu::TextureFormat::RG8Unorm: return MnFormatR8G8_UNORM;
  // ...
  case wgpu::TextureFormat::RGBA8Unorm: return MnFormatR8G8B8A8_UNORM;
  case wgpu::TextureFormat::RGBA8UnormSrgb: return MnFormatR8G8B8A8_SRGB;
  case wgpu::TextureFormat::BGRA8Unorm: return MnFormatB8G8R8A8_UNORM;
  case wgpu::TextureFormat::BGRA8UnormSrgb: return MnFormatB8G8R8A8_SRGB;
  case wgpu::TextureFormat::R16Float: return MnFormatR16_SFLOAT;
  case wgpu::TextureFormat::RG16Float: return MnFormatR16G16_SFLOAT;
  // ...
  case wgpu::TextureFormat::RGBA16Unorm: return MnFormatR16G16B16A16_UNORM;
  case wgpu::TextureFormat::RGBA16Uint: return MnFormatR16G16B16A16_UINT;
  case wgpu::TextureFormat::RGBA16Float: return MnFormatR16G16B16A16_SFLOAT;
  case wgpu::TextureFormat::R32Float: return MnFormatR32_SFLOAT;
  case wgpu::TextureFormat::RG32Float: return MnFormatR32G32_SFLOAT;
  // ...
  case wgpu::TextureFormat::RGBA32Uint: return MnFormatR32G32B32A32_UINT;
  case wgpu::TextureFormat::RGBA32Float: return MnFormatR32G32B32A32_SFLOAT;
  case wgpu::TextureFormat::RGB10A2Unorm: return MnFormatA2R10G10B10_UNORM_PACK32;
  // ...
  case wgpu::TextureFormat::Depth16Unorm: return MnFormatD16_UNORM;
  case wgpu::TextureFormat::Depth32Float: return MnFormatD32_SFLOAT;
  case wgpu::TextureFormat::Depth24PlusStencil8: return MnFormatD16_UNORM_S8_UINT;
  case wgpu::TextureFormat::Depth32FloatStencil8: return MnFormatD32_SFLOAT_S8_UINT;
  case wgpu::TextureFormat::BC1RGBAUnorm: return MnFormatBC1_RGB_UNORM_BLOCK;
  case wgpu::TextureFormat::BC1RGBAUnormSrgb: return MnFormatBC1_RGB_SRGB_BLOCK;
  case wgpu::TextureFormat::BC2RGBAUnorm: return MnFormatBC2_UNORM_BLOCK;
  case wgpu::TextureFormat::BC2RGBAUnormSrgb: return MnFormatBC2_SRGB_BLOCK;
  case wgpu::TextureFormat::BC3RGBAUnorm: return MnFormatBC3_UNORM_BLOCK;
  case wgpu::TextureFormat::BC3RGBAUnormSrgb: return MnFormatBC3_SRGB_BLOCK;
  case wgpu::TextureFormat::BC4RUnorm: return MnFormatBC4_UNORM_BLOCK;
  case wgpu::TextureFormat::BC4RSnorm: return MnFormatBC4_SNORM_BLOCK;
  case wgpu::TextureFormat::BC5RGUnorm: return MnFormatBC5_UNORM_BLOCK;
  case wgpu::TextureFormat::BC5RGSnorm: return MnFormatBC5_SNORM_BLOCK;
  case wgpu::TextureFormat::ETC2RGB8Unorm: return MnFormatETC2_R8G8B8_UNORM_BLOCK;
  case wgpu::TextureFormat::ETC2RGB8UnormSrgb: return MnFormatETC2_R8G8B8_SRGB_BLOCK;
  case wgpu::TextureFormat::ETC2RGBA8Unorm: return MnFormatETC2_R8G8B8A1_UNORM_BLOCK;
  case wgpu::TextureFormat::ETC2RGBA8UnormSrgb: return MnFormatETC2_R8G8B8A1_SRGB_BLOCK;
  case wgpu::TextureFormat::EACR11Unorm: return MnFormatEAC_R11_UNORM_BLOCK;
  case wgpu::TextureFormat::EACR11Snorm: return MnFormatEAC_R11_SNORM_BLOCK;
  case wgpu::TextureFormat::EACRG11Unorm: return MnFormatEAC_R11G11_UNORM_BLOCK;
  case wgpu::TextureFormat::EACRG11Snorm: return MnFormatEAC_R11G11_SNORM_BLOCK;
  case wgpu::TextureFormat::ASTC4x4Unorm: return MnFormatASTC_4x4_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC4x4UnormSrgb: return MnFormatASTC_4x4_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC5x4Unorm: return MnFormatASTC_5x4_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC5x4UnormSrgb: return MnFormatASTC_5x4_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC5x5Unorm: return MnFormatASTC_5x5_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC5x5UnormSrgb: return MnFormatASTC_5x5_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC6x5Unorm: return MnFormatASTC_6x5_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC6x5UnormSrgb: return MnFormatASTC_6x5_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC6x6Unorm: return MnFormatASTC_6x6_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC6x6UnormSrgb: return MnFormatASTC_6x6_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC8x5Unorm: return MnFormatASTC_8x5_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC8x5UnormSrgb: return MnFormatASTC_8x5_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC8x6Unorm: return MnFormatASTC_8x6_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC8x6UnormSrgb: return MnFormatASTC_8x6_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC8x8Unorm: return MnFormatASTC_8x8_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC8x8UnormSrgb: return MnFormatASTC_8x8_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC10x5Unorm: return MnFormatASTC_10x5_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC10x5UnormSrgb: return MnFormatASTC_10x5_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC10x6Unorm: return MnFormatASTC_10x6_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC10x6UnormSrgb: return MnFormatASTC_10x6_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC10x8Unorm: return MnFormatASTC_10x8_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC10x8UnormSrgb: return MnFormatASTC_10x8_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC10x10Unorm: return MnFormatASTC_10x10_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC10x10UnormSrgb: return MnFormatASTC_10x10_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC12x10Unorm: return MnFormatASTC_12x10_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC12x10UnormSrgb: return MnFormatASTC_12x10_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC12x12Unorm: return MnFormatASTC_12x12_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC12x12UnormSrgb: return MnFormatASTC_12x12_SRGB_BLOCK;

    //
    // Unknown format.
  default:
    MBASE_LOG_ERROR("Unknown wgpu::TextureFormat value {}", value);
    return MnFormatUndefined;
  }
}

wgpu::FilterMode ToWgpuFilterMode(mnexus::Filter value) {
  switch (value) {
  case mnexus::Filter::kNearest: return wgpu::FilterMode::Nearest;
  case mnexus::Filter::kLinear:  return wgpu::FilterMode::Linear;
  default:
    MBASE_LOG_ERROR("Unknown Filter value {}", static_cast<uint32_t>(value));
    return wgpu::FilterMode::Nearest;
  }
}

wgpu::MipmapFilterMode ToWgpuMipmapFilterMode(mnexus::Filter value) {
  switch (value) {
  case mnexus::Filter::kNearest: return wgpu::MipmapFilterMode::Nearest;
  case mnexus::Filter::kLinear:  return wgpu::MipmapFilterMode::Linear;
  default:
    MBASE_LOG_ERROR("Unknown Filter value {}", static_cast<uint32_t>(value));
    return wgpu::MipmapFilterMode::Nearest;
  }
}

wgpu::AddressMode ToWgpuAddressMode(mnexus::AddressMode value) {
  switch (value) {
  case mnexus::AddressMode::kRepeat:       return wgpu::AddressMode::Repeat;
  case mnexus::AddressMode::kMirrorRepeat: return wgpu::AddressMode::MirrorRepeat;
  case mnexus::AddressMode::kClampToEdge:  return wgpu::AddressMode::ClampToEdge;
  default:
    MBASE_LOG_ERROR("Unknown AddressMode value {}", static_cast<uint32_t>(value));
    return wgpu::AddressMode::ClampToEdge;
  }
}

wgpu::PrimitiveTopology ToWgpuPrimitiveTopology(mnexus::PrimitiveTopology value) {
  switch (value) {
  case mnexus::PrimitiveTopology::kPointList:     return wgpu::PrimitiveTopology::PointList;
  case mnexus::PrimitiveTopology::kLineList:      return wgpu::PrimitiveTopology::LineList;
  case mnexus::PrimitiveTopology::kLineStrip:     return wgpu::PrimitiveTopology::LineStrip;
  case mnexus::PrimitiveTopology::kTriangleList:  return wgpu::PrimitiveTopology::TriangleList;
  case mnexus::PrimitiveTopology::kTriangleStrip: return wgpu::PrimitiveTopology::TriangleStrip;
  default:
    MBASE_LOG_ERROR("Unknown PrimitiveTopology value {}", static_cast<uint32_t>(value));
    return wgpu::PrimitiveTopology::TriangleList;
  }
}

wgpu::CullMode ToWgpuCullMode(mnexus::CullMode value) {
  switch (value) {
  case mnexus::CullMode::kNone:  return wgpu::CullMode::None;
  case mnexus::CullMode::kFront: return wgpu::CullMode::Front;
  case mnexus::CullMode::kBack:  return wgpu::CullMode::Back;
  default:
    MBASE_LOG_ERROR("Unknown CullMode value {}", static_cast<uint32_t>(value));
    return wgpu::CullMode::None;
  }
}

wgpu::FrontFace ToWgpuFrontFace(mnexus::FrontFace value) {
  switch (value) {
  case mnexus::FrontFace::kCounterClockwise: return wgpu::FrontFace::CCW;
  case mnexus::FrontFace::kClockwise:        return wgpu::FrontFace::CW;
  default:
    MBASE_LOG_ERROR("Unknown FrontFace value {}", static_cast<uint32_t>(value));
    return wgpu::FrontFace::CCW;
  }
}

wgpu::IndexFormat ToWgpuIndexFormat(mnexus::IndexType value) {
  switch (value) {
  case mnexus::IndexType::kUint16: return wgpu::IndexFormat::Uint16;
  case mnexus::IndexType::kUint32: return wgpu::IndexFormat::Uint32;
  default:
    MBASE_LOG_ERROR("Unknown IndexType value {}", static_cast<uint32_t>(value));
    return wgpu::IndexFormat::Uint32;
  }
}

wgpu::VertexStepMode ToWgpuVertexStepMode(mnexus::VertexStepMode value) {
  switch (value) {
  case mnexus::VertexStepMode::kVertex:   return wgpu::VertexStepMode::Vertex;
  case mnexus::VertexStepMode::kInstance: return wgpu::VertexStepMode::Instance;
  default:
    MBASE_LOG_ERROR("Unknown VertexStepMode value {}", static_cast<uint32_t>(value));
    return wgpu::VertexStepMode::Vertex;
  }
}

wgpu::VertexFormat ToWgpuVertexFormat(MnFormat value) {
  switch (value) {
  // 8-bit
  case MnFormatR8_UNORM:            return wgpu::VertexFormat::Unorm8;
  case MnFormatR8G8_UNORM:          return wgpu::VertexFormat::Unorm8x2;
  case MnFormatR8G8B8A8_UNORM:      return wgpu::VertexFormat::Unorm8x4;
  // 16-bit float
  case MnFormatR16_SFLOAT:          return wgpu::VertexFormat::Float16;
  case MnFormatR16G16_SFLOAT:       return wgpu::VertexFormat::Float16x2;
  case MnFormatR16G16B16A16_SFLOAT: return wgpu::VertexFormat::Float16x4;
  // 32-bit float
  case MnFormatR32_SFLOAT:          return wgpu::VertexFormat::Float32;
  case MnFormatR32G32_SFLOAT:       return wgpu::VertexFormat::Float32x2;
  case MnFormatR32G32B32_SFLOAT:    return wgpu::VertexFormat::Float32x3;
  case MnFormatR32G32B32A32_SFLOAT: return wgpu::VertexFormat::Float32x4;
  // 32-bit uint
  case MnFormatR32G32B32A32_UINT:   return wgpu::VertexFormat::Uint32x4;
  default:
    MBASE_LOG_ERROR("MnFormat {} is not supported as a vertex format in WebGPU backend", mnexus::ToString(value));
    return wgpu::VertexFormat::Float32;
  }
}

wgpu::LoadOp ToWgpuLoadOp(mnexus::LoadOp value) {
  switch (value) {
  case mnexus::LoadOp::kLoad:     return wgpu::LoadOp::Load;
  case mnexus::LoadOp::kClear:    return wgpu::LoadOp::Clear;
  case mnexus::LoadOp::kDontCare: return wgpu::LoadOp::Clear; // WebGPU has no DontCare; Clear is the closest.
  default:
    MBASE_LOG_ERROR("Unknown LoadOp value {}", static_cast<uint32_t>(value));
    return wgpu::LoadOp::Clear;
  }
}

wgpu::StoreOp ToWgpuStoreOp(mnexus::StoreOp value) {
  switch (value) {
  case mnexus::StoreOp::kStore:    return wgpu::StoreOp::Store;
  case mnexus::StoreOp::kDontCare: return wgpu::StoreOp::Discard;
  default:
    MBASE_LOG_ERROR("Unknown StoreOp value {}", static_cast<uint32_t>(value));
    return wgpu::StoreOp::Store;
  }
}

wgpu::CompareFunction ToWgpuCompareFunction(mnexus::CompareOp value) {
  switch (value) {
  case mnexus::CompareOp::kNever:        return wgpu::CompareFunction::Never;
  case mnexus::CompareOp::kLess:         return wgpu::CompareFunction::Less;
  case mnexus::CompareOp::kEqual:        return wgpu::CompareFunction::Equal;
  case mnexus::CompareOp::kLessEqual:    return wgpu::CompareFunction::LessEqual;
  case mnexus::CompareOp::kGreater:      return wgpu::CompareFunction::Greater;
  case mnexus::CompareOp::kNotEqual:     return wgpu::CompareFunction::NotEqual;
  case mnexus::CompareOp::kGreaterEqual: return wgpu::CompareFunction::GreaterEqual;
  case mnexus::CompareOp::kAlways:       return wgpu::CompareFunction::Always;
  default:
    MBASE_LOG_ERROR("Unknown CompareOp value {}", static_cast<uint32_t>(value));
    return wgpu::CompareFunction::Always;
  }
}

wgpu::BlendFactor ToWgpuBlendFactor(mnexus::BlendFactor value) {
  switch (value) {
  case mnexus::BlendFactor::kZero:             return wgpu::BlendFactor::Zero;
  case mnexus::BlendFactor::kOne:              return wgpu::BlendFactor::One;
  case mnexus::BlendFactor::kSrcColor:         return wgpu::BlendFactor::Src;
  case mnexus::BlendFactor::kOneMinusSrcColor: return wgpu::BlendFactor::OneMinusSrc;
  case mnexus::BlendFactor::kSrcAlpha:         return wgpu::BlendFactor::SrcAlpha;
  case mnexus::BlendFactor::kOneMinusSrcAlpha: return wgpu::BlendFactor::OneMinusSrcAlpha;
  case mnexus::BlendFactor::kDstColor:         return wgpu::BlendFactor::Dst;
  case mnexus::BlendFactor::kOneMinusDstColor: return wgpu::BlendFactor::OneMinusDst;
  case mnexus::BlendFactor::kDstAlpha:         return wgpu::BlendFactor::DstAlpha;
  case mnexus::BlendFactor::kOneMinusDstAlpha: return wgpu::BlendFactor::OneMinusDstAlpha;
  case mnexus::BlendFactor::kSrcAlphaSaturated: return wgpu::BlendFactor::SrcAlphaSaturated;
  case mnexus::BlendFactor::kConstant:         return wgpu::BlendFactor::Constant;
  case mnexus::BlendFactor::kOneMinusConstant: return wgpu::BlendFactor::OneMinusConstant;
  default:
    MBASE_LOG_ERROR("Unknown BlendFactor value {}", static_cast<uint32_t>(value));
    return wgpu::BlendFactor::One;
  }
}

wgpu::BlendOperation ToWgpuBlendOperation(mnexus::BlendOp value) {
  switch (value) {
  case mnexus::BlendOp::kAdd:             return wgpu::BlendOperation::Add;
  case mnexus::BlendOp::kSubtract:        return wgpu::BlendOperation::Subtract;
  case mnexus::BlendOp::kReverseSubtract: return wgpu::BlendOperation::ReverseSubtract;
  case mnexus::BlendOp::kMin:             return wgpu::BlendOperation::Min;
  case mnexus::BlendOp::kMax:             return wgpu::BlendOperation::Max;
  default:
    MBASE_LOG_ERROR("Unknown BlendOp value {}", static_cast<uint32_t>(value));
    return wgpu::BlendOperation::Add;
  }
}

wgpu::ColorWriteMask ToWgpuColorWriteMask(mnexus::ColorWriteMask value) {
  wgpu::ColorWriteMask result = wgpu::ColorWriteMask::None;
  uint8_t const bits = static_cast<uint8_t>(value);
  if (bits & static_cast<uint8_t>(mnexus::ColorWriteMask::kRed))   result |= wgpu::ColorWriteMask::Red;
  if (bits & static_cast<uint8_t>(mnexus::ColorWriteMask::kGreen)) result |= wgpu::ColorWriteMask::Green;
  if (bits & static_cast<uint8_t>(mnexus::ColorWriteMask::kBlue))  result |= wgpu::ColorWriteMask::Blue;
  if (bits & static_cast<uint8_t>(mnexus::ColorWriteMask::kAlpha)) result |= wgpu::ColorWriteMask::Alpha;
  return result;
}

} // namespace mnexus_backend::webgpu
