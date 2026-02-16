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
  case MnFormat::kUndefined: return wgpu::TextureFormat::Undefined;
  // ...
  case MnFormat::kR8_UNORM: return wgpu::TextureFormat::R8Unorm;
  case MnFormat::kR8G8_UNORM: return wgpu::TextureFormat::RG8Unorm;
  // ...
  case MnFormat::kR8G8B8A8_UNORM: return wgpu::TextureFormat::RGBA8Unorm;
  case MnFormat::kR8G8B8A8_SRGB: return wgpu::TextureFormat::RGBA8UnormSrgb;
  case MnFormat::kB8G8R8A8_UNORM: return wgpu::TextureFormat::BGRA8Unorm;
  case MnFormat::kB8G8R8A8_SRGB: return wgpu::TextureFormat::BGRA8UnormSrgb;
  case MnFormat::kR16_SFLOAT: return wgpu::TextureFormat::R16Float;
  case MnFormat::kR16G16_SFLOAT: return wgpu::TextureFormat::RG16Float;
  // ...
  case MnFormat::kR16G16B16A16_UNORM: return wgpu::TextureFormat::RGBA16Unorm;
  case MnFormat::kR16G16B16A16_UINT: return wgpu::TextureFormat::RGBA16Uint;
  case MnFormat::kR16G16B16A16_SFLOAT: return wgpu::TextureFormat::RGBA16Float;
  case MnFormat::kR32_SFLOAT: return wgpu::TextureFormat::R32Float;
  case MnFormat::kR32G32_SFLOAT: return wgpu::TextureFormat::RG32Float;
  // ...
  case MnFormat::kR32G32B32A32_UINT: return wgpu::TextureFormat::RGBA32Uint;
  case MnFormat::kR32G32B32A32_SFLOAT: return wgpu::TextureFormat::RGBA32Float;
  case MnFormat::kA2R10G10B10_UNORM_PACK32: return wgpu::TextureFormat::RGB10A2Unorm;
  // ...
  case MnFormat::kD16_UNORM: return wgpu::TextureFormat::Depth16Unorm;
  case MnFormat::kD32_SFLOAT: return wgpu::TextureFormat::Depth32Float;
  case MnFormat::kD16_UNORM_S8_UINT: return wgpu::TextureFormat::Depth24PlusStencil8;
  case MnFormat::kD32_SFLOAT_S8_UINT: return wgpu::TextureFormat::Depth32FloatStencil8;
  case MnFormat::kBC1_RGB_UNORM_BLOCK: return wgpu::TextureFormat::BC1RGBAUnorm;
  case MnFormat::kBC1_RGB_SRGB_BLOCK: return wgpu::TextureFormat::BC1RGBAUnormSrgb;
  case MnFormat::kBC2_UNORM_BLOCK: return wgpu::TextureFormat::BC2RGBAUnorm;
  case MnFormat::kBC2_SRGB_BLOCK: return wgpu::TextureFormat::BC2RGBAUnormSrgb;
  case MnFormat::kBC3_UNORM_BLOCK: return wgpu::TextureFormat::BC3RGBAUnorm;
  case MnFormat::kBC3_SRGB_BLOCK: return wgpu::TextureFormat::BC3RGBAUnormSrgb;
  case MnFormat::kBC4_UNORM_BLOCK: return wgpu::TextureFormat::BC4RUnorm;
  case MnFormat::kBC4_SNORM_BLOCK: return wgpu::TextureFormat::BC4RSnorm;
  case MnFormat::kBC5_UNORM_BLOCK: return wgpu::TextureFormat::BC5RGUnorm;
  case MnFormat::kBC5_SNORM_BLOCK: return wgpu::TextureFormat::BC5RGSnorm;
  case MnFormat::kETC2_R8G8B8_UNORM_BLOCK: return wgpu::TextureFormat::ETC2RGB8Unorm;
  case MnFormat::kETC2_R8G8B8_SRGB_BLOCK: return wgpu::TextureFormat::ETC2RGB8UnormSrgb;
  case MnFormat::kETC2_R8G8B8A1_UNORM_BLOCK: return wgpu::TextureFormat::ETC2RGBA8Unorm;
  case MnFormat::kETC2_R8G8B8A1_SRGB_BLOCK: return wgpu::TextureFormat::ETC2RGBA8UnormSrgb;
  case MnFormat::kETC2_R8G8B8A8_UNORM_BLOCK: return wgpu::TextureFormat::ETC2RGBA8Unorm;
  case MnFormat::kETC2_R8G8B8A8_SRGB_BLOCK: return wgpu::TextureFormat::ETC2RGBA8UnormSrgb;
  case MnFormat::kEAC_R11_UNORM_BLOCK: return wgpu::TextureFormat::EACR11Unorm;
  case MnFormat::kEAC_R11_SNORM_BLOCK: return wgpu::TextureFormat::EACR11Snorm;
  case MnFormat::kEAC_R11G11_UNORM_BLOCK: return wgpu::TextureFormat::EACRG11Unorm;
  case MnFormat::kEAC_R11G11_SNORM_BLOCK: return wgpu::TextureFormat::EACRG11Snorm;
  case MnFormat::kASTC_4x4_UNORM_BLOCK: return wgpu::TextureFormat::ASTC4x4Unorm;
  case MnFormat::kASTC_4x4_SRGB_BLOCK: return wgpu::TextureFormat::ASTC4x4UnormSrgb;
  case MnFormat::kASTC_5x4_UNORM_BLOCK: return wgpu::TextureFormat::ASTC5x4Unorm;
  case MnFormat::kASTC_5x4_SRGB_BLOCK: return wgpu::TextureFormat::ASTC5x4UnormSrgb;
  case MnFormat::kASTC_5x5_UNORM_BLOCK: return wgpu::TextureFormat::ASTC5x5Unorm;
  case MnFormat::kASTC_5x5_SRGB_BLOCK: return wgpu::TextureFormat::ASTC5x5UnormSrgb;
  case MnFormat::kASTC_6x5_UNORM_BLOCK: return wgpu::TextureFormat::ASTC6x5Unorm;
  case MnFormat::kASTC_6x5_SRGB_BLOCK: return wgpu::TextureFormat::ASTC6x5UnormSrgb;
  case MnFormat::kASTC_6x6_UNORM_BLOCK: return wgpu::TextureFormat::ASTC6x6Unorm;
  case MnFormat::kASTC_6x6_SRGB_BLOCK: return wgpu::TextureFormat::ASTC6x6UnormSrgb;
  case MnFormat::kASTC_8x5_UNORM_BLOCK: return wgpu::TextureFormat::ASTC8x5Unorm;
  case MnFormat::kASTC_8x5_SRGB_BLOCK: return wgpu::TextureFormat::ASTC8x5UnormSrgb;
  case MnFormat::kASTC_8x6_UNORM_BLOCK: return wgpu::TextureFormat::ASTC8x6Unorm;
  case MnFormat::kASTC_8x6_SRGB_BLOCK: return wgpu::TextureFormat::ASTC8x6UnormSrgb;
  case MnFormat::kASTC_8x8_UNORM_BLOCK: return wgpu::TextureFormat::ASTC8x8Unorm;
  case MnFormat::kASTC_8x8_SRGB_BLOCK: return wgpu::TextureFormat::ASTC8x8UnormSrgb;
  case MnFormat::kASTC_10x5_UNORM_BLOCK: return wgpu::TextureFormat::ASTC10x5Unorm;
  case MnFormat::kASTC_10x5_SRGB_BLOCK: return wgpu::TextureFormat::ASTC10x5UnormSrgb;
  case MnFormat::kASTC_10x6_UNORM_BLOCK: return wgpu::TextureFormat::ASTC10x6Unorm;
  case MnFormat::kASTC_10x6_SRGB_BLOCK: return wgpu::TextureFormat::ASTC10x6UnormSrgb;
  case MnFormat::kASTC_10x8_UNORM_BLOCK: return wgpu::TextureFormat::ASTC10x8Unorm;
  case MnFormat::kASTC_10x8_SRGB_BLOCK: return wgpu::TextureFormat::ASTC10x8UnormSrgb;
  case MnFormat::kASTC_10x10_UNORM_BLOCK: return wgpu::TextureFormat::ASTC10x10Unorm;
  case MnFormat::kASTC_10x10_SRGB_BLOCK: return wgpu::TextureFormat::ASTC10x10UnormSrgb;
  case MnFormat::kASTC_12x10_UNORM_BLOCK: return wgpu::TextureFormat::ASTC12x10Unorm;
  case MnFormat::kASTC_12x10_SRGB_BLOCK: return wgpu::TextureFormat::ASTC12x10UnormSrgb;
  case MnFormat::kASTC_12x12_UNORM_BLOCK: return wgpu::TextureFormat::ASTC12x12Unorm;
  case MnFormat::kASTC_12x12_SRGB_BLOCK: return wgpu::TextureFormat::ASTC12x12UnormSrgb;

  //
  // Unsupported formats in WebGPU.
  //

  case MnFormat::kR5G6B5_UNORM_PACK16:
  case MnFormat::kR5G5B5A1_UNORM_PACK16:
  case MnFormat::kR8G8B8_UNORM:
  case MnFormat::kR16G16B16_SFLOAT:
  case MnFormat::kR32G32B32_SFLOAT:
  case MnFormat::kA2R10G10B10_SNORM_PACK32:
  case MnFormat::kA2R10G10B10_USCALED_PACK32:
  case MnFormat::kA2R10G10B10_SSCALED_PACK32:
  case MnFormat::kA2R10G10B10_UINT_PACK32:
  case MnFormat::kA2R10G10B10_SINT_PACK32:
  case MnFormat::kA2B10G10R10_UNORM_PACK32:
  case MnFormat::kA2B10G10R10_SNORM_PACK32:
  case MnFormat::kA2B10G10R10_USCALED_PACK32:
  case MnFormat::kA2B10G10R10_SSCALED_PACK32:
  case MnFormat::kA2B10G10R10_UINT_PACK32:
  case MnFormat::kA2B10G10R10_SINT_PACK32:
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
  case wgpu::TextureFormat::Undefined: return MnFormat::kUndefined;
  // ...
  case wgpu::TextureFormat::R8Unorm: return MnFormat::kR8_UNORM;
  case wgpu::TextureFormat::RG8Unorm: return MnFormat::kR8G8_UNORM;
  // ...
  case wgpu::TextureFormat::RGBA8Unorm: return MnFormat::kR8G8B8A8_UNORM;
  case wgpu::TextureFormat::RGBA8UnormSrgb: return MnFormat::kR8G8B8A8_SRGB;
  case wgpu::TextureFormat::BGRA8Unorm: return MnFormat::kB8G8R8A8_UNORM;
  case wgpu::TextureFormat::BGRA8UnormSrgb: return MnFormat::kB8G8R8A8_SRGB;
  case wgpu::TextureFormat::R16Float: return MnFormat::kR16_SFLOAT;
  case wgpu::TextureFormat::RG16Float: return MnFormat::kR16G16_SFLOAT;
  // ...
  case wgpu::TextureFormat::RGBA16Unorm: return MnFormat::kR16G16B16A16_UNORM;
  case wgpu::TextureFormat::RGBA16Uint: return MnFormat::kR16G16B16A16_UINT;
  case wgpu::TextureFormat::RGBA16Float: return MnFormat::kR16G16B16A16_SFLOAT;
  case wgpu::TextureFormat::R32Float: return MnFormat::kR32_SFLOAT;
  case wgpu::TextureFormat::RG32Float: return MnFormat::kR32G32_SFLOAT;
  // ...
  case wgpu::TextureFormat::RGBA32Uint: return MnFormat::kR32G32B32A32_UINT;
  case wgpu::TextureFormat::RGBA32Float: return MnFormat::kR32G32B32A32_SFLOAT;
  case wgpu::TextureFormat::RGB10A2Unorm: return MnFormat::kA2R10G10B10_UNORM_PACK32;
  // ...
  case wgpu::TextureFormat::Depth16Unorm: return MnFormat::kD16_UNORM;
  case wgpu::TextureFormat::Depth32Float: return MnFormat::kD32_SFLOAT;
  case wgpu::TextureFormat::Depth24PlusStencil8: return MnFormat::kD16_UNORM_S8_UINT;
  case wgpu::TextureFormat::Depth32FloatStencil8: return MnFormat::kD32_SFLOAT_S8_UINT;
  case wgpu::TextureFormat::BC1RGBAUnorm: return MnFormat::kBC1_RGB_UNORM_BLOCK;
  case wgpu::TextureFormat::BC1RGBAUnormSrgb: return MnFormat::kBC1_RGB_SRGB_BLOCK;
  case wgpu::TextureFormat::BC2RGBAUnorm: return MnFormat::kBC2_UNORM_BLOCK;
  case wgpu::TextureFormat::BC2RGBAUnormSrgb: return MnFormat::kBC2_SRGB_BLOCK;
  case wgpu::TextureFormat::BC3RGBAUnorm: return MnFormat::kBC3_UNORM_BLOCK;
  case wgpu::TextureFormat::BC3RGBAUnormSrgb: return MnFormat::kBC3_SRGB_BLOCK;
  case wgpu::TextureFormat::BC4RUnorm: return MnFormat::kBC4_UNORM_BLOCK;
  case wgpu::TextureFormat::BC4RSnorm: return MnFormat::kBC4_SNORM_BLOCK;
  case wgpu::TextureFormat::BC5RGUnorm: return MnFormat::kBC5_UNORM_BLOCK;
  case wgpu::TextureFormat::BC5RGSnorm: return MnFormat::kBC5_SNORM_BLOCK;
  case wgpu::TextureFormat::ETC2RGB8Unorm: return MnFormat::kETC2_R8G8B8_UNORM_BLOCK;
  case wgpu::TextureFormat::ETC2RGB8UnormSrgb: return MnFormat::kETC2_R8G8B8_SRGB_BLOCK;
  case wgpu::TextureFormat::ETC2RGBA8Unorm: return MnFormat::kETC2_R8G8B8A1_UNORM_BLOCK;
  case wgpu::TextureFormat::ETC2RGBA8UnormSrgb: return MnFormat::kETC2_R8G8B8A1_SRGB_BLOCK;
  case wgpu::TextureFormat::EACR11Unorm: return MnFormat::kEAC_R11_UNORM_BLOCK;
  case wgpu::TextureFormat::EACR11Snorm: return MnFormat::kEAC_R11_SNORM_BLOCK;
  case wgpu::TextureFormat::EACRG11Unorm: return MnFormat::kEAC_R11G11_UNORM_BLOCK;
  case wgpu::TextureFormat::EACRG11Snorm: return MnFormat::kEAC_R11G11_SNORM_BLOCK;
  case wgpu::TextureFormat::ASTC4x4Unorm: return MnFormat::kASTC_4x4_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC4x4UnormSrgb: return MnFormat::kASTC_4x4_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC5x4Unorm: return MnFormat::kASTC_5x4_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC5x4UnormSrgb: return MnFormat::kASTC_5x4_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC5x5Unorm: return MnFormat::kASTC_5x5_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC5x5UnormSrgb: return MnFormat::kASTC_5x5_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC6x5Unorm: return MnFormat::kASTC_6x5_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC6x5UnormSrgb: return MnFormat::kASTC_6x5_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC6x6Unorm: return MnFormat::kASTC_6x6_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC6x6UnormSrgb: return MnFormat::kASTC_6x6_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC8x5Unorm: return MnFormat::kASTC_8x5_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC8x5UnormSrgb: return MnFormat::kASTC_8x5_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC8x6Unorm: return MnFormat::kASTC_8x6_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC8x6UnormSrgb: return MnFormat::kASTC_8x6_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC8x8Unorm: return MnFormat::kASTC_8x8_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC8x8UnormSrgb: return MnFormat::kASTC_8x8_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC10x5Unorm: return MnFormat::kASTC_10x5_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC10x5UnormSrgb: return MnFormat::kASTC_10x5_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC10x6Unorm: return MnFormat::kASTC_10x6_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC10x6UnormSrgb: return MnFormat::kASTC_10x6_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC10x8Unorm: return MnFormat::kASTC_10x8_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC10x8UnormSrgb: return MnFormat::kASTC_10x8_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC10x10Unorm: return MnFormat::kASTC_10x10_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC10x10UnormSrgb: return MnFormat::kASTC_10x10_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC12x10Unorm: return MnFormat::kASTC_12x10_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC12x10UnormSrgb: return MnFormat::kASTC_12x10_SRGB_BLOCK;
  case wgpu::TextureFormat::ASTC12x12Unorm: return MnFormat::kASTC_12x12_UNORM_BLOCK;
  case wgpu::TextureFormat::ASTC12x12UnormSrgb: return MnFormat::kASTC_12x12_SRGB_BLOCK;

    //
    // Unknown format.
  default:
    MBASE_LOG_ERROR("Unknown wgpu::TextureFormat value {}", value);
    return MnFormat::kUndefined;
  }
}

} // namespace mnexus_backend::webgpu
