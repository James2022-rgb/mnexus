#pragma once

// c++ headers ------------------------------------------
#if defined(__cplusplus)
# include <cstdint>

# include <string_view>
# include <string>
# include <optional>
# include <span>
#else
# include <stdint.h>
#endif

// public project headers -------------------------------
#if defined(__cplusplus)
# include "mbase/public/access.h"
# include "mbase/public/bitflags.h"
# include "mbase/public/type_safety.h"

# include "mnexus/public/container/array_proxy.h"
#endif

// ----------------------------------------------------------------------------------------------------
// C/C++ portability macros
//

#if defined(__cplusplus)
# define _MN_FINAL final
# define _MN_INIT(x) = x
# define _MN_NULL nullptr
#else
# define _MN_FINAL
# define _MN_INIT(x)
# define _MN_NULL 0
#endif

typedef uint32_t MnBool32;
static const MnBool32 MnBoolFalse = 0;
static const MnBool32 MnBoolTrue  = 1;

// ----------------------------------------------------------------------------------------------------
// Backend Type

typedef uint8_t MnBackendType;
enum {
  MnBackendTypeWebGpu = 0,
  MnBackendTypeVulkan,
};

// ----------------------------------------------------------------------------------------------------
// Clip Space Convention

typedef uint8_t MnClipSpaceYDirection;
enum {
  MnClipSpaceYDirectionUp = 0,   // WebGPU, OpenGL
  MnClipSpaceYDirectionDown,     // Vulkan
};

typedef uint8_t MnClipSpaceDepthRange;
enum {
  MnClipSpaceDepthRangeZeroToOne = 0,    // Vulkan, WebGPU
  MnClipSpaceDepthRangeNegOneToOne,      // OpenGL
};

typedef struct MnClipSpaceConvention {
  MnClipSpaceYDirection y_direction _MN_INIT(MnClipSpaceYDirectionUp);
  MnClipSpaceDepthRange depth_range _MN_INIT(MnClipSpaceDepthRangeZeroToOne);
} MnClipSpaceConvention;

// ----------------------------------------------------------------------------------------------------
// Color Space

typedef enum MnColorSpace {
  MnColorSpaceLinear = 0,
  MnColorSpaceSrgb,
  MnColorSpaceHdr10St2084,
} MnColorSpace;

// ----------------------------------------------------------------------------------------------------
// Nexus

typedef struct MnNexusDesc {
  MnBool32 headless;
  MnBackendType backend_type _MN_INIT(MnBackendTypeWebGpu);
  char const* app_name _MN_INIT(NULL);
} MnNexusDesc;

// ----------------------------------------------------------------------------------------------------
// Adapter Info

typedef struct MnAdapterInfo {
  char device_name[256];
  char vendor[256];
  char architecture[256];
  char description[256];
  uint32_t vendor_id;
  uint32_t device_id;
} MnAdapterInfo;

// ----------------------------------------------------------------------------------------------------
// Geometry

typedef struct MnExtent2d _MN_FINAL {
  uint32_t width _MN_INIT(0);
  uint32_t height _MN_INIT(0);
} MnExtent2d;

typedef struct MnExtent3d _MN_FINAL {
  uint32_t width _MN_INIT(0);
  uint32_t height _MN_INIT(0);
  uint32_t depth _MN_INIT(0);
} MnExtent3d;

typedef struct MnOffset3d _MN_FINAL {
  uint32_t x _MN_INIT(0);
  uint32_t y _MN_INIT(0);
  uint32_t z _MN_INIT(0);
} MnOffset3d;

// ----------------------------------------------------------------------------------------------------
// Capability

typedef struct MnAdapterCapability _MN_FINAL {
  MnBool32 vertex_shader_storage_write _MN_INIT(MnBoolFalse);
  MnBool32 polygon_mode_line _MN_INIT(MnBoolFalse);
  MnBool32 polygon_mode_point _MN_INIT(MnBoolFalse);
  MnBool32 buffer_mappable _MN_INIT(MnBoolFalse);
  // N.B.: See `mnexus::AdapterCapability`.
} MnAdapterCapability;

// ----------------------------------------------------------------------------------------------------
// Handles
//

typedef uint64_t MnResourceHandle;

static const MnResourceHandle MnInvalidResourceHandle = 0x00000000FFFFFFFF;

// ----------------------------------------------------------------------------------------------------
// Queue
//

typedef struct MnQueueId _MN_FINAL {
  uint32_t queue_family_index _MN_INIT(0);
  uint32_t queue_index _MN_INIT(0);
  // N.B.: See `mnexus::QueueId`.
} MnQueueId;

typedef uint64_t MnIntraQueueSubmissionId;

typedef enum MnQueueFamilyCapabilityFlagBits {
  MnQueueFamilyCapabilityFlagBitNone        = 0,
  MnQueueFamilyCapabilityFlagBitGraphics    = 1 << 0,
  MnQueueFamilyCapabilityFlagBitCompute     = 1 << 1,
  MnQueueFamilyCapabilityFlagBitTransfer    = 1 << 2,
  MnQueueFamilyCapabilityFlagBitVideoDecode = 1 << 3,
  MnQueueFamilyCapabilityFlagBitVideoEncode = 1 << 4,
  MnQueueFamilyCapabilityFlagForce32     = 0x7FFFFFFF,
} MnQueueFamilyCapabilityFlagBits;
typedef uint32_t MnQueueFamilyCapabilityFlags;

typedef struct MnQueueSelection _MN_FINAL {
  MnQueueId present_capable;
  MnBool32  has_dedicated_compute _MN_INIT(MnBoolFalse);
  MnQueueId dedicated_compute;
  MnBool32  has_dedicated_transfer _MN_INIT(MnBoolFalse);
  MnQueueId dedicated_transfer;
  MnBool32  has_dedicated_video_decode _MN_INIT(MnBoolFalse);
  MnQueueId dedicated_video_decode;
  MnBool32  has_dedicated_video_encode _MN_INIT(MnBoolFalse);
  MnQueueId dedicated_video_encode;
} MnQueueSelection;

// ----------------------------------------------------------------------------------------------------
// Command List
//

typedef struct MnCommandListDesc _MN_FINAL {
  uint32_t queue_family_index _MN_INIT(0);
} MnCommandListDesc;

// ----------------------------------------------------------------------------------------------------
// Buffer
//

typedef enum MnBufferUsageFlagBits {
  MnBufferUsageFlagBitNone            = 0,
  MnBufferUsageFlagBitVertex          = 1 << 0,
  MnBufferUsageFlagBitIndex           = 1 << 1,
  MnBufferUsageFlagBitUniform         = 1 << 2,
  MnBufferUsageFlagBitStorage         = 1 << 3,
  MnBufferUsageFlagBitTransferSrc     = 1 << 4,
  MnBufferUsageFlagBitTransferDst     = 1 << 5,
  MnBufferUsageFlagBitIndirect        = 1 << 6,
  MnBufferUsageFlagBitMappable        = 1 << 7,
  /// Vulkan Video bitstream source (e.g. AnnexB H.264/H.265 stream fed to
  /// vkCmdDecodeVideoKHR). Requires the Vulkan Video extensions to have
  /// been enabled on the device; rejected by backends without video support
  /// (e.g. WebGPU).
  MnBufferUsageFlagBitVideoDecodeSrc  = 1 << 8,
  /// Vulkan Video encoded bitstream destination. Same prerequisites as
  /// `MnBufferUsageFlagBitVideoDecodeSrc`.
  MnBufferUsageFlagBitVideoEncodeDst  = 1 << 9,
  MnBufferUsageFlagForce32            = 0x7FFFFFFF,
} MnBufferUsageFlagBits;
typedef uint32_t MnBufferUsageFlags;

typedef struct MnBufferDesc _MN_FINAL {
  MnBufferUsageFlags usage;
  uint32_t size_in_bytes _MN_INIT(0);
  // N.B.: See `mnexus::BufferDesc`.
} MnBufferDesc;

// ----------------------------------------------------------------------------------------------------
// Texture
//

typedef enum MnTextureUsageFlagBits {
  MnTextureUsageFlagBitNone            = 0,
  MnTextureUsageFlagBitAttachment      = 1 << 0,
  MnTextureUsageFlagBitTileLocal       = 1 << 1,
  MnTextureUsageFlagBitSampled         = 1 << 2,
  MnTextureUsageFlagBitUnorderedAccess = 1 << 3,
  MnTextureUsageFlagBitTransferSrc     = 1 << 4,
  MnTextureUsageFlagBitTransferDst     = 1 << 5,
  /// Vulkan Video decode output (reconstructed picture target for
  /// `vkCmdDecodeVideoKHR`). Requires the Vulkan Video extensions to have
  /// been enabled on the device; rejected on backends without video support.
  MnTextureUsageFlagBitVideoDecodeDst  = 1 << 6,
  /// Vulkan Video DPB (reference picture buffer). Same prerequisites as
  /// `MnTextureUsageFlagBitVideoDecodeDst`. For DPB+output coincide
  /// configurations, set both bits on the same texture.
  MnTextureUsageFlagBitVideoDecodeDpb  = 1 << 7,
  MnTextureUsageFlagForce32            = 0x7FFFFFFF,
} MnTextureUsageFlagBits;
typedef uint32_t MnTextureUsageFlags;

