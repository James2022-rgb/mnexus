// TU header --------------------------------------------
#include "pipeline/render_pipeline_state_tracker.h"

// public project headers -------------------------------
#include "mbase/public/assert.h"

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

void RenderPipelineStateTracker::SetDepthTestEnabled(bool enabled) {
  uint8_t const value = enabled ? 1 : 0;
  if (per_draw_.depth_test_enabled != value) {
    per_draw_.depth_test_enabled = value;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetDepthWriteEnabled(bool enabled) {
  uint8_t const value = enabled ? 1 : 0;
  if (per_draw_.depth_write_enabled != value) {
    per_draw_.depth_write_enabled = value;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetDepthCompareOp(mnexus::CompareOp op) {
  uint8_t const value = static_cast<uint8_t>(op);
  if (per_draw_.depth_compare_op != value) {
    per_draw_.depth_compare_op = value;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetStencilTestEnabled(bool enabled) {
  uint8_t const value = enabled ? 1 : 0;
  if (per_draw_.stencil_test_enabled != value) {
    per_draw_.stencil_test_enabled = value;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetStencilFrontOps(
  mnexus::StencilOp fail, mnexus::StencilOp pass,
  mnexus::StencilOp depth_fail, mnexus::CompareOp compare
) {
  uint8_t const f = static_cast<uint8_t>(fail);
  uint8_t const p = static_cast<uint8_t>(pass);
  uint8_t const d = static_cast<uint8_t>(depth_fail);
  uint8_t const c = static_cast<uint8_t>(compare);
  if (per_draw_.stencil_front_fail_op != f ||
      per_draw_.stencil_front_pass_op != p ||
      per_draw_.stencil_front_depth_fail_op != d ||
      per_draw_.stencil_front_compare_op != c) {
    per_draw_.stencil_front_fail_op = f;
    per_draw_.stencil_front_pass_op = p;
    per_draw_.stencil_front_depth_fail_op = d;
    per_draw_.stencil_front_compare_op = c;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetStencilBackOps(
  mnexus::StencilOp fail, mnexus::StencilOp pass,
  mnexus::StencilOp depth_fail, mnexus::CompareOp compare
) {
  uint8_t const f = static_cast<uint8_t>(fail);
  uint8_t const p = static_cast<uint8_t>(pass);
  uint8_t const d = static_cast<uint8_t>(depth_fail);
  uint8_t const c = static_cast<uint8_t>(compare);
  if (per_draw_.stencil_back_fail_op != f ||
      per_draw_.stencil_back_pass_op != p ||
      per_draw_.stencil_back_depth_fail_op != d ||
      per_draw_.stencil_back_compare_op != c) {
    per_draw_.stencil_back_fail_op = f;
    per_draw_.stencil_back_pass_op = p;
    per_draw_.stencil_back_depth_fail_op = d;
    per_draw_.stencil_back_compare_op = c;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetBlendEnabled(uint32_t attachment, bool enabled) {
  MBASE_ASSERT(attachment < per_attachment_.size());
  uint8_t const value = enabled ? 1 : 0;
  if (per_attachment_[attachment].blend_enabled != value) {
    per_attachment_[attachment].blend_enabled = value;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetBlendFactors(
  uint32_t attachment,
  mnexus::BlendFactor src_color, mnexus::BlendFactor dst_color, mnexus::BlendOp color_op,
  mnexus::BlendFactor src_alpha, mnexus::BlendFactor dst_alpha, mnexus::BlendOp alpha_op
) {
  MBASE_ASSERT(attachment < per_attachment_.size());
  auto& att = per_attachment_[attachment];
  uint8_t const sc = static_cast<uint8_t>(src_color);
  uint8_t const dc = static_cast<uint8_t>(dst_color);
  uint8_t const co = static_cast<uint8_t>(color_op);
  uint8_t const sa = static_cast<uint8_t>(src_alpha);
  uint8_t const da = static_cast<uint8_t>(dst_alpha);
  uint8_t const ao = static_cast<uint8_t>(alpha_op);
  if (att.blend_src_color_factor != sc ||
      att.blend_dst_color_factor != dc ||
      att.blend_color_blend_op != co ||
      att.blend_src_alpha_factor != sa ||
      att.blend_dst_alpha_factor != da ||
      att.blend_alpha_blend_op != ao) {
    att.blend_src_color_factor = sc;
    att.blend_dst_color_factor = dc;
    att.blend_color_blend_op = co;
    att.blend_src_alpha_factor = sa;
    att.blend_dst_alpha_factor = da;
    att.blend_alpha_blend_op = ao;
    dirty_ = true;
  }
}

void RenderPipelineStateTracker::SetColorWriteMask(uint32_t attachment, mnexus::ColorWriteMask mask) {
  MBASE_ASSERT(attachment < per_attachment_.size());
  uint8_t const value = static_cast<uint8_t>(mask);
  if (per_attachment_[attachment].color_write_mask != value) {
    per_attachment_[attachment].color_write_mask = value;
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
