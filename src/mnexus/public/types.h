#pragma once

// c++ headers ------------------------------------------
#if defined(__cplusplus)
# include <cstdint>

# include <string_view>
# include <string>
# include <optional>
# include <span>
#endif

// public project headers -------------------------------
#if defined(__cplusplus)
# include "mbase/public/access.h"
# include "mbase/public/bitflags.h"
# include "mbase/public/type_safety.h"

# include "mnexus/public/container/array_proxy.h"
#endif

using MnBool32 = uint32_t;
static const MnBool32 MnBoolFalse = 0;
static const MnBool32 MnBoolTrue  = 1;

struct MnExtent3d final {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 0;
};

struct MnOffset3d final {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t z = 0;
};

// ----------------------------------------------------------------------------------------------------
// Capability

struct MnDeviceCapability final {
  MnBool32 vertex_shader_storage_write = MnBoolFalse;
  // N.B.: See `mnexus::DeviceCapability`.
};

// ----------------------------------------------------------------------------------------------------
// Handles
//

typedef uint64_t MnResourceHandle;

const MnResourceHandle MnInvalidResourceHandle = 0x00000000FFFFFFFF;

// ----------------------------------------------------------------------------------------------------
// Queue
//

struct MnQueueId final {
  uint32_t queue_family_index = 0;
  uint32_t queue_index = 0;
  // N.B.: See `mnexus::QueueId`.
};

typedef uint64_t MnIntraQueueSubmissionId;

enum MnQueueFamilyCapabilityFlagBits {
  MnQueueFamilyCapabilityFlagBitNone        = 0,
  MnQueueFamilyCapabilityFlagBitGraphics    = 1 << 0,
  MnQueueFamilyCapabilityFlagBitCompute     = 1 << 1,
  MnQueueFamilyCapabilityFlagBitTransfer    = 1 << 2,
  MnQueueFamilyCapabilityFlagBitVideoDecode = 1 << 3,
  MnQueueFamilyCapabilityFlagBitVideoEncode = 1 << 4,
  MnQueueFamilyCapabilityFlagForce32     = 0x7FFFFFFF,
};
typedef uint32_t MnQueueFamilyCapabilityFlags;

// ----------------------------------------------------------------------------------------------------
// Buffer
//

enum MnBufferUsageFlagBits {
  MnBufferUsageFlagBitNone            = 0,
  MnBufferUsageFlagBitVertex          = 1 << 0,
  MnBufferUsageFlagBitIndex           = 1 << 1,
  MnBufferUsageFlagBitUniform         = 1 << 2,
  MnBufferUsageFlagBitStorage         = 1 << 3,
  MnBufferUsageFlagBitTransferSrc     = 1 << 4,
  MnBufferUsageFlagBitTransferDst     = 1 << 5,
  MnBufferUsageFlagBitIndirect        = 1 << 6,
  MnBufferUsageFlagForce32            = 0x7FFFFFFF,
};
typedef uint32_t MnBufferUsageFlags;

struct MnBufferDesc final {
  MnBufferUsageFlags usage;
  uint32_t size_in_bytes = 0;
  // N.B.: See `mnexus::BufferDesc`.
};

// ----------------------------------------------------------------------------------------------------
// Texture
//

enum MnTextureUsageFlagBits {
  MnTextureUsageFlagBitNone            = 0,
  MnTextureUsageFlagBitAttachment      = 1 << 0,
  MnTextureUsageFlagBitTileLocal       = 1 << 1,
  MnTextureUsageFlagBitSampled         = 1 << 2,
  MnTextureUsageFlagBitUnorderedAccess = 1 << 3,
  MnTextureUsageFlagBitTransferSrc     = 1 << 4,
  MnTextureUsageFlagBitTransferDst     = 1 << 5,
  MnTextureUsageFlagForce32            = 0x7FFFFFFF,
};
typedef uint32_t MnTextureUsageFlags;

enum MnTextureAspectFlagBits {
  MnTextureAspectFlagBitColor    = 1 << 0,
  MnTextureAspectFlagBitDepth    = 1 << 1,
  MnTextureAspectFlagBitStencil  = 1 << 2,
  MnTextureAspectFlagBitMetadata = 1 << 3,
  MnTextureAspectFlagBitPlane0   = 1 << 4,
  MnTextureAspectFlagBitPlane1   = 1 << 5,
  MnTextureAspectFlagBitPlane2   = 1 << 6,
  MnTextureAspectFlagForce32     = 0x7FFFFFFF,
};

