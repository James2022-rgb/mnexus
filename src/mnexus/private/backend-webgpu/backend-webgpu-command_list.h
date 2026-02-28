#pragma once

// c++ headers ------------------------------------------
#include <mutex>
#include <optional>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/container.h"

#include "mnexus/public/mnexus.h"
#include "mnexus/public/render_state_event_log.h"

// project headers --------------------------------------
#include "backend-webgpu/backend-webgpu-buffer.h"
#include "backend-webgpu/backend-webgpu-compute_pipeline.h"
#include "backend-webgpu/backend-webgpu-render_pipeline.h"
#include "backend-webgpu/backend-webgpu-sampler.h"
#include "backend-webgpu/backend-webgpu-shader.h"
#include "backend-webgpu/backend-webgpu-texture.h"
#include "backend-webgpu/include_dawn.h"

#include "binding/state_tracker.h"

#include "container/generational_pool.h"

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
  );
  ~MnexusCommandListWebGpu() override = default;
  MBASE_DISALLOW_COPY_MOVE(MnexusCommandListWebGpu);

  wgpu::CommandEncoder const& GetWgpuCommandEncoder() const {
    return wgpu_command_encoder_;
  }

  // --------------------------------------------------------------------------------------------------
  // mnexus::ICommandList implementation
  //

  IMPL_VAPI(void, End);

  //
  // Diagnostics
  //

  IMPL_VAPI(mnexus::RenderStateEventLog&, GetStateEventLog);

  //
  // Transfer
  //

  IMPL_VAPI(void, ClearTexture,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ClearValue const& clear_value
  );

  IMPL_VAPI(void, CopyBufferToTexture,
    mnexus::BufferHandle src_buffer_handle,
    uint32_t src_buffer_offset,
    mnexus::TextureHandle dst_texture_handle,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Extent3d const& copy_extent
  );

  IMPL_VAPI(void, CopyTextureToBuffer,
    mnexus::TextureHandle src_texture_handle,
    mnexus::TextureSubresourceRange const& src_subresource_range,
    mnexus::BufferHandle dst_buffer_handle,
    uint32_t dst_buffer_offset,
    mnexus::Extent3d const& copy_extent
  );

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
  );

  //
  // Compute
  //

  IMPL_VAPI(void, BindExplicitComputePipeline, mnexus::ComputePipelineHandle compute_pipeline_handle);

  IMPL_VAPI(void, DispatchCompute,
    uint32_t workgroup_count_x,
    uint32_t workgroup_count_y,
    uint32_t workgroup_count_z
  );

  //
  // Resource Binding
  //

  IMPL_VAPI(void, BindUniformBuffer,
    mnexus::BindingId const& id,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  );

  IMPL_VAPI(void, BindStorageBuffer,
    mnexus::BindingId const& id,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  );

  IMPL_VAPI(void, BindSampledTexture,
    mnexus::BindingId const& id,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range
  );

  IMPL_VAPI(void, BindSampler,
    mnexus::BindingId const& id,
    mnexus::SamplerHandle sampler_handle
  );

  //
  // Explicit Pipeline Binding
  //

  IMPL_VAPI(void, BindExplicitRenderPipeline, mnexus::RenderPipelineHandle render_pipeline_handle);

  //
  // Render Pass
  //

  IMPL_VAPI(void, BeginRenderPass, mnexus::RenderPassDesc const& desc);

  IMPL_VAPI(void, EndRenderPass);

  //
  // Render State (auto-generation path)
  //

  IMPL_VAPI(void, BindRenderProgram, mnexus::ProgramHandle program_handle);

  IMPL_VAPI(void, SetVertexInputLayout,
    mnexus::container::ArrayProxy<mnexus::VertexInputBindingDesc const> bindings,
    mnexus::container::ArrayProxy<mnexus::VertexInputAttributeDesc const> attributes
  );

  IMPL_VAPI(void, BindVertexBuffer,
    uint32_t binding,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset
  );

  IMPL_VAPI(void, BindIndexBuffer,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    mnexus::IndexType index_type
  );

  IMPL_VAPI(void, SetPrimitiveTopology, mnexus::PrimitiveTopology topology);
  IMPL_VAPI(void, SetPolygonMode, mnexus::PolygonMode mode);
  IMPL_VAPI(void, SetCullMode, mnexus::CullMode cull_mode);
  IMPL_VAPI(void, SetFrontFace, mnexus::FrontFace front_face);

  // Depth
  IMPL_VAPI(void, SetDepthTestEnabled, bool enabled);
  IMPL_VAPI(void, SetDepthWriteEnabled, bool enabled);
  IMPL_VAPI(void, SetDepthCompareOp, mnexus::CompareOp op);

  // Stencil
  IMPL_VAPI(void, SetStencilTestEnabled, bool enabled);

  IMPL_VAPI(void, SetStencilFrontOps,
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  );

  IMPL_VAPI(void, SetStencilBackOps,
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  );

  // Per-attachment blend
  IMPL_VAPI(void, SetBlendEnabled, uint32_t attachment, bool enabled);

  IMPL_VAPI(void, SetBlendFactors,
    uint32_t attachment,
    mnexus::BlendFactor src_color, mnexus::BlendFactor dst_color, mnexus::BlendOp color_op,
    mnexus::BlendFactor src_alpha, mnexus::BlendFactor dst_alpha, mnexus::BlendOp alpha_op
  );

  IMPL_VAPI(void, SetColorWriteMask, uint32_t attachment, mnexus::ColorWriteMask mask);

  //
  // Draw
  //

  IMPL_VAPI(void, Draw,
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t first_vertex,
    uint32_t first_instance
  );

  IMPL_VAPI(void, DrawIndexed,
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t vertex_offset,
    uint32_t first_instance
  );

  //
  // Viewport / Scissor
  //

  IMPL_VAPI(void, SetViewport,
    float x, float y, float width, float height,
    float min_depth, float max_depth
  );

  IMPL_VAPI(void, SetScissor,
    int32_t x, int32_t y, uint32_t width, uint32_t height
  );

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

  void EndCurrentComputePass();
  void EndCurrentRenderPass();

  /// Resolves the render pipeline from the state tracker, binds it and any dirty bind groups/vertex buffers.
  void ResolveRenderPipelineAndBindState();

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
  mnexus::RenderStateEventLog render_state_event_log_;
  mbase::SmallVector<BoundVertexBuffer, 4> bound_vertex_buffers_;
  BoundIndexBuffer bound_index_buffer_;

  binding::BindGroupStateTracker bind_group_state_tracker_;
};

} // namespace mnexus_backend::webgpu
