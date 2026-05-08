#pragma once

// c++ headers ------------------------------------------
#include <array>
#include <optional>

// public project headers -------------------------------
#include "mbase/public/container.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

#include "backend-vulkan/descriptor/fwd.h"
#include "backend-vulkan/device/fwd.h"
#include "backend-vulkan/object/fwd.h"
#include "backend-vulkan/resource/fwd.h"

namespace mnexus_backend::vulkan {

union RenderTargetClearValue final {
  /// Also used for depth.
  std::array<float, 4> f32;
  std::array<int32_t, 4> i32;
  /// Also used for stencil.
  std::array<uint32_t, 4> u32;
};

struct RenderTargetDesc final {
  VulkanImage const* vk_image = nullptr;
  mnexus::TextureSubresourceRange subresource_range {};
  std::optional<RenderTargetClearValue> clear_value;
  std::optional<RenderTargetClearValue> stencil_clear_value;
};

struct DynamicRenderPassDesc final {
  uint32_t view_mask = 0;
  mbase::SmallVector<RenderTargetDesc, 1> color_attachments;
  std::optional<RenderTargetDesc> depth_stencil_attachment;
};

// ----------------------------------------------------------------------------------------------------
// ICommandEncoder
//
// Wraps a VkCommandBuffer and tracks Vulkan-side recording state.
// Implementation is hidden in the .cpp file.
//

struct CommandEncoderDesc final {
  VkCommandBuffer vk_cb_handle = VK_NULL_HANDLE;
  IVulkanDevice const* vk_device = nullptr;
  IDescriptorSetAllocator* ds_allocator = nullptr;
  ResourceStorage* resource_storage = nullptr;
};

class ICommandEncoder {
public:
  virtual ~ICommandEncoder() = default;
  MBASE_DEFAULT_COPY_MOVE(ICommandEncoder);

  static ICommandEncoder* Create(CommandEncoderDesc const& desc);

  /// Clean up internal state and delete this object. The owned
  /// `VkCommandBuffer` is NOT freed (its lifetime is managed by the
  /// caller's `VkCommandPool`).
  virtual void Shutdown() = 0;

  [[nodiscard]] virtual VkCommandBuffer vk_cb_handle() const = 0;

  virtual void End() = 0;

  /// Clears the specified subresource range of the given image to the given value.
  ///
  /// ## Render Pass Scope
  /// Outside.
  ///
  /// ## Resource State
  /// The subresource range MUST be in `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` layout.
  virtual void CmdClearImageSubresourceRange(
    VulkanImage const& vk_image,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ClearValue const& clear_value
  ) = 0;

  /// Copies data from a buffer to the specified subresource range of the given image.
  ///
  /// ## Render Pass Scope
  /// Outside.
  ///
  /// ## Resource State
  /// The destination subresource range MUST be in `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` layout.
  virtual void CmdCopyBufferToImageSubresource(
    VulkanBuffer const& src_vk_buffer,
    uint32_t src_buffer_offset,
    VulkanImage const& dst_vk_image,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Extent3d const& copy_extent
  ) = 0;

  /// Copies data from a subresource range of the given image to a buffer.
  ///
  /// ## Render Pass Scope
  /// Outside.
  ///
  /// ## Resource State
  /// The source subresource range MUST be in `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` layout.
  virtual void CmdCopyImageSubresourceToBuffer(
    VulkanImage const& src_vk_image,
    mnexus::TextureSubresourceRange const& src_subresource_range,
    VulkanBuffer const& dst_vk_buffer,
    uint32_t dst_buffer_offset,
    mnexus::Extent3d const& copy_extent
  ) = 0;

  /// Copies a subresource of `src_vk_image` to a subresource of
  /// `dst_vk_image`. The aspect masks may differ (e.g. plane 0 of a
  /// multi-planar source to COLOR of a single-plane destination).
  ///
  /// ## Render Pass Scope
  /// Outside.
  ///
  /// ## Resource State
  /// Source MUST be `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`, destination
  /// MUST be `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`.
  virtual void CmdCopyImageSubresourceToImageSubresource(
    VulkanImage const& src_vk_image,
    mnexus::TextureSubresourceRange const& src_subresource_range,
    VulkanImage const& dst_vk_image,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Extent3d const& copy_extent
  ) = 0;

