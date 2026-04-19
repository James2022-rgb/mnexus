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

// ----------------------------------------------------------------------------------------------------
// Samplers
//

VkFilter             ToVkFilter(mnexus::Filter value);
VkSamplerMipmapMode  ToVkSamplerMipmapMode(mnexus::Filter value);
VkSamplerAddressMode ToVkSamplerAddressMode(mnexus::AddressMode value);

// ----------------------------------------------------------------------------------------------------
// Compare / stencil / blend
//

VkCompareOp   ToVkCompareOp(mnexus::CompareOp value);
VkStencilOp   ToVkStencilOp(mnexus::StencilOp value);
VkBlendFactor ToVkBlendFactor(mnexus::BlendFactor value);
VkBlendOp     ToVkBlendOp(mnexus::BlendOp value);

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

} // namespace mnexus_backend::vulkan
