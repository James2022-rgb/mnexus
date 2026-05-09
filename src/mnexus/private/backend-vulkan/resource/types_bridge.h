#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// Buffer / texture usage flags
//

VkBufferUsageFlags ToVkBufferUsageFlags(mnexus::BufferUsageFlags usage);
VkImageUsageFlags  ToVkImageUsageFlags(mnexus::TextureUsageFlags usage, VkFormat vk_format);

// ----------------------------------------------------------------------------------------------------
// Texture dimension
//

/// Converts to a `VkImageType`. Note that cube maps (`kCube`) map to
/// `VK_IMAGE_TYPE_2D` -- Vulkan represents cubes as 2D images with 6 array
/// layers and the `VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT` flag.
VkImageType ToVkImageType(mnexus::TextureDimension value);

// ----------------------------------------------------------------------------------------------------
// Format
//

VkFormat       ToVkFormat(mnexus::Format value);
mnexus::Format FromVkFormat(VkFormat value);

// ----------------------------------------------------------------------------------------------------
// Color space
//

VkColorSpaceKHR    ToVkColorSpaceKHR(mnexus::ColorSpace value);
mnexus::ColorSpace FromVkColorSpace(VkColorSpaceKHR value);
/// `FromVkColorSpace` without the warning log -- returns `nullopt` for any
/// `VkColorSpaceKHR` that doesn't have a `mnexus::ColorSpace` counterpart.
/// Used by `WsiSwapchain::QuerySurfaceCapability` to silently drop entries
/// the public API can't represent yet (scRGB linear, BT.2020 nonlinear,
/// AdobeRGB, etc.).
std::optional<mnexus::ColorSpace> TryFromVkColorSpace(VkColorSpaceKHR value);

// ----------------------------------------------------------------------------------------------------
// Samplers
//

VkFilter             ToVkFilter(mnexus::Filter value);
VkSamplerMipmapMode  ToVkSamplerMipmapMode(mnexus::Filter value);
VkSamplerAddressMode ToVkSamplerAddressMode(mnexus::AddressMode value);

// ----------------------------------------------------------------------------------------------------
// Compare / stencil / blend
//

VkCompareOp           ToVkCompareOp(mnexus::CompareOp value);
VkStencilOp           ToVkStencilOp(mnexus::StencilOp value);
VkBlendFactor         ToVkBlendFactor(mnexus::BlendFactor value);
VkBlendOp             ToVkBlendOp(mnexus::BlendOp value);
VkColorComponentFlags ToVkColorComponentFlags(mnexus::ColorWriteMask value);

// ----------------------------------------------------------------------------------------------------
// Rasterization
//

VkCullModeFlags     ToVkCullMode(mnexus::CullMode value);
VkFrontFace         ToVkFrontFace(mnexus::FrontFace value);
VkPrimitiveTopology ToVkPrimitiveTopology(mnexus::PrimitiveTopology value);
VkPolygonMode       ToVkPolygonMode(mnexus::PolygonMode value);

// ----------------------------------------------------------------------------------------------------
// Vertex input
//

VkIndexType                       ToVkIndexType(mnexus::IndexType value);
VkVertexInputRate                 ToVkVertexInputRate(mnexus::VertexStepMode value);
VkVertexInputBindingDescription   ToVkVertexInputBindingDescription(mnexus::VertexInputBindingDesc const& value);
VkVertexInputAttributeDescription ToVkVertexInputAttributeDescription(mnexus::VertexInputAttributeDesc const& value);

// ----------------------------------------------------------------------------------------------------
// Render passes
//

VkAttachmentLoadOp  ToVkAttachmentLoadOp(mnexus::LoadOp value);
VkAttachmentStoreOp ToVkAttachmentStoreOp(mnexus::StoreOp value);

// ----------------------------------------------------------------------------------------------------
// Image aspect / subresource range
//

VkImageAspectFlags      ToVkImageAspectMask(mnexus::TextureAspectFlags flags);
VkImageSubresourceRange  ToVkImageSubresourceRange(mnexus::TextureSubresourceRange const& subresource_range);
VkImageSubresourceLayers ToVkImageSubresourceLayers(mnexus::TextureSubresourceRange const& subresource_range);

// ----------------------------------------------------------------------------------------------------
// Descriptor type (from BindGroupLayoutEntryType)
//

VkDescriptorType ToVkDescriptorType(mnexus::BindGroupLayoutEntryType value);

// ----------------------------------------------------------------------------------------------------
// Resource barriers
//

VkPipelineStageFlags2KHR ToVkPipelineStageFlags2(mnexus::ResourceBarrierStageFlags value);

/// Maps a ResourceBarrierState to the matching VkAccessFlags2KHR, masked by
/// `stage_flags` so the result satisfies the per-stage access constraints in
/// the Vulkan spec (e.g. DEPTH_STENCIL_ATTACHMENT_* requires EARLY/LATE
/// FRAGMENT_TESTS; COLOR_ATTACHMENT_* requires COLOR_ATTACHMENT_OUTPUT).
VkAccessFlags2KHR        ToVkAccessFlags2(mnexus::ResourceBarrierState state, mnexus::ResourceBarrierStageFlags stage_flags);

VkImageLayout            ToVkImageLayout(mnexus::ResourceBarrierState value);

} // namespace mnexus_backend::vulkan