typedef enum MnTextureAspectFlagBits {
  MnTextureAspectFlagBitColor    = 1 << 0,
  MnTextureAspectFlagBitDepth    = 1 << 1,
  MnTextureAspectFlagBitStencil  = 1 << 2,
  MnTextureAspectFlagBitMetadata = 1 << 3,
  MnTextureAspectFlagBitPlane0   = 1 << 4,
  MnTextureAspectFlagBitPlane1   = 1 << 5,
  MnTextureAspectFlagBitPlane2   = 1 << 6,
  MnTextureAspectFlagForce32     = 0x7FFFFFFF,
} MnTextureAspectFlagBits;

typedef enum MnTextureDimension {
  MnTextureDimension1D = 0,
  MnTextureDimension2D = 1,
  MnTextureDimension3D = 2,
  MnTextureDimensionCube = 3,
  /// 2D array. Same image-type as `2D` on Vulkan, but distinguished here so the
  /// view-type and user intent are explicit (e.g. Vulkan Video DPB array).
  MnTextureDimension2DArray = 4,
  MnTextureDimensionForce32 = 0x7FFFFFFF,
} MnTextureDimension;

typedef enum MnFilter {
  MnFilterNearest = 0,
  MnFilterLinear  = 1,
  MnFilterForce32 = 0x7FFFFFFF,
} MnFilter;

typedef enum MnAddressMode {
  MnAddressModeRepeat       = 0,
  MnAddressModeMirrorRepeat = 1,
  MnAddressModeClampToEdge  = 2,
  MnAddressModeForce32      = 0x7FFFFFFF,
} MnAddressMode;

typedef struct MnSamplerDesc _MN_FINAL {
  MnFilter min_filter _MN_INIT(MnFilterNearest);
  MnFilter mag_filter _MN_INIT(MnFilterNearest);
  MnFilter mipmap_filter _MN_INIT(MnFilterNearest);
  MnAddressMode address_mode_u _MN_INIT(MnAddressModeClampToEdge);
  MnAddressMode address_mode_v _MN_INIT(MnAddressModeClampToEdge);
  MnAddressMode address_mode_w _MN_INIT(MnAddressModeClampToEdge);
  // N.B.: See `mnexus::SamplerDesc`.
} MnSamplerDesc;

typedef struct MnTextureSubresourceRange _MN_FINAL {
  MnTextureAspectFlagBits aspect_mask _MN_INIT(MnTextureAspectFlagBitColor);
  uint32_t base_mip_level _MN_INIT(0);
  uint32_t mip_level_count _MN_INIT(1);
  uint32_t base_array_layer _MN_INIT(0);
  uint32_t array_layer_count _MN_INIT(1);
  // N.B.: See `mnexus::TextureSubresourceRange`.
} MnTextureSubresourceRange;

// ----------------------------------------------------------------------------------------------------
// Resource barrier state / stage
//

typedef uint8_t MnResourceBarrierState;
enum {
  MnResourceBarrierStateIndirectArgument = 0,
  MnResourceBarrierStateIndexBuffer,
  MnResourceBarrierStateVertexBuffer,
  MnResourceBarrierStateUniformBuffer,
  MnResourceBarrierStateReadOnly,
  MnResourceBarrierStateAttachment,
  MnResourceBarrierStateUnorderedAccess,
  MnResourceBarrierStateTransferSrc,
  MnResourceBarrierStateTransferDst,
  /// VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR (decode reconstructed picture).
  MnResourceBarrierStateVideoDecodeDst,
  /// VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR (DPB read/write). Can also be used
  /// for the DPB-and-output-coincide configuration since the layout supports
  /// both reading reference pictures and writing the reconstructed picture.
  MnResourceBarrierStateVideoDecodeDpb,
  /// VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR (decode bitstream source image).
  MnResourceBarrierStateVideoDecodeSrc,
};

typedef enum MnResourceBarrierStageFlagBits {
  MnResourceBarrierStageFlagBitDrawIndirectInput     = 1 << 0,
  MnResourceBarrierStageFlagBitVertexInput           = 1 << 1,
  MnResourceBarrierStageFlagBitVertexShader          = 1 << 2,
  MnResourceBarrierStageFlagBitEarlyFragmentTests    = 1 << 3,
  MnResourceBarrierStageFlagBitFragmentShader        = 1 << 4,
  MnResourceBarrierStageFlagBitLateFragmentTests     = 1 << 5,
  MnResourceBarrierStageFlagBitColorAttachmentOutput = 1 << 6,

  MnResourceBarrierStageFlagBitComputeIndirectInput  = 1 << 7,
  MnResourceBarrierStageFlagBitComputeShader         = 1 << 8,

  MnResourceBarrierStageFlagBitTransfer              = 1 << 9,

  /// VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR. Used by both the source
  /// (release) and destination (acquire) sides of barriers around
  /// `vkCmdDecodeVideoKHR`.
  MnResourceBarrierStageFlagBitVideoDecode           = 1 << 10,

  MnResourceBarrierStageFlagBitForce32 = 0x7FFFFFFF,
} MnResourceBarrierStageFlagBits;
typedef uint32_t MnResourceBarrierStageFlags;

// ----------------------------------------------------------------------------------------------------
// Format
//

typedef enum MnFormat {
  MnFormatUndefined = 0,

  MnFormatR5G6B5_UNORM_PACK16,
  MnFormatR5G5B5A1_UNORM_PACK16,

  MnFormatR8_UNORM,
  MnFormatR8G8_UNORM,
  MnFormatR8G8B8_UNORM,
  MnFormatR8G8B8A8_UNORM,
  MnFormatR8G8B8A8_SRGB,
  MnFormatB8G8R8A8_UNORM,
  MnFormatB8G8R8A8_SRGB,

  MnFormatR16_SFLOAT,
  MnFormatR16G16_SFLOAT,
  MnFormatR16G16B16_SFLOAT,
  MnFormatR16G16B16A16_UNORM,
  MnFormatR16G16B16A16_UINT,
  MnFormatR16G16B16A16_SFLOAT,

  MnFormatR32_SFLOAT,
  MnFormatR32G32_SFLOAT,
  MnFormatR32G32B32_SFLOAT,
  MnFormatR32G32B32A32_UINT,
  MnFormatR32G32B32A32_SFLOAT,

  MnFormatA2R10G10B10_UNORM_PACK32,
  MnFormatA2R10G10B10_SNORM_PACK32,
  MnFormatA2R10G10B10_USCALED_PACK32,
  MnFormatA2R10G10B10_SSCALED_PACK32,
  MnFormatA2R10G10B10_UINT_PACK32,
  MnFormatA2R10G10B10_SINT_PACK32,
  MnFormatA2B10G10R10_UNORM_PACK32,
  MnFormatA2B10G10R10_SNORM_PACK32,
  MnFormatA2B10G10R10_USCALED_PACK32,
  MnFormatA2B10G10R10_SSCALED_PACK32,
  MnFormatA2B10G10R10_UINT_PACK32,
  MnFormatA2B10G10R10_SINT_PACK32,

  MnFormatD16_UNORM,
  MnFormatD32_SFLOAT,
  MnFormatD16_UNORM_S8_UINT,
  MnFormatD24_UNORM_S8_UINT,
  MnFormatD32_SFLOAT_S8_UINT,

  // BC1 (DXT1) formats.
  MnFormatBC1_RGB_UNORM_BLOCK,
  MnFormatBC1_RGB_SRGB_BLOCK,
  MnFormatBC1_RGBA_UNORM_BLOCK,
  MnFormatBC1_RGBA_SRGB_BLOCK,
  // BC2 (DXT2/3) formats.
  MnFormatBC2_UNORM_BLOCK,
  MnFormatBC2_SRGB_BLOCK,
  // BC3 (DXT4/5) formats.
  MnFormatBC3_UNORM_BLOCK,
  MnFormatBC3_SRGB_BLOCK,
  // BC4 (ATI1) formats.
  MnFormatBC4_UNORM_BLOCK,
  MnFormatBC4_SNORM_BLOCK,
  // BC5 (ATI2) formats.
  MnFormatBC5_UNORM_BLOCK,
  MnFormatBC5_SNORM_BLOCK,

  MnFormatETC2_R8G8B8_UNORM_BLOCK,
  MnFormatETC2_R8G8B8_SRGB_BLOCK,
  MnFormatETC2_R8G8B8A1_UNORM_BLOCK,
  MnFormatETC2_R8G8B8A1_SRGB_BLOCK,
  MnFormatETC2_R8G8B8A8_UNORM_BLOCK,
  MnFormatETC2_R8G8B8A8_SRGB_BLOCK,
  MnFormatEAC_R11_UNORM_BLOCK,
  MnFormatEAC_R11_SNORM_BLOCK,
  MnFormatEAC_R11G11_UNORM_BLOCK,
  MnFormatEAC_R11G11_SNORM_BLOCK,

  MnFormatASTC_4x4_UNORM_BLOCK,
  MnFormatASTC_4x4_SRGB_BLOCK,
  MnFormatASTC_5x4_UNORM_BLOCK,
  MnFormatASTC_5x4_SRGB_BLOCK,
  MnFormatASTC_5x5_UNORM_BLOCK,
  MnFormatASTC_5x5_SRGB_BLOCK,
  MnFormatASTC_6x5_UNORM_BLOCK,
  MnFormatASTC_6x5_SRGB_BLOCK,
  MnFormatASTC_6x6_UNORM_BLOCK,
  MnFormatASTC_6x6_SRGB_BLOCK,
  MnFormatASTC_8x5_UNORM_BLOCK,
  MnFormatASTC_8x5_SRGB_BLOCK,
  MnFormatASTC_8x6_UNORM_BLOCK,
  MnFormatASTC_8x6_SRGB_BLOCK,
  MnFormatASTC_8x8_UNORM_BLOCK,
  MnFormatASTC_8x8_SRGB_BLOCK,
  MnFormatASTC_10x5_UNORM_BLOCK,
  MnFormatASTC_10x5_SRGB_BLOCK,
  MnFormatASTC_10x6_UNORM_BLOCK,
  MnFormatASTC_10x6_SRGB_BLOCK,
  MnFormatASTC_10x8_UNORM_BLOCK,
  MnFormatASTC_10x8_SRGB_BLOCK,
  MnFormatASTC_10x10_UNORM_BLOCK,
  MnFormatASTC_10x10_SRGB_BLOCK,
  MnFormatASTC_12x10_UNORM_BLOCK,
  MnFormatASTC_12x10_SRGB_BLOCK,
  MnFormatASTC_12x12_UNORM_BLOCK,
  MnFormatASTC_12x12_SRGB_BLOCK,

  // Multi-planar YCbCr 4:2:0 formats (mainly for Vulkan Video decode output).
  // Plane 0: luma; Plane 1: interleaved CbCr at half resolution in X and Y.
  MnFormatG8_B8R8_2PLANE_420_UNORM,                  // 8-bit (NV12)
  MnFormatG10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, // 10-bit in 16-bit container (P010-like)

  MnFormatForce32 = 0x7FFFFFFF,
} MnFormat;