enum MnTextureDimension {
  MnTextureDimension1D = 0,
  MnTextureDimension2D = 1,
  MnTextureDimension3D = 2,
  MnTextureDimensionCube = 3,
  MnTextureDimensionForce32 = 0x7FFFFFFF,
};

enum MnFilter {
  MnFilterNearest = 0,
  MnFilterLinear  = 1,
  MnFilterForce32 = 0x7FFFFFFF,
};

struct MnTextureSubresourceRange final {
  MnTextureAspectFlagBits aspect_mask = MnTextureAspectFlagBits::MnTextureAspectFlagBitColor;
  uint32_t base_mip_level = 0;
  uint32_t mip_level_count = 1;
  uint32_t base_array_layer = 0;
  uint32_t array_layer_count = 1;
  // N.B.: See `mnexus::TextureSubresourceRange`.
};

enum class MnFormat : uint32_t {
  kUndefined,

  kR5G6B5_UNORM_PACK16,
  kR5G5B5A1_UNORM_PACK16,

  kR8_UNORM,
  kR8G8_UNORM,
  kR8G8B8_UNORM,
  kR8G8B8A8_UNORM,
  kR8G8B8A8_SRGB,
  kB8G8R8A8_UNORM,
  kB8G8R8A8_SRGB,

  kR16_SFLOAT,
  kR16G16_SFLOAT,
  kR16G16B16_SFLOAT,
  kR16G16B16A16_UNORM,
  kR16G16B16A16_UINT,
  kR16G16B16A16_SFLOAT,

  kR32_SFLOAT,
  kR32G32_SFLOAT,
  kR32G32B32_SFLOAT,
  kR32G32B32A32_UINT,
  kR32G32B32A32_SFLOAT,

  kA2R10G10B10_UNORM_PACK32,
  kA2R10G10B10_SNORM_PACK32,
  kA2R10G10B10_USCALED_PACK32,
  kA2R10G10B10_SSCALED_PACK32,
  kA2R10G10B10_UINT_PACK32,
  kA2R10G10B10_SINT_PACK32,
  kA2B10G10R10_UNORM_PACK32,
  kA2B10G10R10_SNORM_PACK32,
  kA2B10G10R10_USCALED_PACK32,
  kA2B10G10R10_SSCALED_PACK32,
  kA2B10G10R10_UINT_PACK32,
  kA2B10G10R10_SINT_PACK32,

  kD16_UNORM,
  kD32_SFLOAT,
  kD16_UNORM_S8_UINT,
  kD24_UNORM_S8_UINT,
  kD32_SFLOAT_S8_UINT,

  // BC1 (DXT1) formats.
  kBC1_RGB_UNORM_BLOCK,
  kBC1_RGB_SRGB_BLOCK,
  kBC1_RGBA_UNORM_BLOCK,
  kBC1_RGBA_SRGB_BLOCK,
  // BC2 (DXT2/3) formats.
  kBC2_UNORM_BLOCK,
  kBC2_SRGB_BLOCK,
  // BC3 (DXT4/5) formats.
  kBC3_UNORM_BLOCK,
  kBC3_SRGB_BLOCK,
  // BC4 (ATI1) formats.
  kBC4_UNORM_BLOCK,
  kBC4_SNORM_BLOCK,
  // BC5 (ATI2) formats.
  kBC5_UNORM_BLOCK,
  kBC5_SNORM_BLOCK,

  kETC2_R8G8B8_UNORM_BLOCK,
  kETC2_R8G8B8_SRGB_BLOCK,
  kETC2_R8G8B8A1_UNORM_BLOCK,
  kETC2_R8G8B8A1_SRGB_BLOCK,
  kETC2_R8G8B8A8_UNORM_BLOCK,
  kETC2_R8G8B8A8_SRGB_BLOCK,
  kEAC_R11_UNORM_BLOCK,
  kEAC_R11_SNORM_BLOCK,
  kEAC_R11G11_UNORM_BLOCK,
  kEAC_R11G11_SNORM_BLOCK,

