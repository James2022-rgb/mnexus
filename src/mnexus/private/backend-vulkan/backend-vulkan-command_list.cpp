// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-command_list.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "impl/impl_macros.h"

#include "backend-vulkan/device/vk-device.h"
#include "backend-vulkan/resource/resource_storage.h"
#include "backend-vulkan/resource/types_bridge.h"

namespace mnexus_backend::vulkan {

namespace {

VkCommandPool CreateCommandPool(IVulkanDevice* vk_device) {
  uint32_t const queue_family_index = vk_device->queue_selection().present_capable.queue_family_index;
  VkCommandPoolCreateInfo pool_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
    .queueFamilyIndex = queue_family_index,
  };
  VkCommandPool vk_pool = VK_NULL_HANDLE;
  VkResult const result = vkCreateCommandPool(vk_device->handle(), &pool_info, nullptr, &vk_pool);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateCommandPool failed: {}", string_VkResult(result));
  }
  return vk_pool;
}

VkCommandBuffer AllocateAndBeginCommandBuffer(IVulkanDevice* vk_device, VkCommandPool vk_pool) {
  VkCommandBufferAllocateInfo alloc_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,
    .commandPool = vk_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  VkCommandBuffer vk_cb = VK_NULL_HANDLE;
  VkResult const alloc_result = vkAllocateCommandBuffers(vk_device->handle(), &alloc_info, &vk_cb);
  if (alloc_result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkAllocateCommandBuffers failed: {}", string_VkResult(alloc_result));
    return VK_NULL_HANDLE;
  }

  VkCommandBufferBeginInfo begin_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = nullptr,
  };
  VkResult const begin_result = vkBeginCommandBuffer(vk_cb, &begin_info);
  if (begin_result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkBeginCommandBuffer failed: {}", string_VkResult(begin_result));
  }
  return vk_cb;
}

VulkanCommandPool MakeVulkanCommandPool(IVulkanDevice* vk_device, VkCommandPool vk_pool) {
  VkDevice const vk_device_handle = vk_device->handle();
  return VulkanCommandPool(
    vk_pool,
    [vk_device_handle, vk_pool] { vkDestroyCommandPool(vk_device_handle, vk_pool, nullptr); },
    vk_device->GetDeferredDestroyer()
  );
}

} // namespace