// ----------------------------------------------------------------------------------------------------
// Texture Descriptor
//

typedef struct MnTextureDesc _MN_FINAL {
  MnTextureUsageFlags usage;
  MnFormat format _MN_INIT(MnFormatUndefined);
  MnTextureDimension dimension _MN_INIT(MnTextureDimension2D);
  uint32_t width _MN_INIT(0);
  uint32_t height _MN_INIT(0);
  uint32_t depth _MN_INIT(1);
  uint32_t mip_level_count _MN_INIT(1);
  uint32_t array_layer_count _MN_INIT(1);
} MnTextureDesc;

/// Returns the size in bytes of a single texel (or compressed block) for the given format.
/// Returns 0 for `MnFormatUndefined`.
#if defined(__cplusplus)
uint32_t MnGetFormatSizeInBytes(MnFormat value);
#endif

/// Returns the texel block extent for the given format.
/// For uncompressed formats, returns {1, 1, 1}.
/// For block-compressed formats (BC, ETC2, ASTC), returns the block dimensions.
#if defined(__cplusplus)
MnExtent3d MnGetFormatTexelBlockExtent(MnFormat value);
#endif

// ----------------------------------------------------------------------------------------------------
// Video Coding
//
// All types are declared regardless of `MNEXUS_ENABLE_VIDEO_CODING`. Implementation
// of the runtime APIs (`MnDeviceQueryVideoDecodeH265Capabilities` etc.) returns
// failure when video coding was disabled at mnexus build time.
//

typedef enum MnVideoH265Profile {
  MnVideoH265ProfileMain    = 0,
  MnVideoH265ProfileMain10  = 1,
  MnVideoH265ProfileForce32 = 0x7FFFFFFF,
} MnVideoH265Profile;

typedef enum MnVideoBitDepth {
  MnVideoBitDepth8       = 0,
  MnVideoBitDepth10      = 1,
  MnVideoBitDepthForce32 = 0x7FFFFFFF,
} MnVideoBitDepth;

/// H.265 conformance level. Independent of both the H.265 spec's
/// `general_level_idc` (uint8 = level x 30, e.g. 5.1 -> 153) and Vulkan's
/// `StdVideoH265LevelIdc` (sequential ordinal). Convert via
/// `MnVideoH265LevelToSpecIdc` / `MnVideoH265LevelFromSpecIdc`.
typedef enum MnVideoH265Level {
  MnVideoH265Level1_0     = 0,
  MnVideoH265Level2_0     = 1,
  MnVideoH265Level2_1     = 2,
  MnVideoH265Level3_0     = 3,
  MnVideoH265Level3_1     = 4,
  MnVideoH265Level4_0     = 5,
  MnVideoH265Level4_1     = 6,
  MnVideoH265Level5_0     = 7,
  MnVideoH265Level5_1     = 8,
  MnVideoH265Level5_2     = 9,
  MnVideoH265Level6_0     = 10,
  MnVideoH265Level6_1     = 11,
  MnVideoH265Level6_2     = 12,
  MnVideoH265LevelForce32 = 0x7FFFFFFF,
} MnVideoH265Level;

/// Coding-shared (operation-agnostic) capabilities. Mirrors VkVideoCapabilitiesKHR
/// minus the Vulkan-only fields (sType / pNext / stdHeaderVersion).
typedef struct MnVideoCommonCapabilities _MN_FINAL {
  MnExtent2d picture_access_granularity;
  MnExtent2d min_coded_extent;
  MnExtent2d max_coded_extent;
  uint64_t   min_bitstream_buffer_offset_alignment _MN_INIT(0);
  uint64_t   min_bitstream_buffer_size_alignment _MN_INIT(0);
  uint32_t   max_dpb_slots _MN_INIT(0);
  uint32_t   max_active_reference_pictures _MN_INIT(0);

  // From VkVideoCapabilityFlagsKHR.
  MnBool32   protected_content _MN_INIT(MnBoolFalse);
  MnBool32   separate_reference_images _MN_INIT(MnBoolFalse);
} MnVideoCommonCapabilities;

/// Decode-shared capabilities. Mirrors VkVideoDecodeCapabilitiesKHR::flags.
typedef struct MnVideoDecodeCommonCapabilities _MN_FINAL {
  MnBool32 dpb_and_output_coincide _MN_INIT(MnBoolFalse);
  MnBool32 dpb_and_output_distinct _MN_INIT(MnBoolFalse);
} MnVideoDecodeCommonCapabilities;

/// H.265 decode capabilities. Composition of (coding common + decode common +
/// codec-specific).
typedef struct MnVideoDecodeH265Capabilities _MN_FINAL {
  MnVideoCommonCapabilities       common;
  MnVideoDecodeCommonCapabilities decode_common;
  MnVideoH265Level                max_level _MN_INIT(MnVideoH265Level1_0);
  /// e.g. `MnFormatG8_B8R8_2PLANE_420_UNORM` for 8-bit, P010-equivalent for 10-bit.
  MnFormat                        picture_format _MN_INIT(MnFormatUndefined);
} MnVideoDecodeH265Capabilities;

/// Convert an `MnVideoH265Level` to the H.265 spec's `general_level_idc`
/// (NAL unit raw value, = level x 30; e.g. `MnVideoH265Level5_1` -> 153).
#if defined(__cplusplus)
uint8_t MnVideoH265LevelToSpecIdc(MnVideoH265Level level);

/// Inverse of `MnVideoH265LevelToSpecIdc`. Returns `MnBoolTrue` and writes
/// `*out_level` on success; returns `MnBoolFalse` (and leaves `*out_level`
/// untouched) for unrecognized spec idc values.
MnBool32 MnVideoH265LevelFromSpecIdc(uint8_t spec_idc, MnVideoH265Level* out_level);
#endif

/// Descriptor for H.265 decode `VideoSession` creation. Mirrors the operation-
/// specific fields of `VkVideoSessionCreateInfoKHR`.
typedef struct MnVideoSessionDecodeH265Desc _MN_FINAL {
  MnVideoH265Profile profile _MN_INIT(MnVideoH265ProfileMain);
  MnVideoBitDepth    bit_depth _MN_INIT(MnVideoBitDepth8);
  MnFormat           picture_format _MN_INIT(MnFormatUndefined);
  MnFormat           reference_picture_format _MN_INIT(MnFormatUndefined);
  MnExtent2d         max_coded_extent;
  uint32_t           max_dpb_slots _MN_INIT(0);
  uint32_t           max_active_reference_pictures _MN_INIT(0);
} MnVideoSessionDecodeH265Desc;

