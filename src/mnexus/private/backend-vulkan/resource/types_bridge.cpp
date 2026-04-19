// TU header --------------------------------------------
#include "backend-vulkan/resource/types_bridge.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

// external headers -------------------------------------
#include "vulkan/utility/vk_format_utils.h"

namespace mnexus_backend::vulkan {

// ====================================================================================================
// Buffer / texture usage flags
//

VkBufferUsageFlags ToVkBufferUsageFlags(mnexus::BufferUsageFlags usage) {
  VkBufferUsageFlags result = 0;

  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kUniform))     { result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kStorage))     { result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kIndex))       { result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kVertex))      { result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kIndirect))    { result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kTransferSrc)) { result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT; }
  if (usage.HasAnyOf(mnexus::BufferUsageFlagBits::kTransferDst)) { result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; }

  return result;
}

VkImageType ToVkImageType(mnexus::TextureDimension value) {
  switch (value) {
  case mnexus::TextureDimension::k1D:   return VK_IMAGE_TYPE_1D;
  case mnexus::TextureDimension::k2D:   return VK_IMAGE_TYPE_2D;
  case mnexus::TextureDimension::k3D:   return VK_IMAGE_TYPE_3D;
  case mnexus::TextureDimension::kCube: return VK_IMAGE_TYPE_2D; // 2D + 6 array layers + CUBE_COMPATIBLE flag.
  }
  MBASE_LOG_ERROR("Unknown mnexus::TextureDimension value");
  return VK_IMAGE_TYPE_2D;
}

