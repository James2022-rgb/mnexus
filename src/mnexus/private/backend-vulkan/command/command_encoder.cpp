// TU header --------------------------------------------
#include "backend-vulkan/command/command_encoder.h"

// c++ headers ------------------------------------------
#include <variant>

// public project headers -------------------------------
#include "mbase/public/assert.h"

// project headers --------------------------------------
#include "backend-vulkan/object/vk-object-image.h"
#include "backend-vulkan/resource/types_bridge.h"
#include "backend-vulkan/resource/resource_storage.h"

#include "impl/impl_macros.h"

namespace mnexus_backend::vulkan {

namespace {

} // namespace

CommandEncoder::CommandEncoder(
  VkCommandBuffer vk_cb_handle,
  IVulkanDevice const* vk_device,
  IDescriptorSetAllocator* ds_allocator,
  ResourceStorage* resource_storage
) :
  vk_cb_handle_(vk_cb_handle),
  vk_device_(vk_device),
  ds_allocator_(ds_allocator),
  resource_storage_(resource_storage)
{
  MBASE_ASSERT(ds_allocator_ != nullptr);
}

void CommandEncoder::End() {
  vkEndCommandBuffer(vk_cb_handle_);
}

void CommandEncoder::CmdClearImageSubresourceRange(
  VulkanImage const& vk_image,
  mnexus::TextureSubresourceRange const& subresource_range,
  mnexus::ClearValue const& clear_value
) {
  constexpr VkImageLayout kExpectedLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

  VkImage const vk_image_handle = vk_image.handle();
  VkImageSubresourceRange const vk_range = ToVkImageSubresourceRange(subresource_range);

  VkImageAspectFlags const aspect = vk_range.aspectMask;
  if (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
    VkClearDepthStencilValue const ds{
      .depth = clear_value.depth_stencil.depth,
      .stencil = clear_value.depth_stencil.stencil,
    };
    vkCmdClearDepthStencilImage(
      vk_cb_handle_,
      vk_image_handle,
      kExpectedLayout,
      &ds, 1, &vk_range
    );
  }
  else {
    VkClearColorValue const color{
      .float32 = {
        clear_value.color.r,
        clear_value.color.g,
        clear_value.color.b,
        clear_value.color.a,
      },
    };
    vkCmdClearColorImage(
      vk_cb_handle_,
      vk_image_handle,
      kExpectedLayout,
      &color, 1, &vk_range
    );
  }
}

void CommandEncoder::CmdCopyBufferToImageSubresource(
  VulkanBuffer const& src_vk_buffer,
  uint32_t src_buffer_offset,
  VulkanImage const& dst_vk_image,
  mnexus::TextureSubresourceRange const& dst_subresource_range,
  mnexus::Extent3d const& copy_extent
) {
  constexpr VkImageLayout kExpectedLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

  // Build copy region. Vulkan supports tightly packed source data natively (bufferRowLength = 0).
  VkBufferImageCopy const region {
    .bufferOffset = src_buffer_offset,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource = ToVkImageSubresourceLayers(dst_subresource_range),
    .imageOffset = { 0, 0, 0 },
    .imageExtent = VkExtent3D { copy_extent.width, copy_extent.height, copy_extent.depth },
  };

  vkCmdCopyBufferToImage(
    vk_cb_handle_,
    src_vk_buffer.handle(),
    dst_vk_image.handle(),
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &region
  );
}

void CommandEncoder::CmdBeginRendering(DynamicRenderPassDesc const& desc) {
  // TODO: Image view type should be configurable.
  constexpr VkImageViewType kImageViewType = VK_IMAGE_VIEW_TYPE_2D;
  constexpr uint32_t kLayerCount = 1;

  constexpr VkAttachmentStoreOp kStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

  constexpr VkImageLayout kExpectedLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;

  auto make_vk_image_view = [this](ImageViewCacheKey const& key) {
    VkImageViewCreateInfo const create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = key.vk_image,
      .viewType = key.view_type,
      .format = key.format,
      .components = VkComponentMapping {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
      .subresourceRange = key.subresource_range,
    };

    VkImageView vk_image_view_handle = VK_NULL_HANDLE;
    VkResult const result = vkCreateImageView(this->vk_device_->handle(), &create_info, nullptr, &vk_image_view_handle);
    MBASE_ASSERT(result == VK_SUCCESS);

    return std::make_shared<VulkanImageView>(
      vk_image_view_handle,
      [vk_device = this->vk_device_, vk_image_view_handle]() {
        vkDestroyImageView(vk_device->handle(), vk_image_view_handle, nullptr);
      },
      this->vk_device_->GetDeferredDestroyer()
    );
  };

  auto to_vk_color_value = [](RenderTargetClearValue const& clear_value, VkFormat vk_format) -> VkClearColorValue {
    VkClearColorValue clear_color_value {};
    if (vkuFormatIsSampledFloat(vk_format)) {
      std::memcpy(std::addressof(clear_color_value.float32), std::addressof(clear_value.f32), sizeof(clear_color_value.float32));
    }
    else if (vkuFormatIsUINT(vk_format)) {
      std::memcpy(std::addressof(clear_color_value.uint32), std::addressof(clear_value.u32), sizeof(clear_color_value.uint32));
    }
    else if (vkuFormatIsSINT(vk_format)) {
      std::memcpy(std::addressof(clear_color_value.int32), std::addressof(clear_value.i32), sizeof(clear_color_value.int32));
    }
    return clear_color_value;
  };

  gfx_state_.render_area = VkExtent2D {
    .width = 0,
    .height = 0,
  };

  mbase::SmallVector<VkRenderingAttachmentInfoKHR, 1> color_attachments;
  for (RenderTargetDesc const& attachment : desc.color_attachments) {
    VulkanImage const* vk_image = attachment.vk_image;

    VkExtent3D const& extent = vk_image->extent();
    gfx_state_.render_area.width  = std::max(gfx_state_.render_area.width, extent.width);
    gfx_state_.render_area.height = std::max(gfx_state_.render_area.height, extent.height);

    VkImage const vk_image_handle = vk_image->handle();
    VkFormat const vk_format = vk_image->vk_format();
    VkImageSubresourceRange const vk_image_subresource_range = ToVkImageSubresourceRange(attachment.subresource_range);

    // FIXME: We probably should take a `shared_ptr<VulkanImage>` ...
    VulkanImageViewPtr vk_image_view = resource_storage_->image_view_cache.FindOrInsert(
      ImageViewCacheKey {
        .vk_image = vk_image_handle,
        .view_type = kImageViewType,
        .format = vk_format,
        .subresource_range = vk_image_subresource_range,
      },
      // SAFETY: Executed immediately.
      make_vk_image_view
    );

    VkClearColorValue clear_color_value = to_vk_color_value(attachment.clear_value.value_or(RenderTargetClearValue{}), vk_format);

    color_attachments.emplace_back(
      VkRenderingAttachmentInfoKHR {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .pNext = nullptr,
        .imageView = vk_image_view->handle(),
        .imageLayout = kExpectedLayout,
        .resolveMode = VK_RESOLVE_MODE_NONE_KHR,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = attachment.clear_value.has_value() ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = kStoreOp,
        .clearValue = VkClearValue { .color = clear_color_value },
      }
    );
  }

  bool has_stencil = false;
  std::optional<VkRenderingAttachmentInfoKHR> depth_stencil_attachment;
  if (desc.depth_stencil_attachment.has_value()) {
    VulkanImage const* vk_image = desc.depth_stencil_attachment->vk_image;

    VkExtent3D const& extent = vk_image->extent();
    gfx_state_.render_area.width  = std::max(gfx_state_.render_area.width, extent.width);
    gfx_state_.render_area.height = std::max(gfx_state_.render_area.height, extent.height);

    VkImage const vk_image_handle = vk_image->handle();
    VkFormat const vk_format = vk_image->vk_format();
    VkImageSubresourceRange const vk_image_subresource_range = ToVkImageSubresourceRange(desc.depth_stencil_attachment->subresource_range);

    // FIXME: We probably should take a `shared_ptr<VulkanImage>` ...
    VulkanImageViewPtr vk_image_view = resource_storage_->image_view_cache.FindOrInsert(
      ImageViewCacheKey {
        .vk_image = vk_image_handle,
        .view_type = kImageViewType,
        .format = vk_format,
        .subresource_range = vk_image_subresource_range,
      },
      // SAFETY: Executed immediately.
      make_vk_image_view
    );

    RenderTargetDesc const& attachment = desc.depth_stencil_attachment.value();

    VkClearDepthStencilValue clear_depth_stencil_value {};
    if (attachment.clear_value.has_value()) {
      clear_depth_stencil_value.depth = attachment.clear_value->f32[0];
      clear_depth_stencil_value.stencil = attachment.clear_value->u32[0];
    }

    depth_stencil_attachment = VkRenderingAttachmentInfoKHR {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .pNext = nullptr,
      .imageView = vk_image_view->handle(),
      .imageLayout = kExpectedLayout,
      .resolveMode = VK_RESOLVE_MODE_NONE_KHR,
      .resolveImageView = VK_NULL_HANDLE,
      .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .loadOp = attachment.clear_value.has_value() ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = kStoreOp,
      .clearValue = VkClearValue { .depthStencil = clear_depth_stencil_value },
    };
  }

  VkRenderingInfoKHR const rendering_info {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = VkRect2D {
      .offset = VkOffset2D {.x = 0, .y = 0 },
      .extent = gfx_state_.render_area,
    },
    .layerCount = kLayerCount,
    .viewMask = desc.view_mask,
    .colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
    .pColorAttachments = color_attachments.data(),
    .pDepthAttachment = depth_stencil_attachment.has_value() ? &depth_stencil_attachment.value() : nullptr,
    .pStencilAttachment = has_stencil ? &depth_stencil_attachment.value() : nullptr,
  };

  vkCmdBeginRenderingKHR(vk_cb_handle_, &rendering_info);

  gfx_state_.rp_state = desc;
  gfx_state_.viewport = VkViewport {
    .x = 0.0f,
    .y = 0.0f,
    .width = static_cast<float>(gfx_state_.render_area.width),
    .height = static_cast<float>(gfx_state_.render_area.height),
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };
  gfx_state_.scissor = VkRect2D {
    .offset = VkOffset2D {.x = 0, .y = 0 },
    .extent = gfx_state_.render_area,
  };

  dirty_flags_ |= CeDirtyFlagBits::kGfxRenderPass | CeDirtyFlagBits::kGfxDynamicStateBits;
}

void CommandEncoder::CmdEndRendering() {
  vkCmdEndRenderingKHR(vk_cb_handle_);

  gfx_state_.rp_state = std::monostate{};
}

// ----------------------------------------------------------------------------------------------
// Compute Pipeline
//

void CommandEncoder::BindComputePipeline(
  VkPipeline pipeline,
  VkPipelineLayout layout,
  VulkanDescriptorSetLayout const* descriptor_set_layouts,
  uint32_t descriptor_set_count
) {
  current_compute_pipeline_ = pipeline;
  current_pipeline_layout_ = layout;

  vkCmdBindPipeline(vk_cb_handle_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

  descriptor_set_binder_.AssumePipelineLayout(layout, descriptor_set_layouts, descriptor_set_count);
}

void CommandEncoder::CmdDispatchCompute(uint32_t workgroup_count_x, uint32_t workgroup_count_y, uint32_t workgroup_count_z) {
  this->ResolveDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE);

  vkCmdDispatch(vk_cb_handle_, workgroup_count_x, workgroup_count_y, workgroup_count_z);
}

// ----------------------------------------------------------------------------------------------
// Shader Resource Binding
//

void CommandEncoder::BindBuffer(
  uint32_t set, uint32_t binding, uint32_t array_element,
  VkDescriptorType descriptor_type, uint64_t handle_id,
  VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range
) {
  descriptor_set_binder_.SetBuffer(set, binding, array_element, descriptor_type, handle_id, buffer, offset, range);
}

//
// Private
//

void CommandEncoder::ResolveDescriptorSets(VkPipelineBindPoint vk_bind_point) {
  descriptor_set_binder_.CmdBindDescriptorSets(vk_cb_handle_, vk_bind_point, vk_device_->handle(), ds_allocator_);
}

} // namespace mnexus_backend::vulkan
