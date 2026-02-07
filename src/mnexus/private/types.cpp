// TU header --------------------------------------------
#include "mnexus/public/types.h"

// c++ headers ------------------------------------------
#include <unordered_map>

// public project headers -------------------------------
#include "mbase/public/type_util.h"
#include "mbase/public/algorithm/my_ostream_joiner.h"

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

  #define MAP(value) { MnFormat::k##value, #value }

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

} // namespace mnexus
