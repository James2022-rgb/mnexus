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
// Nexus

typedef struct MnNexusDesc {
  MnBool32 headless;
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
  MnResourceHandle shader_module _MN_INIT(MnInvalidResourceHandle);
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
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(CommandListDesc, MnCommandListDesc);

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
  ShaderModuleHandle shader_module;
  // N.B.: Placeholder for future expansion.
};
_MNEXUS_STATIC_ASSERT_ABI_EQUIVALENCE(ComputePipelineDesc, MnComputePipelineDesc);

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

inline uint32_t GetFormatSizeInBytes(MnFormat value) { return MnGetFormatSizeInBytes(value); }
inline Extent3d GetFormatTexelBlockExtent(MnFormat value) {
  MnExtent3d e = MnGetFormatTexelBlockExtent(value);
  return Extent3d { e.width, e.height, e.depth };
}

} // namespace mnexus

#endif // defined(__cplusplus)
