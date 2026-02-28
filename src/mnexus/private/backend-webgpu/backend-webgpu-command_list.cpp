// TU header --------------------------------------------
#include "backend-webgpu/backend-webgpu-command_list.h"

// public project headers -------------------------------
#include "mbase/public/assert.h"

// project headers --------------------------------------
#include "backend-webgpu/backend-webgpu-binding.h"
#include "backend-webgpu/blit_texture.h"
#include "backend-webgpu/buffer_row_repack.h"
#include "backend-webgpu/types_bridge.h"

namespace mnexus_backend::webgpu {

MnexusCommandListWebGpu::MnexusCommandListWebGpu(
  ResourceStorage* resource_storage,
  wgpu::Device wgpu_device,
  wgpu::CommandEncoder wgpu_command_encoder
) :
  resource_storage_(resource_storage),
  wgpu_device_(std::move(wgpu_device)),
  wgpu_command_encoder_(std::move(wgpu_command_encoder))
{
  render_pipeline_state_tracker_.SetEventLog(&render_state_event_log_);
}

// --------------------------------------------------------------------------------------------------
// mnexus::ICommandList implementation
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::End() {
  MBASE_ASSERT_MSG(!current_render_pass_.has_value(),
    "Active render pass must be ended via EndRenderPass before calling End");
  this->EndCurrentComputePass();
}

//
// Diagnostics
//

MNEXUS_NO_THROW mnexus::RenderStateEventLog& MNEXUS_CALL MnexusCommandListWebGpu::GetStateEventLog() {
  return render_state_event_log_;
}

//
// Transfer
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::ClearTexture(
  mnexus::TextureHandle texture_handle,
  mnexus::TextureSubresourceRange const& subresource_range,
  mnexus::ClearValue const& clear_value
) {
  // TODO: We're assuming only color aspect for now.
  constexpr wgpu::TextureAspect kSupportedAspects = wgpu::TextureAspect::All;

  auto pool_handle = container::ResourceHandle::FromU64(texture_handle.Get());
  auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

  // Swapchain texture hot handle can be null if not acquired this frame.
  if (!hot.wgpu_texture) {
    return;
  }

  wgpu::TextureFormat const wgpu_texture_format = ToWgpuTextureFormat(cold.desc.format);

  wgpu::TextureViewDescriptor view_desc = MakeWgpuTextureViewDesc(
    wgpu_texture_format,
    wgpu::TextureViewDimension::e2D,
    subresource_range,
    kSupportedAspects
  );

  wgpu::TextureView view = hot.wgpu_texture.CreateView(&view_desc);

  wgpu::RenderPassColorAttachment attachment {
    .view = view,
    .resolveTarget = nullptr,
    .loadOp = wgpu::LoadOp::Clear,
    .storeOp = wgpu::StoreOp::Store,
    .clearValue = {
      clear_value.color.r,
      clear_value.color.g,
      clear_value.color.b,
      clear_value.color.a,
    },
  };

  wgpu::RenderPassDescriptor pass_desc {
    .colorAttachmentCount = 1,
    .colorAttachments = &attachment,
    .depthStencilAttachment = nullptr,
  };

  wgpu::RenderPassEncoder pass = wgpu_command_encoder_.BeginRenderPass(&pass_desc);
  pass.End();
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::CopyBufferToTexture(
  mnexus::BufferHandle src_buffer_handle,
  uint32_t src_buffer_offset,
  mnexus::TextureHandle dst_texture_handle,
  mnexus::TextureSubresourceRange const& dst_subresource_range,
  mnexus::Extent3d const& copy_extent
) {
  // Transfer commands must not be recorded inside any pass.
  this->EndCurrentRenderPass();
  this->EndCurrentComputePass();

  auto src_buffer_pool_handle = container::ResourceHandle::FromU64(src_buffer_handle.Get());
  auto [src_buffer_hot, src_buffer_lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(src_buffer_pool_handle);

  auto dst_texture_pool_handle = container::ResourceHandle::FromU64(dst_texture_handle.Get());
  auto [dst_texture_hot, dst_texture_cold, dst_texture_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(dst_texture_pool_handle);

  // Swapchain texture hot handle can be null if not acquired this frame.
  if (!dst_texture_hot.wgpu_texture) {
    return;
  }

  uint32_t const format_size = MnGetFormatSizeInBytes(static_cast<MnFormat>(dst_texture_cold.desc.format));
  MnExtent3d const block_extent = MnGetFormatTexelBlockExtent(static_cast<MnFormat>(dst_texture_cold.desc.format));

  // Compute bytesPerRow: number of bytes per row of texel blocks, aligned to 256.
  uint32_t const blocks_per_row = (copy_extent.width + block_extent.width - 1) / block_extent.width;
  uint32_t const bytes_per_row_unaligned = blocks_per_row * format_size;
  uint32_t const bytes_per_row_aligned = (bytes_per_row_unaligned + 255) & ~uint32_t(255);

  uint32_t const rows_per_image = (copy_extent.height + block_extent.height - 1) / block_extent.height;

  wgpu::TexelCopyTextureInfo dst {};
  dst.texture = dst_texture_hot.wgpu_texture;
  dst.mipLevel = dst_subresource_range.base_mip_level;
  dst.origin = { 0, 0, dst_subresource_range.base_array_layer };
  dst.aspect = wgpu::TextureAspect::All;

  wgpu::Extent3D wgpu_copy_size {
    copy_extent.width,
    copy_extent.height,
    copy_extent.depth,
  };

  if (bytes_per_row_unaligned == bytes_per_row_aligned) {
    // Fast path: source data is already 256-byte aligned.
    wgpu::TexelCopyBufferInfo src {};
    src.buffer = src_buffer_hot.wgpu_buffer;
    src.layout.offset = src_buffer_offset;
    src.layout.bytesPerRow = bytes_per_row_aligned;
    src.layout.rowsPerImage = rows_per_image;
    wgpu_command_encoder_.CopyBufferToTexture(&src, &dst, &wgpu_copy_size);
  } else if (bytes_per_row_unaligned % 4 == 0) {
    // Compute repack path: use internal compute shader to repack rows into an aligned temp buffer.
    wgpu::Buffer temp_buffer = buffer_row_repack::RepackRows(
      wgpu_device_,
      wgpu_command_encoder_,
      src_buffer_hot.wgpu_buffer,
      src_buffer_offset,
      bytes_per_row_unaligned,
      bytes_per_row_aligned,
      rows_per_image
    );
    wgpu::TexelCopyBufferInfo src {};
    src.buffer = temp_buffer;
    src.layout.offset = 0;
    src.layout.bytesPerRow = bytes_per_row_aligned;
    src.layout.rowsPerImage = rows_per_image;
    wgpu_command_encoder_.CopyBufferToTexture(&src, &dst, &wgpu_copy_size);
  } else {
    // Row-by-row fallback for formats where bytes_per_row is not 4-byte aligned (e.g. R8, RG8, R16).
    for (uint32_t row = 0; row < rows_per_image; ++row) {
      wgpu::TexelCopyBufferInfo src {};
      src.buffer = src_buffer_hot.wgpu_buffer;
      src.layout.offset = src_buffer_offset + row * bytes_per_row_unaligned;
      src.layout.bytesPerRow = bytes_per_row_aligned;
      src.layout.rowsPerImage = block_extent.height;

      wgpu::TexelCopyTextureInfo row_dst = dst;
      row_dst.origin.y = row * block_extent.height;

      wgpu::Extent3D row_copy_size {
        copy_extent.width,
        block_extent.height,
        copy_extent.depth,
      };
      wgpu_command_encoder_.CopyBufferToTexture(&src, &row_dst, &row_copy_size);
    }
  }
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::CopyTextureToBuffer(
  mnexus::TextureHandle src_texture_handle,
  mnexus::TextureSubresourceRange const& src_subresource_range,
  mnexus::BufferHandle dst_buffer_handle,
  uint32_t dst_buffer_offset,
  mnexus::Extent3d const& copy_extent
) {
  // Transfer commands must not be recorded inside any pass.
  this->EndCurrentRenderPass();
  this->EndCurrentComputePass();

  auto src_texture_pool_handle = container::ResourceHandle::FromU64(src_texture_handle.Get());
  auto [src_texture_hot, src_texture_cold, src_texture_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(src_texture_pool_handle);

  if (!src_texture_hot.wgpu_texture) {
    return;
  }

  auto dst_buffer_pool_handle = container::ResourceHandle::FromU64(dst_buffer_handle.Get());
  auto [dst_buffer_hot, dst_buffer_lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(dst_buffer_pool_handle);

  uint32_t const format_size = MnGetFormatSizeInBytes(static_cast<MnFormat>(src_texture_cold.desc.format));
  MnExtent3d const block_extent = MnGetFormatTexelBlockExtent(static_cast<MnFormat>(src_texture_cold.desc.format));

  uint32_t const blocks_per_row = (copy_extent.width + block_extent.width - 1) / block_extent.width;
  uint32_t const bytes_per_row_unaligned = blocks_per_row * format_size;
  uint32_t const bytes_per_row_aligned = (bytes_per_row_unaligned + 255) & ~uint32_t(255);

  uint32_t const rows_per_image = (copy_extent.height + block_extent.height - 1) / block_extent.height;

  wgpu::TexelCopyTextureInfo src {};
  src.texture = src_texture_hot.wgpu_texture;
  src.mipLevel = src_subresource_range.base_mip_level;
  src.origin = { 0, 0, src_subresource_range.base_array_layer };
  src.aspect = wgpu::TextureAspect::All;

  wgpu::TexelCopyBufferInfo dst {};
  dst.buffer = dst_buffer_hot.wgpu_buffer;
  dst.layout.offset = dst_buffer_offset;
  dst.layout.bytesPerRow = bytes_per_row_aligned;
  dst.layout.rowsPerImage = rows_per_image;

  wgpu::Extent3D wgpu_copy_size {
    copy_extent.width,
    copy_extent.height,
    copy_extent.depth,
  };

  wgpu_command_encoder_.CopyTextureToBuffer(&src, &dst, &wgpu_copy_size);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::BlitTexture(
  mnexus::TextureHandle src_texture_handle,
  mnexus::TextureSubresourceRange const& src_subresource_range,
  mnexus::Offset3d const& src_offset,
  mnexus::Extent3d const& src_extent,
  mnexus::TextureHandle dst_texture_handle,
  mnexus::TextureSubresourceRange const& dst_subresource_range,
  mnexus::Offset3d const& dst_offset,
  mnexus::Extent3d const& dst_extent,
  mnexus::Filter filter
) {
  // Internal pass conflicts with user's; end any active one.
  this->EndCurrentRenderPass();
  this->EndCurrentComputePass();

  auto src_pool_handle = container::ResourceHandle::FromU64(src_texture_handle.Get());
  auto [src_hot, src_cold, src_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(src_pool_handle);

  auto dst_pool_handle = container::ResourceHandle::FromU64(dst_texture_handle.Get());
  auto [dst_hot, dst_cold, dst_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(dst_pool_handle);

  if (!src_hot.wgpu_texture || !dst_hot.wgpu_texture) {
    return;
  }

  wgpu::TextureFormat const src_format = ToWgpuTextureFormat(src_cold.desc.format);
  wgpu::TextureFormat const dst_format = ToWgpuTextureFormat(dst_cold.desc.format);

  blit_texture::BlitTexture2D(
    wgpu_device_,
    wgpu_command_encoder_,
    src_hot.wgpu_texture, src_format, src_subresource_range,
    src_offset.x, src_offset.y,
    src_extent.width, src_extent.height,
    dst_hot.wgpu_texture, dst_format, dst_subresource_range,
    dst_offset.x, dst_offset.y,
    dst_extent.width, dst_extent.height,
    filter
  );
}

//
// Compute
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::BindExplicitComputePipeline(
  mnexus::ComputePipelineHandle compute_pipeline_handle
) {
  // End any active render pass (mutual exclusion).
  this->EndCurrentRenderPass();

  auto pool_handle = container::ResourceHandle::FromU64(compute_pipeline_handle.Get());
  auto [hot, lock] = resource_storage_->compute_pipelines.GetHotConstRefWithSharedLockGuard(pool_handle);

  if (!current_compute_pass_.has_value()) {
    wgpu::ComputePassEncoder pass = wgpu_command_encoder_.BeginComputePass();
    current_compute_pass_ = std::move(pass);
  }

  current_compute_pipeline_ = hot.wgpu_compute_pipeline;
  current_compute_pass_->SetPipeline(current_compute_pipeline_);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::DispatchCompute(
  uint32_t workgroup_count_x,
  uint32_t workgroup_count_y,
  uint32_t workgroup_count_z
) {
  MBASE_ASSERT(current_compute_pass_.has_value());

  ResolveAndSetBindGroups(
    wgpu_device_,
    *current_compute_pass_,
    current_compute_pipeline_,
    bind_group_state_tracker_,
    resource_storage_->buffers,
    resource_storage_->textures,
    resource_storage_->samplers
  );

  current_compute_pass_->DispatchWorkgroups(workgroup_count_x, workgroup_count_y, workgroup_count_z);
}

//
// Resource Binding
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::BindUniformBuffer(
  mnexus::BindingId const& id,
  mnexus::BufferHandle buffer_handle,
  uint64_t offset,
  uint64_t size
) {
  bind_group_state_tracker_.SetBuffer(
    id.group, id.binding, id.array_element,
    mnexus::BindGroupLayoutEntryType::kUniformBuffer,
    buffer_handle, offset, size
  );
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::BindStorageBuffer(
  mnexus::BindingId const& id,
  mnexus::BufferHandle buffer_handle,
  uint64_t offset,
  uint64_t size
) {
  bind_group_state_tracker_.SetBuffer(
    id.group, id.binding, id.array_element,
    mnexus::BindGroupLayoutEntryType::kStorageBuffer,
    buffer_handle, offset, size
  );
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::BindSampledTexture(
  mnexus::BindingId const& id,
  mnexus::TextureHandle texture_handle,
  mnexus::TextureSubresourceRange const& subresource_range
) {
  bind_group_state_tracker_.SetTexture(
    id.group, id.binding, id.array_element,
    mnexus::BindGroupLayoutEntryType::kSampledTexture,
    texture_handle, subresource_range
  );
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::BindSampler(
  mnexus::BindingId const& id,
  mnexus::SamplerHandle sampler_handle
) {
  bind_group_state_tracker_.SetSampler(
    id.group, id.binding, id.array_element,
    sampler_handle
  );
}

//
// Explicit Pipeline Binding
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::BindExplicitRenderPipeline(
  mnexus::RenderPipelineHandle render_pipeline_handle
) {
  auto pool_handle = container::ResourceHandle::FromU64(render_pipeline_handle.Get());
  auto [hot, lock] = resource_storage_->render_pipelines.GetHotConstRefWithSharedLockGuard(pool_handle);
  current_render_pipeline_ = hot.wgpu_render_pipeline;
  explicit_render_pipeline_bound_ = true;
  render_pipeline_state_tracker_.MarkClean();
}

//
// Render Pass
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::BeginRenderPass(
  mnexus::RenderPassDesc const& desc
) {
  // End any active compute pass (mutual exclusion).
  this->EndCurrentComputePass();
  // End any active render pass.
  this->EndCurrentRenderPass();

  // Build wgpu render pass descriptor.
  mbase::SmallVector<wgpu::RenderPassColorAttachment, 4> wgpu_color_attachments;
  wgpu_color_attachments.reserve(desc.color_attachments.size());

  mbase::SmallVector<mnexus::Format, 4> color_formats;
  color_formats.reserve(desc.color_attachments.size());

  for (uint32_t i = 0; i < desc.color_attachments.size(); ++i) {
    mnexus::ColorAttachmentDesc const& att = desc.color_attachments[i];

    auto pool_handle = container::ResourceHandle::FromU64(att.texture.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    if (!hot.wgpu_texture) {
      continue;
    }

    wgpu::TextureFormat const wgpu_format = ToWgpuTextureFormat(cold.desc.format);
    color_formats.emplace_back(cold.desc.format);

    wgpu::TextureViewDescriptor view_desc = MakeWgpuTextureViewDesc(
      wgpu_format,
      wgpu::TextureViewDimension::e2D,
      att.subresource_range,
      wgpu::TextureAspect::All
    );
    wgpu::TextureView view = hot.wgpu_texture.CreateView(&view_desc);

    wgpu_color_attachments.emplace_back(
      wgpu::RenderPassColorAttachment {
        .view = view,
        .resolveTarget = nullptr,
        .loadOp = ToWgpuLoadOp(att.load_op),
        .storeOp = ToWgpuStoreOp(att.store_op),
        .clearValue = {
          att.clear_value.color.r,
          att.clear_value.color.g,
          att.clear_value.color.b,
          att.clear_value.color.a,
        },
      }
    );
  }

  mnexus::Format depth_stencil_format = mnexus::Format::kUndefined;

  // Depth/stencil attachment.
  wgpu::RenderPassDepthStencilAttachment wgpu_depth_stencil {};
  bool has_depth_stencil = false;
  if (desc.depth_stencil_attachment != nullptr) {
    auto const& ds = *desc.depth_stencil_attachment;
    auto pool_handle = container::ResourceHandle::FromU64(ds.texture.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    if (hot.wgpu_texture) {
      wgpu::TextureFormat const wgpu_format = ToWgpuTextureFormat(cold.desc.format);
      depth_stencil_format = cold.desc.format;

      wgpu::TextureViewDescriptor view_desc = MakeWgpuTextureViewDesc(
        wgpu_format,
        wgpu::TextureViewDimension::e2D,
        ds.subresource_range,
        wgpu::TextureAspect::All
      );
      wgpu::TextureView view = hot.wgpu_texture.CreateView(&view_desc);

      wgpu_depth_stencil.view = view;
      wgpu_depth_stencil.depthLoadOp = ToWgpuLoadOp(ds.depth_load_op);
      wgpu_depth_stencil.depthStoreOp = ToWgpuStoreOp(ds.depth_store_op);
      wgpu_depth_stencil.depthClearValue = ds.depth_clear_value;
      wgpu_depth_stencil.stencilLoadOp = ToWgpuLoadOp(ds.stencil_load_op);
      wgpu_depth_stencil.stencilStoreOp = ToWgpuStoreOp(ds.stencil_store_op);
      wgpu_depth_stencil.stencilClearValue = ds.stencil_clear_value;
      has_depth_stencil = true;
    }
  }

  wgpu::RenderPassDescriptor pass_desc {
    .colorAttachmentCount = wgpu_color_attachments.size(),
    .colorAttachments = wgpu_color_attachments.data(),
    .depthStencilAttachment = has_depth_stencil ? &wgpu_depth_stencil : nullptr,
  };

  current_render_pass_ = wgpu_command_encoder_.BeginRenderPass(&pass_desc);

  // Configure state tracker with render target info.
  render_pipeline_state_tracker_.SetRenderTargetConfig(
    std::move(color_formats),
    depth_stencil_format,
    1 // sample_count (always 1 for now)
  );

  if (render_state_event_log_.IsEnabled()) {
    render_state_event_log_.Record(
      mnexus::RenderStateEventTag::kBeginRenderPass,
      render_pipeline_state_tracker_.BuildSnapshot());
  }
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::EndRenderPass() {
  if (render_state_event_log_.IsEnabled()) {
    render_state_event_log_.Record(
      mnexus::RenderStateEventTag::kEndRenderPass,
      render_pipeline_state_tracker_.BuildSnapshot());
  }
  this->EndCurrentRenderPass();
}

//
// Render State (auto-generation path)
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::BindRenderProgram(
  mnexus::ProgramHandle program_handle
) {
  explicit_render_pipeline_bound_ = false;
  render_pipeline_state_tracker_.SetProgram(program_handle);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetVertexInputLayout(
  mnexus::container::ArrayProxy<mnexus::VertexInputBindingDesc const> bindings,
  mnexus::container::ArrayProxy<mnexus::VertexInputAttributeDesc const> attributes
) {
  mbase::SmallVector<mnexus::VertexInputBindingDesc, 4> bindings_vec;
  bindings_vec.reserve(bindings.size());
  for (uint32_t i = 0; i < bindings.size(); ++i) {
    bindings_vec.emplace_back(bindings[i]);
  }

  mbase::SmallVector<mnexus::VertexInputAttributeDesc, 8> attributes_vec;
  attributes_vec.reserve(attributes.size());
  for (uint32_t i = 0; i < attributes.size(); ++i) {
    attributes_vec.emplace_back(attributes[i]);
  }

  render_pipeline_state_tracker_.SetVertexInputLayout(
    std::move(bindings_vec),
    std::move(attributes_vec)
  );
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::BindVertexBuffer(
  uint32_t binding,
  mnexus::BufferHandle buffer_handle,
  uint64_t offset
) {
  // Store vertex buffer binding for use at draw time.
  if (binding >= bound_vertex_buffers_.size()) {
    bound_vertex_buffers_.resize(binding + 1);
  }
  bound_vertex_buffers_[binding] = BoundVertexBuffer {
    .buffer_handle = buffer_handle,
    .offset = offset,
  };
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::BindIndexBuffer(
  mnexus::BufferHandle buffer_handle,
  uint64_t offset,
  mnexus::IndexType index_type
) {
  bound_index_buffer_ = BoundIndexBuffer {
    .buffer_handle = buffer_handle,
    .offset = offset,
    .index_type = index_type,
  };
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetPrimitiveTopology(
  mnexus::PrimitiveTopology topology
) {
  render_pipeline_state_tracker_.SetPrimitiveTopology(topology);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetPolygonMode(
  mnexus::PolygonMode mode
) {
  render_pipeline_state_tracker_.SetPolygonMode(mode);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetCullMode(
  mnexus::CullMode cull_mode
) {
  render_pipeline_state_tracker_.SetCullMode(cull_mode);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetFrontFace(
  mnexus::FrontFace front_face
) {
  render_pipeline_state_tracker_.SetFrontFace(front_face);
}

// Depth

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetDepthTestEnabled(bool enabled) {
  render_pipeline_state_tracker_.SetDepthTestEnabled(enabled);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetDepthWriteEnabled(bool enabled) {
  render_pipeline_state_tracker_.SetDepthWriteEnabled(enabled);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetDepthCompareOp(mnexus::CompareOp op) {
  render_pipeline_state_tracker_.SetDepthCompareOp(op);
}

// Stencil

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetStencilTestEnabled(bool enabled) {
  render_pipeline_state_tracker_.SetStencilTestEnabled(enabled);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetStencilFrontOps(
  mnexus::StencilOp fail, mnexus::StencilOp pass,
  mnexus::StencilOp depth_fail, mnexus::CompareOp compare
) {
  render_pipeline_state_tracker_.SetStencilFrontOps(fail, pass, depth_fail, compare);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetStencilBackOps(
  mnexus::StencilOp fail, mnexus::StencilOp pass,
  mnexus::StencilOp depth_fail, mnexus::CompareOp compare
) {
  render_pipeline_state_tracker_.SetStencilBackOps(fail, pass, depth_fail, compare);
}

// Per-attachment blend

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetBlendEnabled(
  uint32_t attachment, bool enabled
) {
  render_pipeline_state_tracker_.SetBlendEnabled(attachment, enabled);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetBlendFactors(
  uint32_t attachment,
  mnexus::BlendFactor src_color, mnexus::BlendFactor dst_color, mnexus::BlendOp color_op,
  mnexus::BlendFactor src_alpha, mnexus::BlendFactor dst_alpha, mnexus::BlendOp alpha_op
) {
  render_pipeline_state_tracker_.SetBlendFactors(
    attachment, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op
  );
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetColorWriteMask(
  uint32_t attachment, mnexus::ColorWriteMask mask
) {
  render_pipeline_state_tracker_.SetColorWriteMask(attachment, mask);
}

//
// Draw
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::Draw(
  uint32_t vertex_count,
  uint32_t instance_count,
  uint32_t first_vertex,
  uint32_t first_instance
) {
  MBASE_ASSERT_MSG(current_render_pass_.has_value(), "Draw called outside of a render pass");

  this->ResolveRenderPipelineAndBindState();

  if (render_state_event_log_.IsEnabled()) {
    render_state_event_log_.Record(
      mnexus::RenderStateEventTag::kDraw,
      render_pipeline_state_tracker_.BuildSnapshot());
  }

  current_render_pass_->Draw(vertex_count, instance_count, first_vertex, first_instance);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::DrawIndexed(
  uint32_t index_count,
  uint32_t instance_count,
  uint32_t first_index,
  int32_t vertex_offset,
  uint32_t first_instance
) {
  MBASE_ASSERT_MSG(current_render_pass_.has_value(), "DrawIndexed called outside of a render pass");

  this->ResolveRenderPipelineAndBindState();

  if (render_state_event_log_.IsEnabled()) {
    render_state_event_log_.Record(
      mnexus::RenderStateEventTag::kDrawIndexed,
      render_pipeline_state_tracker_.BuildSnapshot());
  }

  current_render_pass_->DrawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

//
// Viewport / Scissor
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetViewport(
  float x, float y, float width, float height,
  float min_depth, float max_depth
) {
  MBASE_ASSERT_MSG(current_render_pass_.has_value(), "SetViewport called outside of a render pass");
  current_render_pass_->SetViewport(x, y, width, height, min_depth, max_depth);
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListWebGpu::SetScissor(
  int32_t x, int32_t y, uint32_t width, uint32_t height
) {
  MBASE_ASSERT_MSG(current_render_pass_.has_value(), "SetScissor called outside of a render pass");
  current_render_pass_->SetScissorRect(
    static_cast<uint32_t>(x),
    static_cast<uint32_t>(y),
    width,
    height
  );
}

// --------------------------------------------------------------------------------------------------
// Private helpers
//

void MnexusCommandListWebGpu::EndCurrentComputePass() {
  if (current_compute_pass_.has_value()) {
    current_compute_pass_->End();
    current_compute_pass_ = std::nullopt;
  }
}

void MnexusCommandListWebGpu::EndCurrentRenderPass() {
  if (current_render_pass_.has_value()) {
    current_render_pass_->End();
    current_render_pass_ = std::nullopt;
  }
}

void MnexusCommandListWebGpu::ResolveRenderPipelineAndBindState() {
  MBASE_ASSERT(current_render_pass_.has_value());

  if (explicit_render_pipeline_bound_) {
    // Explicit pipeline: just set it on the pass (once).
    current_render_pass_->SetPipeline(current_render_pipeline_);
  } else if (render_pipeline_state_tracker_.IsDirty()) {
    pipeline::RenderPipelineCacheKey key = render_pipeline_state_tracker_.BuildCacheKey();
    render_pipeline_state_tracker_.MarkClean();

    bool cache_hit = false;
    current_render_pipeline_ = resource_storage_->render_pipeline_cache.FindOrInsert(
      key,
      [&](pipeline::RenderPipelineCacheKey const& k) {
        return CreateWgpuRenderPipelineFromCacheKey(
          wgpu_device_,
          k,
          resource_storage_->programs,
          resource_storage_->shader_modules
        );
      },
      &cache_hit
    );

    if (render_state_event_log_.IsEnabled()) {
      render_state_event_log_.RecordPso(
        render_pipeline_state_tracker_.BuildSnapshot(),
        key.ComputeHash(),
        cache_hit);
    }

    current_render_pass_->SetPipeline(current_render_pipeline_);
  }

  // Resolve and set bind groups.
  ResolveAndSetBindGroups(
    wgpu_device_,
    *current_render_pass_,
    current_render_pipeline_,
    bind_group_state_tracker_,
    resource_storage_->buffers,
    resource_storage_->textures,
    resource_storage_->samplers
  );

  // Set vertex buffers.
  for (size_t i = 0; i < bound_vertex_buffers_.size(); ++i) {
    auto const& vb = bound_vertex_buffers_[i];
    if (!vb.buffer_handle.IsValid()) {
      continue;
    }
    auto pool_handle = container::ResourceHandle::FromU64(vb.buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);
    current_render_pass_->SetVertexBuffer(static_cast<uint32_t>(i), hot.wgpu_buffer, vb.offset);
  }

  // Set index buffer (if bound).
  if (bound_index_buffer_.buffer_handle.IsValid()) {
    auto pool_handle = container::ResourceHandle::FromU64(bound_index_buffer_.buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);
    current_render_pass_->SetIndexBuffer(
      hot.wgpu_buffer,
      ToWgpuIndexFormat(bound_index_buffer_.index_type),
      bound_index_buffer_.offset
    );
  }
}

} // namespace mnexus_backend::webgpu