/// Descriptor for H.265 decode `VideoSessionParameters` creation. Carries
/// raw NAL byte arrays for VPS / SPS / PPS (start code excluded; NAL header
/// included). mnexus parses these internally via vidsynt.
typedef struct MnVideoSessionParametersDecodeH265Desc _MN_FINAL {
  MnResourceHandle session _MN_INIT(MnInvalidResourceHandle);
  uint8_t const* vps_data _MN_INIT(_MN_NULL);
  uint32_t       vps_size _MN_INIT(0);
  uint8_t const* sps_data _MN_INIT(_MN_NULL);
  uint32_t       sps_size _MN_INIT(0);
  uint8_t const* pps_data _MN_INIT(_MN_NULL);
  uint32_t       pps_size _MN_INIT(0);
} MnVideoSessionParametersDecodeH265Desc;

// ----------------------------------------------------------------------------------------------------
// Shader
//

typedef enum MnShaderSourceLanguage {
  MnShaderSourceLanguageSpirV = 0,
  MnShaderSourceLanguageForce32 = 0x7FFFFFFF,
} MnShaderSourceLanguage;

typedef struct MnShaderModuleDesc _MN_FINAL {
  MnShaderSourceLanguage source_language _MN_INIT(MnShaderSourceLanguageSpirV);
  uint64_t code_ptr _MN_INIT(0);
  uint32_t code_size_in_bytes _MN_INIT(0);
  // N.B.: See `mnexus::ShaderModuleDesc`.
} MnShaderModuleDesc;

// ----------------------------------------------------------------------------------------------------
// Program
//

typedef struct MnProgramDesc _MN_FINAL {
  uint32_t struct_type _MN_INIT(0); // Placeholder for future expansion.
  void* next_ptr _MN_INIT(_MN_NULL); // Placeholder for future expansion.
  // shader_modules: ArrayProxy-compatible layout (count, ptr).
  // N.B.: Compiler inserts implicit padding between count and ptr on 64-bit targets.
  uint32_t shader_module_count _MN_INIT(0);
  MnResourceHandle const* shader_modules _MN_INIT(_MN_NULL);
  // N.B.: See `mnexus::ProgramDesc`.
} MnProgramDesc;

// ----------------------------------------------------------------------------------------------------
// BindGroupLayout

typedef enum MnBindGroupLayoutEntryType {
  MnBindGroupLayoutEntryTypeUniformBuffer          = 0,
  MnBindGroupLayoutEntryTypeStorageBuffer          = 1,
  MnBindGroupLayoutEntryTypeSampledTexture         = 2,
  MnBindGroupLayoutEntryTypeSampler                = 3,
  MnBindGroupLayoutEntryTypeStorageTexture         = 4,
  MnBindGroupLayoutEntryTypeAccelerationStructure  = 5,
  MnBindGroupLayoutEntryTypeCombinedTextureSampler = 6,
  MnBindGroupLayoutEntryTypeForce32                = 0x7FFFFFFF,
  // N.B.: See `mnexus::BindGroupLayoutEntryType`.
} MnBindGroupLayoutEntryType;

typedef struct MnBindGroupLayoutEntry _MN_FINAL {
  uint32_t binding _MN_INIT(0);
  MnBindGroupLayoutEntryType type _MN_INIT(MnBindGroupLayoutEntryTypeUniformBuffer);
  uint32_t count _MN_INIT(1);
  // N.B.: See `mnexus::BindGroupLayoutEntry`.
} MnBindGroupLayoutEntry;

// ----------------------------------------------------------------------------------------------------
// BindingId

typedef struct MnBindingId _MN_FINAL {
  uint32_t group _MN_INIT(0);
  uint32_t binding _MN_INIT(0);
  uint32_t array_element _MN_INIT(0);
  // N.B.: See `mnexus::BindingId`.
} MnBindingId;

// ----------------------------------------------------------------------------------------------------
// ComputePipeline
//

typedef struct MnComputePipelineDesc _MN_FINAL {
  MnResourceHandle program _MN_INIT(MnInvalidResourceHandle);
  // N.B.: Placeholder for future expansion.
  // N.B.: See `mnexus::ComputePipelineDesc`.
} MnComputePipelineDesc;

// ----------------------------------------------------------------------------------------------------
// Render State Enums
//

typedef uint8_t MnPrimitiveTopology;
enum {
  MnPrimitiveTopologyPointList = 0,
  MnPrimitiveTopologyLineList,
  MnPrimitiveTopologyLineStrip,
  MnPrimitiveTopologyTriangleList,
  MnPrimitiveTopologyTriangleStrip,
};

typedef uint8_t MnPolygonMode;
enum {
  MnPolygonModeFill = 0,
  MnPolygonModeLine,
  MnPolygonModePoint,
};

typedef uint8_t MnCullMode;
enum {
  MnCullModeNone = 0,
  MnCullModeFront,
  MnCullModeBack,
};

typedef uint8_t MnFrontFace;
enum {
  MnFrontFaceCounterClockwise = 0,
  MnFrontFaceClockwise,
};

typedef uint8_t MnCompareOp;
enum {
  MnCompareOpNever = 0,
  MnCompareOpLess,
  MnCompareOpEqual,
  MnCompareOpLessEqual,
  MnCompareOpGreater,
  MnCompareOpNotEqual,
  MnCompareOpGreaterEqual,
  MnCompareOpAlways,
};

typedef uint8_t MnStencilOp;
enum {
  MnStencilOpKeep = 0,
  MnStencilOpZero,
  MnStencilOpReplace,
  MnStencilOpIncrementClamp,
  MnStencilOpDecrementClamp,
  MnStencilOpInvert,
  MnStencilOpIncrementWrap,
  MnStencilOpDecrementWrap,
};

typedef uint8_t MnBlendFactor;
enum {
  MnBlendFactorZero = 0,
  MnBlendFactorOne,
  MnBlendFactorSrcColor,
  MnBlendFactorOneMinusSrcColor,
  MnBlendFactorSrcAlpha,
  MnBlendFactorOneMinusSrcAlpha,
  MnBlendFactorDstColor,
  MnBlendFactorOneMinusDstColor,
  MnBlendFactorDstAlpha,
  MnBlendFactorOneMinusDstAlpha,
  MnBlendFactorSrcAlphaSaturated,
  MnBlendFactorConstant,
  MnBlendFactorOneMinusConstant,
};

typedef uint8_t MnBlendOp;
enum {
  MnBlendOpAdd = 0,
  MnBlendOpSubtract,
  MnBlendOpReverseSubtract,
  MnBlendOpMin,
  MnBlendOpMax,
};

typedef uint8_t MnIndexType;
enum {
  MnIndexTypeUint16 = 0,
  MnIndexTypeUint32,
};

typedef uint8_t MnVertexStepMode;
enum {
  MnVertexStepModeVertex = 0,
  MnVertexStepModeInstance,
};

typedef uint8_t MnLoadOp;
enum {
  MnLoadOpLoad = 0,
  MnLoadOpClear,
  MnLoadOpDontCare,
};

typedef uint8_t MnStoreOp;
enum {
  MnStoreOpStore = 0,
  MnStoreOpDontCare,
};

typedef uint8_t MnColorWriteMaskFlags;
enum {
  MnColorWriteMaskBitNone  = 0,
  MnColorWriteMaskBitRed   = 1 << 0,
  MnColorWriteMaskBitGreen = 1 << 1,
  MnColorWriteMaskBitBlue  = 1 << 2,
  MnColorWriteMaskBitAlpha = 1 << 3,
  MnColorWriteMaskBitAll   = 0x0F,
};

// ----------------------------------------------------------------------------------------------------
// Vertex Input
//

typedef struct MnVertexInputBindingDesc _MN_FINAL {
  uint32_t binding _MN_INIT(0);
  uint32_t stride _MN_INIT(0);
  MnVertexStepMode step_mode _MN_INIT(MnVertexStepModeVertex);
  // N.B.: See `mnexus::VertexInputBindingDesc`.
} MnVertexInputBindingDesc;

typedef struct MnVertexInputAttributeDesc _MN_FINAL {
  uint32_t location _MN_INIT(0);
  uint32_t binding _MN_INIT(0);
  MnFormat format _MN_INIT(MnFormatUndefined);
  uint32_t offset _MN_INIT(0);
  // N.B.: See `mnexus::VertexInputAttributeDesc`.
} MnVertexInputAttributeDesc;

// ----------------------------------------------------------------------------------------------------
// RenderPipeline
//

