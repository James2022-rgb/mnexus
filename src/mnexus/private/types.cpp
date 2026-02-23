// TU header --------------------------------------------
#include "mnexus/public/types.h"

// c++ headers ------------------------------------------
#include <unordered_map>

// public project headers -------------------------------
#include "mbase/public/type_util.h"
#include "mbase/public/algorithm/my_ostream_joiner.h"

// ----------------------------------------------------------------------------------------------------
// Format queries (C API)
//

uint32_t MnGetFormatSizeInBytes(MnFormat value) {
  switch (value) {
  case MnFormatUndefined:                  return 0;

  // 16-bit packed.
  case MnFormatR5G6B5_UNORM_PACK16:        return 2;
  case MnFormatR5G5B5A1_UNORM_PACK16:      return 2;

  // 8-bit per channel.
  case MnFormatR8_UNORM:                   return 1;
  case MnFormatR8G8_UNORM:                 return 2;
  case MnFormatR8G8B8_UNORM:               return 3;
  case MnFormatR8G8B8A8_UNORM:             return 4;
  case MnFormatR8G8B8A8_SRGB:              return 4;
  case MnFormatB8G8R8A8_UNORM:             return 4;
  case MnFormatB8G8R8A8_SRGB:              return 4;

  // 16-bit per channel.
  case MnFormatR16_SFLOAT:                 return 2;
  case MnFormatR16G16_SFLOAT:              return 4;
  case MnFormatR16G16B16_SFLOAT:           return 6;
  case MnFormatR16G16B16A16_UNORM:         return 8;
  case MnFormatR16G16B16A16_UINT:          return 8;
  case MnFormatR16G16B16A16_SFLOAT:        return 8;

  // 32-bit per channel.
  case MnFormatR32_SFLOAT:                 return 4;
  case MnFormatR32G32_SFLOAT:              return 8;
  case MnFormatR32G32B32_SFLOAT:           return 12;
  case MnFormatR32G32B32A32_UINT:          return 16;
  case MnFormatR32G32B32A32_SFLOAT:        return 16;

  // 10/10/10/2 packed.
  case MnFormatA2R10G10B10_UNORM_PACK32:   return 4;
  case MnFormatA2R10G10B10_SNORM_PACK32:   return 4;
  case MnFormatA2R10G10B10_USCALED_PACK32: return 4;
  case MnFormatA2R10G10B10_SSCALED_PACK32: return 4;
  case MnFormatA2R10G10B10_UINT_PACK32:    return 4;
  case MnFormatA2R10G10B10_SINT_PACK32:    return 4;
  case MnFormatA2B10G10R10_UNORM_PACK32:   return 4;
  case MnFormatA2B10G10R10_SNORM_PACK32:   return 4;
  case MnFormatA2B10G10R10_USCALED_PACK32: return 4;
  case MnFormatA2B10G10R10_SSCALED_PACK32: return 4;
  case MnFormatA2B10G10R10_UINT_PACK32:    return 4;
  case MnFormatA2B10G10R10_SINT_PACK32:    return 4;

  // Depth/stencil.
  case MnFormatD16_UNORM:                  return 2;
  case MnFormatD32_SFLOAT:                 return 4;
  case MnFormatD16_UNORM_S8_UINT:          return 3;
  case MnFormatD24_UNORM_S8_UINT:          return 4;
  case MnFormatD32_SFLOAT_S8_UINT:         return 5;

  // BC (block size = bytes per 4x4 block).
  case MnFormatBC1_RGB_UNORM_BLOCK:        return 8;
  case MnFormatBC1_RGB_SRGB_BLOCK:         return 8;
  case MnFormatBC1_RGBA_UNORM_BLOCK:       return 8;
  case MnFormatBC1_RGBA_SRGB_BLOCK:        return 8;
  case MnFormatBC2_UNORM_BLOCK:            return 16;
  case MnFormatBC2_SRGB_BLOCK:             return 16;
  case MnFormatBC3_UNORM_BLOCK:            return 16;
  case MnFormatBC3_SRGB_BLOCK:             return 16;
  case MnFormatBC4_UNORM_BLOCK:            return 8;
  case MnFormatBC4_SNORM_BLOCK:            return 8;
  case MnFormatBC5_UNORM_BLOCK:            return 16;
  case MnFormatBC5_SNORM_BLOCK:            return 16;

  // ETC2/EAC (block size = bytes per 4x4 block).
  case MnFormatETC2_R8G8B8_UNORM_BLOCK:    return 8;
  case MnFormatETC2_R8G8B8_SRGB_BLOCK:     return 8;
  case MnFormatETC2_R8G8B8A1_UNORM_BLOCK:  return 8;
  case MnFormatETC2_R8G8B8A1_SRGB_BLOCK:   return 8;
  case MnFormatETC2_R8G8B8A8_UNORM_BLOCK:  return 16;
  case MnFormatETC2_R8G8B8A8_SRGB_BLOCK:   return 16;
  case MnFormatEAC_R11_UNORM_BLOCK:        return 8;
  case MnFormatEAC_R11_SNORM_BLOCK:        return 8;
  case MnFormatEAC_R11G11_UNORM_BLOCK:     return 16;
  case MnFormatEAC_R11G11_SNORM_BLOCK:     return 16;

  // ASTC (all block sizes are 16 bytes per block).
  case MnFormatASTC_4x4_UNORM_BLOCK:       return 16;
  case MnFormatASTC_4x4_SRGB_BLOCK:        return 16;
  case MnFormatASTC_5x4_UNORM_BLOCK:       return 16;
  case MnFormatASTC_5x4_SRGB_BLOCK:        return 16;
  case MnFormatASTC_5x5_UNORM_BLOCK:       return 16;
  case MnFormatASTC_5x5_SRGB_BLOCK:        return 16;
  case MnFormatASTC_6x5_UNORM_BLOCK:       return 16;
  case MnFormatASTC_6x5_SRGB_BLOCK:        return 16;
  case MnFormatASTC_6x6_UNORM_BLOCK:       return 16;
  case MnFormatASTC_6x6_SRGB_BLOCK:        return 16;
  case MnFormatASTC_8x5_UNORM_BLOCK:       return 16;
  case MnFormatASTC_8x5_SRGB_BLOCK:        return 16;
  case MnFormatASTC_8x6_UNORM_BLOCK:       return 16;
  case MnFormatASTC_8x6_SRGB_BLOCK:        return 16;
  case MnFormatASTC_8x8_UNORM_BLOCK:       return 16;
  case MnFormatASTC_8x8_SRGB_BLOCK:        return 16;
  case MnFormatASTC_10x5_UNORM_BLOCK:      return 16;
  case MnFormatASTC_10x5_SRGB_BLOCK:       return 16;
  case MnFormatASTC_10x6_UNORM_BLOCK:      return 16;
  case MnFormatASTC_10x6_SRGB_BLOCK:       return 16;
  case MnFormatASTC_10x8_UNORM_BLOCK:      return 16;
  case MnFormatASTC_10x8_SRGB_BLOCK:       return 16;
  case MnFormatASTC_10x10_UNORM_BLOCK:     return 16;
  case MnFormatASTC_10x10_SRGB_BLOCK:      return 16;
  case MnFormatASTC_12x10_UNORM_BLOCK:     return 16;
  case MnFormatASTC_12x10_SRGB_BLOCK:      return 16;
  case MnFormatASTC_12x12_UNORM_BLOCK:     return 16;
  case MnFormatASTC_12x12_SRGB_BLOCK:      return 16;
  }
  return 0;
}

