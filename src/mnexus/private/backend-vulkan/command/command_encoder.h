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

protected:
  ICommandEncoder() = default;
};

} // namespace mnexus_backend::vulkan