typedef struct MnRenderPipelineDesc _MN_FINAL {
  MnResourceHandle program _MN_INIT(MnInvalidResourceHandle);
  // vertex_bindings: ArrayProxy-compatible layout (count, ptr).
  // N.B.: Compiler inserts implicit padding between count and ptr on 64-bit targets.
  uint32_t vertex_binding_count _MN_INIT(0);
  MnVertexInputBindingDesc const* vertex_bindings _MN_INIT(_MN_NULL);
  // vertex_attributes: ArrayProxy-compatible layout (count, ptr).
  // N.B.: Compiler inserts implicit padding between count and ptr on 64-bit targets.
  uint32_t vertex_attribute_count _MN_INIT(0);
  MnVertexInputAttributeDesc const* vertex_attributes _MN_INIT(_MN_NULL);
  // color_formats: ArrayProxy-compatible layout (count, ptr).
  // N.B.: Compiler inserts implicit padding between count and ptr on 64-bit targets.
  uint32_t color_format_count _MN_INIT(0);
  MnFormat const* color_formats _MN_INIT(_MN_NULL);
  // Scalar fields.
  MnFormat depth_stencil_format _MN_INIT(MnFormatUndefined);
  uint32_t sample_count _MN_INIT(1);
  MnPrimitiveTopology topology _MN_INIT(MnPrimitiveTopologyTriangleList);
  MnCullMode cull_mode _MN_INIT(MnCullModeNone);
  MnFrontFace front_face _MN_INIT(MnFrontFaceCounterClockwise);
  MnCompareOp depth_compare_op _MN_INIT(MnCompareOpAlways);
  MnBool32 depth_test_enabled _MN_INIT(MnBoolFalse);
  MnBool32 depth_write_enabled _MN_INIT(MnBoolFalse);
  // N.B.: See `mnexus::RenderPipelineDesc`.
} MnRenderPipelineDesc;

// ----------------------------------------------------------------------------------------------------
// Clear Value
//

typedef struct MnClearValue _MN_FINAL {
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
  // N.B.: See `mnexus::ClearValue`.
} MnClearValue;

// ----------------------------------------------------------------------------------------------------
// Render Pass
//

typedef struct MnColorAttachmentDesc _MN_FINAL {
  MnResourceHandle texture _MN_INIT(MnInvalidResourceHandle);
  MnTextureSubresourceRange subresource_range;
  MnLoadOp load_op _MN_INIT(MnLoadOpClear);
  MnStoreOp store_op _MN_INIT(MnStoreOpStore);
  MnClearValue clear_value _MN_INIT({});
  // N.B.: See `mnexus::ColorAttachmentDesc`.
} MnColorAttachmentDesc;

typedef struct MnDepthStencilAttachmentDesc _MN_FINAL {
  MnResourceHandle texture _MN_INIT(MnInvalidResourceHandle);
  MnTextureSubresourceRange subresource_range;
  MnLoadOp depth_load_op _MN_INIT(MnLoadOpClear);
  MnStoreOp depth_store_op _MN_INIT(MnStoreOpStore);
  float depth_clear_value _MN_INIT(1.0f);
  MnLoadOp stencil_load_op _MN_INIT(MnLoadOpDontCare);
  MnStoreOp stencil_store_op _MN_INIT(MnStoreOpDontCare);
  uint32_t stencil_clear_value _MN_INIT(0);
  // N.B.: See `mnexus::DepthStencilAttachmentDesc`.
} MnDepthStencilAttachmentDesc;

typedef struct MnRenderPassDesc _MN_FINAL {
  // color_attachments: ArrayProxy-compatible layout (count, ptr).
  // N.B.: Compiler inserts implicit padding between count and ptr on 64-bit targets.
  uint32_t color_attachment_count _MN_INIT(0);
  MnColorAttachmentDesc const* color_attachments _MN_INIT(_MN_NULL);
  MnDepthStencilAttachmentDesc const* depth_stencil_attachment _MN_INIT(_MN_NULL);
  // N.B.: See `mnexus::RenderPassDesc`.
} MnRenderPassDesc;

// ====================================================================================================
// C++ types (namespace mnexus)
// ====================================================================================================

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

struct Extent2d final {
  uint32_t width = 0;
  uint32_t height = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(Extent2d, MnExtent2d);

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

struct AdapterCapability final {
  MnBool32 vertex_shader_storage_write = MnBoolFalse;
  MnBool32 polygon_mode_line = MnBoolFalse;
  MnBool32 polygon_mode_point = MnBoolFalse;
  MnBool32 buffer_mappable = MnBoolFalse;
  // N.B.: See `MnAdapterCapability`.
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(AdapterCapability, MnAdapterCapability);

struct AdapterInfo final {
  char device_name[256] = {};
  char vendor[256] = {};
  char architecture[256] = {};
  char description[256] = {};
  uint32_t vendor_id = 0;
  uint32_t device_id = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(AdapterInfo, MnAdapterInfo);

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
_MNEXUS_DEFINE_TYPESAFE_HANDLE(SamplerHandle);
_MNEXUS_DEFINE_TYPESAFE_HANDLE(VideoSessionHandle);
_MNEXUS_DEFINE_TYPESAFE_HANDLE(VideoSessionParametersHandle);

// Resource type tags embedded in bits 59-63 of the handle's u64 representation.
// Type 0 is reserved for null/invalid handles.
inline constexpr uint8_t kResourceTypeInvalid                 = 0;
inline constexpr uint8_t kResourceTypeBuffer                  = 1;
inline constexpr uint8_t kResourceTypeTexture                 = 2;
inline constexpr uint8_t kResourceTypeShaderModule            = 3;
inline constexpr uint8_t kResourceTypeProgram                 = 4;
inline constexpr uint8_t kResourceTypeComputePipeline         = 5;
inline constexpr uint8_t kResourceTypeRenderPipeline          = 6;
inline constexpr uint8_t kResourceTypeSampler                 = 7;
inline constexpr uint8_t kResourceTypeVideoSession            = 8;
inline constexpr uint8_t kResourceTypeVideoSessionParameters  = 9;

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
///
/// `IntraQueueSubmissionId` values form a per-queue timeline:
/// - A valid value is always non-zero.
/// - Values are monotonically increasing within the queue that produced them.
/// - Comparing values from different queues is meaningless.
/// - A value V is "completed" when `IDevice::QueueGetCompletedValue() >= V`.
using IntraQueueSubmissionId = mbase::TypesafeHandle<struct MnIntraQueueSubmissionIdTag, uint64_t, 0>;
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(IntraQueueSubmissionId, MnIntraQueueSubmissionId);

struct QueueSelection final {
  QueueId present_capable;
  std::optional<QueueId> dedicated_compute;
  std::optional<QueueId> dedicated_transfer;
  std::optional<QueueId> dedicated_video_decode;
  std::optional<QueueId> dedicated_video_encode;
};

// ----------------------------------------------------------------------------------------------------
// Command List
//

struct CommandListDesc final {
  uint32_t queue_family_index = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(CommandListDesc, MnCommandListDesc);

// ----------------------------------------------------------------------------------------------------
// Buffer
//

enum class BufferUsageFlagBits : uint32_t {
  kVertex         = MnBufferUsageFlagBitVertex,
  kIndex          = MnBufferUsageFlagBitIndex,
  kUniform        = MnBufferUsageFlagBitUniform,
  kStorage        = MnBufferUsageFlagBitStorage,
  kTransferSrc    = MnBufferUsageFlagBitTransferSrc,
  kTransferDst    = MnBufferUsageFlagBitTransferDst,
  kIndirect       = MnBufferUsageFlagBitIndirect,
  kMappable       = MnBufferUsageFlagBitMappable,
  kVideoDecodeSrc = MnBufferUsageFlagBitVideoDecodeSrc,
  kVideoEncodeDst = MnBufferUsageFlagBitVideoEncodeDst,
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
  kVideoDecodeDst   = MnTextureUsageFlagBitVideoDecodeDst,
  kVideoDecodeDpb   = MnTextureUsageFlagBitVideoDecodeDpb,
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
  k1D      = MnTextureDimension1D,
  k2D      = MnTextureDimension2D,
  k3D      = MnTextureDimension3D,
  kCube    = MnTextureDimensionCube,
  k2DArray = MnTextureDimension2DArray,
};

enum class Filter : uint32_t {
  kNearest = MnFilterNearest,
  kLinear  = MnFilterLinear,
};

enum class AddressMode : uint32_t {
  kRepeat       = MnAddressModeRepeat,
  kMirrorRepeat = MnAddressModeMirrorRepeat,
  kClampToEdge  = MnAddressModeClampToEdge,
};

struct SamplerDesc final {
  Filter min_filter         = Filter::kNearest;
  Filter mag_filter         = Filter::kNearest;
  Filter mipmap_filter      = Filter::kNearest;
  AddressMode address_mode_u = AddressMode::kClampToEdge;
  AddressMode address_mode_v = AddressMode::kClampToEdge;
  AddressMode address_mode_w = AddressMode::kClampToEdge;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(SamplerDesc, MnSamplerDesc);

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

// ----------------------------------------------------------------------------------------------------
// Resource barrier state / stage
//

/// Abstract resource state. Used by `ICommandList::TextureBarrier` to express
/// the destination state for an upcoming layout transition.
enum class ResourceBarrierState : uint8_t {
  kIndirectArgument = MnResourceBarrierStateIndirectArgument,
  kIndexBuffer      = MnResourceBarrierStateIndexBuffer,
  kVertexBuffer     = MnResourceBarrierStateVertexBuffer,
  kUniformBuffer    = MnResourceBarrierStateUniformBuffer,
  /// Sampled image, read-only storage, or shader read-only buffer.
  kReadOnly         = MnResourceBarrierStateReadOnly,
  /// Color or depth-stencil attachment in a render pass.
  kAttachment       = MnResourceBarrierStateAttachment,
  /// Storage image or storage buffer with read-write access.
  kUnorderedAccess  = MnResourceBarrierStateUnorderedAccess,
  /// Source for copy/blit/clear operations.
  kTransferSrc      = MnResourceBarrierStateTransferSrc,
  /// Destination for copy/blit/clear operations.
  kTransferDst      = MnResourceBarrierStateTransferDst,
  /// Vulkan Video decode reconstructed-picture target.
  kVideoDecodeDst   = MnResourceBarrierStateVideoDecodeDst,
  /// Vulkan Video decoded picture buffer (read/write). Use this for
  /// DPB+output coincide configurations.
  kVideoDecodeDpb   = MnResourceBarrierStateVideoDecodeDpb,
  /// Vulkan Video bitstream source image (rare; bitstream is usually a buffer).
  kVideoDecodeSrc   = MnResourceBarrierStateVideoDecodeSrc,
};

/// Pipeline-stage bits used by `ICommandList::TextureBarrier` to specify
/// the destination scope of an upcoming layout transition.
enum class ResourceBarrierStageFlagBits : uint32_t {
  kDrawIndirectInput     = MnResourceBarrierStageFlagBitDrawIndirectInput,
  kVertexInput           = MnResourceBarrierStageFlagBitVertexInput,
  kVertexShader          = MnResourceBarrierStageFlagBitVertexShader,
  kEarlyFragmentTests    = MnResourceBarrierStageFlagBitEarlyFragmentTests,
  kFragmentShader        = MnResourceBarrierStageFlagBitFragmentShader,
  kLateFragmentTests     = MnResourceBarrierStageFlagBitLateFragmentTests,
  kColorAttachmentOutput = MnResourceBarrierStageFlagBitColorAttachmentOutput,

