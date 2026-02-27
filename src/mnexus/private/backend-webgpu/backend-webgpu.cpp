// TU header --------------------------------------------
#include "backend-webgpu/backend-webgpu.h"

// c++ headers ------------------------------------------
#include <algorithm>
#include <cstring>
#include <mutex>
#include <vector>

// platform detection header ----------------------------
#include "mbase/public/platform.h"

// conditional platform headers -------------------------
#if MBASE_PLATFORM_WINDOWS
# if !defined(NOMINMAX)
#   define NOMINMAX
# endif
# if !defined(WIN32_LEAN_AND_MEAN)
#   define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# elif MBASE_PLATFORM_WEB
# include <emscripten/html5.h>
#endif

// public project headers -------------------------------
#include "mbase/public/log.h"
#include "mbase/public/assert.h"
#include "mbase/public/tsa.h"

// project headers --------------------------------------
#include "container/generational_pool.h"

#include "backend-webgpu/backend-webgpu-compute_pipeline.h"
#include "backend-webgpu/backend-webgpu-binding.h"
#include "backend-webgpu/backend-webgpu-buffer.h"
#include "backend-webgpu/backend-webgpu-layout.h"
#include "backend-webgpu/backend-webgpu-render_pipeline.h"
#include "backend-webgpu/backend-webgpu-shader.h"
#include "backend-webgpu/backend-webgpu-sampler.h"
#include "backend-webgpu/backend-webgpu-texture.h"
#include "backend-webgpu/include_dawn.h"
#include "backend-webgpu/types_bridge.h"
#include "backend-webgpu/builtin_shader.h"
#include "backend-webgpu/blit_texture.h"
#include "backend-webgpu/buffer_row_repack.h"

#include "pipeline/pipeline_layout_cache.h"
#include "pipeline/render_pipeline_cache.h"
#include "pipeline/render_pipeline_state_tracker.h"

namespace mnexus_backend::webgpu {

struct ResourceStorage final {
  ShaderModuleResourcePool shader_modules;
  ProgramResourcePool programs;
  ComputePipelineResourcePool compute_pipelines;
  RenderPipelineResourcePool render_pipelines;

  BufferResourcePool buffers;
  TextureResourcePool textures;
  SamplerResourcePool samplers;

  pipeline::TPipelineLayoutCache<wgpu::PipelineLayout> pipeline_layout_cache;
  pipeline::TRenderPipelineCache<wgpu::RenderPipeline> render_pipeline_cache;

  std::mutex swapchain_texture_mutex; // Protects `TextureHot` and `TextureCold`.
  container::ResourceHandle swapchain_texture_handle = container::ResourceHandle::Null(); // Not protected; set only during initialization.
};

#define IMPL_VAPI(ret, func, ...) \
  MNEXUS_NO_THROW ret MNEXUS_CALL func(__VA_ARGS__) override

class MnexusCommandListWebGpu : public mnexus::ICommandList {
public:
  explicit MnexusCommandListWebGpu(
    ResourceStorage* resource_storage,
    wgpu::Device wgpu_device,
    wgpu::CommandEncoder wgpu_command_encoder
  ) :
    resource_storage_(resource_storage),
    wgpu_device_(std::move(wgpu_device)),
    wgpu_command_encoder_(std::move(wgpu_command_encoder))
  {}
  ~MnexusCommandListWebGpu() override = default;
  MBASE_DISALLOW_COPY_MOVE(MnexusCommandListWebGpu);

  wgpu::CommandEncoder const& GetWgpuCommandEncoder() const {
    return wgpu_command_encoder_;
  }

  // --------------------------------------------------------------------------------------------------
  // mnexus::ICommandList implementation
  //

  IMPL_VAPI(void, End) {
    MBASE_ASSERT_MSG(!current_render_pass_.has_value(),
      "Active render pass must be ended via EndRenderPass before calling End");
    this->EndCurrentComputePass();
  }

  //
  // Transfer
  //