  kASTC_4x4_UNORM_BLOCK,
  kASTC_4x4_SRGB_BLOCK,
  kASTC_5x4_UNORM_BLOCK,
  kASTC_5x4_SRGB_BLOCK,
  kASTC_5x5_UNORM_BLOCK,
  kASTC_5x5_SRGB_BLOCK,
  kASTC_6x5_UNORM_BLOCK,
  kASTC_6x5_SRGB_BLOCK,
  kASTC_6x6_UNORM_BLOCK,
  kASTC_6x6_SRGB_BLOCK,
  kASTC_8x5_UNORM_BLOCK,
  kASTC_8x5_SRGB_BLOCK,
  kASTC_8x6_UNORM_BLOCK,
  kASTC_8x6_SRGB_BLOCK,
  kASTC_8x8_UNORM_BLOCK,
  kASTC_8x8_SRGB_BLOCK,
  kASTC_10x5_UNORM_BLOCK,
  kASTC_10x5_SRGB_BLOCK,
  kASTC_10x6_UNORM_BLOCK,
  kASTC_10x6_SRGB_BLOCK,
  kASTC_10x8_UNORM_BLOCK,
  kASTC_10x8_SRGB_BLOCK,
  kASTC_10x10_UNORM_BLOCK,
  kASTC_10x10_SRGB_BLOCK,
  kASTC_12x10_UNORM_BLOCK,
  kASTC_12x10_SRGB_BLOCK,
  kASTC_12x12_UNORM_BLOCK,
  kASTC_12x12_SRGB_BLOCK,
};

/// Returns the size in bytes of a single texel (or compressed block) for the given format.
/// Returns 0 for `kUndefined`.
uint32_t MnGetFormatSizeInBytes(MnFormat value);

/// Returns the texel block extent for the given format.
/// For uncompressed formats, returns {1, 1, 1}.
/// For block-compressed formats (BC, ETC2, ASTC), returns the block dimensions.
MnExtent3d MnGetFormatTexelBlockExtent(MnFormat value);

// ----------------------------------------------------------------------------------------------------
// Shader
//

enum MnShaderSourceLanguage {
  MnShaderSourceLanguageSpirV = 0,
  MnShaderSourceLanguageForce32 = 0x7FFFFFFF,
};

struct MnShaderModuleDesc final {
  MnShaderSourceLanguage source_language = MnShaderSourceLanguage::MnShaderSourceLanguageSpirV;
  uint64_t code_ptr = 0;
  uint32_t code_size_in_bytes = 0;
  // N.B.: See `mnexus::ShaderModuleDesc`.
};

// ----------------------------------------------------------------------------------------------------
// Program
//

struct MnProgramDesc final {
  uint32_t struct_type = 0; // Placeholder for future expansion.
  void* next_ptr = nullptr; // Placeholder for future expansion.
  MnResourceHandle const* shader_modules = nullptr;
  uint32_t shader_module_count = 0;
  // N.B.: See `mnexus::ProgramDesc`.
};

// ----------------------------------------------------------------------------------------------------
// BindGroupLayout

enum MnBindGroupLayoutEntryType {
  MnBindGroupLayoutEntryTypeUniformBuffer          = 0,
  MnBindGroupLayoutEntryTypeStorageBuffer          = 1,
  MnBindGroupLayoutEntryTypeSampledTexture         = 2,
  MnBindGroupLayoutEntryTypeSampler                = 3,
  MnBindGroupLayoutEntryTypeStorageTexture         = 4,
  MnBindGroupLayoutEntryTypeAccelerationStructure  = 5,
  MnBindGroupLayoutEntryTypeCombinedTextureSampler = 6,
  MnBindGroupLayoutEntryTypeForce32                = 0x7FFFFFFF,
  // N.B.: See `mnexus::BindGroupLayoutEntryType`.
};

struct MnBindGroupLayoutEntry final {
  uint32_t binding = 0;
  MnBindGroupLayoutEntryType type = MnBindGroupLayoutEntryTypeUniformBuffer;
  uint32_t count = 1;
  // N.B.: See `mnexus::BindGroupLayoutEntry`.
};

// ----------------------------------------------------------------------------------------------------
// BindingId

struct MnBindingId final {
  uint32_t group = 0;
  uint32_t binding = 0;
  uint32_t array_element = 0;
  // N.B.: See `mnexus::BindingId`.
};

// ----------------------------------------------------------------------------------------------------
// ComputePipeline
//

struct MnComputePipelineDesc final {
  MnResourceHandle shader_module = MnInvalidResourceHandle;
  // N.B.: Placeholder for future expansion.
  // N.B.: See `mnexus::ComputePipelineDesc`.
};

// ----------------------------------------------------------------------------------------------------
// RenderPipeline
//