  kComputeIndirectInput  = MnResourceBarrierStageFlagBitComputeIndirectInput,
  kComputeShader         = MnResourceBarrierStageFlagBitComputeShader,

  kTransfer              = MnResourceBarrierStageFlagBitTransfer,

  kVideoDecode           = MnResourceBarrierStageFlagBitVideoDecode,

  kFragmentTestsBits = kEarlyFragmentTests | kLateFragmentTests,

  kGraphicsBits = kDrawIndirectInput
                | kVertexInput
                | kVertexShader
                | kEarlyFragmentTests
                | kFragmentShader
                | kLateFragmentTests
                | kColorAttachmentOutput,

  kComputeBits  = kComputeIndirectInput
                | kComputeShader,
};
MBASE_DEFINE_ENUM_CLASS_BITFLAGS_OPERATORS(ResourceBarrierStageFlagBits);
using ResourceBarrierStageFlags = mbase::BitFlags<ResourceBarrierStageFlagBits>;

// ----------------------------------------------------------------------------------------------------
// Format (C++ enum class referencing the C MnFormat values)
//

enum class Format : uint32_t {
  kUndefined = MnFormatUndefined,

  kR5G6B5_UNORM_PACK16 = MnFormatR5G6B5_UNORM_PACK16,
  kR5G5B5A1_UNORM_PACK16 = MnFormatR5G5B5A1_UNORM_PACK16,

  kR8_UNORM = MnFormatR8_UNORM,
  kR8G8_UNORM = MnFormatR8G8_UNORM,
  kR8G8B8_UNORM = MnFormatR8G8B8_UNORM,
  kR8G8B8A8_UNORM = MnFormatR8G8B8A8_UNORM,
  kR8G8B8A8_SRGB = MnFormatR8G8B8A8_SRGB,
  kB8G8R8A8_UNORM = MnFormatB8G8R8A8_UNORM,
  kB8G8R8A8_SRGB = MnFormatB8G8R8A8_SRGB,

  kR16_SFLOAT = MnFormatR16_SFLOAT,
  kR16G16_SFLOAT = MnFormatR16G16_SFLOAT,
  kR16G16B16_SFLOAT = MnFormatR16G16B16_SFLOAT,
  kR16G16B16A16_UNORM = MnFormatR16G16B16A16_UNORM,
  kR16G16B16A16_UINT = MnFormatR16G16B16A16_UINT,
  kR16G16B16A16_SFLOAT = MnFormatR16G16B16A16_SFLOAT,

  kR32_SFLOAT = MnFormatR32_SFLOAT,
  kR32G32_SFLOAT = MnFormatR32G32_SFLOAT,
  kR32G32B32_SFLOAT = MnFormatR32G32B32_SFLOAT,
  kR32G32B32A32_UINT = MnFormatR32G32B32A32_UINT,
  kR32G32B32A32_SFLOAT = MnFormatR32G32B32A32_SFLOAT,

  kA2R10G10B10_UNORM_PACK32 = MnFormatA2R10G10B10_UNORM_PACK32,
  kA2R10G10B10_SNORM_PACK32 = MnFormatA2R10G10B10_SNORM_PACK32,
  kA2R10G10B10_USCALED_PACK32 = MnFormatA2R10G10B10_USCALED_PACK32,
  kA2R10G10B10_SSCALED_PACK32 = MnFormatA2R10G10B10_SSCALED_PACK32,
  kA2R10G10B10_UINT_PACK32 = MnFormatA2R10G10B10_UINT_PACK32,
  kA2R10G10B10_SINT_PACK32 = MnFormatA2R10G10B10_SINT_PACK32,
  kA2B10G10R10_UNORM_PACK32 = MnFormatA2B10G10R10_UNORM_PACK32,
  kA2B10G10R10_SNORM_PACK32 = MnFormatA2B10G10R10_SNORM_PACK32,
  kA2B10G10R10_USCALED_PACK32 = MnFormatA2B10G10R10_USCALED_PACK32,
  kA2B10G10R10_SSCALED_PACK32 = MnFormatA2B10G10R10_SSCALED_PACK32,
  kA2B10G10R10_UINT_PACK32 = MnFormatA2B10G10R10_UINT_PACK32,
  kA2B10G10R10_SINT_PACK32 = MnFormatA2B10G10R10_SINT_PACK32,

  kD16_UNORM = MnFormatD16_UNORM,
  kD32_SFLOAT = MnFormatD32_SFLOAT,
  kD16_UNORM_S8_UINT = MnFormatD16_UNORM_S8_UINT,
  kD24_UNORM_S8_UINT = MnFormatD24_UNORM_S8_UINT,
  kD32_SFLOAT_S8_UINT = MnFormatD32_SFLOAT_S8_UINT,

  kBC1_RGB_UNORM_BLOCK = MnFormatBC1_RGB_UNORM_BLOCK,
  kBC1_RGB_SRGB_BLOCK = MnFormatBC1_RGB_SRGB_BLOCK,
  kBC1_RGBA_UNORM_BLOCK = MnFormatBC1_RGBA_UNORM_BLOCK,
  kBC1_RGBA_SRGB_BLOCK = MnFormatBC1_RGBA_SRGB_BLOCK,
  kBC2_UNORM_BLOCK = MnFormatBC2_UNORM_BLOCK,
  kBC2_SRGB_BLOCK = MnFormatBC2_SRGB_BLOCK,
  kBC3_UNORM_BLOCK = MnFormatBC3_UNORM_BLOCK,
  kBC3_SRGB_BLOCK = MnFormatBC3_SRGB_BLOCK,
  kBC4_UNORM_BLOCK = MnFormatBC4_UNORM_BLOCK,
  kBC4_SNORM_BLOCK = MnFormatBC4_SNORM_BLOCK,
  kBC5_UNORM_BLOCK = MnFormatBC5_UNORM_BLOCK,
  kBC5_SNORM_BLOCK = MnFormatBC5_SNORM_BLOCK,

  kETC2_R8G8B8_UNORM_BLOCK = MnFormatETC2_R8G8B8_UNORM_BLOCK,
  kETC2_R8G8B8_SRGB_BLOCK = MnFormatETC2_R8G8B8_SRGB_BLOCK,
  kETC2_R8G8B8A1_UNORM_BLOCK = MnFormatETC2_R8G8B8A1_UNORM_BLOCK,
  kETC2_R8G8B8A1_SRGB_BLOCK = MnFormatETC2_R8G8B8A1_SRGB_BLOCK,
  kETC2_R8G8B8A8_UNORM_BLOCK = MnFormatETC2_R8G8B8A8_UNORM_BLOCK,
  kETC2_R8G8B8A8_SRGB_BLOCK = MnFormatETC2_R8G8B8A8_SRGB_BLOCK,
  kEAC_R11_UNORM_BLOCK = MnFormatEAC_R11_UNORM_BLOCK,
  kEAC_R11_SNORM_BLOCK = MnFormatEAC_R11_SNORM_BLOCK,
  kEAC_R11G11_UNORM_BLOCK = MnFormatEAC_R11G11_UNORM_BLOCK,
  kEAC_R11G11_SNORM_BLOCK = MnFormatEAC_R11G11_SNORM_BLOCK,

