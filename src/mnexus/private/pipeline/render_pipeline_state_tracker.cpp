// TU header --------------------------------------------
#include "pipeline/render_pipeline_state_tracker.h"

namespace pipeline {

void RenderPipelineStateTracker::SetProgram(mnexus::ProgramHandle program) {
  if (program_ != program) {
    program_ = program;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetVertexInputLayout(
  mbase::SmallVector<mnexus::VertexInputBindingDesc, 4> bindings,
  mbase::SmallVector<mnexus::VertexInputAttributeDesc, 8> attributes
) {
  vertex_bindings_ = std::move(bindings);
  vertex_attributes_ = std::move(attributes);
  dirty_ = true;
}

void RenderPipelineStateTracker::SetPrimitiveTopology(mnexus::PrimitiveTopology topology) {
  uint8_t const value = static_cast<uint8_t>(topology);
  if (per_draw_.ia_primitive_topology != value) {
    per_draw_.ia_primitive_topology = value;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetPolygonMode(mnexus::PolygonMode mode) {
  uint8_t const value = static_cast<uint8_t>(mode);
  if (per_draw_.raster_polygon_mode != value) {
    per_draw_.raster_polygon_mode = value;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetCullMode(mnexus::CullMode cull_mode) {
  uint8_t const value = static_cast<uint8_t>(cull_mode);
  if (per_draw_.raster_cull_mode != value) {
    per_draw_.raster_cull_mode = value;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetFrontFace(mnexus::FrontFace front_face) {
  uint8_t const value = static_cast<uint8_t>(front_face);
  if (per_draw_.raster_front_face != value) {
    per_draw_.raster_front_face = value;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetRenderTargetConfig(
  mbase::SmallVector<MnFormat, 4> color_formats,
  MnFormat depth_stencil_format,
  uint32_t sample_count
) {
  color_formats_ = std::move(color_formats);
  depth_stencil_format_ = depth_stencil_format;
  sample_count_ = sample_count;

  // Initialize per-attachment state to default for each color attachment.
  per_attachment_.resize(color_formats_.size());
  for (auto& att : per_attachment_) {
    att = PerAttachmentFixedFunctionStaticState {};
  }

  dirty_ = true;
}

RenderPipelineCacheKey RenderPipelineStateTracker::BuildCacheKey() const {
  RenderPipelineCacheKey key;
  key.program = program_;
  key.per_draw = per_draw_;
  key.per_attachment = per_attachment_;
  key.vertex_bindings = vertex_bindings_;
  key.vertex_attributes = vertex_attributes_;
  key.color_formats = color_formats_;
  key.depth_stencil_format = depth_stencil_format_;
  key.sample_count = sample_count_;
  return key;
}

void RenderPipelineStateTracker::Reset() {
  dirty_ = true;
  program_ = mnexus::ProgramHandle::Invalid();
  per_draw_ = PerDrawFixedFunctionStaticState {};
  per_attachment_.clear();
  vertex_bindings_.clear();
  vertex_attributes_.clear();
  color_formats_.clear();
  depth_stencil_format_ = MnFormat::kUndefined;
  sample_count_ = 1;
}

} // namespace pipeline