struct MnRenderPipelineDesc final {
  uint32_t dummy = 0;
  // N.B.: Placeholder for future expansion.
  // N.B.: See `mnexus::RenderPipelineDesc`.
};

#if defined(__cplusplus)

namespace mnexus {

// ----------------------------------------------------------------------------------------------------
// Common Macros
//

#define _MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(cxx_type, c_type)                                 \
  static_assert(                                                                                \
    sizeof(cxx_type) == sizeof(c_type) && alignof(cxx_type) == alignof(c_type),                 \
    "ABI mismatch between " #cxx_type " and " #c_type                                           \
  );

struct Extent3d final {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(Extent3d, MnExtent3d);

struct Offset3d final {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t z = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(Offset3d, MnOffset3d);

// ----------------------------------------------------------------------------------------------------
// Surface
//

struct SurfaceSourceDesc final {
  /// - Windows: HINSTANCE
  /// - Android: JNIEnv*
  uint64_t instance_handle = 0;
  /// - Linux: X11 Display*
  /// - Android: GameActivity*
  uint64_t display_handle = 0;
  /// - Windows: HWND
  /// - Linux: X11 Window
  /// - Android: ANativeWindow*
  uint64_t window_handle = 0;

  /// - Web: Pointer to HTML canvas selector string
  char const* canvas_selector = nullptr;
  /// - Web: Length of HTML canvas selector string
  uint32_t canvas_selector_length = 0;
};

// ----------------------------------------------------------------------------------------------------
// Capability

struct DeviceCapability final {
  bool vertex_shader_storage_write = false;
  // N.B.: See `MnDeviceCapability`.
};

// ----------------------------------------------------------------------------------------------------
// Handles
//

#define _MNEXUS_DEFINE_TYPESAFE_HANDLE(name)                                                          \
  using name = mbase::TypesafeHandle<struct name##Tag, MnResourceHandle, MnInvalidResourceHandle>;    \
  static_assert(sizeof(name) == sizeof(MnResourceHandle) && alignof(name) == alignof(MnResourceHandle));

_MNEXUS_DEFINE_TYPESAFE_HANDLE(BufferHandle);
_MNEXUS_DEFINE_TYPESAFE_HANDLE(TextureHandle);
_MNEXUS_DEFINE_TYPESAFE_HANDLE(ShaderModuleHandle);
_MNEXUS_DEFINE_TYPESAFE_HANDLE(ProgramHandle);
_MNEXUS_DEFINE_TYPESAFE_HANDLE(ComputePipelineHandle);
_MNEXUS_DEFINE_TYPESAFE_HANDLE(RenderPipelineHandle);

// ----------------------------------------------------------------------------------------------------
// Queue
//

enum class QueueFamilyCapabilityFlagBits : uint32_t {
  kNone     = MnQueueFamilyCapabilityFlagBitNone,
  kGraphics = MnQueueFamilyCapabilityFlagBitGraphics,
  kCompute  = MnQueueFamilyCapabilityFlagBitCompute,
  kTransfer = MnQueueFamilyCapabilityFlagBitTransfer,
  kVideoDecode = MnQueueFamilyCapabilityFlagBitVideoDecode,
  kVideoEncode = MnQueueFamilyCapabilityFlagBitVideoEncode,
};
MBASE_DEFINE_ENUM_CLASS_BITFLAGS_OPERATORS(QueueFamilyCapabilityFlagBits);
using QueueFamilyCapabilityFlags = mbase::BitFlags<QueueFamilyCapabilityFlagBits>;

struct QueueFamilyDesc final {
  uint32_t queue_count = 0;
  QueueFamilyCapabilityFlags capabilities = QueueFamilyCapabilityFlagBits::kNone;
};

struct QueueId final {
  uint32_t queue_family_index = 0;
  uint32_t queue_index = 0;

  QueueId() = default;
  explicit QueueId(uint32_t family_index, uint32_t index) :
    queue_family_index(family_index),
    queue_index(index)
  {}
  ~QueueId() = default;
  MBASE_DEFAULT_COPY_MOVE(QueueId);

  explicit QueueId(MnQueueId const& c_id) :
    queue_family_index(c_id.queue_family_index),
    queue_index(c_id.queue_index) {
  }

  static bool InSameQueueFamily(std::optional<QueueId> const& lhs, std::optional<QueueId> const& rhs);

  static bool InDifferentQueueFamily(std::optional<QueueId> const& needle, std::span<std::optional<QueueId> const> haystack);

