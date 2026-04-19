#pragma once

// c++ headers ------------------------------------------
#include <vector>

// public project headers -------------------------------
#include "mbase/public/array_proxy.h"

#include "mnexus/public/mnexus.h"
#include "mnexus/public/render_state_event_log.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/command/command_encoder.h"
#include "backend-vulkan/command/image_layout_tracker.h"

namespace mnexus_backend::vulkan {

struct ResourceStorage;

class MnexusCommandListVulkan : public mnexus::ICommandList {
public:
  MnexusCommandListVulkan(CommandEncoder encoder, ResourceStorage* resource_storage);
  ~MnexusCommandListVulkan() override = default;

  [[nodiscard]] CommandEncoder& encoder() { return encoder_; }
  [[nodiscard]] mbase::ArrayProxy<resource_pool::ResourceHandle const> GetReferencedResources() const { return referenced_resources_; }

  // --------------------------------------------------------------------------------------------------
  // mnexus::ICommandList implementation
  //

  MNEXUS_NO_THROW void MNEXUS_CALL End() override;

  //
  // Diagnostics
  //

  MNEXUS_NO_THROW mnexus::RenderStateEventLog& MNEXUS_CALL GetStateEventLog() override;

  //
  // Debug Markers
  //

  MNEXUS_NO_THROW void MNEXUS_CALL PushDebugGroup(
    mnexus::container::ArrayProxy<char const> name, float const* color
  ) override;
  MNEXUS_NO_THROW void MNEXUS_CALL PopDebugGroup() override;

  //
  // Transfer
  //

  MNEXUS_NO_THROW void MNEXUS_CALL ClearTexture(
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ClearValue const& clear_value
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL CopyBufferToTexture(
    mnexus::BufferHandle src_buffer_handle,
    uint32_t src_buffer_offset,
    mnexus::TextureHandle dst_texture_handle,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Extent3d const& copy_extent
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL CopyTextureToBuffer(
    mnexus::TextureHandle src_texture_handle,
    mnexus::TextureSubresourceRange const& src_subresource_range,
    mnexus::BufferHandle dst_buffer_handle,
    uint32_t dst_buffer_offset,
    mnexus::Extent3d const& copy_extent
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL BlitTexture(
    mnexus::TextureHandle src_texture_handle,
    mnexus::TextureSubresourceRange const& src_subresource_range,
    mnexus::Offset3d const& src_offset,
    mnexus::Extent3d const& src_extent,
    mnexus::TextureHandle dst_texture_handle,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Offset3d const& dst_offset,
    mnexus::Extent3d const& dst_extent,
    mnexus::Filter filter
  ) override;

  //
  // Compute
  //

  MNEXUS_NO_THROW void MNEXUS_CALL BindExplicitComputePipeline(
    mnexus::ComputePipelineHandle compute_pipeline_handle
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL DispatchCompute(
    uint32_t workgroup_count_x,
    uint32_t workgroup_count_y,
    uint32_t workgroup_count_z
  ) override;

  //
  // Resource Binding
  //

  MNEXUS_NO_THROW void MNEXUS_CALL BindUniformBuffer(
    mnexus::BindingId const& id,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL BindStorageBuffer(
    mnexus::BindingId const& id,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL BindSampledTexture(
    mnexus::BindingId const& id,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL BindSampler(
    mnexus::BindingId const& id,
    mnexus::SamplerHandle sampler_handle
  ) override;

  //
  // Explicit Pipeline Binding
  //

  MNEXUS_NO_THROW void MNEXUS_CALL BindExplicitRenderPipeline(
    mnexus::RenderPipelineHandle render_pipeline_handle
  ) override;

  //
  // Render Pass
  //

  MNEXUS_NO_THROW void MNEXUS_CALL BeginRenderPass(mnexus::RenderPassDesc const& desc) override;
  MNEXUS_NO_THROW void MNEXUS_CALL EndRenderPass() override;

  //
  // Render State (auto-generation path)
  //

  MNEXUS_NO_THROW void MNEXUS_CALL BindRenderProgram(mnexus::ProgramHandle program_handle) override;

  MNEXUS_NO_THROW void MNEXUS_CALL SetVertexInputLayout(
    mnexus::container::ArrayProxy<mnexus::VertexInputBindingDesc const> bindings,
    mnexus::container::ArrayProxy<mnexus::VertexInputAttributeDesc const> attributes
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL BindVertexBuffer(
    uint32_t binding,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL BindIndexBuffer(
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    mnexus::IndexType index_type
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL SetPrimitiveTopology(mnexus::PrimitiveTopology topology) override;
  MNEXUS_NO_THROW void MNEXUS_CALL SetPolygonMode(mnexus::PolygonMode mode) override;
  MNEXUS_NO_THROW void MNEXUS_CALL SetCullMode(mnexus::CullMode cull_mode) override;
  MNEXUS_NO_THROW void MNEXUS_CALL SetFrontFace(mnexus::FrontFace front_face) override;

  // Depth
  MNEXUS_NO_THROW void MNEXUS_CALL SetDepthTestEnabled(bool enabled) override;
  MNEXUS_NO_THROW void MNEXUS_CALL SetDepthWriteEnabled(bool enabled) override;
  MNEXUS_NO_THROW void MNEXUS_CALL SetDepthCompareOp(mnexus::CompareOp op) override;

  // Stencil
  MNEXUS_NO_THROW void MNEXUS_CALL SetStencilTestEnabled(bool enabled) override;

  MNEXUS_NO_THROW void MNEXUS_CALL SetStencilFrontOps(
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL SetStencilBackOps(
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  ) override;

  // Per-attachment blend
  MNEXUS_NO_THROW void MNEXUS_CALL SetBlendEnabled(uint32_t attachment, bool enabled) override;

  MNEXUS_NO_THROW void MNEXUS_CALL SetBlendFactors(
    uint32_t attachment,
    mnexus::BlendFactor src_color, mnexus::BlendFactor dst_color, mnexus::BlendOp color_op,
    mnexus::BlendFactor src_alpha, mnexus::BlendFactor dst_alpha, mnexus::BlendOp alpha_op
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL SetColorWriteMask(uint32_t attachment, mnexus::ColorWriteMask mask) override;

  //
  // Draw
  //

  MNEXUS_NO_THROW void MNEXUS_CALL Draw(
    uint32_t vertex_count, uint32_t instance_count,
    uint32_t first_vertex, uint32_t first_instance
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL DrawIndexed(
    uint32_t index_count, uint32_t instance_count,
    uint32_t first_index, int32_t vertex_offset, uint32_t first_instance
  ) override;

  //
  // Viewport / Scissor
  //

  MNEXUS_NO_THROW void MNEXUS_CALL SetViewport(
    float x, float y, float width, float height,
    float min_depth, float max_depth
  ) override;

  MNEXUS_NO_THROW void MNEXUS_CALL SetScissor(
    int32_t x, int32_t y, uint32_t width, uint32_t height
  ) override;

private:
  CommandEncoder encoder_;
  ResourceStorage* resource_storage_ = nullptr;
  std::vector<resource_pool::ResourceHandle> referenced_resources_;
  ImageLayoutTracker image_layout_tracker_;
  PendingPipelineBarrier pending_pipeline_barrier_;
  mnexus::RenderStateEventLog render_state_event_log_;
};

} // namespace mnexus_backend::vulkan
