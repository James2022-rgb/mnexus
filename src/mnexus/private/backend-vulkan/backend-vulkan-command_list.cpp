// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-command_list.h"

// project headers --------------------------------------
#include "impl/impl_macros.h"

#include "backend-vulkan/resource_storage.h"
#include "backend-vulkan/types_bridge.h"

namespace mnexus_backend::vulkan {

MnexusCommandListVulkan::MnexusCommandListVulkan(
  CommandEncoder encoder,
  ResourceStorage* resource_storage
) :
  encoder_(std::move(encoder)),
  resource_storage_(resource_storage)
{
}

// --------------------------------------------------------------------------------------------------
// mnexus::ICommandList implementation
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::End() {
  // Transition all tracked images back to their default layouts before finalizing.
  image_layout_tracker_.TransitionAllToDefaults();
  image_layout_tracker_.FlushPendingTransitions(pending_pipeline_barrier_);
  pending_pipeline_barrier_.FlushAndClear(encoder_.command_buffer());

  encoder_.End();
}

//
// Diagnostics
//

MNEXUS_NO_THROW mnexus::RenderStateEventLog& MNEXUS_CALL MnexusCommandListVulkan::GetStateEventLog() {
  return render_state_event_log_;
}

//
// Debug Markers
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::PushDebugGroup(
  mnexus::container::ArrayProxy<char const> /*name*/, float const* /*color*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::PopDebugGroup() {
  STUB_NOT_IMPLEMENTED();
}

//
// Transfer
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::ClearTexture(
  mnexus::TextureHandle texture_handle,
  mnexus::TextureSubresourceRange const& subresource_range,
  mnexus::ClearValue const& clear_value
) {
  auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
  auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

  VkImage const vk_image = hot.GetVkImage().handle();
  mnexus::TextureDesc const& desc = cold.GetTextureDesc();
  VkFormat const vk_format = ToVkFormat(desc.format);

  // Register and transition target subresources to TRANSFER_DST.
  image_layout_tracker_.RegisterImage(
    vk_image,
    ToVkImageUsageFlags(desc.usage, vk_format),
    vk_format,
    desc.mip_level_count,
    desc.array_layer_count
  );

  for (uint32_t mip = subresource_range.base_mip_level;
       mip < subresource_range.base_mip_level + subresource_range.mip_level_count;
       ++mip) {
    for (uint32_t layer = subresource_range.base_array_layer;
         layer < subresource_range.base_array_layer + subresource_range.array_layer_count;
         ++layer) {
      image_layout_tracker_.TransitionToTransferDst(
        vk_image, { .mip_level = mip, .array_layer = layer }
      );
    }
  }

  image_layout_tracker_.FlushPendingTransitions(pending_pipeline_barrier_);
  pending_pipeline_barrier_.FlushAndClear(encoder_.command_buffer());

  VkImageSubresourceRange const vk_range = ToVkImageSubresourceRange(subresource_range);

  VkImageAspectFlags const aspect = ImageLayoutTracker::GetAspectMaskFromFormat(vk_format);
  if (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
    VkClearDepthStencilValue const ds {
      .depth = clear_value.depth_stencil.depth,
      .stencil = clear_value.depth_stencil.stencil,
    };
    vkCmdClearDepthStencilImage(
      encoder_.command_buffer(), vk_image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      &ds, 1, &vk_range
    );
  } else {
    VkClearColorValue const color {
      .float32 = {
        clear_value.color.r,
        clear_value.color.g,
        clear_value.color.b,
        clear_value.color.a,
      },
    };
    vkCmdClearColorImage(
      encoder_.command_buffer(), vk_image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      &color, 1, &vk_range
    );
  }

  referenced_resources_.push_back(pool_handle);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::CopyBufferToTexture(
  mnexus::BufferHandle src_buffer_handle,
  uint32_t src_buffer_offset,
  mnexus::TextureHandle dst_texture_handle,
  mnexus::TextureSubresourceRange const& dst_subresource_range,
  mnexus::Extent3d const& copy_extent
) {
  // Resolve source buffer.
  auto const src_pool_handle = resource_pool::ResourceHandle::FromU64(src_buffer_handle.Get());
  auto [src_hot, src_lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(src_pool_handle);

  // Resolve destination texture.
  auto const dst_pool_handle = resource_pool::ResourceHandle::FromU64(dst_texture_handle.Get());
  auto [dst_hot, dst_cold, dst_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(dst_pool_handle);

  VkImage const vk_image = dst_hot.GetVkImage().handle();
  mnexus::TextureDesc const& dst_desc = dst_cold.GetTextureDesc();
  VkFormat const vk_format = ToVkFormat(dst_desc.format);

  // Register the image and transition the target subresource to TRANSFER_DST_OPTIMAL.
  image_layout_tracker_.RegisterImage(
    vk_image,
    ToVkImageUsageFlags(dst_desc.usage, vk_format),
    vk_format,
    dst_desc.mip_level_count,
    dst_desc.array_layer_count
  );

  // Transition each target subresource (mip level × array layer) to transfer dst.
  for (uint32_t layer = dst_subresource_range.base_array_layer;
       layer < dst_subresource_range.base_array_layer + dst_subresource_range.array_layer_count;
       ++layer) {
    image_layout_tracker_.TransitionToTransferDst(
      vk_image,
      { .mip_level = dst_subresource_range.base_mip_level, .array_layer = layer }
    );
  }

  // Flush the layout transition barrier before the copy.
  image_layout_tracker_.FlushPendingTransitions(pending_pipeline_barrier_);
  pending_pipeline_barrier_.FlushAndClear(encoder_.command_buffer());

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
    encoder_.command_buffer(),
    src_hot.vk_buffer.handle(),
    vk_image,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &region
  );

  // Track referenced resources for submit-time stamping.
  referenced_resources_.push_back(src_pool_handle);
  referenced_resources_.push_back(dst_pool_handle);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::CopyTextureToBuffer(
  mnexus::TextureHandle /*src_texture_handle*/,
  mnexus::TextureSubresourceRange const& /*src_subresource_range*/,
  mnexus::BufferHandle /*dst_buffer_handle*/,
  uint32_t /*dst_buffer_offset*/,
  mnexus::Extent3d const& /*copy_extent*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BlitTexture(
  mnexus::TextureHandle /*src_texture_handle*/,
  mnexus::TextureSubresourceRange const& /*src_subresource_range*/,
  mnexus::Offset3d const& /*src_offset*/,
  mnexus::Extent3d const& /*src_extent*/,
  mnexus::TextureHandle /*dst_texture_handle*/,
  mnexus::TextureSubresourceRange const& /*dst_subresource_range*/,
  mnexus::Offset3d const& /*dst_offset*/,
  mnexus::Extent3d const& /*dst_extent*/,
  mnexus::Filter /*filter*/
) {
  STUB_NOT_IMPLEMENTED();
}

//
// Compute
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindExplicitComputePipeline(
  mnexus::ComputePipelineHandle compute_pipeline_handle
) {
  auto const pool_handle = resource_pool::ResourceHandle::FromU64(compute_pipeline_handle.Get());
  auto [hot, cold, lock] = resource_storage_->compute_pipelines.GetConstRefWithSharedLockGuard(pool_handle);
  encoder_.BindComputePipeline(
    hot.vk_compute_pipeline.handle(),
    hot.vk_pipeline_layout,
    hot.pipeline_layout_ref->descriptor_set_layouts.data(),
    static_cast<uint32_t>(hot.pipeline_layout_ref->descriptor_set_layouts.size())
  );

  // Track referenced resources for submit-time stamping.
  referenced_resources_.push_back(pool_handle);
  referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(cold.program_handle.Get()));
  referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(cold.shader_module_handle.Get()));
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::DispatchCompute(
  uint32_t workgroup_count_x,
  uint32_t workgroup_count_y,
  uint32_t workgroup_count_z
) {
  encoder_.DispatchCompute(workgroup_count_x, workgroup_count_y, workgroup_count_z);
}

