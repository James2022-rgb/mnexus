#pragma once

// c++ headers ------------------------------------------
#include <array>
#include <variant>

// public project headers -------------------------------
#include "mbase/public/accessor.h"
#include "mbase/public/container.h"
#include "mbase/public/bitflags.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/descriptor/descriptor_set_binder.h"

#include "backend-vulkan/descriptor/fwd.h"
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
// CommandEncoder
//
// Wraps a VkCommandBuffer and tracks Vulkan-side recording state.
//

enum CeDirtyFlagBits : uint32_t {
  kGfxRenderPass = 1 << 0,

  kGfxPipelineBits = kGfxRenderPass,

  kGfxDynamicStateViewport  = 1 << 4,
  kGfxDynamicStateScissor   = 1 << 5,
  kGfxDynamicStateDepthBias = 1 << 6,
  kGfxDynamicStateStencil   = 1 << 7,

  kGfxDynamicStateBits = kGfxDynamicStateViewport | kGfxDynamicStateScissor | kGfxDynamicStateDepthBias | kGfxDynamicStateStencil,

  kAll = kGfxPipelineBits,
};
MBASE_DEFINE_ENUM_CLASS_BITFLAGS_OPERATORS(CeDirtyFlagBits);
using CeDirtyFlags = mbase::BitFlags<CeDirtyFlagBits>;

class CommandEncoder final {
public:
  CommandEncoder() = default;
  explicit CommandEncoder(
    VkCommandBuffer vk_cb_handle,
    IVulkanDevice const* vk_device,
    IDescriptorSetAllocator* ds_allocator,
    ResourceStorage* resource_storage
  );

  MBASE_ACCESSOR_GETV(VkCommandBuffer, vk_cb_handle);

  void End();

  /// Clears the specified subresource range of the given image to the given value.
  ///
  /// ## Render Pass Scope
  /// Outside.
  ///
  /// ## Resource State
  /// The subresource range MUST be in `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` layout.
  void CmdClearImageSubresourceRange(
    VulkanImage const& vk_image,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ClearValue const& clear_value
  );

  /// Copies data from a buffer to the specified subresource range of the given image.
  ///
  /// ## Render Pass Scope
  /// Outside.
  /// 
  /// ## Resource State
  /// The destination subresource range MUST be in `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` layout.
  void CmdCopyBufferToImageSubresource(
    VulkanBuffer const& src_vk_buffer,
    uint32_t src_buffer_offset,
    VulkanImage const& dst_vk_image,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Extent3d const& copy_extent
  );

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
  void CmdBeginRendering(DynamicRenderPassDesc const& desc);

  /// Ends the current dynamic render pass.
  ///
  /// ## Render Pass Scope
  /// Inside. Ends the current render pass scope.
  void CmdEndRendering();

  // ----------------------------------------------------------------------------------------------
  // Compute Pipeline
  //

  void BindComputePipeline(VkPipeline pipeline, VkPipelineLayout layout,
                           VulkanDescriptorSetLayout const* descriptor_set_layouts, uint32_t descriptor_set_count);
  void CmdDispatchCompute(uint32_t workgroup_count_x, uint32_t workgroup_count_y, uint32_t workgroup_count_z);

  // ----------------------------------------------------------------------------------------------
  // Shader Resource Binding
  //

  void BindBuffer(uint32_t set, uint32_t binding, uint32_t array_element,
                  VkDescriptorType descriptor_type, uint64_t handle_id,
                  VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

private:
  void ResolveDescriptorSets(VkPipelineBindPoint vk_bind_point);

  VkCommandBuffer vk_cb_handle_ = VK_NULL_HANDLE;
  IVulkanDevice const* vk_device_ = VK_NULL_HANDLE;
  IDescriptorSetAllocator* ds_allocator_ = nullptr;
  ResourceStorage* resource_storage_ = nullptr;

  struct GfxState final {
    std::variant<std::monostate, DynamicRenderPassDesc> rp_state;
    VkExtent2D render_area{};
    VkViewport viewport{};
    VkRect2D scissor{};
  } gfx_state_;

  VkPipeline current_compute_pipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout current_pipeline_layout_ = VK_NULL_HANDLE;

  DescriptorSetBinder descriptor_set_binder_;

  CeDirtyFlags dirty_flags_ = CeDirtyFlagBits::kAll;
};

} // namespace mnexus_backend::vulkan