  kASTC_4x4_UNORM_BLOCK = MnFormatASTC_4x4_UNORM_BLOCK,
  kASTC_4x4_SRGB_BLOCK = MnFormatASTC_4x4_SRGB_BLOCK,
  kASTC_5x4_UNORM_BLOCK = MnFormatASTC_5x4_UNORM_BLOCK,
  kASTC_5x4_SRGB_BLOCK = MnFormatASTC_5x4_SRGB_BLOCK,
  kASTC_5x5_UNORM_BLOCK = MnFormatASTC_5x5_UNORM_BLOCK,
  kASTC_5x5_SRGB_BLOCK = MnFormatASTC_5x5_SRGB_BLOCK,
  kASTC_6x5_UNORM_BLOCK = MnFormatASTC_6x5_UNORM_BLOCK,
  kASTC_6x5_SRGB_BLOCK = MnFormatASTC_6x5_SRGB_BLOCK,
  kASTC_6x6_UNORM_BLOCK = MnFormatASTC_6x6_UNORM_BLOCK,
  kASTC_6x6_SRGB_BLOCK = MnFormatASTC_6x6_SRGB_BLOCK,
  kASTC_8x5_UNORM_BLOCK = MnFormatASTC_8x5_UNORM_BLOCK,
  kASTC_8x5_SRGB_BLOCK = MnFormatASTC_8x5_SRGB_BLOCK,
  kASTC_8x6_UNORM_BLOCK = MnFormatASTC_8x6_UNORM_BLOCK,
  kASTC_8x6_SRGB_BLOCK = MnFormatASTC_8x6_SRGB_BLOCK,
  kASTC_8x8_UNORM_BLOCK = MnFormatASTC_8x8_UNORM_BLOCK,
  kASTC_8x8_SRGB_BLOCK = MnFormatASTC_8x8_SRGB_BLOCK,
  kASTC_10x5_UNORM_BLOCK = MnFormatASTC_10x5_UNORM_BLOCK,
  kASTC_10x5_SRGB_BLOCK = MnFormatASTC_10x5_SRGB_BLOCK,
  kASTC_10x6_UNORM_BLOCK = MnFormatASTC_10x6_UNORM_BLOCK,
  kASTC_10x6_SRGB_BLOCK = MnFormatASTC_10x6_SRGB_BLOCK,
  kASTC_10x8_UNORM_BLOCK = MnFormatASTC_10x8_UNORM_BLOCK,
  kASTC_10x8_SRGB_BLOCK = MnFormatASTC_10x8_SRGB_BLOCK,
  kASTC_10x10_UNORM_BLOCK = MnFormatASTC_10x10_UNORM_BLOCK,
  kASTC_10x10_SRGB_BLOCK = MnFormatASTC_10x10_SRGB_BLOCK,
  kASTC_12x10_UNORM_BLOCK = MnFormatASTC_12x10_UNORM_BLOCK,
  kASTC_12x10_SRGB_BLOCK = MnFormatASTC_12x10_SRGB_BLOCK,
  kASTC_12x12_UNORM_BLOCK = MnFormatASTC_12x12_UNORM_BLOCK,
  kASTC_12x12_SRGB_BLOCK = MnFormatASTC_12x12_SRGB_BLOCK,