MnExtent3d MnGetFormatTexelBlockExtent(MnFormat value) {
  switch (value) {
  // BC formats: 4x4 blocks.
  case MnFormatBC1_RGB_UNORM_BLOCK:
  case MnFormatBC1_RGB_SRGB_BLOCK:
  case MnFormatBC1_RGBA_UNORM_BLOCK:
  case MnFormatBC1_RGBA_SRGB_BLOCK:
  case MnFormatBC2_UNORM_BLOCK:
  case MnFormatBC2_SRGB_BLOCK:
  case MnFormatBC3_UNORM_BLOCK:
  case MnFormatBC3_SRGB_BLOCK:
  case MnFormatBC4_UNORM_BLOCK:
  case MnFormatBC4_SNORM_BLOCK:
  case MnFormatBC5_UNORM_BLOCK:
  case MnFormatBC5_SNORM_BLOCK:
  // ETC2/EAC formats: 4x4 blocks.
  case MnFormatETC2_R8G8B8_UNORM_BLOCK:
  case MnFormatETC2_R8G8B8_SRGB_BLOCK:
  case MnFormatETC2_R8G8B8A1_UNORM_BLOCK:
  case MnFormatETC2_R8G8B8A1_SRGB_BLOCK:
  case MnFormatETC2_R8G8B8A8_UNORM_BLOCK:
  case MnFormatETC2_R8G8B8A8_SRGB_BLOCK:
  case MnFormatEAC_R11_UNORM_BLOCK:
  case MnFormatEAC_R11_SNORM_BLOCK:
  case MnFormatEAC_R11G11_UNORM_BLOCK:
  case MnFormatEAC_R11G11_SNORM_BLOCK:
  // ASTC 4x4.
  case MnFormatASTC_4x4_UNORM_BLOCK:
  case MnFormatASTC_4x4_SRGB_BLOCK:
    return { 4, 4, 1 };

  case MnFormatASTC_5x4_UNORM_BLOCK:
  case MnFormatASTC_5x4_SRGB_BLOCK:
    return { 5, 4, 1 };
  case MnFormatASTC_5x5_UNORM_BLOCK:
  case MnFormatASTC_5x5_SRGB_BLOCK:
    return { 5, 5, 1 };
  case MnFormatASTC_6x5_UNORM_BLOCK:
  case MnFormatASTC_6x5_SRGB_BLOCK:
    return { 6, 5, 1 };
  case MnFormatASTC_6x6_UNORM_BLOCK:
  case MnFormatASTC_6x6_SRGB_BLOCK:
    return { 6, 6, 1 };
  case MnFormatASTC_8x5_UNORM_BLOCK:
  case MnFormatASTC_8x5_SRGB_BLOCK:
    return { 8, 5, 1 };
  case MnFormatASTC_8x6_UNORM_BLOCK:
  case MnFormatASTC_8x6_SRGB_BLOCK:
    return { 8, 6, 1 };
  case MnFormatASTC_8x8_UNORM_BLOCK:
  case MnFormatASTC_8x8_SRGB_BLOCK:
    return { 8, 8, 1 };
  case MnFormatASTC_10x5_UNORM_BLOCK:
  case MnFormatASTC_10x5_SRGB_BLOCK:
    return { 10, 5, 1 };
  case MnFormatASTC_10x6_UNORM_BLOCK:
  case MnFormatASTC_10x6_SRGB_BLOCK:
    return { 10, 6, 1 };
  case MnFormatASTC_10x8_UNORM_BLOCK:
  case MnFormatASTC_10x8_SRGB_BLOCK:
    return { 10, 8, 1 };
  case MnFormatASTC_10x10_UNORM_BLOCK:
  case MnFormatASTC_10x10_SRGB_BLOCK:
    return { 10, 10, 1 };
  case MnFormatASTC_12x10_UNORM_BLOCK:
  case MnFormatASTC_12x10_SRGB_BLOCK:
    return { 12, 10, 1 };
  case MnFormatASTC_12x12_UNORM_BLOCK:
  case MnFormatASTC_12x12_SRGB_BLOCK:
    return { 12, 12, 1 };

  // All uncompressed formats: 1x1x1.
  default:
    return { 1, 1, 1 };
  }
}