  friend bool operator==(QueueId const& lhs, QueueId const& rhs);
  friend bool operator!=(QueueId const& lhs, QueueId const& rhs) { return !operator==(lhs, rhs); }
};
static_assert(sizeof(QueueId) == sizeof(MnQueueId) && alignof(QueueId) == alignof(MnQueueId));

/// Uniquely identifies a submission within a queue.
/// Nothing is guarenteed about the value of this value, except:
/// - It it an integer that monotonically increases with each submission.
/// - It is unique within the queue from which it was obtained.
/// - A valid value is non-zero.
using IntraQueueSubmissionId = mbase::TypesafeHandle<struct MnIntraQueueSubmissionIdTag, uint64_t, 0>;
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(IntraQueueSubmissionId, MnIntraQueueSubmissionId);

// ----------------------------------------------------------------------------------------------------
// Command List
//

struct CommandListDesc final {
  uint32_t queue_family_index = 0;
};

// ----------------------------------------------------------------------------------------------------
// Buffer
//

enum class BufferUsageFlagBits : uint32_t {
  kVertex        = MnBufferUsageFlagBitVertex,
  kIndex         = MnBufferUsageFlagBitIndex,
  kUniform       = MnBufferUsageFlagBitUniform,
  kStorage       = MnBufferUsageFlagBitStorage,
  kTransferSrc   = MnBufferUsageFlagBitTransferSrc,
  kTransferDst   = MnBufferUsageFlagBitTransferDst,
  kIndirect      = MnBufferUsageFlagBitIndirect,
};
MBASE_DEFINE_ENUM_CLASS_BITFLAGS_OPERATORS(BufferUsageFlagBits);
using BufferUsageFlags = mbase::BitFlags<BufferUsageFlagBits>;

struct BufferDesc final {
  BufferUsageFlags usage;
  uint32_t size_in_bytes = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(BufferDesc, MnBufferDesc);

// ----------------------------------------------------------------------------------------------------
// Texture
//

enum class TextureUsageFlagBits : uint32_t {
  kAttachment       = MnTextureUsageFlagBitAttachment,
  kTileLocal        = MnTextureUsageFlagBitTileLocal,
  kSampled          = MnTextureUsageFlagBitSampled,
  kUnorderedAccess  = MnTextureUsageFlagBitUnorderedAccess,
  kTransferSrc      = MnTextureUsageFlagBitTransferSrc,
  kTransferDst      = MnTextureUsageFlagBitTransferDst,
};
MBASE_DEFINE_ENUM_CLASS_BITFLAGS_OPERATORS(TextureUsageFlagBits);
using TextureUsageFlags = mbase::BitFlags<TextureUsageFlagBits>;

enum class TextureAspectFlagBits : uint32_t {
  kColor    = MnTextureAspectFlagBitColor,
  kDepth    = MnTextureAspectFlagBitDepth,
  kStencil  = MnTextureAspectFlagBitStencil,
  kMetadata = MnTextureAspectFlagBitMetadata,
  kPlane0   = MnTextureAspectFlagBitPlane0,
  kPlane1   = MnTextureAspectFlagBitPlane1,
  kPlane2   = MnTextureAspectFlagBitPlane2,
};
MBASE_DEFINE_ENUM_CLASS_BITFLAGS_OPERATORS(TextureAspectFlagBits);
using TextureAspectFlags = mbase::BitFlags<TextureAspectFlagBits>;

enum class TextureDimension : uint32_t {
  k1D    = MnTextureDimension1D,
  k2D    = MnTextureDimension2D,
  k3D    = MnTextureDimension3D,
  kCube  = MnTextureDimensionCube,
};

enum class Filter : uint32_t {
  kNearest = MnFilterNearest,
  kLinear  = MnFilterLinear,
};

struct TextureSubresourceRange final {
  TextureAspectFlags aspect_mask = TextureAspectFlagBits::kColor;
  uint32_t base_mip_level = 0;
  uint32_t mip_level_count = 1;
  uint32_t base_array_layer = 0;
  uint32_t array_layer_count = 1;