  // ----------------------------------------------------------------------------------------------
  // Graphics Pipeline
  //

  /// Begins a dynamic render pass.
  ///
  /// ## Render Pass Scope
  /// Outside. Begins a render pass scope.
  ///
  /// ## Resource State
  /// The render targets specified in the dynamic render pass description must be in `VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR` layout.
  virtual void CmdBeginRendering(DynamicRenderPassDesc const& desc) = 0;

  /// Ends the current dynamic render pass.
  ///
  /// ## Render Pass Scope
  /// Inside. Ends the current render pass scope.
  virtual void CmdEndRendering() = 0;

  /// Tells the encoder that the upcoming descriptor-set / push-constant
  /// writes target this pipeline layout. Called when the user binds a
  /// render program -- before any Bind*Buffer / BindSampledImage /
  /// BindSampler calls. The actual VkPipeline is bound later via
  /// `BindRenderPipeline` at draw flush time.
  virtual void AssumeRenderPipelineLayout(
    VkPipelineLayout layout,
    VulkanDescriptorSetLayout const* descriptor_set_layouts,
    uint32_t descriptor_set_count
  ) = 0;

  /// Binds a graphics pipeline. The descriptor_set_layouts must match the
  /// pipeline layout. Same shape as `BindComputePipeline`.
  virtual void BindRenderPipeline(
    VkPipeline pipeline,
    VkPipelineLayout layout,
    VulkanDescriptorSetLayout const* descriptor_set_layouts,
    uint32_t descriptor_set_count
  ) = 0;

  /// Binds a single vertex buffer slot. Forwards directly to vkCmdBindVertexBuffers.
  virtual void CmdBindVertexBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset) = 0;

  /// Binds the index buffer. Forwards directly to vkCmdBindIndexBuffer.
  virtual void CmdBindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType index_type) = 0;

  // ----------------------------------------------------------------------------------------------
  // Dynamic State (Vulkan 1.0 viewport/scissor)
  //

  virtual void CmdSetViewport(VkViewport const& viewport) = 0;
  virtual void CmdSetScissor(VkRect2D const& scissor) = 0;

  // ----------------------------------------------------------------------------------------------
  // Draw
  //

  virtual void CmdDraw(
    uint32_t vertex_count, uint32_t instance_count,
    uint32_t first_vertex, uint32_t first_instance
  ) = 0;

  virtual void CmdDrawIndexed(
    uint32_t index_count, uint32_t instance_count,
    uint32_t first_index, int32_t vertex_offset, uint32_t first_instance
  ) = 0;

  // ----------------------------------------------------------------------------------------------
  // Compute Pipeline
  //

  virtual void BindComputePipeline(
    VkPipeline pipeline,
    VkPipelineLayout layout,
    VulkanDescriptorSetLayout const* descriptor_set_layouts,
    uint32_t descriptor_set_count
  ) = 0;
  virtual void CmdDispatchCompute(uint32_t workgroup_count_x, uint32_t workgroup_count_y, uint32_t workgroup_count_z) = 0;

  // ----------------------------------------------------------------------------------------------
  // Shader Resource Binding
  //

  virtual void BindBuffer(
    uint32_t set, uint32_t binding, uint32_t array_element,
    VkDescriptorType descriptor_type, uint64_t handle_id,
    VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range
  ) = 0;

  virtual void BindSampledImage(
    uint32_t set, uint32_t binding, uint32_t array_element,
    uint64_t image_view_handle_id,
    VkImageView image_view, VkImageLayout image_layout
  ) = 0;

  virtual void BindSampler(
    uint32_t set, uint32_t binding, uint32_t array_element,
    uint64_t sampler_handle_id,
    VkSampler sampler
  ) = 0;

protected:
  ICommandEncoder() = default;
};

} // namespace mnexus_backend::vulkan