namespace mnexus {

//
// QueueId
//

bool QueueId::InSameQueueFamily(std::optional<QueueId> const& lhs, std::optional<QueueId> const& rhs) {
  if (!lhs.has_value() || !rhs.has_value()) {
    return false;
  }
  return lhs->queue_family_index == rhs->queue_family_index;
}

bool QueueId::InDifferentQueueFamily(std::optional<QueueId> const& needle, std::span<std::optional<QueueId> const> haystack) {
  if (!needle.has_value()) {
    return false;
  }
  return std::none_of(
    haystack.begin(),
    haystack.end(),
    [&needle](std::optional<QueueId> const& queue_id) {
      return queue_id.has_value() && queue_id->queue_family_index == needle->queue_family_index;
    }
  );
}

bool operator==(QueueId const& lhs, QueueId const& rhs) {
  return
    lhs.queue_family_index == rhs.queue_family_index &&
    lhs.queue_index == rhs.queue_index;
}

//
// TextureSubresourceRange
//

TextureSubresourceRange TextureSubresourceRange::SingleSubresourceColor(uint32_t base_mip_level, uint32_t base_array_layer) {
  return SingleSubresource(TextureAspectFlagBits::kColor, base_mip_level, base_array_layer);
}
TextureSubresourceRange TextureSubresourceRange::SingleSubresourceDepth(uint32_t base_mip_level, uint32_t base_array_layer) {
  return SingleSubresource(TextureAspectFlagBits::kDepth, base_mip_level, base_array_layer);
}
TextureSubresourceRange TextureSubresourceRange::SingleSubresource(TextureAspectFlags aspect_mask, uint32_t base_mip_level, uint32_t base_array_layer) {
  return TextureSubresourceRange {
    .aspect_mask = aspect_mask,
    .base_mip_level = base_mip_level,
    .mip_level_count = 1,
    .base_array_layer = base_array_layer,
    .array_layer_count = 1,
  };
}

namespace {

template<class T>
constexpr bool IsPot(T value) {
  auto v = mbase::UnderlyingCast(value);
  return (v != 0) && ((v & (v - 1)) == 0);
}

template<class TFlags, class TFlagBits>
std::string DescribeFlagBits(TFlags value, std::unordered_map<TFlagBits, std::string> const& table) {
  std::ostringstream oss;
  mbase::my_ostream_joiner<std::string> joiner(oss);

  for (const auto& [ key, description ] : table) {
    if (IsPot(key) && bool(value  & key)) {
      joiner = description;
    }
  }

  std::string result = oss.str();
  return !result.empty() ? result : "N/A";
}

}

std::string_view ToString(MnFormat value) {
  // In C++11 and later, std::string is guaranteed to be contiguous and null-terminated.

  #define MAP(value) { MnFormat##value, #value }

  static std::unordered_map<MnFormat, std::string> const kTable = {
    MAP(Undefined),
    MAP(R5G6B5_UNORM_PACK16),
    MAP(R5G5B5A1_UNORM_PACK16),
    MAP(R8_UNORM),
    MAP(R8G8_UNORM),
    MAP(R8G8B8_UNORM),
    MAP(R8G8B8A8_UNORM),
    MAP(R8G8B8A8_SRGB),
    MAP(B8G8R8A8_UNORM),
    MAP(B8G8R8A8_SRGB),
    MAP(R16_SFLOAT),
    MAP(R16G16_SFLOAT),
    MAP(R16G16B16_SFLOAT),
    MAP(R16G16B16A16_UNORM),
    MAP(R16G16B16A16_UINT),
    MAP(R16G16B16A16_SFLOAT),
    MAP(R32_SFLOAT),
    MAP(R32G32_SFLOAT),
    MAP(R32G32B32_SFLOAT),
    MAP(R32G32B32A32_UINT),
    MAP(R32G32B32A32_SFLOAT),
    MAP(A2R10G10B10_UNORM_PACK32),
    MAP(A2R10G10B10_SNORM_PACK32),
    MAP(A2R10G10B10_USCALED_PACK32),
    MAP(A2R10G10B10_SSCALED_PACK32),
    MAP(A2R10G10B10_UINT_PACK32),
    MAP(A2R10G10B10_SINT_PACK32),
    MAP(A2B10G10R10_UNORM_PACK32),
    MAP(A2B10G10R10_SNORM_PACK32),
    MAP(A2B10G10R10_USCALED_PACK32),
    MAP(A2B10G10R10_SSCALED_PACK32),
    MAP(A2B10G10R10_UINT_PACK32),
    MAP(A2B10G10R10_SINT_PACK32),
    MAP(D16_UNORM),
    MAP(D32_SFLOAT),
    MAP(D16_UNORM_S8_UINT),
    MAP(D24_UNORM_S8_UINT),
    MAP(D32_SFLOAT_S8_UINT),
    MAP(BC1_RGB_UNORM_BLOCK),
    MAP(BC1_RGB_SRGB_BLOCK),
    MAP(BC1_RGBA_UNORM_BLOCK),
    MAP(BC1_RGBA_SRGB_BLOCK),
    MAP(BC2_UNORM_BLOCK),
    MAP(BC2_SRGB_BLOCK),
    MAP(BC3_UNORM_BLOCK),
    MAP(BC3_SRGB_BLOCK),
    MAP(BC4_UNORM_BLOCK),
    MAP(BC4_SNORM_BLOCK),
    MAP(BC5_UNORM_BLOCK),
    MAP(BC5_SNORM_BLOCK),
    MAP(ETC2_R8G8B8_UNORM_BLOCK),
    MAP(ETC2_R8G8B8_SRGB_BLOCK),
    MAP(ETC2_R8G8B8A1_UNORM_BLOCK),
    MAP(ETC2_R8G8B8A1_SRGB_BLOCK),
    MAP(ETC2_R8G8B8A8_UNORM_BLOCK),
    MAP(ETC2_R8G8B8A8_SRGB_BLOCK),
    MAP(EAC_R11_UNORM_BLOCK),
    MAP(EAC_R11_SNORM_BLOCK),
    MAP(EAC_R11G11_UNORM_BLOCK),
    MAP(EAC_R11G11_SNORM_BLOCK),
    MAP(ASTC_4x4_UNORM_BLOCK),
    MAP(ASTC_4x4_SRGB_BLOCK),
    MAP(ASTC_5x4_UNORM_BLOCK),
    MAP(ASTC_5x4_SRGB_BLOCK),
    MAP(ASTC_5x5_UNORM_BLOCK),
    MAP(ASTC_5x5_SRGB_BLOCK),
    MAP(ASTC_6x5_UNORM_BLOCK),
    MAP(ASTC_6x5_SRGB_BLOCK),
    MAP(ASTC_6x6_UNORM_BLOCK),
    MAP(ASTC_6x6_SRGB_BLOCK),
    MAP(ASTC_8x5_UNORM_BLOCK),
    MAP(ASTC_8x5_SRGB_BLOCK),
    MAP(ASTC_8x6_UNORM_BLOCK),
    MAP(ASTC_8x6_SRGB_BLOCK),
    MAP(ASTC_8x8_UNORM_BLOCK),
    MAP(ASTC_8x8_SRGB_BLOCK),
    MAP(ASTC_10x5_UNORM_BLOCK),
    MAP(ASTC_10x5_SRGB_BLOCK),
    MAP(ASTC_10x6_UNORM_BLOCK),
    MAP(ASTC_10x6_SRGB_BLOCK),
    MAP(ASTC_10x8_UNORM_BLOCK),
    MAP(ASTC_10x8_SRGB_BLOCK),
    MAP(ASTC_10x10_UNORM_BLOCK),
    MAP(ASTC_10x10_SRGB_BLOCK),
    MAP(ASTC_12x10_UNORM_BLOCK),
    MAP(ASTC_12x10_SRGB_BLOCK),
    MAP(ASTC_12x12_UNORM_BLOCK),
    MAP(ASTC_12x12_SRGB_BLOCK),
  };
  #undef MAP

  if (kTable.count(value) > 0) {
    return kTable.at(value);
  }

  static std::string const kError = "N/A";
  return kError;
}

std::string_view ToString(BindGroupLayoutEntryType value) {
  #define MAP(value) { BindGroupLayoutEntryType::k##value, #value }

  static std::unordered_map<BindGroupLayoutEntryType, std::string> const kTable = {
    MAP(UniformBuffer),
    MAP(StorageBuffer),
    MAP(SampledTexture),
    MAP(Sampler),
    MAP(StorageTexture),
    MAP(AccelerationStructure),
  };
  #undef MAP

  if (kTable.count(value) > 0) {
    return kTable.at(value);
  }

  static std::string const kError = "N/A";
  return kError;
}

std::string ToString(TextureUsageFlags value) {
  #define MAP(value) { TextureUsageFlagBits::k##value, #value }

  static std::unordered_map<TextureUsageFlagBits, std::string> const kTable = {
    MAP(Attachment),
    MAP(TileLocal),
    MAP(Sampled),
    MAP(UnorderedAccess),
    MAP(TransferSrc),
    MAP(TransferDst),
  };
  #undef MAP

  return DescribeFlagBits(value, kTable);
}

std::string_view ToString(PrimitiveTopology value) {
  switch (value) {
  case PrimitiveTopology::kPointList:     return "PointList";
  case PrimitiveTopology::kLineList:      return "LineList";
  case PrimitiveTopology::kLineStrip:     return "LineStrip";
  case PrimitiveTopology::kTriangleList:  return "TriangleList";
  case PrimitiveTopology::kTriangleStrip: return "TriangleStrip";
  }
  return "N/A";
}

std::string_view ToString(PolygonMode value) {
  switch (value) {
  case PolygonMode::kFill:  return "Fill";
  case PolygonMode::kLine:  return "Line";
  case PolygonMode::kPoint: return "Point";
  }
  return "N/A";
}

std::string_view ToString(CullMode value) {
  switch (value) {
  case CullMode::kNone:  return "None";
  case CullMode::kFront: return "Front";
  case CullMode::kBack:  return "Back";
  }
  return "N/A";
}

std::string_view ToString(FrontFace value) {
  switch (value) {
  case FrontFace::kCounterClockwise: return "CounterClockwise";
  case FrontFace::kClockwise:        return "Clockwise";
  }
  return "N/A";
}

std::string_view ToString(CompareOp value) {
  switch (value) {
  case CompareOp::kNever:        return "Never";
  case CompareOp::kLess:         return "Less";
  case CompareOp::kEqual:        return "Equal";
  case CompareOp::kLessEqual:    return "LessEqual";
  case CompareOp::kGreater:      return "Greater";
  case CompareOp::kNotEqual:     return "NotEqual";
  case CompareOp::kGreaterEqual: return "GreaterEqual";
  case CompareOp::kAlways:       return "Always";
  }
  return "N/A";
}

std::string_view ToString(StencilOp value) {
  switch (value) {
  case StencilOp::kKeep:           return "Keep";
  case StencilOp::kZero:           return "Zero";
  case StencilOp::kReplace:        return "Replace";
  case StencilOp::kIncrementClamp: return "IncrementClamp";
  case StencilOp::kDecrementClamp: return "DecrementClamp";
  case StencilOp::kInvert:         return "Invert";
  case StencilOp::kIncrementWrap:  return "IncrementWrap";
  case StencilOp::kDecrementWrap:  return "DecrementWrap";
  }
  return "N/A";
}

std::string_view ToString(BlendFactor value) {
  switch (value) {
  case BlendFactor::kZero:             return "Zero";
  case BlendFactor::kOne:              return "One";
  case BlendFactor::kSrcColor:         return "SrcColor";
  case BlendFactor::kOneMinusSrcColor: return "OneMinusSrcColor";
  case BlendFactor::kSrcAlpha:         return "SrcAlpha";
  case BlendFactor::kOneMinusSrcAlpha: return "OneMinusSrcAlpha";
  case BlendFactor::kDstColor:         return "DstColor";
  case BlendFactor::kOneMinusDstColor: return "OneMinusDstColor";
  case BlendFactor::kDstAlpha:         return "DstAlpha";
  case BlendFactor::kOneMinusDstAlpha: return "OneMinusDstAlpha";
  case BlendFactor::kSrcAlphaSaturated: return "SrcAlphaSaturated";
  case BlendFactor::kConstant:         return "Constant";
  case BlendFactor::kOneMinusConstant: return "OneMinusConstant";
  }
  return "N/A";
}

std::string_view ToString(BlendOp value) {
  switch (value) {
  case BlendOp::kAdd:             return "Add";
  case BlendOp::kSubtract:        return "Subtract";
  case BlendOp::kReverseSubtract: return "ReverseSubtract";
  case BlendOp::kMin:             return "Min";
  case BlendOp::kMax:             return "Max";
  }
  return "N/A";
}

std::string_view ToString(IndexType value) {
  switch (value) {
  case IndexType::kUint16: return "Uint16";
  case IndexType::kUint32: return "Uint32";
  }
  return "N/A";
}

std::string_view ToString(VertexStepMode value) {
  switch (value) {
  case VertexStepMode::kVertex:   return "Vertex";
  case VertexStepMode::kInstance: return "Instance";
  }
  return "N/A";
}

std::string_view ToString(LoadOp value) {
  switch (value) {
  case LoadOp::kLoad:     return "Load";
  case LoadOp::kClear:    return "Clear";
  case LoadOp::kDontCare: return "DontCare";
  }
  return "N/A";
}

std::string_view ToString(StoreOp value) {
  switch (value) {
  case StoreOp::kStore:    return "Store";
  case StoreOp::kDontCare: return "DontCare";
  }
  return "N/A";
}

} // namespace mnexus