  [[nodiscard]] static TextureSubresourceRange SingleSubresourceColor(uint32_t base_mip_level, uint32_t base_array_layer);
  [[nodiscard]] static TextureSubresourceRange SingleSubresourceDepth(uint32_t base_mip_level, uint32_t base_array_layer);
  [[nodiscard]] static TextureSubresourceRange SingleSubresource(TextureAspectFlags aspect_mask, uint32_t base_mip_level, uint32_t base_array_layer);
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(TextureSubresourceRange, MnTextureSubresourceRange);

struct TextureDesc final {
  TextureUsageFlags usage;
  MnFormat format = MnFormat::kUndefined;
  TextureDimension dimension = TextureDimension::k2D;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 1;
  uint32_t mip_level_count = 1;
  uint32_t array_layer_count = 1;
};

// ----------------------------------------------------------------------------------------------------
// Shader
//

enum class ShaderSourceLanguage : uint32_t {
  kSpirV = MnShaderSourceLanguage::MnShaderSourceLanguageSpirV,
};

struct ShaderModuleDesc final {
  ShaderSourceLanguage source_language = ShaderSourceLanguage::kSpirV;
  uint64_t code_ptr = 0;
  uint32_t code_size_in_bytes = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(ShaderModuleDesc, MnShaderModuleDesc);

// ----------------------------------------------------------------------------------------------------
// Program
//

struct ProgramDesc final {
  uint32_t struct_type = 0; // Placeholder for future expansion.
  void* next_ptr = nullptr; // Placeholder for future expansion.
  container::ArrayProxy<ShaderModuleHandle const> shader_modules;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(ProgramDesc, MnProgramDesc);

// ----------------------------------------------------------------------------------------------------
// BindGroupLayout
//

enum class BindGroupLayoutEntryType : uint32_t {
  kUniformBuffer          = MnBindGroupLayoutEntryType::MnBindGroupLayoutEntryTypeUniformBuffer,
  kStorageBuffer          = MnBindGroupLayoutEntryType::MnBindGroupLayoutEntryTypeStorageBuffer,
  kSampledTexture         = MnBindGroupLayoutEntryType::MnBindGroupLayoutEntryTypeSampledTexture,
  kSampler                = MnBindGroupLayoutEntryType::MnBindGroupLayoutEntryTypeSampler,
  kStorageTexture         = MnBindGroupLayoutEntryType::MnBindGroupLayoutEntryTypeStorageTexture,
  kAccelerationStructure  = MnBindGroupLayoutEntryType::MnBindGroupLayoutEntryTypeAccelerationStructure,
  kCombinedTextureSampler = MnBindGroupLayoutEntryType::MnBindGroupLayoutEntryTypeCombinedTextureSampler,
};

struct BindGroupLayoutEntry final {
  uint32_t binding = 0;
  BindGroupLayoutEntryType type = BindGroupLayoutEntryType::kUniformBuffer;
  uint32_t count = 1;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(BindGroupLayoutEntry, MnBindGroupLayoutEntry);

// ----------------------------------------------------------------------------------------------------
// BindingId
//

struct BindingId final {
  uint32_t group = 0;
  uint32_t binding = 0;
  uint32_t array_element = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(BindingId, MnBindingId);

// ----------------------------------------------------------------------------------------------------
// ComputePipeline
//

struct ComputePipelineDesc final {
  ShaderModuleHandle shader_module;
  // N.B.: Placeholder for future expansion.
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(ComputePipelineDesc, MnComputePipelineDesc);

// ----------------------------------------------------------------------------------------------------
// RenderPipeline
//

struct RenderPipelineDesc final {
  uint32_t dummy = 0;
  // N.B.: Placeholder for future expansion.
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(RenderPipelineDesc, MnRenderPipelineDesc);

// ----------------------------------------------------------------------------------------------------
// Misc
//

struct ClearValue final {
  union {
    struct {
      float r;
      float g;
      float b;
      float a;
    } color;
    struct {
      float depth;
      uint32_t stencil;
    } depth_stencil;
  };
};

// ----------------------------------------------------------------------------------------------------
// Utilities
//

//
// Functions returning `std::string_view` return pointers to static strings, null-terminated.
//

std::string_view ToString(MnFormat value);
std::string_view ToString(BindGroupLayoutEntryType value);
std::string ToString(TextureUsageFlags value);

inline uint32_t GetFormatSizeInBytes(MnFormat value) { return MnGetFormatSizeInBytes(value); }
inline Extent3d GetFormatTexelBlockExtent(MnFormat value) {
  MnExtent3d e = MnGetFormatTexelBlockExtent(value);
  return Extent3d { e.width, e.height, e.depth };
}

} // namespace mnexus

#else // defined(__cplusplus)

#endif // defined(__cplusplus)