MnexusCommandListVulkan::MnexusCommandListVulkan(
  IVulkanDevice* vk_device,
  IDescriptorSetAllocator* ds_allocator,
  ResourceStorage* resource_storage
) :
  vk_command_pool_(MakeVulkanCommandPool(vk_device, CreateCommandPool(vk_device))),
  encoder_(AllocateAndBeginCommandBuffer(vk_device, vk_command_pool_.handle()), vk_device, ds_allocator, resource_storage),
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
  pending_pipeline_barrier_.FlushAndClear(encoder_.vk_cb_handle());

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
  mnexus::container::ArrayProxy<char const> name, float const* color
) {
  if (vkCmdBeginDebugUtilsLabelEXT != nullptr) {
    // `VkDebugUtilsLabelEXT` expects a null-terminated string, but `name` may not be null-terminated. Create a temporary null-terminated string for this call.
    std::string name_str(name.data(), name.size());

    VkDebugUtilsLabelEXT label_info{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = nullptr,
      .pLabelName = name_str.c_str(),
      .color = { 0.f, 0.f, 0.f, 0.f }
    };

    std::array<float, 4> default_color = { 0.f, 0.f, 0.f, 1.f };
    if (color != nullptr) {
      std::copy_n(color, 4, label_info.color);
    } else {
      std::copy_n(default_color.data(), 4, label_info.color);
    }

    vkCmdBeginDebugUtilsLabelEXT(encoder_.vk_cb_handle(), &label_info);
  }
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::PopDebugGroup() {
  if (vkCmdEndDebugUtilsLabelEXT != nullptr) {
    vkCmdEndDebugUtilsLabelEXT(encoder_.vk_cb_handle());
  }
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

  VulkanImage const& vk_image = hot.GetVkImage();
  VkImage const vk_image_handle = vk_image.handle();
  mnexus::TextureDesc const& desc = cold.GetTextureDesc();
  VkFormat const vk_format = ToVkFormat(desc.format);

  // Register and transition target subresources to TRANSFER_DST.
  image_layout_tracker_.RegisterImage(
    vk_image_handle,
    ToVkImageUsageFlags(desc.usage, vk_format),
    vk_format,
    desc.mip_level_count,
    desc.array_layer_count
  );

  image_layout_tracker_.TransitionRangeToTransferDst(
    vk_image_handle,
    { .mip_level = subresource_range.base_mip_level, .array_layer = subresource_range.base_array_layer },
    subresource_range.mip_level_count,
    subresource_range.array_layer_count
  );

  image_layout_tracker_.FlushPendingTransitions(pending_pipeline_barrier_);
  pending_pipeline_barrier_.FlushAndClear(encoder_.vk_cb_handle());

  encoder_.CmdClearImageSubresourceRange(vk_image, subresource_range, clear_value);

  referenced_resources_.push_back(pool_handle);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::CopyBufferToTexture(
  mnexus::BufferHandle src_buffer_handle,
  uint32_t src_buffer_offset,
  mnexus::TextureHandle dst_texture_handle,
  mnexus::TextureSubresourceRange const& dst_subresource_range,
  mnexus::Extent3d const& copy_extent
) {
  MBASE_ASSERT(dst_subresource_range.mip_level_count == 1);

  // Resolve source buffer.
  auto const src_pool_handle = resource_pool::ResourceHandle::FromU64(src_buffer_handle.Get());
  auto [src_hot, src_lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(src_pool_handle);

  // Resolve destination texture.
  auto const dst_pool_handle = resource_pool::ResourceHandle::FromU64(dst_texture_handle.Get());
  auto [dst_hot, dst_cold, dst_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(dst_pool_handle);

  VulkanImage const& vk_image = dst_hot.GetVkImage();
  VkImage const vk_image_handle = vk_image.handle();
  mnexus::TextureDesc const& dst_desc = dst_cold.GetTextureDesc();
  VkFormat const vk_format = ToVkFormat(dst_desc.format);

  // Register the image and transition the target subresource to TRANSFER_DST_OPTIMAL.
  image_layout_tracker_.RegisterImage(
    vk_image_handle,
    ToVkImageUsageFlags(dst_desc.usage, vk_format),
    vk_format,
    dst_desc.mip_level_count,
    dst_desc.array_layer_count
  );

  image_layout_tracker_.TransitionRangeToTransferDst(
    vk_image_handle,
    { .mip_level = dst_subresource_range.base_mip_level, .array_layer = dst_subresource_range.base_array_layer },
    dst_subresource_range.mip_level_count,
    dst_subresource_range.array_layer_count
  );

  // Flush the layout transition barrier before the copy.
  image_layout_tracker_.FlushPendingTransitions(pending_pipeline_barrier_);
  pending_pipeline_barrier_.FlushAndClear(encoder_.vk_cb_handle());

  encoder_.CmdCopyBufferToImageSubresource(
    src_hot.vk_buffer,
    src_buffer_offset,
    dst_hot.GetVkImage(),
    dst_subresource_range,
    copy_extent
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

  VulkanPipelineLayoutPtr const& pipeline_layout_ref = hot.pipeline_layout_ref();

  encoder_.BindComputePipeline(
    hot.vk_compute_pipeline().handle(),
    pipeline_layout_ref->handle(),
    pipeline_layout_ref->descriptor_set_layouts.data(),
    static_cast<uint32_t>(pipeline_layout_ref->descriptor_set_layouts.size())
  );

  // Track referenced resources for submit-time stamping.
  referenced_resources_.push_back(pool_handle);
  referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(cold.program_handle().Get()));
  referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(cold.shader_module_handle().Get()));
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::DispatchCompute(
  uint32_t workgroup_count_x,
  uint32_t workgroup_count_y,
  uint32_t workgroup_count_z
) {
  encoder_.CmdDispatchCompute(workgroup_count_x, workgroup_count_y, workgroup_count_z);
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
  mnexus::RenderPassDesc const& desc
) {
  DynamicRenderPassDesc dyn_rp_desc{};

  for (mnexus::ColorAttachmentDesc const& attachment_desc : desc.color_attachments) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(attachment_desc.texture.Get());
    referenced_resources_.push_back(pool_handle);

    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    RenderTargetDesc& rt_desc = dyn_rp_desc.color_attachments.emplace_back();
    rt_desc.vk_image = &hot.GetVkImage();
    rt_desc.subresource_range = attachment_desc.subresource_range;
    if (attachment_desc.load_op == mnexus::LoadOp::kClear) {
      rt_desc.clear_value = RenderTargetClearValue {
        .f32 = {
          attachment_desc.clear_value.color.r,
          attachment_desc.clear_value.color.g,
          attachment_desc.clear_value.color.b,
          attachment_desc.clear_value.color.a,
        },
      };
    }
  }

  if (desc.depth_stencil_attachment != nullptr && desc.depth_stencil_attachment->texture.IsValid()) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(desc.depth_stencil_attachment->texture.Get());
    referenced_resources_.push_back(pool_handle);

    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    RenderTargetDesc& rt_desc = dyn_rp_desc.depth_stencil_attachment.emplace();
    rt_desc.vk_image = &hot.GetVkImage();
    rt_desc.subresource_range = desc.depth_stencil_attachment->subresource_range;
    if (desc.depth_stencil_attachment->depth_load_op == mnexus::LoadOp::kClear) {
      rt_desc.clear_value = RenderTargetClearValue {
        .f32 = {
          desc.depth_stencil_attachment->depth_clear_value,
        },
      };
    }
    if (desc.depth_stencil_attachment->stencil_load_op == mnexus::LoadOp::kClear) {
      rt_desc.stencil_clear_value = RenderTargetClearValue {
          .u32 = {
            desc.depth_stencil_attachment->stencil_clear_value,
          },
      };
    }
  }

  encoder_.CmdBeginRendering(dyn_rp_desc);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::EndRenderPass() {
  encoder_.CmdEndRendering();
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