  kG8_B8R8_2PLANE_420_UNORM                  = MnFormatG8_B8R8_2PLANE_420_UNORM,
  kG10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 = MnFormatG10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
};

struct TextureDesc final {
  TextureUsageFlags usage;
  Format format = Format::kUndefined;
  TextureDimension dimension = TextureDimension::k2D;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 1;
  uint32_t mip_level_count = 1;
  uint32_t array_layer_count = 1;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(TextureDesc, MnTextureDesc);

// ----------------------------------------------------------------------------------------------------
// Video Coding
//

enum class VideoH265Profile : uint32_t {
  kMain   = MnVideoH265ProfileMain,
  kMain10 = MnVideoH265ProfileMain10,
};

enum class VideoBitDepth : uint32_t {
  k8  = MnVideoBitDepth8,
  k10 = MnVideoBitDepth10,
};

enum class VideoH265Level : uint32_t {
  k1_0 = MnVideoH265Level1_0,
  k2_0 = MnVideoH265Level2_0,
  k2_1 = MnVideoH265Level2_1,
  k3_0 = MnVideoH265Level3_0,
  k3_1 = MnVideoH265Level3_1,
  k4_0 = MnVideoH265Level4_0,
  k4_1 = MnVideoH265Level4_1,
  k5_0 = MnVideoH265Level5_0,
  k5_1 = MnVideoH265Level5_1,
  k5_2 = MnVideoH265Level5_2,
  k6_0 = MnVideoH265Level6_0,
  k6_1 = MnVideoH265Level6_1,
  k6_2 = MnVideoH265Level6_2,
};

struct VideoCommonCapabilities final {
  Extent2d picture_access_granularity;
  Extent2d min_coded_extent;
  Extent2d max_coded_extent;
  uint64_t min_bitstream_buffer_offset_alignment = 0;
  uint64_t min_bitstream_buffer_size_alignment = 0;
  uint32_t max_dpb_slots = 0;
  uint32_t max_active_reference_pictures = 0;
  MnBool32 protected_content = MnBoolFalse;
  MnBool32 separate_reference_images = MnBoolFalse;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(VideoCommonCapabilities, MnVideoCommonCapabilities);

struct VideoDecodeCommonCapabilities final {
  MnBool32 dpb_and_output_coincide = MnBoolFalse;
  MnBool32 dpb_and_output_distinct = MnBoolFalse;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(VideoDecodeCommonCapabilities, MnVideoDecodeCommonCapabilities);

struct VideoDecodeH265Capabilities final {
  VideoCommonCapabilities       common;
  VideoDecodeCommonCapabilities decode_common;
  VideoH265Level                max_level = VideoH265Level::k1_0;
  Format                        picture_format = Format::kUndefined;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(VideoDecodeH265Capabilities, MnVideoDecodeH265Capabilities);

/// `VideoH265Level` <-> H.265 spec `general_level_idc` (uint8 = level x 30).
uint8_t                       VideoH265LevelToSpecIdc(VideoH265Level level);
std::optional<VideoH265Level> VideoH265LevelFromSpecIdc(uint8_t spec_idc);

struct VideoSessionDecodeH265Desc final {
  VideoH265Profile profile = VideoH265Profile::kMain;
  VideoBitDepth    bit_depth = VideoBitDepth::k8;
  Format           picture_format = Format::kUndefined;
  Format           reference_picture_format = Format::kUndefined;
  Extent2d         max_coded_extent;
  uint32_t         max_dpb_slots = 0;
  uint32_t         max_active_reference_pictures = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(VideoSessionDecodeH265Desc, MnVideoSessionDecodeH265Desc);

struct VideoSessionParametersDecodeH265Desc final {
  VideoSessionHandle session;
  uint8_t const*     vps_data = nullptr;
  uint32_t           vps_size = 0;
  uint8_t const*     sps_data = nullptr;
  uint32_t           sps_size = 0;
  uint8_t const*     pps_data = nullptr;
  uint32_t           pps_size = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(VideoSessionParametersDecodeH265Desc, MnVideoSessionParametersDecodeH265Desc);

// ----------------------------------------------------------------------------------------------------
// Shader
//

enum class ShaderSourceLanguage : uint32_t {
  kSpirV = MnShaderSourceLanguageSpirV,
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
  kUniformBuffer          = MnBindGroupLayoutEntryTypeUniformBuffer,
  kStorageBuffer          = MnBindGroupLayoutEntryTypeStorageBuffer,
  kSampledTexture         = MnBindGroupLayoutEntryTypeSampledTexture,
  kSampler                = MnBindGroupLayoutEntryTypeSampler,
  kStorageTexture         = MnBindGroupLayoutEntryTypeStorageTexture,
  kAccelerationStructure  = MnBindGroupLayoutEntryTypeAccelerationStructure,
  kCombinedTextureSampler = MnBindGroupLayoutEntryTypeCombinedTextureSampler,
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
  ProgramHandle program;
  // N.B.: Placeholder for future expansion.
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(ComputePipelineDesc, MnComputePipelineDesc);

// ----------------------------------------------------------------------------------------------------
// Backend Type
//

enum class BackendType : uint8_t {
  kWebGpu = MnBackendTypeWebGpu,
  kVulkan = MnBackendTypeVulkan,
};

// ----------------------------------------------------------------------------------------------------
// Clip Space Convention
//

enum class ClipSpaceYDirection : uint8_t {
  kUp   = MnClipSpaceYDirectionUp,
  kDown = MnClipSpaceYDirectionDown,
};

enum class ClipSpaceDepthRange : uint8_t {
  kZeroToOne    = MnClipSpaceDepthRangeZeroToOne,
  kNegOneToOne  = MnClipSpaceDepthRangeNegOneToOne,
};

struct ClipSpaceConvention final {
  ClipSpaceYDirection y_direction = ClipSpaceYDirection::kUp;
  ClipSpaceDepthRange depth_range = ClipSpaceDepthRange::kZeroToOne;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(ClipSpaceConvention, MnClipSpaceConvention);

// ----------------------------------------------------------------------------------------------------
// Color Space
//

enum class ColorSpace : uint8_t {
  kLinear      = MnColorSpaceLinear,
  kSrgb        = MnColorSpaceSrgb,
  kHdr10St2084 = MnColorSpaceHdr10St2084,
};

// ----------------------------------------------------------------------------------------------------
// Render State Enums
//

enum class PrimitiveTopology : uint8_t {
  kPointList     = MnPrimitiveTopologyPointList,
  kLineList      = MnPrimitiveTopologyLineList,
  kLineStrip     = MnPrimitiveTopologyLineStrip,
  kTriangleList  = MnPrimitiveTopologyTriangleList,
  kTriangleStrip = MnPrimitiveTopologyTriangleStrip,
};

enum class PolygonMode : uint8_t {
  kFill  = MnPolygonModeFill,
  kLine  = MnPolygonModeLine,
  kPoint = MnPolygonModePoint,
};

enum class CullMode : uint8_t {
  kNone  = MnCullModeNone,
  kFront = MnCullModeFront,
  kBack  = MnCullModeBack,
};

enum class FrontFace : uint8_t {
  kCounterClockwise = MnFrontFaceCounterClockwise,
  kClockwise        = MnFrontFaceClockwise,
};

enum class CompareOp : uint8_t {
  kNever        = MnCompareOpNever,
  kLess         = MnCompareOpLess,
  kEqual        = MnCompareOpEqual,
  kLessEqual    = MnCompareOpLessEqual,
  kGreater      = MnCompareOpGreater,
  kNotEqual     = MnCompareOpNotEqual,
  kGreaterEqual = MnCompareOpGreaterEqual,
  kAlways       = MnCompareOpAlways,
};

enum class StencilOp : uint8_t {
  kKeep           = MnStencilOpKeep,
  kZero           = MnStencilOpZero,
  kReplace        = MnStencilOpReplace,
  kIncrementClamp = MnStencilOpIncrementClamp,
  kDecrementClamp = MnStencilOpDecrementClamp,
  kInvert         = MnStencilOpInvert,
  kIncrementWrap  = MnStencilOpIncrementWrap,
  kDecrementWrap  = MnStencilOpDecrementWrap,
};

enum class BlendFactor : uint8_t {
  kZero             = MnBlendFactorZero,
  kOne              = MnBlendFactorOne,
  kSrcColor         = MnBlendFactorSrcColor,
  kOneMinusSrcColor = MnBlendFactorOneMinusSrcColor,
  kSrcAlpha         = MnBlendFactorSrcAlpha,
  kOneMinusSrcAlpha = MnBlendFactorOneMinusSrcAlpha,
  kDstColor         = MnBlendFactorDstColor,
  kOneMinusDstColor = MnBlendFactorOneMinusDstColor,
  kDstAlpha         = MnBlendFactorDstAlpha,
  kOneMinusDstAlpha = MnBlendFactorOneMinusDstAlpha,
  kSrcAlphaSaturated = MnBlendFactorSrcAlphaSaturated,
  kConstant         = MnBlendFactorConstant,
  kOneMinusConstant = MnBlendFactorOneMinusConstant,
};

enum class BlendOp : uint8_t {
  kAdd             = MnBlendOpAdd,
  kSubtract        = MnBlendOpSubtract,
  kReverseSubtract = MnBlendOpReverseSubtract,
  kMin             = MnBlendOpMin,
  kMax             = MnBlendOpMax,
};

enum class IndexType : uint8_t {
  kUint16 = MnIndexTypeUint16,
  kUint32 = MnIndexTypeUint32,
};

enum class VertexStepMode : uint8_t {
  kVertex   = MnVertexStepModeVertex,
  kInstance = MnVertexStepModeInstance,
};

enum class LoadOp : uint8_t {
  kLoad     = MnLoadOpLoad,
  kClear    = MnLoadOpClear,
  kDontCare = MnLoadOpDontCare,
};

enum class StoreOp : uint8_t {
  kStore    = MnStoreOpStore,
  kDontCare = MnStoreOpDontCare,
};

enum class ColorWriteMask : uint8_t {
  kNone  = MnColorWriteMaskBitNone,
  kRed   = MnColorWriteMaskBitRed,
  kGreen = MnColorWriteMaskBitGreen,
  kBlue  = MnColorWriteMaskBitBlue,
  kAlpha = MnColorWriteMaskBitAlpha,
  kAll   = MnColorWriteMaskBitAll,
};
MBASE_DEFINE_ENUM_CLASS_BITFLAGS_OPERATORS(ColorWriteMask);

// ----------------------------------------------------------------------------------------------------
// Vertex Input
//

struct VertexInputBindingDesc final {
  uint32_t binding = 0;
  uint32_t stride = 0;
  VertexStepMode step_mode = VertexStepMode::kVertex;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(VertexInputBindingDesc, MnVertexInputBindingDesc);

struct VertexInputAttributeDesc final {
  uint32_t location = 0;
  uint32_t binding = 0;
  Format format = Format::kUndefined;
  uint32_t offset = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(VertexInputAttributeDesc, MnVertexInputAttributeDesc);

// ----------------------------------------------------------------------------------------------------
// RenderPipeline
//

struct RenderPipelineDesc final {
  ProgramHandle program;
  container::ArrayProxy<VertexInputBindingDesc const> vertex_bindings;
  container::ArrayProxy<VertexInputAttributeDesc const> vertex_attributes;
  container::ArrayProxy<Format const> color_formats;
  Format depth_stencil_format = Format::kUndefined;
  uint32_t sample_count = 1;
  PrimitiveTopology topology = PrimitiveTopology::kTriangleList;
  CullMode cull_mode = CullMode::kNone;
  FrontFace front_face = FrontFace::kCounterClockwise;
  CompareOp depth_compare_op = CompareOp::kAlways;
  MnBool32 depth_test_enabled = MnBoolFalse;
  MnBool32 depth_write_enabled = MnBoolFalse;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(RenderPipelineDesc, MnRenderPipelineDesc);

// ----------------------------------------------------------------------------------------------------
// Clear Value
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
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(ClearValue, MnClearValue);

// ----------------------------------------------------------------------------------------------------
// Render Pass
//

struct ColorAttachmentDesc final {
  TextureHandle texture;
  TextureSubresourceRange subresource_range;
  LoadOp load_op = LoadOp::kClear;
  StoreOp store_op = StoreOp::kStore;
  ClearValue clear_value = {};
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(ColorAttachmentDesc, MnColorAttachmentDesc);

struct DepthStencilAttachmentDesc final {
  TextureHandle texture;
  TextureSubresourceRange subresource_range;
  LoadOp depth_load_op = LoadOp::kClear;
  StoreOp depth_store_op = StoreOp::kStore;
  float depth_clear_value = 1.0f;
  LoadOp stencil_load_op = LoadOp::kDontCare;
  StoreOp stencil_store_op = StoreOp::kDontCare;
  uint32_t stencil_clear_value = 0;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(DepthStencilAttachmentDesc, MnDepthStencilAttachmentDesc);

struct RenderPassDesc final {
  container::ArrayProxy<ColorAttachmentDesc const> color_attachments;
  DepthStencilAttachmentDesc const* depth_stencil_attachment = nullptr;
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(RenderPassDesc, MnRenderPassDesc);

// ----------------------------------------------------------------------------------------------------
// Utilities
//

//
// Functions returning `std::string_view` return pointers to static strings, null-terminated.
//

std::string_view ToString(BackendType value);
std::string_view ToString(ColorSpace value);

std::string_view ToString(MnFormat value);
std::string_view ToString(BindGroupLayoutEntryType value);
std::string ToString(TextureUsageFlags value);

std::string_view ToString(PrimitiveTopology value);
std::string_view ToString(PolygonMode value);
std::string_view ToString(CullMode value);
std::string_view ToString(FrontFace value);
std::string_view ToString(CompareOp value);
std::string_view ToString(StencilOp value);
std::string_view ToString(BlendFactor value);
std::string_view ToString(BlendOp value);
std::string_view ToString(IndexType value);
std::string_view ToString(VertexStepMode value);
std::string_view ToString(LoadOp value);
std::string_view ToString(StoreOp value);
std::string ToString(ColorWriteMask value);

inline uint32_t GetFormatSizeInBytes(MnFormat value) { return MnGetFormatSizeInBytes(value); }
inline Extent3d GetFormatTexelBlockExtent(MnFormat value) {
  MnExtent3d e = MnGetFormatTexelBlockExtent(value);
  return Extent3d { e.width, e.height, e.depth };
}

} // namespace mnexus

#endif // defined(__cplusplus)
