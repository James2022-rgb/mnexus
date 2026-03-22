// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-command_list.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/resource_storage.h"

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
  MBASE_LOG_WARN("Vulkan backend: PushDebugGroup() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::PopDebugGroup() {
  MBASE_LOG_WARN("Vulkan backend: PopDebugGroup() not implemented");
}

//
// Transfer
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::ClearTexture(
  mnexus::TextureHandle /*texture_handle*/,
  mnexus::TextureSubresourceRange const& /*subresource_range*/,
  mnexus::ClearValue const& /*clear_value*/
) {
  MBASE_LOG_WARN("Vulkan backend: ClearTexture() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::CopyBufferToTexture(
  mnexus::BufferHandle /*src_buffer_handle*/,
  uint32_t /*src_buffer_offset*/,
  mnexus::TextureHandle /*dst_texture_handle*/,
  mnexus::TextureSubresourceRange const& /*dst_subresource_range*/,
  mnexus::Extent3d const& /*copy_extent*/
) {
  MBASE_LOG_WARN("Vulkan backend: CopyBufferToTexture() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::CopyTextureToBuffer(
  mnexus::TextureHandle /*src_texture_handle*/,
  mnexus::TextureSubresourceRange const& /*src_subresource_range*/,
  mnexus::BufferHandle /*dst_buffer_handle*/,
  uint32_t /*dst_buffer_offset*/,
  mnexus::Extent3d const& /*copy_extent*/
) {
  MBASE_LOG_WARN("Vulkan backend: CopyTextureToBuffer() not implemented");
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
  MBASE_LOG_WARN("Vulkan backend: BlitTexture() not implemented");
}

//
// Compute
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindExplicitComputePipeline(
  mnexus::ComputePipelineHandle compute_pipeline_handle
) {
  auto const pool_handle = container::ResourceHandle::FromU64(compute_pipeline_handle.Get());
  auto [hot, cold, lock] = resource_storage_->compute_pipelines.GetConstRefWithSharedLockGuard(pool_handle);
  encoder_.BindComputePipeline(
    hot.vk_compute_pipeline.handle(),
    hot.vk_pipeline_layout,
    hot.pipeline_layout_ref->descriptor_set_layouts.data(),
    static_cast<uint32_t>(hot.pipeline_layout_ref->descriptor_set_layouts.size())
  );

  // Track referenced resources for submit-time stamping.
  referenced_resources_.push_back(pool_handle);
  referenced_resources_.push_back(container::ResourceHandle::FromU64(cold.program_handle.Get()));
  referenced_resources_.push_back(container::ResourceHandle::FromU64(cold.shader_module_handle.Get()));
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
  auto const pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());
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
  auto const pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());
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
  MBASE_LOG_WARN("Vulkan backend: BindSampledTexture() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindSampler(
  mnexus::BindingId const& /*id*/,
  mnexus::SamplerHandle /*sampler_handle*/
) {
  MBASE_LOG_WARN("Vulkan backend: BindSampler() not implemented");
}

//
// Explicit Pipeline Binding
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindExplicitRenderPipeline(
  mnexus::RenderPipelineHandle /*render_pipeline_handle*/
) {
  MBASE_LOG_WARN("Vulkan backend: BindExplicitRenderPipeline() not implemented");
}

//
// Render Pass
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BeginRenderPass(
  mnexus::RenderPassDesc const& /*desc*/
) {
  MBASE_LOG_WARN("Vulkan backend: BeginRenderPass() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::EndRenderPass() {
  MBASE_LOG_WARN("Vulkan backend: EndRenderPass() not implemented");
}

//
// Render State (auto-generation path)
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindRenderProgram(
  mnexus::ProgramHandle /*program_handle*/
) {
  MBASE_LOG_WARN("Vulkan backend: BindRenderProgram() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetVertexInputLayout(
  mnexus::container::ArrayProxy<mnexus::VertexInputBindingDesc const> /*bindings*/,
  mnexus::container::ArrayProxy<mnexus::VertexInputAttributeDesc const> /*attributes*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetVertexInputLayout() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindVertexBuffer(
  uint32_t /*binding*/,
  mnexus::BufferHandle /*buffer_handle*/,
  uint64_t /*offset*/
) {
  MBASE_LOG_WARN("Vulkan backend: BindVertexBuffer() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::BindIndexBuffer(
  mnexus::BufferHandle /*buffer_handle*/,
  uint64_t /*offset*/,
  mnexus::IndexType /*index_type*/
) {
  MBASE_LOG_WARN("Vulkan backend: BindIndexBuffer() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetPrimitiveTopology(
  mnexus::PrimitiveTopology /*topology*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetPrimitiveTopology() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetPolygonMode(
  mnexus::PolygonMode /*mode*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetPolygonMode() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetCullMode(
  mnexus::CullMode /*cull_mode*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetCullMode() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetFrontFace(
  mnexus::FrontFace /*front_face*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetFrontFace() not implemented");
}

// Depth

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetDepthTestEnabled(bool /*enabled*/) {
  MBASE_LOG_WARN("Vulkan backend: SetDepthTestEnabled() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetDepthWriteEnabled(bool /*enabled*/) {
  MBASE_LOG_WARN("Vulkan backend: SetDepthWriteEnabled() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetDepthCompareOp(
  mnexus::CompareOp /*op*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetDepthCompareOp() not implemented");
}

// Stencil

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetStencilTestEnabled(bool /*enabled*/) {
  MBASE_LOG_WARN("Vulkan backend: SetStencilTestEnabled() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetStencilFrontOps(
  mnexus::StencilOp /*fail*/, mnexus::StencilOp /*pass*/,
  mnexus::StencilOp /*depth_fail*/, mnexus::CompareOp /*compare*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetStencilFrontOps() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetStencilBackOps(
  mnexus::StencilOp /*fail*/, mnexus::StencilOp /*pass*/,
  mnexus::StencilOp /*depth_fail*/, mnexus::CompareOp /*compare*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetStencilBackOps() not implemented");
}

// Per-attachment blend

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetBlendEnabled(
  uint32_t /*attachment*/, bool /*enabled*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetBlendEnabled() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetBlendFactors(
  uint32_t /*attachment*/,
  mnexus::BlendFactor /*src_color*/, mnexus::BlendFactor /*dst_color*/, mnexus::BlendOp /*color_op*/,
  mnexus::BlendFactor /*src_alpha*/, mnexus::BlendFactor /*dst_alpha*/, mnexus::BlendOp /*alpha_op*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetBlendFactors() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetColorWriteMask(
  uint32_t /*attachment*/, mnexus::ColorWriteMask /*mask*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetColorWriteMask() not implemented");
}

//
// Draw
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::Draw(
  uint32_t /*vertex_count*/, uint32_t /*instance_count*/,
  uint32_t /*first_vertex*/, uint32_t /*first_instance*/
) {
  MBASE_LOG_WARN("Vulkan backend: Draw() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::DrawIndexed(
  uint32_t /*index_count*/, uint32_t /*instance_count*/,
  uint32_t /*first_index*/, int32_t /*vertex_offset*/, uint32_t /*first_instance*/
) {
  MBASE_LOG_WARN("Vulkan backend: DrawIndexed() not implemented");
}

//
// Viewport / Scissor
//

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetViewport(
  float /*x*/, float /*y*/, float /*width*/, float /*height*/,
  float /*min_depth*/, float /*max_depth*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetViewport() not implemented");
}

MNEXUS_NO_THROW void MNEXUS_CALL MnexusCommandListVulkan::SetScissor(
  int32_t /*x*/, int32_t /*y*/, uint32_t /*width*/, uint32_t /*height*/
) {
  MBASE_LOG_WARN("Vulkan backend: SetScissor() not implemented");
}

} // namespace mnexus_backend::vulkan