  IMPL_VAPI(void, ClearTexture,
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

  IMPL_VAPI(void, CopyBufferToTexture,
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

  IMPL_VAPI(void, CopyTextureToBuffer,
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

  IMPL_VAPI(void, BlitTexture,
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

  IMPL_VAPI(void, BindExplicitComputePipeline, mnexus::ComputePipelineHandle compute_pipeline_handle) {
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

  IMPL_VAPI(void, DispatchCompute,
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

  IMPL_VAPI(void, BindUniformBuffer,
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

  IMPL_VAPI(void, BindStorageBuffer,
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

  IMPL_VAPI(void, BindSampledTexture,
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

  IMPL_VAPI(void, BindSampler,
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

  IMPL_VAPI(void, BindExplicitRenderPipeline, mnexus::RenderPipelineHandle render_pipeline_handle) {
    auto pool_handle = container::ResourceHandle::FromU64(render_pipeline_handle.Get());
    auto [hot, lock] = resource_storage_->render_pipelines.GetHotConstRefWithSharedLockGuard(pool_handle);
    current_render_pipeline_ = hot.wgpu_render_pipeline;
    explicit_render_pipeline_bound_ = true;
    render_pipeline_state_tracker_.MarkClean();
  }

  //
  // Render Pass
  //

  IMPL_VAPI(void, BeginRenderPass, mnexus::RenderPassDesc const& desc) {
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
  }

  IMPL_VAPI(void, EndRenderPass) {
    this->EndCurrentRenderPass();
  }

  //
  // Render State (auto-generation path)
  //

  IMPL_VAPI(void, BindRenderProgram, mnexus::ProgramHandle program_handle) {
    explicit_render_pipeline_bound_ = false;
    render_pipeline_state_tracker_.SetProgram(program_handle);
  }

  IMPL_VAPI(void, SetVertexInputLayout,
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

  IMPL_VAPI(void, BindVertexBuffer,
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

  IMPL_VAPI(void, BindIndexBuffer,
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

  IMPL_VAPI(void, SetPrimitiveTopology, mnexus::PrimitiveTopology topology) {
    render_pipeline_state_tracker_.SetPrimitiveTopology(topology);
  }

  IMPL_VAPI(void, SetPolygonMode, mnexus::PolygonMode mode) {
    render_pipeline_state_tracker_.SetPolygonMode(mode);
  }

  IMPL_VAPI(void, SetCullMode, mnexus::CullMode cull_mode) {
    render_pipeline_state_tracker_.SetCullMode(cull_mode);
  }

  IMPL_VAPI(void, SetFrontFace, mnexus::FrontFace front_face) {
    render_pipeline_state_tracker_.SetFrontFace(front_face);
  }

  // Depth

  IMPL_VAPI(void, SetDepthTestEnabled, bool enabled) {
    render_pipeline_state_tracker_.SetDepthTestEnabled(enabled);
  }

  IMPL_VAPI(void, SetDepthWriteEnabled, bool enabled) {
    render_pipeline_state_tracker_.SetDepthWriteEnabled(enabled);
  }

  IMPL_VAPI(void, SetDepthCompareOp, mnexus::CompareOp op) {
    render_pipeline_state_tracker_.SetDepthCompareOp(op);
  }

  // Stencil

  IMPL_VAPI(void, SetStencilTestEnabled, bool enabled) {
    render_pipeline_state_tracker_.SetStencilTestEnabled(enabled);
  }

  IMPL_VAPI(void, SetStencilFrontOps,
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  ) {
    render_pipeline_state_tracker_.SetStencilFrontOps(fail, pass, depth_fail, compare);
  }

  IMPL_VAPI(void, SetStencilBackOps,
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  ) {
    render_pipeline_state_tracker_.SetStencilBackOps(fail, pass, depth_fail, compare);
  }

  // Per-attachment blend

  IMPL_VAPI(void, SetBlendEnabled, uint32_t attachment, bool enabled) {
    render_pipeline_state_tracker_.SetBlendEnabled(attachment, enabled);
  }

  IMPL_VAPI(void, SetBlendFactors,
    uint32_t attachment,
    mnexus::BlendFactor src_color, mnexus::BlendFactor dst_color, mnexus::BlendOp color_op,
    mnexus::BlendFactor src_alpha, mnexus::BlendFactor dst_alpha, mnexus::BlendOp alpha_op
  ) {
    render_pipeline_state_tracker_.SetBlendFactors(
      attachment, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op
    );
  }

  IMPL_VAPI(void, SetColorWriteMask, uint32_t attachment, mnexus::ColorWriteMask mask) {
    render_pipeline_state_tracker_.SetColorWriteMask(attachment, mask);
  }

  //
  // Draw
  //

  IMPL_VAPI(void, Draw,
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t first_vertex,
    uint32_t first_instance
  ) {
    MBASE_ASSERT_MSG(current_render_pass_.has_value(), "Draw called outside of a render pass");

    this->ResolveRenderPipelineAndBindState();

    current_render_pass_->Draw(vertex_count, instance_count, first_vertex, first_instance);
  }

  IMPL_VAPI(void, DrawIndexed,
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t vertex_offset,
    uint32_t first_instance
  ) {
    MBASE_ASSERT_MSG(current_render_pass_.has_value(), "DrawIndexed called outside of a render pass");

    this->ResolveRenderPipelineAndBindState();

    current_render_pass_->DrawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
  }

  //
  // Viewport / Scissor
  //

  IMPL_VAPI(void, SetViewport,
    float x, float y, float width, float height,
    float min_depth, float max_depth
  ) {
    MBASE_ASSERT_MSG(current_render_pass_.has_value(), "SetViewport called outside of a render pass");
    current_render_pass_->SetViewport(x, y, width, height, min_depth, max_depth);
  }

  IMPL_VAPI(void, SetScissor,
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

private:
  struct BoundVertexBuffer {
    mnexus::BufferHandle buffer_handle;
    uint64_t offset = 0;
  };

  struct BoundIndexBuffer {
    mnexus::BufferHandle buffer_handle;
    uint64_t offset = 0;
    mnexus::IndexType index_type = mnexus::IndexType::kUint32;
  };
  void EndCurrentComputePass() {
    if (current_compute_pass_.has_value()) {
      current_compute_pass_->End();
      current_compute_pass_ = std::nullopt;
    }
  }

  void EndCurrentRenderPass() {
    if (current_render_pass_.has_value()) {
      current_render_pass_->End();
      current_render_pass_ = std::nullopt;
    }
  }

  /// Resolves the render pipeline from the state tracker, binds it and any dirty bind groups/vertex buffers.
  void ResolveRenderPipelineAndBindState() {
    MBASE_ASSERT(current_render_pass_.has_value());

    if (explicit_render_pipeline_bound_) {
      // Explicit pipeline: just set it on the pass (once).
      current_render_pass_->SetPipeline(current_render_pipeline_);
    } else if (render_pipeline_state_tracker_.IsDirty()) {
      pipeline::RenderPipelineCacheKey key = render_pipeline_state_tracker_.BuildCacheKey();
      render_pipeline_state_tracker_.MarkClean();

      current_render_pipeline_ = resource_storage_->render_pipeline_cache.FindOrInsert(
        key,
        [&](pipeline::RenderPipelineCacheKey const& k) {
          return CreateWgpuRenderPipelineFromCacheKey(
            wgpu_device_,
            k,
            resource_storage_->programs,
            resource_storage_->shader_modules
          );
        }
      );

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

  ResourceStorage* resource_storage_ = nullptr;
  wgpu::Device wgpu_device_;
  wgpu::CommandEncoder wgpu_command_encoder_;

  // Compute pass state.
  std::optional<wgpu::ComputePassEncoder> current_compute_pass_;
  wgpu::ComputePipeline current_compute_pipeline_;

  // Render pass state.
  std::optional<wgpu::RenderPassEncoder> current_render_pass_;
  wgpu::RenderPipeline current_render_pipeline_;
  bool explicit_render_pipeline_bound_ = false;
  pipeline::RenderPipelineStateTracker render_pipeline_state_tracker_;
  mbase::SmallVector<BoundVertexBuffer, 4> bound_vertex_buffers_;
  BoundIndexBuffer bound_index_buffer_;

  binding::BindGroupStateTracker bind_group_state_tracker_;
};

class MnexusDeviceWebGpu : public mnexus::IDevice {
public:
  // ----------------------------------------------------------------------------------------------------
  // mnexus::IDevice implementation
  //

  // -----------------------------------------------------------------------------------------------
  // Queue
  //

  IMPL_VAPI(uint32_t, QueueGetFamilyCount) {
    // Only one queue family is supported.
    return 1;
  }

  IMPL_VAPI(MnBool32, QueueGetFamilyDesc,
    uint32_t queue_family_index,
    mnexus::QueueFamilyDesc& out_desc
  ) {
    if (queue_family_index != 0) {
      // Only one queue family is supported.
      return MnBoolFalse;
    }

    out_desc = mnexus::QueueFamilyDesc {
      .queue_count = 1,
      .capabilities = mnexus::QueueFamilyCapabilityFlagBits::kGraphics |
                      mnexus::QueueFamilyCapabilityFlagBits::kCompute |
                      mnexus::QueueFamilyCapabilityFlagBits::kTransfer,
    };
    return MnBoolTrue;
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueSubmitCommandList,
    mnexus::QueueId const& queue_id,
    mnexus::ICommandList* command_list
  ) {
    MBASE_ASSERT_MSG(queue_id.queue_family_index == 0 && queue_id.queue_index == 0, "WebGPU backend only supports a single queue");

    mbase::LockGuard queue_lock(queue_mutex_);

    this->PollPendingOps();

    // Downcast to our command list implementation.
    MnexusCommandListWebGpu* webgpu_command_list = dynamic_cast<MnexusCommandListWebGpu*>(command_list);

    wgpu::CommandBuffer wgpu_command_buffer = webgpu_command_list->GetWgpuCommandEncoder().Finish();

    wgpu::Queue wgpu_queue = wgpu_device_.GetQueue();
    wgpu_queue.Submit(1, &wgpu_command_buffer);

    this->DiscardCommandList(command_list);

    // Track GPU-side completion via OnSubmittedWorkDone.
    wgpu::Future work_done_future = wgpu_queue.OnSubmittedWorkDone(
      wgpu::CallbackMode::WaitAnyOnly,
      [](wgpu::QueueWorkDoneStatus status, wgpu::StringView) {
        if (status != wgpu::QueueWorkDoneStatus::Success) {
          MBASE_LOG_ERROR("OnSubmittedWorkDone failed: {}", static_cast<uint32_t>(status));
        }
      }
    );

    mnexus::IntraQueueSubmissionId const id = this->AdvanceTimeline();

    pending_ops_.emplace_back(
      PendingOp {
        .timeline_value = id.Get(),
        .future = work_done_future,
      }
    );

    this->UpdateCompletedValue();
    return id;
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueWriteBuffer,
    mnexus::QueueId const& queue_id,
    mnexus::BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void const* data,
    uint32_t data_size_in_bytes
  ) {
    MBASE_ASSERT_MSG(queue_id.queue_family_index == 0 && queue_id.queue_index == 0, "WebGPU backend only supports a single queue");

    mbase::LockGuard queue_lock(queue_mutex_);

    auto pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());

    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);

    wgpu::Queue wgpu_queue = wgpu_device_.GetQueue();

    wgpu_queue.WriteBuffer(
      hot.wgpu_buffer,
      buffer_offset,
      data,
      data_size_in_bytes
    );

    mnexus::IntraQueueSubmissionId const id = this->AdvanceTimeline();
    this->UpdateCompletedValue();
    return id;
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueReadBuffer,
    mnexus::QueueId const& queue_id,
    mnexus::BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void* dst,
    uint32_t size_in_bytes
  ) {
    MBASE_ASSERT_MSG(queue_id.queue_family_index == 0 && queue_id.queue_index == 0, "WebGPU backend only supports a single queue");
    MBASE_ASSERT_MSG((buffer_offset % 4) == 0, "buffer_offset must be 4-byte aligned");
    MBASE_ASSERT_MSG((size_in_bytes % 4) == 0, "size_in_bytes must be 4-byte aligned");

    mbase::LockGuard queue_lock(queue_mutex_);

    auto pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);

    // Create a staging buffer for the readback.
    wgpu::BufferDescriptor staging_desc {
      .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
      .size = size_in_bytes,
    };
    wgpu::Buffer staging_buffer = wgpu_device_.CreateBuffer(&staging_desc);

    // Encode copy from source buffer to staging buffer.
    wgpu::CommandEncoder encoder = wgpu_device_.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(hot.wgpu_buffer, buffer_offset, staging_buffer, 0, size_in_bytes);
    wgpu::CommandBuffer command_buffer = encoder.Finish();

    wgpu::Queue wgpu_queue = wgpu_device_.GetQueue();
    wgpu_queue.Submit(1, &command_buffer);

    // Initiate async map.
    wgpu::Future map_future = staging_buffer.MapAsync(
      wgpu::MapMode::Read, 0, size_in_bytes,
      wgpu::CallbackMode::WaitAnyOnly,
      [](wgpu::MapAsyncStatus status, wgpu::StringView message) {
        if (status != wgpu::MapAsyncStatus::Success) {
          MBASE_LOG_ERROR("MapAsync failed: {}", message);
        }
      }
    );

    mnexus::IntraQueueSubmissionId const id = this->AdvanceTimeline();

    pending_readbacks_.emplace_back(
      PendingReadback {
        .timeline_value = id.Get(),
        .staging_buffer = std::move(staging_buffer),
        .map_future = map_future,
        .dst = dst,
        .size_in_bytes = size_in_bytes,
      }
    );

    this->UpdateCompletedValue();
    return id;
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueGetCompletedValue,
    mnexus::QueueId const& queue_id
  ) {
    MBASE_ASSERT_MSG(queue_id.queue_family_index == 0 && queue_id.queue_index == 0, "WebGPU backend only supports a single queue");

    mbase::LockGuard queue_lock(queue_mutex_);

    this->PollPendingOps();
    this->UpdateCompletedValue();
    return mnexus::IntraQueueSubmissionId { completed_value_ };
  }

  IMPL_VAPI(void, QueueWaitIdle,
    mnexus::QueueId const& queue_id,
    mnexus::IntraQueueSubmissionId value
  ) {
    MBASE_ASSERT_MSG(queue_id.queue_family_index == 0 && queue_id.queue_index == 0, "WebGPU backend only supports a single queue");

    mbase::LockGuard queue_lock(queue_mutex_);

    uint64_t const target = value.Get();

    while (completed_value_ < target) {
      bool found_pending = false;

      // Block on pending submit/write ops.
      for (size_t i = 0; i < pending_ops_.size(); ) {
        PendingOp& op = pending_ops_[i];
        if (op.timeline_value <= target) {
          found_pending = true;

          wgpu::WaitStatus wait_status = wgpu_instance_.WaitAny(op.future, UINT64_MAX);
          if (wait_status == wgpu::WaitStatus::Success) {
            pending_ops_.erase(pending_ops_.begin() + static_cast<ptrdiff_t>(i));
          } else {
            MBASE_LOG_ERROR("WaitAny failed during QueueWaitIdle (pending op)");
            ++i;
          }
        } else {
          ++i;
        }
      }

      // Block on pending readbacks.
      for (size_t i = 0; i < pending_readbacks_.size(); ) {
        PendingReadback& rb = pending_readbacks_[i];
        if (rb.timeline_value <= target) {
          found_pending = true;

          wgpu::WaitStatus wait_status = wgpu_instance_.WaitAny(rb.map_future, UINT64_MAX);
          if (wait_status == wgpu::WaitStatus::Success) {
            void const* mapped = rb.staging_buffer.GetConstMappedRange(0, rb.size_in_bytes);
            MBASE_ASSERT(mapped != nullptr);
            std::memcpy(rb.dst, mapped, rb.size_in_bytes);
            rb.staging_buffer.Unmap();

            pending_readbacks_.erase(pending_readbacks_.begin() + static_cast<ptrdiff_t>(i));
          } else {
            MBASE_LOG_ERROR("WaitAny failed during QueueWait (pending readback)");
            ++i;
          }
        } else {
          ++i;
        }
      }

      this->UpdateCompletedValue();

      if (!found_pending) {
        break;
      }
    }
  }

  // -----------------------------------------------------------------------------------------------
  // Resource creation/acquisition.
  //

  //
  // Command List
  //

  IMPL_VAPI(mnexus::ICommandList*, CreateCommandList, mnexus::CommandListDesc const& desc) {
    (void)desc; // Unused for now.

    wgpu::CommandEncoder wgpu_command_encoder = wgpu_device_.CreateCommandEncoder();

    return new MnexusCommandListWebGpu(resource_storage_, wgpu_device_, std::move(wgpu_command_encoder));
  }

  MNEXUS_NO_THROW void MNEXUS_CALL DiscardCommandList(mnexus::ICommandList* command_list) override {
    MBASE_ASSERT(command_list != nullptr);
    delete command_list;
  }

  //
  // Buffer
  //

  IMPL_VAPI(mnexus::BufferHandle, CreateBuffer,
    mnexus::BufferDesc const& desc
  ) {
    wgpu::Buffer wgpu_buffer = CreateWgpuBuffer(wgpu_device_, desc);

    container::ResourceHandle pool_handle = resource_storage_->buffers.Emplace(
      std::forward_as_tuple(BufferHot { std::move(wgpu_buffer) }),
      std::forward_as_tuple(BufferCold { desc })
    );

    return mnexus::BufferHandle { pool_handle.AsU64() };
  }
  IMPL_VAPI(void, DestroyBuffer,
    mnexus::BufferHandle buffer_handle
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());
    resource_storage_->buffers.Erase(pool_handle);
  }

  IMPL_VAPI(void, GetBufferDesc,
    mnexus::BufferHandle buffer_handle,
    mnexus::BufferDesc& out_desc
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());
    {
      auto [cold, lock] = resource_storage_->buffers.GetColdConstRefWithSharedLockGuard(pool_handle);
      out_desc = cold.desc;
    }
  }

  //
  // Texture
  //

  IMPL_VAPI(mnexus::TextureHandle, GetSwapchainTexture) {
    return mnexus::TextureHandle { resource_storage_->swapchain_texture_handle.AsU64() };
  }

  IMPL_VAPI(mnexus::TextureHandle, CreateTexture,
    mnexus::TextureDesc const& desc
  ) {
    wgpu::TextureUsage const usage = ToWgpuTextureUsage(desc.usage) | wgpu::TextureUsage::CopyDst;

    wgpu::TextureDescriptor wgpu_texture_desc {
      .usage = usage,
      .dimension = ToWgpuTextureDimension(desc.dimension),
      .size = {
        .width = desc.width,
        .height = desc.height,
        .depthOrArrayLayers = 1,
      },
      .format = ToWgpuTextureFormat(desc.format),
      .mipLevelCount = 1,
      .sampleCount = 1,
      .viewFormatCount = 0,
      .viewFormats = nullptr,
    };

    wgpu::Texture wgpu_texture = wgpu_device_.CreateTexture(&wgpu_texture_desc);

    container::ResourceHandle pool_handle = resource_storage_->textures.Emplace(
      std::forward_as_tuple(TextureHot { std::move(wgpu_texture) }),
      std::forward_as_tuple(TextureCold { desc })
    );

    return mnexus::TextureHandle { pool_handle.AsU64() };
  }

  IMPL_VAPI(void, DestroyTexture,
    mnexus::TextureHandle texture_handle
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(texture_handle.Get());

    MBASE_ASSERT(pool_handle != resource_storage_->swapchain_texture_handle);

    resource_storage_->textures.Erase(pool_handle);
  }

  IMPL_VAPI(void, GetTextureDesc,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureDesc& out_desc
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(texture_handle.Get());

    {
      auto [cold, lock] = resource_storage_->textures.GetColdConstRefWithSharedLockGuard(pool_handle);

      out_desc = cold.desc;
    }
  }

  //
  // Sampler
  //

  IMPL_VAPI(mnexus::SamplerHandle, CreateSampler,
    mnexus::SamplerDesc const& desc
  ) {
    wgpu::SamplerDescriptor wgpu_sampler_desc {};
    wgpu_sampler_desc.minFilter    = ToWgpuFilterMode(desc.min_filter);
    wgpu_sampler_desc.magFilter    = ToWgpuFilterMode(desc.mag_filter);
    wgpu_sampler_desc.mipmapFilter = ToWgpuMipmapFilterMode(desc.mipmap_filter);
    wgpu_sampler_desc.addressModeU = ToWgpuAddressMode(desc.address_mode_u);
    wgpu_sampler_desc.addressModeV = ToWgpuAddressMode(desc.address_mode_v);
    wgpu_sampler_desc.addressModeW = ToWgpuAddressMode(desc.address_mode_w);

    wgpu::Sampler wgpu_sampler = wgpu_device_.CreateSampler(&wgpu_sampler_desc);

    container::ResourceHandle pool_handle = resource_storage_->samplers.Emplace(
      std::forward_as_tuple(SamplerHot { std::move(wgpu_sampler) }),
      std::forward_as_tuple(SamplerCold { desc })
    );

    return mnexus::SamplerHandle { pool_handle.AsU64() };
  }

  IMPL_VAPI(void, DestroySampler,
    mnexus::SamplerHandle sampler_handle
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(sampler_handle.Get());
    resource_storage_->samplers.Erase(pool_handle);
  }

  //
  // ShaderModule
  //

  IMPL_VAPI(mnexus::ShaderModuleHandle, CreateShaderModule,
    mnexus::ShaderModuleDesc const& desc
  ) {
    container::ResourceHandle pool_handle = EmplaceShaderModuleResourcePool(
      resource_storage_->shader_modules,
      wgpu_device_,
      desc
    );

    if (pool_handle.IsNull()) {
      return mnexus::ShaderModuleHandle::Invalid();
    }

    return mnexus::ShaderModuleHandle { pool_handle.AsU64() };
  }
  IMPL_VAPI(void, DestroyShaderModule,
    mnexus::ShaderModuleHandle shader_module_handle
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(shader_module_handle.Get());
    resource_storage_->shader_modules.Erase(pool_handle);
  }

  //
  // Program
  //

  IMPL_VAPI(mnexus::ProgramHandle, CreateProgram,
    mnexus::ProgramDesc const& desc
  ) {
    container::ResourceHandle pool_handle = EmplaceProgramResourcePool(
      resource_storage_->programs,
      wgpu_device_,
      desc,
      resource_storage_->shader_modules,
      [](mnexus::ShaderModuleHandle shader_module_handle) {
        return container::ResourceHandle::FromU64(shader_module_handle.Get());
      },
      resource_storage_->pipeline_layout_cache
    );
    if (pool_handle.IsNull()) {
      return mnexus::ProgramHandle::Invalid();
    }

    return mnexus::ProgramHandle { pool_handle.AsU64() };
  }
  IMPL_VAPI(void, DestroyProgram,
    mnexus::ProgramHandle program_handle
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(program_handle.Get());
    resource_storage_->programs.Erase(pool_handle);
  }

  //
  // ComputePipeline
  //

  IMPL_VAPI(mnexus::ComputePipelineHandle, CreateComputePipeline,
    mnexus::ComputePipelineDesc const& desc
  ) {
    auto shader_module_pool_handle = container::ResourceHandle::FromU64(desc.shader_module.Get());

    auto [shader_module_hot, lock] = resource_storage_->shader_modules.GetHotConstRefWithSharedLockGuard(
      shader_module_pool_handle
    );

    wgpu::ComputePipeline wgpu_compute_pipeline = CreateWgpuComputePipeline(
      wgpu_device_,
      shader_module_hot.wgpu_shader_module
    );

    if (!wgpu_compute_pipeline) {
      return mnexus::ComputePipelineHandle::Invalid();
    }

    container::ResourceHandle pool_handle = resource_storage_->compute_pipelines.Emplace(
      std::forward_as_tuple(ComputePipelineHot { std::move(wgpu_compute_pipeline) }),
      std::forward_as_tuple(ComputePipelineCold { })
    );

    return mnexus::ComputePipelineHandle { pool_handle.AsU64() };
  }
  IMPL_VAPI(void, DestroyComputePipeline,
    mnexus::ComputePipelineHandle compute_pipeline_handle
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(compute_pipeline_handle.Get());
    resource_storage_->compute_pipelines.Erase(pool_handle);
  }

  //
  // RenderPipeline
  //

  IMPL_VAPI(mnexus::RenderPipelineHandle, CreateRenderPipeline,
    mnexus::RenderPipelineDesc const& desc
  ) {
    // Build a cache key from the descriptor.
    pipeline::RenderPipelineCacheKey key;
    key.program = desc.program;

    key.vertex_bindings.assign(desc.vertex_bindings.begin(), desc.vertex_bindings.end());
    key.vertex_attributes.assign(desc.vertex_attributes.begin(), desc.vertex_attributes.end());
    key.color_formats.assign(desc.color_formats.begin(), desc.color_formats.end());
    key.depth_stencil_format = desc.depth_stencil_format;
    key.sample_count = desc.sample_count;

    // Per-draw fixed-function state from desc.
    key.per_draw.ia_primitive_topology   = static_cast<uint8_t>(desc.topology);
    key.per_draw.raster_cull_mode        = static_cast<uint8_t>(desc.cull_mode);
    key.per_draw.raster_front_face       = static_cast<uint8_t>(desc.front_face);
    key.per_draw.depth_test_enabled      = desc.depth_test_enabled ? 1 : 0;
    key.per_draw.depth_write_enabled     = desc.depth_write_enabled ? 1 : 0;
    key.per_draw.depth_compare_op        = static_cast<uint8_t>(desc.depth_compare_op);

    // Per-attachment: default blend state for each color attachment.
    key.per_attachment.resize(desc.color_formats.size());

    // Look up or create the wgpu pipeline via the cache.
    wgpu::RenderPipeline wgpu_pipeline = resource_storage_->render_pipeline_cache.FindOrInsert(
      key,
      [&](pipeline::RenderPipelineCacheKey const& k) {
        return CreateWgpuRenderPipelineFromCacheKey(
          wgpu_device_,
          k,
          resource_storage_->programs,
          resource_storage_->shader_modules
        );
      }
    );

    if (!wgpu_pipeline) {
      return mnexus::RenderPipelineHandle::Invalid();
    }

    container::ResourceHandle pool_handle = resource_storage_->render_pipelines.Emplace(
      std::forward_as_tuple(RenderPipelineHot { std::move(wgpu_pipeline) }),
      std::forward_as_tuple(RenderPipelineCold { })
    );

    return mnexus::RenderPipelineHandle { pool_handle.AsU64() };
  }

  //
  // Device Capability
  //

  IMPL_VAPI(mnexus::AdapterCapability, GetAdapterCapability) {
    return adapter_capability_;
  }

  IMPL_VAPI(void, GetAdapterInfo, mnexus::AdapterInfo& out_info) {
    out_info = adapter_info_;
  }

  //
  // Module local
  //

  void Initialize(
    wgpu::Instance wgpu_instance,
    wgpu::Adapter wgpu_adapter,
    wgpu::Device wgpu_device,
    ResourceStorage* resource_storage
  ) {
    MBASE_ASSERT(!wgpu_device_);

    wgpu_instance_ = std::move(wgpu_instance);
    wgpu_device_ = std::move(wgpu_device);
    resource_storage_ = resource_storage;

    // Populate adapter info.
    {
      wgpu::AdapterInfo info {};
      wgpu_adapter.GetInfo(&info);

      auto copy_string_view = [](char* dst, size_t dst_size, wgpu::StringView sv) {
        if (sv.data == nullptr || sv.length == 0) {
          dst[0] = '\0';
          return;
        }
        size_t len = (sv.length < dst_size - 1) ? sv.length : dst_size - 1;
        std::memcpy(dst, sv.data, len);
        dst[len] = '\0';
      };

      copy_string_view(adapter_info_.device_name, sizeof(adapter_info_.device_name), info.device);
      copy_string_view(adapter_info_.vendor, sizeof(adapter_info_.vendor), info.vendor);
      copy_string_view(adapter_info_.architecture, sizeof(adapter_info_.architecture), info.architecture);
      copy_string_view(adapter_info_.description, sizeof(adapter_info_.description), info.description);
      adapter_info_.vendor_id = info.vendorID;
      adapter_info_.device_id = info.deviceID;
    }

    resource_storage_->swapchain_texture_handle = resource_storage_->textures.Emplace(
      std::forward_as_tuple(TextureHot {}),
      std::forward_as_tuple(TextureCold {})
    );

    InitializeShaderSubsystem();
    builtin_shader::Initialize(wgpu_device_);
    buffer_row_repack::Initialize(wgpu_device_);
    blit_texture::Initialize(wgpu_device_);
  }

  void Shutdown() {
    {
      mbase::LockGuard queue_lock(queue_mutex_);

      if (!pending_ops_.empty() || !pending_readbacks_.empty()) {
        MBASE_LOG_WARN(
          "Shutting down with {} pending op(s) and {} pending readback(s)",
          pending_ops_.size(), pending_readbacks_.size()
        );
      }

      pending_ops_.clear();
      pending_readbacks_.clear();
    }

    blit_texture::Shutdown();
    buffer_row_repack::Shutdown();
    builtin_shader::Shutdown();
    ShutdownShaderSubsystem();
    resource_storage_ = nullptr;
    wgpu_device_ = nullptr;
    wgpu_instance_ = nullptr;
  }

  void OnWgpuSurfaceConfigured(wgpu::SurfaceConfiguration const& surface_config) {
    mbase::LockGuard sw_lock(resource_storage_->swapchain_texture_mutex);

    auto [hot, cold, lock] = resource_storage_->textures.GetRefWithSharedLockGuard(
      resource_storage_->swapchain_texture_handle
    );

    hot = TextureHot {}; // No actual `wgpu::Texture` until acquired from the swapchain.

    mnexus::TextureDesc desc {
      .usage = FromWgpuTextureUsage(surface_config.usage),
      .format = static_cast<mnexus::Format>(FromWgpuTextureFormat(surface_config.format)),
      .dimension = mnexus::TextureDimension::k2D,
      .width = surface_config.width,
      .height = surface_config.height,
      .depth = 1,
      .mip_level_count = 1,
      .array_layer_count = 1,
    };

    cold = TextureCold { .desc = desc };
  }
  void OnWgpuSurfaceUnconfigured() {
    mbase::LockGuard sw_lock(resource_storage_->swapchain_texture_mutex);

    auto [hot, cold, lock] = resource_storage_->textures.GetRefWithSharedLockGuard(
      resource_storage_->swapchain_texture_handle
    );

    hot = TextureHot {};
    cold = TextureCold {};
  }

  void OnWgpuSurfaceTextureAcquired(wgpu::Texture wgpu_texture) {
    auto [hot, cold, lock] = resource_storage_->textures.GetRefWithSharedLockGuard(
      resource_storage_->swapchain_texture_handle
    );

    hot.wgpu_texture = std::move(wgpu_texture);

  }
  void OnWgpuSurfaceTextureReleased() {
    auto [hot, cold, lock] = resource_storage_->textures.GetRefWithSharedLockGuard(
      resource_storage_->swapchain_texture_handle
    );

    hot.wgpu_texture = wgpu::Texture {};
  }

private:
  // Tracks GPU-side completion of queue submissions (via OnSubmittedWorkDone).
  struct PendingOp {
    uint64_t timeline_value;
    wgpu::Future future;
  };

  // Tracks a GPU->CPU readback (staging buffer map + memcpy).
  struct PendingReadback {
    uint64_t timeline_value;
    wgpu::Buffer staging_buffer;
    wgpu::Future map_future;
    void* dst;
    uint32_t size_in_bytes;
  };

  mnexus::IntraQueueSubmissionId AdvanceTimeline() MBASE_REQUIRES(queue_mutex_) {
    return mnexus::IntraQueueSubmissionId { next_timeline_value_++ };
  }

  void UpdateCompletedValue() MBASE_REQUIRES(queue_mutex_) {
    uint64_t min_pending = next_timeline_value_;

    for (auto const& op : pending_ops_) {
      if (op.timeline_value < min_pending) {
        min_pending = op.timeline_value;
      }
    }
    for (auto const& rb : pending_readbacks_) {
      if (rb.timeline_value < min_pending) {
        min_pending = rb.timeline_value;
      }
    }

    completed_value_ = min_pending - 1;
  }

  void PollPendingOps() MBASE_REQUIRES(queue_mutex_) {
    for (size_t i = 0; i < pending_ops_.size(); ) {
      PendingOp& op = pending_ops_[i];

      wgpu::WaitStatus status = wgpu_instance_.WaitAny(op.future, 0);
      if (status == wgpu::WaitStatus::Success) {
        pending_ops_.erase(pending_ops_.begin() + static_cast<ptrdiff_t>(i));
      } else {
        ++i;
      }
    }

    for (size_t i = 0; i < pending_readbacks_.size(); ) {
      PendingReadback& rb = pending_readbacks_[i];

      wgpu::WaitStatus status = wgpu_instance_.WaitAny(rb.map_future, 0);
      if (status == wgpu::WaitStatus::Success) {
        void const* mapped = rb.staging_buffer.GetConstMappedRange(0, rb.size_in_bytes);
        MBASE_ASSERT(mapped != nullptr);
        std::memcpy(rb.dst, mapped, rb.size_in_bytes);
        rb.staging_buffer.Unmap();

        pending_readbacks_.erase(pending_readbacks_.begin() + static_cast<ptrdiff_t>(i));
      } else {
        ++i;
      }
    }
  }

  wgpu::Instance wgpu_instance_;
  wgpu::Device wgpu_device_;
  ResourceStorage* resource_storage_ = nullptr;
  mnexus::AdapterCapability adapter_capability_;
  mnexus::AdapterInfo adapter_info_;

  mbase::Lockable<std::mutex> queue_mutex_;
  uint64_t next_timeline_value_ MBASE_GUARDED_BY(queue_mutex_) = 1;
  uint64_t completed_value_ MBASE_GUARDED_BY(queue_mutex_) = 0;
  std::vector<PendingOp> pending_ops_ MBASE_GUARDED_BY(queue_mutex_);
  std::vector<PendingReadback> pending_readbacks_ MBASE_GUARDED_BY(queue_mutex_);
};


class BackendWebGpu final : public IBackendWebGpu {
public:
  explicit BackendWebGpu(
    wgpu::Instance instance,
    wgpu::Adapter adapter,
    wgpu::Device device
  ) :
    instance_(std::move(instance)),
    adapter_(std::move(adapter)),
    device_(std::move(device))
  {
    mnexus_device_.Initialize(instance_, adapter_, device_, &resource_storage_);
  }
  ~BackendWebGpu() override {
    mnexus_device_.Shutdown();
  }

  // ----------------------------------------------------------------------------------------------
  // Surface lifecycle.

  void OnDisplayChanged() override {
    // No-op for now.
  }

  void OnSurfaceDestroyed() override {
    MBASE_ASSERT(bool(surface_));

    mnexus_device_.OnWgpuSurfaceUnconfigured();

    surface_.Unconfigure();
    // Keep the wgpu::Surface alive so that OnSurfaceRecreated can reuse it
    // when the native window handle hasn't changed (e.g. resize). Destroying
    // and recreating the underlying DXGI swapchain on every resize can fail
    // if Dawn's D3D12 backend hasn't fully released the old swapchain yet.
  }

  void OnSurfaceRecreated(mnexus::SurfaceSourceDesc const& surface_source_desc) override {
    constexpr wgpu::TextureFormat kPreferredFormat = wgpu::TextureFormat::RGBA8Unorm;
    constexpr wgpu::TextureUsage kTextureUsage = wgpu::TextureUsage::RenderAttachment;

    // Check if we can reuse the existing wgpu::Surface. We can reuse it when the
    // native window handle hasn't changed (e.g. desktop resize). Reusing avoids
    // destroying and recreating the underlying DXGI swapchain, which can fail on
    // Dawn's D3D12 backend if the old swapchain hasn't been fully released yet.
    bool const can_reuse_surface =
      surface_ && surface_source_desc.window_handle == last_surface_window_handle_;

    if (can_reuse_surface) {
      // Surface already unconfigured by OnSurfaceDestroyed; just reconfigure below.
    } else {
      // Destroy old surface if one exists (native window changed).
      if (surface_) {
        surface_ = nullptr;
      }

      // Create a new surface.
      {
#if MBASE_PLATFORM_WINDOWS
        wgpu::SurfaceSourceWindowsHWND chained_desc{};
        chained_desc.hinstance = reinterpret_cast<void*>(surface_source_desc.instance_handle);
        chained_desc.hwnd = reinterpret_cast<void*>(surface_source_desc.window_handle);
#elif MBASE_PLATFORM_ANDROID
        wgpu::SurfaceSourceAndroidNativeWindow chained_desc{};
        chained_desc.window = reinterpret_cast<void*>(surface_source_desc.window_handle);
#elif MBASE_PLATFORM_WEB
        wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector chained_desc{};
        chained_desc.selector = wgpu::StringView(surface_source_desc.canvas_selector, surface_source_desc.canvas_selector_length);
#endif

        wgpu::SurfaceDescriptor desc{};
        desc.nextInChain = &chained_desc;

        surface_ = instance_.CreateSurface(&desc);
      }

      wgpu::SurfaceCapabilities surface_caps{};
      surface_.GetCapabilities(adapter_, &surface_caps);

      // Log surface capabilities.
      {
        MBASE_LOG_INFO("Surface Capabilities:");
        MBASE_LOG_INFO("Format count: {}", surface_caps.formatCount);
        for (uint32_t i = 0; i < surface_caps.formatCount; ++i) {
          MBASE_LOG_INFO("  Format[{}]: {}", i, surface_caps.formats[i]);
        }
        MBASE_LOG_INFO("Present mode count: {}", surface_caps.presentModeCount);
        for (uint32_t i = 0; i < surface_caps.presentModeCount; ++i) {
          MBASE_LOG_INFO("  PresentMode[{}]: {}", i, surface_caps.presentModes[i]);
        }
        MBASE_LOG_INFO("Alpha mode count: {}", surface_caps.alphaModeCount);
        for (uint32_t i = 0; i < surface_caps.alphaModeCount; ++i) {
          MBASE_LOG_INFO("  AlphaMode[{}]: {}", i, surface_caps.alphaModes[i]);
        }
      }

      // Assert that the surface supports the preferred format.
      {
        bool format_supported = false;
        for (uint32_t i = 0; i < surface_caps.formatCount; ++i) {
          if (surface_caps.formats[i] == kPreferredFormat) {
            format_supported = true;
            break;
          }
        }

        MBASE_ASSERT_MSG(
          format_supported,
          "Preferred surface format {} is not supported by the surface!",
          kPreferredFormat
        );
      }
    }

    uint32_t width = 0;
    uint32_t height = 0;
    {
#if MBASE_PLATFORM_WINDOWS
      RECT rect;
      GetClientRect(reinterpret_cast<HWND>(surface_source_desc.window_handle), &rect);

      width = static_cast<uint32_t>(rect.right - rect.left);
      height = static_cast<uint32_t>(rect.bottom - rect.top);
#elif MBASE_PLATFORM_WEB
      std::string canvas_selector(
        surface_source_desc.canvas_selector,
        surface_source_desc.canvas_selector_length
      );

      int w = 0, h = 0;
      emscripten_get_canvas_element_size(canvas_selector.c_str(), &w, &h);

      width = static_cast<uint32_t>(w);
      height = static_cast<uint32_t>(h);
#else
# error Unsupported Platform
#endif
    }

    // Clamp to 1x1 minimum (0x0 is invalid in WebGPU surface configuration).
    width = std::max(width, 1u);
    height = std::max(height, 1u);

    MBASE_LOG_INFO("Configuring surface with size {}x{}...", width, height);

    wgpu::SurfaceConfiguration surface_config {
      .device = device_,
      .format = kPreferredFormat,
      .usage = kTextureUsage,
      .width = width,
      .height = height,
      .viewFormatCount = 0,
      .viewFormats = nullptr,
      .alphaMode = wgpu::CompositeAlphaMode::Auto,
      .presentMode = wgpu::PresentMode::Fifo,
    };

    surface_.Configure(&surface_config);

    last_surface_window_handle_ = surface_source_desc.window_handle;
    mnexus_device_.OnWgpuSurfaceConfigured(surface_config);
  }

  // ----------------------------------------------------------------------------------------------
  // Presentation.

  void OnPresentPrologue() override {
    MBASE_ASSERT(bool(surface_));

    wgpu::SurfaceTexture surface_texture;
    surface_.GetCurrentTexture(&surface_texture);

    MBASE_ASSERT(
      surface_texture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal ||
      surface_texture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal
    );

    mnexus_device_.OnWgpuSurfaceTextureAcquired(surface_texture.texture);

    return;

    wgpu::RenderPassColorAttachment attachment {
      .view = surface_texture.texture.CreateView(),
      .resolveTarget = nullptr,
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = { 1.0f, 0.0f, 0.0f, 1.0f },
    };

    wgpu::RenderPassDescriptor desc {
        .colorAttachmentCount = 1,
        .colorAttachments = &attachment,
        .depthStencilAttachment = nullptr,
    };

    wgpu::CommandEncoder encoder = device_.CreateCommandEncoder(nullptr);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&desc);
    pass.End();

    wgpu::CommandBuffer command_buffer = encoder.Finish();
    device_.GetQueue().Submit(1, &command_buffer);

    
  }

  void OnPresentEpilogue() override {
#if !MBASE_PLATFORM_WEB
    // On Emscripten, presentation happens automatically via requestAnimationFrame.
    // wgpuSurfacePresent is not supported.
    surface_.Present();
#endif

    mnexus_device_.OnWgpuSurfaceTextureReleased();
  }

  // ----------------------------------------------------------------------------------------------
  // Resource creation.

  mnexus::IDevice* GetDevice() override {
    return &mnexus_device_;
  }

private:
  wgpu::Instance instance_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;

  ResourceStorage resource_storage_;
  MnexusDeviceWebGpu mnexus_device_;

  wgpu::Surface surface_;
  uint64_t last_surface_window_handle_ = 0;
};

std::unique_ptr<IBackendWebGpu> IBackendWebGpu::Create() {
  std::vector<wgpu::InstanceFeatureName> required_features = {
    wgpu::InstanceFeatureName::TimedWaitAny
  };

#if MNEXUS_INTERNAL_USE_DAWN
  required_features.emplace_back(wgpu::InstanceFeatureName::ShaderSourceSPIRV);
#endif

  wgpu::InstanceDescriptor instance_desc {
    .requiredFeatureCount = static_cast<uint32_t>(required_features.size()),
    .requiredFeatures = required_features.data()
  };

  wgpu::Instance instance = wgpu::CreateInstance(&instance_desc);

  wgpu::Adapter adapter;
  {
    wgpu::Future f1 = instance.RequestAdapter(
      nullptr,
      wgpu::CallbackMode::WaitAnyOnly,
      [&adapter](wgpu::RequestAdapterStatus status, wgpu::Adapter a, wgpu::StringView message) {
        if (status != wgpu::RequestAdapterStatus::Success) {
          MBASE_LOG_ERROR("RequestAdapter failed: {}", message);
          return;
        }
        adapter = std::move(a);
      }
    );

    instance.WaitAny(f1, UINT64_MAX);

    if (!adapter) return nullptr;
  }

  {
    wgpu::AdapterInfo info {};
    adapter.GetInfo(&info);

    MBASE_LOG_INFO("Adapter Info:");
    MBASE_LOG_INFO("  Name: {}", info.device);
    MBASE_LOG_INFO("  VendorID: {}", info.vendorID);
    MBASE_LOG_INFO("  Architecture: {}", info.architecture);
    MBASE_LOG_INFO("  Description: {}", info.description);
  }

  wgpu::Device device;
  {
    wgpu::DeviceDescriptor desc {};
    desc.SetUncapturedErrorCallback(
      [](const wgpu::Device&, wgpu::ErrorType errorType, wgpu::StringView message) {
        MBASE_LOG_ERROR("Uncaptured error ({}): {}", errorType, message);
        mbase::Trap();
      }
    );

    wgpu::Future f2 = adapter.RequestDevice(
      &desc,
      wgpu::CallbackMode::WaitAnyOnly,
      [&device](wgpu::RequestDeviceStatus status, wgpu::Device d, wgpu::StringView message) {
        if (status != wgpu::RequestDeviceStatus::Success) {
          MBASE_LOG_ERROR("RequestDevice failed: {}", message);
          return;
        }
        device = std::move(d);
      }
    );

    instance.WaitAny(f2, UINT64_MAX);

    if (!device) return nullptr;
  }

  auto backend = std::make_unique<BackendWebGpu>(
    std::move(instance),
    std::move(adapter),
    std::move(device)
  );
  return backend;
}

} // namespace mnexus_backend::webgpu