//
// Resource Binding
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindUniformBuffer(
  mnexus::BindingId const& id,
  mnexus::BufferHandle buffer_handle,
  uint64_t offset,
  uint64_t size
) {
  auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
  auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);
  encoder_.BindBuffer(
    id.group, id.binding, id.array_element,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    buffer_handle.Get(), hot.vk_buffer.handle(), offset, size
  );
  referenced_resources_.push_back(pool_handle);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindStorageBuffer(
  mnexus::BindingId const& id,
  mnexus::BufferHandle buffer_handle,
  uint64_t offset,
  uint64_t size
) {
  auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
  auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);
  encoder_.BindBuffer(
    id.group, id.binding, id.array_element,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    buffer_handle.Get(), hot.vk_buffer.handle(), offset, size
  );
  referenced_resources_.push_back(pool_handle);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindSampledTexture(
  mnexus::BindingId const& /*id*/,
  mnexus::TextureHandle /*texture_handle*/,
  mnexus::TextureSubresourceRange const& /*subresource_range*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindSampler(
  mnexus::BindingId const& /*id*/,
  mnexus::SamplerHandle /*sampler_handle*/
) {
  STUB_NOT_IMPLEMENTED();
}

//
// Explicit Pipeline Binding
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindExplicitRenderPipeline(
  mnexus::RenderPipelineHandle /*render_pipeline_handle*/
) {
  STUB_NOT_IMPLEMENTED();
}