VkImageUsageFlags ToVkImageUsageFlags(mnexus::TextureUsageFlags usage, VkFormat vk_format) {
  VkImageUsageFlags result = 0;

  if (usage.HasAnyOf(mnexus::TextureUsageFlagBits::kAttachment)) {
    result |= vkuFormatIsDepthOrStencil(vk_format)
      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  if (usage.HasAnyOf(mnexus::TextureUsageFlagBits::kTileLocal))       { result |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT; }
  if (usage.HasAnyOf(mnexus::TextureUsageFlagBits::kSampled))         { result |= VK_IMAGE_USAGE_SAMPLED_BIT; }
  if (usage.HasAnyOf(mnexus::TextureUsageFlagBits::kUnorderedAccess)) { result |= VK_IMAGE_USAGE_STORAGE_BIT; }
  if (usage.HasAnyOf(mnexus::TextureUsageFlagBits::kTransferSrc))     { result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; }
  if (usage.HasAnyOf(mnexus::TextureUsageFlagBits::kTransferDst))     { result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; }

  return result;
}

// ====================================================================================================
// Format
//

VkFormat ToVkFormat(mnexus::Format value) {
  switch (value) {
  case mnexus::Format::kUndefined:             return VK_FORMAT_UNDEFINED;

  case mnexus::Format::kR5G6B5_UNORM_PACK16:   return VK_FORMAT_R5G6B5_UNORM_PACK16;
  case mnexus::Format::kR5G5B5A1_UNORM_PACK16: return VK_FORMAT_R5G5B5A1_UNORM_PACK16;

  case mnexus::Format::kR8_UNORM:              return VK_FORMAT_R8_UNORM;
  case mnexus::Format::kR8G8_UNORM:            return VK_FORMAT_R8G8_UNORM;
  case mnexus::Format::kR8G8B8_UNORM:          return VK_FORMAT_R8G8B8_UNORM;
  case mnexus::Format::kR8G8B8A8_UNORM:        return VK_FORMAT_R8G8B8A8_UNORM;
  case mnexus::Format::kR8G8B8A8_SRGB:         return VK_FORMAT_R8G8B8A8_SRGB;
  case mnexus::Format::kB8G8R8A8_UNORM:        return VK_FORMAT_B8G8R8A8_UNORM;
  case mnexus::Format::kB8G8R8A8_SRGB:         return VK_FORMAT_B8G8R8A8_SRGB;

  case mnexus::Format::kR16_SFLOAT:            return VK_FORMAT_R16_SFLOAT;
  case mnexus::Format::kR16G16_SFLOAT:         return VK_FORMAT_R16G16_SFLOAT;
  case mnexus::Format::kR16G16B16_SFLOAT:      return VK_FORMAT_R16G16B16_SFLOAT;
  case mnexus::Format::kR16G16B16A16_UNORM:    return VK_FORMAT_R16G16B16A16_UNORM;
  case mnexus::Format::kR16G16B16A16_UINT:     return VK_FORMAT_R16G16B16A16_UINT;
  case mnexus::Format::kR16G16B16A16_SFLOAT:   return VK_FORMAT_R16G16B16A16_SFLOAT;

  case mnexus::Format::kR32_SFLOAT:            return VK_FORMAT_R32_SFLOAT;
  case mnexus::Format::kR32G32_SFLOAT:         return VK_FORMAT_R32G32_SFLOAT;
  case mnexus::Format::kR32G32B32_SFLOAT:      return VK_FORMAT_R32G32B32_SFLOAT;
  case mnexus::Format::kR32G32B32A32_UINT:     return VK_FORMAT_R32G32B32A32_UINT;
  case mnexus::Format::kR32G32B32A32_SFLOAT:   return VK_FORMAT_R32G32B32A32_SFLOAT;

  case mnexus::Format::kA2R10G10B10_UNORM_PACK32:   return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
  case mnexus::Format::kA2R10G10B10_SNORM_PACK32:   return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
  case mnexus::Format::kA2R10G10B10_USCALED_PACK32: return VK_FORMAT_A2R10G10B10_USCALED_PACK32;
  case mnexus::Format::kA2R10G10B10_SSCALED_PACK32: return VK_FORMAT_A2R10G10B10_SSCALED_PACK32;
  case mnexus::Format::kA2R10G10B10_UINT_PACK32:    return VK_FORMAT_A2R10G10B10_UINT_PACK32;
  case mnexus::Format::kA2R10G10B10_SINT_PACK32:    return VK_FORMAT_A2R10G10B10_SINT_PACK32;
  case mnexus::Format::kA2B10G10R10_UNORM_PACK32:   return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
  case mnexus::Format::kA2B10G10R10_SNORM_PACK32:   return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
  case mnexus::Format::kA2B10G10R10_USCALED_PACK32: return VK_FORMAT_A2B10G10R10_USCALED_PACK32;
  case mnexus::Format::kA2B10G10R10_SSCALED_PACK32: return VK_FORMAT_A2B10G10R10_SSCALED_PACK32;
  case mnexus::Format::kA2B10G10R10_UINT_PACK32:    return VK_FORMAT_A2B10G10R10_UINT_PACK32;
  case mnexus::Format::kA2B10G10R10_SINT_PACK32:    return VK_FORMAT_A2B10G10R10_SINT_PACK32;

  case mnexus::Format::kD16_UNORM:             return VK_FORMAT_D16_UNORM;
  case mnexus::Format::kD32_SFLOAT:            return VK_FORMAT_D32_SFLOAT;
  case mnexus::Format::kD16_UNORM_S8_UINT:     return VK_FORMAT_D16_UNORM_S8_UINT;
  case mnexus::Format::kD24_UNORM_S8_UINT:     return VK_FORMAT_D24_UNORM_S8_UINT;
  case mnexus::Format::kD32_SFLOAT_S8_UINT:    return VK_FORMAT_D32_SFLOAT_S8_UINT;

  case mnexus::Format::kBC1_RGB_UNORM_BLOCK:   return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
  case mnexus::Format::kBC1_RGB_SRGB_BLOCK:    return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
  case mnexus::Format::kBC1_RGBA_UNORM_BLOCK:  return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
  case mnexus::Format::kBC1_RGBA_SRGB_BLOCK:   return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
  case mnexus::Format::kBC2_UNORM_BLOCK:       return VK_FORMAT_BC2_UNORM_BLOCK;
  case mnexus::Format::kBC2_SRGB_BLOCK:        return VK_FORMAT_BC2_SRGB_BLOCK;
  case mnexus::Format::kBC3_UNORM_BLOCK:       return VK_FORMAT_BC3_UNORM_BLOCK;
  case mnexus::Format::kBC3_SRGB_BLOCK:        return VK_FORMAT_BC3_SRGB_BLOCK;
  case mnexus::Format::kBC4_UNORM_BLOCK:       return VK_FORMAT_BC4_UNORM_BLOCK;
  case mnexus::Format::kBC4_SNORM_BLOCK:       return VK_FORMAT_BC4_SNORM_BLOCK;
  case mnexus::Format::kBC5_UNORM_BLOCK:       return VK_FORMAT_BC5_UNORM_BLOCK;
  case mnexus::Format::kBC5_SNORM_BLOCK:       return VK_FORMAT_BC5_SNORM_BLOCK;

  case mnexus::Format::kETC2_R8G8B8_UNORM_BLOCK:   return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
  case mnexus::Format::kETC2_R8G8B8_SRGB_BLOCK:    return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
  case mnexus::Format::kETC2_R8G8B8A1_UNORM_BLOCK: return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
  case mnexus::Format::kETC2_R8G8B8A1_SRGB_BLOCK:  return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
  case mnexus::Format::kETC2_R8G8B8A8_UNORM_BLOCK: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
  case mnexus::Format::kETC2_R8G8B8A8_SRGB_BLOCK:  return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
  case mnexus::Format::kEAC_R11_UNORM_BLOCK:       return VK_FORMAT_EAC_R11_UNORM_BLOCK;
  case mnexus::Format::kEAC_R11_SNORM_BLOCK:       return VK_FORMAT_EAC_R11_SNORM_BLOCK;
  case mnexus::Format::kEAC_R11G11_UNORM_BLOCK:    return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
  case mnexus::Format::kEAC_R11G11_SNORM_BLOCK:    return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;

  case mnexus::Format::kASTC_4x4_UNORM_BLOCK:   return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
  case mnexus::Format::kASTC_4x4_SRGB_BLOCK:    return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
  case mnexus::Format::kASTC_5x4_UNORM_BLOCK:   return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
  case mnexus::Format::kASTC_5x4_SRGB_BLOCK:    return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
  case mnexus::Format::kASTC_5x5_UNORM_BLOCK:   return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
  case mnexus::Format::kASTC_5x5_SRGB_BLOCK:    return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
  case mnexus::Format::kASTC_6x5_UNORM_BLOCK:   return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
  case mnexus::Format::kASTC_6x5_SRGB_BLOCK:    return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
  case mnexus::Format::kASTC_6x6_UNORM_BLOCK:   return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
  case mnexus::Format::kASTC_6x6_SRGB_BLOCK:    return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
  case mnexus::Format::kASTC_8x5_UNORM_BLOCK:   return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
  case mnexus::Format::kASTC_8x5_SRGB_BLOCK:    return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
  case mnexus::Format::kASTC_8x6_UNORM_BLOCK:   return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
  case mnexus::Format::kASTC_8x6_SRGB_BLOCK:    return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
  case mnexus::Format::kASTC_8x8_UNORM_BLOCK:   return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
  case mnexus::Format::kASTC_8x8_SRGB_BLOCK:    return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
  case mnexus::Format::kASTC_10x5_UNORM_BLOCK:  return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
  case mnexus::Format::kASTC_10x5_SRGB_BLOCK:   return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
  case mnexus::Format::kASTC_10x6_UNORM_BLOCK:  return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
  case mnexus::Format::kASTC_10x6_SRGB_BLOCK:   return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
  case mnexus::Format::kASTC_10x8_UNORM_BLOCK:  return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
  case mnexus::Format::kASTC_10x8_SRGB_BLOCK:   return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
  case mnexus::Format::kASTC_10x10_UNORM_BLOCK: return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
  case mnexus::Format::kASTC_10x10_SRGB_BLOCK:  return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
  case mnexus::Format::kASTC_12x10_UNORM_BLOCK: return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
  case mnexus::Format::kASTC_12x10_SRGB_BLOCK:  return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
  case mnexus::Format::kASTC_12x12_UNORM_BLOCK: return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
  case mnexus::Format::kASTC_12x12_SRGB_BLOCK:  return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
  }
  MBASE_LOG_ERROR("Unknown mnexus::Format value");
  return VK_FORMAT_UNDEFINED;
}

mnexus::Format FromVkFormat(VkFormat value) {
  switch (value) {
  case VK_FORMAT_UNDEFINED:                  return mnexus::Format::kUndefined;

  case VK_FORMAT_R5G6B5_UNORM_PACK16:        return mnexus::Format::kR5G6B5_UNORM_PACK16;
  case VK_FORMAT_R5G5B5A1_UNORM_PACK16:      return mnexus::Format::kR5G5B5A1_UNORM_PACK16;

  case VK_FORMAT_R8_UNORM:                   return mnexus::Format::kR8_UNORM;
  case VK_FORMAT_R8G8_UNORM:                 return mnexus::Format::kR8G8_UNORM;
  case VK_FORMAT_R8G8B8_UNORM:               return mnexus::Format::kR8G8B8_UNORM;
  case VK_FORMAT_R8G8B8A8_UNORM:             return mnexus::Format::kR8G8B8A8_UNORM;
  case VK_FORMAT_R8G8B8A8_SRGB:              return mnexus::Format::kR8G8B8A8_SRGB;
  case VK_FORMAT_B8G8R8A8_UNORM:             return mnexus::Format::kB8G8R8A8_UNORM;
  case VK_FORMAT_B8G8R8A8_SRGB:              return mnexus::Format::kB8G8R8A8_SRGB;

  case VK_FORMAT_R16_SFLOAT:                 return mnexus::Format::kR16_SFLOAT;
  case VK_FORMAT_R16G16_SFLOAT:              return mnexus::Format::kR16G16_SFLOAT;
  case VK_FORMAT_R16G16B16_SFLOAT:           return mnexus::Format::kR16G16B16_SFLOAT;
  case VK_FORMAT_R16G16B16A16_UNORM:         return mnexus::Format::kR16G16B16A16_UNORM;
  case VK_FORMAT_R16G16B16A16_UINT:          return mnexus::Format::kR16G16B16A16_UINT;
  case VK_FORMAT_R16G16B16A16_SFLOAT:        return mnexus::Format::kR16G16B16A16_SFLOAT;

  case VK_FORMAT_R32_SFLOAT:                 return mnexus::Format::kR32_SFLOAT;
  case VK_FORMAT_R32G32_SFLOAT:              return mnexus::Format::kR32G32_SFLOAT;
  case VK_FORMAT_R32G32B32_SFLOAT:           return mnexus::Format::kR32G32B32_SFLOAT;
  case VK_FORMAT_R32G32B32A32_UINT:          return mnexus::Format::kR32G32B32A32_UINT;
  case VK_FORMAT_R32G32B32A32_SFLOAT:        return mnexus::Format::kR32G32B32A32_SFLOAT;

  case VK_FORMAT_A2R10G10B10_UNORM_PACK32:   return mnexus::Format::kA2R10G10B10_UNORM_PACK32;
  case VK_FORMAT_A2R10G10B10_SNORM_PACK32:   return mnexus::Format::kA2R10G10B10_SNORM_PACK32;
  case VK_FORMAT_A2R10G10B10_USCALED_PACK32: return mnexus::Format::kA2R10G10B10_USCALED_PACK32;
  case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: return mnexus::Format::kA2R10G10B10_SSCALED_PACK32;
  case VK_FORMAT_A2R10G10B10_UINT_PACK32:    return mnexus::Format::kA2R10G10B10_UINT_PACK32;
  case VK_FORMAT_A2R10G10B10_SINT_PACK32:    return mnexus::Format::kA2R10G10B10_SINT_PACK32;
  case VK_FORMAT_A2B10G10R10_UNORM_PACK32:   return mnexus::Format::kA2B10G10R10_UNORM_PACK32;
  case VK_FORMAT_A2B10G10R10_SNORM_PACK32:   return mnexus::Format::kA2B10G10R10_SNORM_PACK32;
  case VK_FORMAT_A2B10G10R10_USCALED_PACK32: return mnexus::Format::kA2B10G10R10_USCALED_PACK32;
  case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: return mnexus::Format::kA2B10G10R10_SSCALED_PACK32;
  case VK_FORMAT_A2B10G10R10_UINT_PACK32:    return mnexus::Format::kA2B10G10R10_UINT_PACK32;
  case VK_FORMAT_A2B10G10R10_SINT_PACK32:    return mnexus::Format::kA2B10G10R10_SINT_PACK32;

  case VK_FORMAT_D16_UNORM:                  return mnexus::Format::kD16_UNORM;
  case VK_FORMAT_D32_SFLOAT:                 return mnexus::Format::kD32_SFLOAT;
  case VK_FORMAT_D16_UNORM_S8_UINT:          return mnexus::Format::kD16_UNORM_S8_UINT;
  case VK_FORMAT_D24_UNORM_S8_UINT:          return mnexus::Format::kD24_UNORM_S8_UINT;
  case VK_FORMAT_D32_SFLOAT_S8_UINT:         return mnexus::Format::kD32_SFLOAT_S8_UINT;

  case VK_FORMAT_BC1_RGB_UNORM_BLOCK:        return mnexus::Format::kBC1_RGB_UNORM_BLOCK;
  case VK_FORMAT_BC1_RGB_SRGB_BLOCK:         return mnexus::Format::kBC1_RGB_SRGB_BLOCK;
  case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:       return mnexus::Format::kBC1_RGBA_UNORM_BLOCK;
  case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:        return mnexus::Format::kBC1_RGBA_SRGB_BLOCK;
  case VK_FORMAT_BC2_UNORM_BLOCK:            return mnexus::Format::kBC2_UNORM_BLOCK;
  case VK_FORMAT_BC2_SRGB_BLOCK:             return mnexus::Format::kBC2_SRGB_BLOCK;
  case VK_FORMAT_BC3_UNORM_BLOCK:            return mnexus::Format::kBC3_UNORM_BLOCK;
  case VK_FORMAT_BC3_SRGB_BLOCK:             return mnexus::Format::kBC3_SRGB_BLOCK;
  case VK_FORMAT_BC4_UNORM_BLOCK:            return mnexus::Format::kBC4_UNORM_BLOCK;
  case VK_FORMAT_BC4_SNORM_BLOCK:            return mnexus::Format::kBC4_SNORM_BLOCK;
  case VK_FORMAT_BC5_UNORM_BLOCK:            return mnexus::Format::kBC5_UNORM_BLOCK;
  case VK_FORMAT_BC5_SNORM_BLOCK:            return mnexus::Format::kBC5_SNORM_BLOCK;

  case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:    return mnexus::Format::kETC2_R8G8B8_UNORM_BLOCK;
  case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:     return mnexus::Format::kETC2_R8G8B8_SRGB_BLOCK;
  case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:  return mnexus::Format::kETC2_R8G8B8A1_UNORM_BLOCK;
  case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:   return mnexus::Format::kETC2_R8G8B8A1_SRGB_BLOCK;
  case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:  return mnexus::Format::kETC2_R8G8B8A8_UNORM_BLOCK;
  case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:   return mnexus::Format::kETC2_R8G8B8A8_SRGB_BLOCK;
  case VK_FORMAT_EAC_R11_UNORM_BLOCK:        return mnexus::Format::kEAC_R11_UNORM_BLOCK;
  case VK_FORMAT_EAC_R11_SNORM_BLOCK:        return mnexus::Format::kEAC_R11_SNORM_BLOCK;
  case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:     return mnexus::Format::kEAC_R11G11_UNORM_BLOCK;
  case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:     return mnexus::Format::kEAC_R11G11_SNORM_BLOCK;

  case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:       return mnexus::Format::kASTC_4x4_UNORM_BLOCK;
  case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:        return mnexus::Format::kASTC_4x4_SRGB_BLOCK;
  case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:       return mnexus::Format::kASTC_5x4_UNORM_BLOCK;
  case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:        return mnexus::Format::kASTC_5x4_SRGB_BLOCK;
  case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:       return mnexus::Format::kASTC_5x5_UNORM_BLOCK;
  case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:        return mnexus::Format::kASTC_5x5_SRGB_BLOCK;
  case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:       return mnexus::Format::kASTC_6x5_UNORM_BLOCK;
  case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:        return mnexus::Format::kASTC_6x5_SRGB_BLOCK;
  case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:       return mnexus::Format::kASTC_6x6_UNORM_BLOCK;
  case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:        return mnexus::Format::kASTC_6x6_SRGB_BLOCK;
  case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:       return mnexus::Format::kASTC_8x5_UNORM_BLOCK;
  case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:        return mnexus::Format::kASTC_8x5_SRGB_BLOCK;
  case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:       return mnexus::Format::kASTC_8x6_UNORM_BLOCK;
  case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:        return mnexus::Format::kASTC_8x6_SRGB_BLOCK;
  case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:       return mnexus::Format::kASTC_8x8_UNORM_BLOCK;
  case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:        return mnexus::Format::kASTC_8x8_SRGB_BLOCK;
  case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:      return mnexus::Format::kASTC_10x5_UNORM_BLOCK;
  case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:       return mnexus::Format::kASTC_10x5_SRGB_BLOCK;
  case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:      return mnexus::Format::kASTC_10x6_UNORM_BLOCK;
  case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:       return mnexus::Format::kASTC_10x6_SRGB_BLOCK;
  case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:      return mnexus::Format::kASTC_10x8_UNORM_BLOCK;
  case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:       return mnexus::Format::kASTC_10x8_SRGB_BLOCK;
  case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:     return mnexus::Format::kASTC_10x10_UNORM_BLOCK;
  case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:      return mnexus::Format::kASTC_10x10_SRGB_BLOCK;
  case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:     return mnexus::Format::kASTC_12x10_UNORM_BLOCK;
  case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:      return mnexus::Format::kASTC_12x10_SRGB_BLOCK;
  case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:     return mnexus::Format::kASTC_12x12_UNORM_BLOCK;
  case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:      return mnexus::Format::kASTC_12x12_SRGB_BLOCK;

  default:
    MBASE_LOG_ERROR("Unknown VkFormat value");
    return mnexus::Format::kUndefined;
  }
}

// ====================================================================================================
// Color space
//

VkColorSpaceKHR ToVkColorSpaceKHR(mnexus::ColorSpace value) {
  switch (value) {
  case mnexus::ColorSpace::kLinear:      return VK_COLOR_SPACE_PASS_THROUGH_EXT;
  case mnexus::ColorSpace::kSrgb:        return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  case mnexus::ColorSpace::kHdr10St2084: return VK_COLOR_SPACE_HDR10_ST2084_EXT;
  }
  MBASE_LOG_ERROR("Unknown mnexus::ColorSpace value");
  return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
}

mnexus::ColorSpace FromVkColorSpace(VkColorSpaceKHR value) {
  switch (value) {
  case VK_COLOR_SPACE_PASS_THROUGH_EXT:    return mnexus::ColorSpace::kLinear;
  case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:  return mnexus::ColorSpace::kSrgb;
  case VK_COLOR_SPACE_HDR10_ST2084_EXT:    return mnexus::ColorSpace::kHdr10St2084;
  default:
    MBASE_LOG_ERROR("Unknown VkColorSpaceKHR value");
    return mnexus::ColorSpace::kSrgb;
  }
}

// ====================================================================================================
// Samplers
//

VkFilter ToVkFilter(mnexus::Filter value) {
  switch (value) {
  case mnexus::Filter::kNearest: return VK_FILTER_NEAREST;
  case mnexus::Filter::kLinear:  return VK_FILTER_LINEAR;
  }
  MBASE_LOG_ERROR("Unknown mnexus::Filter value");
  return VK_FILTER_NEAREST;
}

VkSamplerMipmapMode ToVkSamplerMipmapMode(mnexus::Filter value) {
  switch (value) {
  case mnexus::Filter::kNearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  case mnexus::Filter::kLinear:  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
  MBASE_LOG_ERROR("Unknown mnexus::Filter value");
  return VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

VkSamplerAddressMode ToVkSamplerAddressMode(mnexus::AddressMode value) {
  switch (value) {
  case mnexus::AddressMode::kRepeat:       return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case mnexus::AddressMode::kMirrorRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  case mnexus::AddressMode::kClampToEdge:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
  MBASE_LOG_ERROR("Unknown mnexus::AddressMode value");
  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

// ====================================================================================================
// Compare / stencil / blend
//

VkCompareOp ToVkCompareOp(mnexus::CompareOp value) {
  switch (value) {
  case mnexus::CompareOp::kNever:        return VK_COMPARE_OP_NEVER;
  case mnexus::CompareOp::kLess:         return VK_COMPARE_OP_LESS;
  case mnexus::CompareOp::kEqual:        return VK_COMPARE_OP_EQUAL;
  case mnexus::CompareOp::kLessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
  case mnexus::CompareOp::kGreater:      return VK_COMPARE_OP_GREATER;
  case mnexus::CompareOp::kNotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
  case mnexus::CompareOp::kGreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
  case mnexus::CompareOp::kAlways:       return VK_COMPARE_OP_ALWAYS;
  }
  MBASE_LOG_ERROR("Unknown mnexus::CompareOp value");
  return VK_COMPARE_OP_ALWAYS;
}

VkStencilOp ToVkStencilOp(mnexus::StencilOp value) {
  switch (value) {
  case mnexus::StencilOp::kKeep:           return VK_STENCIL_OP_KEEP;
  case mnexus::StencilOp::kZero:           return VK_STENCIL_OP_ZERO;
  case mnexus::StencilOp::kReplace:        return VK_STENCIL_OP_REPLACE;
  case mnexus::StencilOp::kIncrementClamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
  case mnexus::StencilOp::kDecrementClamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
  case mnexus::StencilOp::kInvert:         return VK_STENCIL_OP_INVERT;
  case mnexus::StencilOp::kIncrementWrap:  return VK_STENCIL_OP_INCREMENT_AND_WRAP;
  case mnexus::StencilOp::kDecrementWrap:  return VK_STENCIL_OP_DECREMENT_AND_WRAP;
  }
  MBASE_LOG_ERROR("Unknown mnexus::StencilOp value");
  return VK_STENCIL_OP_KEEP;
}

VkBlendFactor ToVkBlendFactor(mnexus::BlendFactor value) {
  switch (value) {
  case mnexus::BlendFactor::kZero:              return VK_BLEND_FACTOR_ZERO;
  case mnexus::BlendFactor::kOne:               return VK_BLEND_FACTOR_ONE;
  case mnexus::BlendFactor::kSrcColor:          return VK_BLEND_FACTOR_SRC_COLOR;
  case mnexus::BlendFactor::kOneMinusSrcColor:  return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  case mnexus::BlendFactor::kSrcAlpha:          return VK_BLEND_FACTOR_SRC_ALPHA;
  case mnexus::BlendFactor::kOneMinusSrcAlpha:  return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  case mnexus::BlendFactor::kDstColor:          return VK_BLEND_FACTOR_DST_COLOR;
  case mnexus::BlendFactor::kOneMinusDstColor:  return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  case mnexus::BlendFactor::kDstAlpha:          return VK_BLEND_FACTOR_DST_ALPHA;
  case mnexus::BlendFactor::kOneMinusDstAlpha:  return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  case mnexus::BlendFactor::kSrcAlphaSaturated: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
  case mnexus::BlendFactor::kConstant:          return VK_BLEND_FACTOR_CONSTANT_COLOR;
  case mnexus::BlendFactor::kOneMinusConstant:  return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
  }
  MBASE_LOG_ERROR("Unknown mnexus::BlendFactor value");
  return VK_BLEND_FACTOR_ZERO;
}

VkBlendOp ToVkBlendOp(mnexus::BlendOp value) {
  switch (value) {
  case mnexus::BlendOp::kAdd:             return VK_BLEND_OP_ADD;
  case mnexus::BlendOp::kSubtract:        return VK_BLEND_OP_SUBTRACT;
  case mnexus::BlendOp::kReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
  case mnexus::BlendOp::kMin:             return VK_BLEND_OP_MIN;
  case mnexus::BlendOp::kMax:             return VK_BLEND_OP_MAX;
  }
  MBASE_LOG_ERROR("Unknown mnexus::BlendOp value");
  return VK_BLEND_OP_ADD;
}

// ====================================================================================================
// Rasterization
//

VkCullModeFlags ToVkCullMode(mnexus::CullMode value) {
  switch (value) {
  case mnexus::CullMode::kNone:  return VK_CULL_MODE_NONE;
  case mnexus::CullMode::kFront: return VK_CULL_MODE_FRONT_BIT;
  case mnexus::CullMode::kBack:  return VK_CULL_MODE_BACK_BIT;
  }
  MBASE_LOG_ERROR("Unknown mnexus::CullMode value");
  return VK_CULL_MODE_NONE;
}

VkFrontFace ToVkFrontFace(mnexus::FrontFace value) {
  switch (value) {
  case mnexus::FrontFace::kCounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  case mnexus::FrontFace::kClockwise:        return VK_FRONT_FACE_CLOCKWISE;
  }
  MBASE_LOG_ERROR("Unknown mnexus::FrontFace value");
  return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

VkPrimitiveTopology ToVkPrimitiveTopology(mnexus::PrimitiveTopology value) {
  switch (value) {
  case mnexus::PrimitiveTopology::kPointList:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  case mnexus::PrimitiveTopology::kLineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  case mnexus::PrimitiveTopology::kLineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  case mnexus::PrimitiveTopology::kTriangleList:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case mnexus::PrimitiveTopology::kTriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  }
  MBASE_LOG_ERROR("Unknown mnexus::PrimitiveTopology value");
  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkPolygonMode ToVkPolygonMode(mnexus::PolygonMode value) {
  switch (value) {
  case mnexus::PolygonMode::kFill:  return VK_POLYGON_MODE_FILL;
  case mnexus::PolygonMode::kLine:  return VK_POLYGON_MODE_LINE;
  case mnexus::PolygonMode::kPoint: return VK_POLYGON_MODE_POINT;
  }
  MBASE_LOG_ERROR("Unknown mnexus::PolygonMode value");
  return VK_POLYGON_MODE_FILL;
}

// ====================================================================================================
// Vertex input
//

VkIndexType ToVkIndexType(mnexus::IndexType value) {
  switch (value) {
  case mnexus::IndexType::kUint16: return VK_INDEX_TYPE_UINT16;
  case mnexus::IndexType::kUint32: return VK_INDEX_TYPE_UINT32;
  }
  MBASE_LOG_ERROR("Unknown mnexus::IndexType value");
  return VK_INDEX_TYPE_UINT16;
}

VkVertexInputRate ToVkVertexInputRate(mnexus::VertexStepMode value) {
  switch (value) {
  case mnexus::VertexStepMode::kVertex:   return VK_VERTEX_INPUT_RATE_VERTEX;
  case mnexus::VertexStepMode::kInstance: return VK_VERTEX_INPUT_RATE_INSTANCE;
  }
  MBASE_LOG_ERROR("Unknown mnexus::VertexStepMode value");
  return VK_VERTEX_INPUT_RATE_VERTEX;
}

VkVertexInputBindingDescription ToVkVertexInputBindingDescription(mnexus::VertexInputBindingDesc const& value) {
  return VkVertexInputBindingDescription {
    .binding   = value.binding,
    .stride    = value.stride,
    .inputRate = ToVkVertexInputRate(value.step_mode),
  };
}

VkVertexInputAttributeDescription ToVkVertexInputAttributeDescription(mnexus::VertexInputAttributeDesc const& value) {
  return VkVertexInputAttributeDescription {
    .location = value.location,
    .binding  = value.binding,
    .format   = ToVkFormat(value.format),
    .offset   = value.offset,
  };
}

// ====================================================================================================
// Render passes
//

VkAttachmentLoadOp ToVkAttachmentLoadOp(mnexus::LoadOp value) {
  switch (value) {
  case mnexus::LoadOp::kLoad:     return VK_ATTACHMENT_LOAD_OP_LOAD;
  case mnexus::LoadOp::kClear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
  case mnexus::LoadOp::kDontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  }
  MBASE_LOG_ERROR("Unknown mnexus::LoadOp value");
  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp ToVkAttachmentStoreOp(mnexus::StoreOp value) {
  switch (value) {
  case mnexus::StoreOp::kStore:    return VK_ATTACHMENT_STORE_OP_STORE;
  case mnexus::StoreOp::kDontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }
  MBASE_LOG_ERROR("Unknown mnexus::StoreOp value");
  return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

// ====================================================================================================
// Image aspect / subresource range
//

VkImageAspectFlags ToVkImageAspectMask(mnexus::TextureAspectFlags flags) {
  VkImageAspectFlags result = 0;
  if (flags.HasAnyOf(mnexus::TextureAspectFlagBits::kColor))    { result |= VK_IMAGE_ASPECT_COLOR_BIT; }
  if (flags.HasAnyOf(mnexus::TextureAspectFlagBits::kDepth))    { result |= VK_IMAGE_ASPECT_DEPTH_BIT; }
  if (flags.HasAnyOf(mnexus::TextureAspectFlagBits::kStencil))  { result |= VK_IMAGE_ASPECT_STENCIL_BIT; }
  if (flags.HasAnyOf(mnexus::TextureAspectFlagBits::kMetadata)) { result |= VK_IMAGE_ASPECT_METADATA_BIT; }
  if (flags.HasAnyOf(mnexus::TextureAspectFlagBits::kPlane0))   { result |= VK_IMAGE_ASPECT_PLANE_0_BIT; }
  if (flags.HasAnyOf(mnexus::TextureAspectFlagBits::kPlane1))   { result |= VK_IMAGE_ASPECT_PLANE_1_BIT; }
  if (flags.HasAnyOf(mnexus::TextureAspectFlagBits::kPlane2))   { result |= VK_IMAGE_ASPECT_PLANE_2_BIT; }
  return result;
}

VkImageSubresourceRange ToVkImageSubresourceRange(mnexus::TextureSubresourceRange const& subresource_range) {
  return VkImageSubresourceRange {
    .aspectMask     = ToVkImageAspectMask(subresource_range.aspect_mask),
    .baseMipLevel   = subresource_range.base_mip_level,
    .levelCount     = subresource_range.mip_level_count,
    .baseArrayLayer = subresource_range.base_array_layer,
    .layerCount     = subresource_range.array_layer_count,
  };
}

VkImageSubresourceLayers ToVkImageSubresourceLayers(mnexus::TextureSubresourceRange const& subresource_range) {
  return VkImageSubresourceLayers {
    .aspectMask     = ToVkImageAspectMask(subresource_range.aspect_mask),
    .mipLevel       = subresource_range.base_mip_level,
    .baseArrayLayer = subresource_range.base_array_layer,
    .layerCount     = subresource_range.array_layer_count,
  };
}

// ====================================================================================================
// Descriptor type (from BindGroupLayoutEntryType)
//

VkDescriptorType ToVkDescriptorType(mnexus::BindGroupLayoutEntryType value) {
  switch (value) {
  case mnexus::BindGroupLayoutEntryType::kUniformBuffer:          return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  case mnexus::BindGroupLayoutEntryType::kStorageBuffer:          return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  case mnexus::BindGroupLayoutEntryType::kSampledTexture:         return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  case mnexus::BindGroupLayoutEntryType::kSampler:                return VK_DESCRIPTOR_TYPE_SAMPLER;
  case mnexus::BindGroupLayoutEntryType::kStorageTexture:         return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  case mnexus::BindGroupLayoutEntryType::kCombinedTextureSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  case mnexus::BindGroupLayoutEntryType::kAccelerationStructure:  return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
  }
  MBASE_LOG_ERROR("Unknown mnexus::BindGroupLayoutEntryType value");
  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

} // namespace mnexus_backend::vulkan