//
// Render Pass
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BeginRenderPass(
  mnexus::RenderPassDesc const& /*desc*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::EndRenderPass() {
  STUB_NOT_IMPLEMENTED();
}

//
// Render State (auto-generation path)
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindRenderProgram(
  mnexus::ProgramHandle /*program_handle*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetVertexInputLayout(
  mnexus::container::ArrayProxy<mnexus::VertexInputBindingDesc const> /*bindings*/,
  mnexus::container::ArrayProxy<mnexus::VertexInputAttributeDesc const> /*attributes*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindVertexBuffer(
  uint32_t /*binding*/,
  mnexus::BufferHandle /*buffer_handle*/,
  uint64_t /*offset*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindIndexBuffer(
  mnexus::BufferHandle /*buffer_handle*/,
  uint64_t /*offset*/,
  mnexus::IndexType /*index_type*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetPrimitiveTopology(
  mnexus::PrimitiveTopology /*topology*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetPolygonMode(
  mnexus::PolygonMode /*mode*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetCullMode(
  mnexus::CullMode /*cull_mode*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetFrontFace(
  mnexus::FrontFace /*front_face*/
) {
  STUB_NOT_IMPLEMENTED();
}

// Depth

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetDepthTestEnabled(bool /*enabled*/) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetDepthWriteEnabled(bool /*enabled*/) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetDepthCompareOp(
  mnexus::CompareOp /*op*/
) {
  STUB_NOT_IMPLEMENTED();
}

// Stencil

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetStencilTestEnabled(bool /*enabled*/) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetStencilFrontOps(
  mnexus::StencilOp /*fail*/, mnexus::StencilOp /*pass*/,
  mnexus::StencilOp /*depth_fail*/, mnexus::CompareOp /*compare*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetStencilBackOps(
  mnexus::StencilOp /*fail*/, mnexus::StencilOp /*pass*/,
  mnexus::StencilOp /*depth_fail*/, mnexus::CompareOp /*compare*/
) {
  STUB_NOT_IMPLEMENTED();
}

// Per-attachment blend

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetBlendEnabled(
  uint32_t /*attachment*/, bool /*enabled*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetBlendFactors(
  uint32_t /*attachment*/,
  mnexus::BlendFactor /*src_color*/, mnexus::BlendFactor /*dst_color*/, mnexus::BlendOp /*color_op*/,
  mnexus::BlendFactor /*src_alpha*/, mnexus::BlendFactor /*dst_alpha*/, mnexus::BlendOp /*alpha_op*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetColorWriteMask(
  uint32_t /*attachment*/, mnexus::ColorWriteMask /*mask*/
) {
  STUB_NOT_IMPLEMENTED();
}

//
// Draw
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::Draw(
  uint32_t /*vertex_count*/, uint32_t /*instance_count*/,
  uint32_t /*first_vertex*/, uint32_t /*first_instance*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::DrawIndexed(
  uint32_t /*index_count*/, uint32_t /*instance_count*/,
  uint32_t /*first_index*/, int32_t /*vertex_offset*/, uint32_t /*first_instance*/
) {
  STUB_NOT_IMPLEMENTED();
}

//
// Viewport / Scissor
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetViewport(
  float /*x*/, float /*y*/, float /*width*/, float /*height*/,
  float /*min_depth*/, float /*max_depth*/
) {
  STUB_NOT_IMPLEMENTED();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetScissor(
  int32_t /*x*/, int32_t /*y*/, uint32_t /*width*/, uint32_t /*height*/
) {
  STUB_NOT_IMPLEMENTED();
}

} // namespace mnexus_backend::vulkan
