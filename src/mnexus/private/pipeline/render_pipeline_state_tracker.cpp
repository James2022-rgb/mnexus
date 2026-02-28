// TU header --------------------------------------------
#include "pipeline/render_pipeline_state_tracker.h"

// c++ headers ------------------------------------------
#include <cstdio>

#include <sstream>

// public project headers -------------------------------
#include "mbase/public/assert.h"

#include "mnexus/public/render_state_event_log.h"

namespace pipeline {

// ----------------------------------------------------------------------------------------------------
// Event recording helper
//

void RenderPipelineStateTracker::SetEventLog(mnexus::RenderStateEventLog* log) {
  event_log_ = log;
}

void RenderPipelineStateTracker::RecordEvent(mnexus::RenderStateEventTag tag) {
  if (event_log_ && event_log_->IsEnabled()) {
    event_log_->Record(tag, BuildSnapshot());
  }
}

// ----------------------------------------------------------------------------------------------------
// State setters
//

void RenderPipelineStateTracker::SetProgram(mnexus::ProgramHandle program) {
  if (program_ != program) {
    program_ = program;
    dirty_ = true;
    RecordEvent(mnexus::RenderStateEventTag::kSetProgram);
  }
}

void RenderPipelineStateTracker::SetVertexInputLayout(
  mbase::SmallVector<mnexus::VertexInputBindingDesc, 4> bindings,
  mbase::SmallVector<mnexus::VertexInputAttributeDesc, 8> attributes
) {
  vertex_bindings_ = std::move(bindings);
  vertex_attributes_ = std::move(attributes);
  dirty_ = true;
  RecordEvent(mnexus::RenderStateEventTag::kSetVertexInputLayout);
}

void RenderPipelineStateTracker::SetPrimitiveTopology(mnexus::PrimitiveTopology topology) {
  uint8_t const value = static_cast<uint8_t>(topology);
  if (per_draw_.ia_primitive_topology != value) {
    per_draw_.ia_primitive_topology = value;
    dirty_ = true;
    RecordEvent(mnexus::RenderStateEventTag::kSetPrimitiveTopology);
  }
}

void RenderPipelineStateTracker::SetPolygonMode(mnexus::PolygonMode mode) {
  uint8_t const value = static_cast<uint8_t>(mode);
  if (per_draw_.raster_polygon_mode != value) {
    per_draw_.raster_polygon_mode = value;
    dirty_ = true;
    RecordEvent(mnexus::RenderStateEventTag::kSetPolygonMode);
  }
}

void RenderPipelineStateTracker::SetCullMode(mnexus::CullMode cull_mode) {
  uint8_t const value = static_cast<uint8_t>(cull_mode);
  if (per_draw_.raster_cull_mode != value) {
    per_draw_.raster_cull_mode = value;
    dirty_ = true;
    RecordEvent(mnexus::RenderStateEventTag::kSetCullMode);
  }
}

void RenderPipelineStateTracker::SetFrontFace(mnexus::FrontFace front_face) {
  uint8_t const value = static_cast<uint8_t>(front_face);
  if (per_draw_.raster_front_face != value) {
    per_draw_.raster_front_face = value;
    dirty_ = true;
    RecordEvent(mnexus::RenderStateEventTag::kSetFrontFace);
  }
}

void RenderPipelineStateTracker::SetDepthTestEnabled(bool enabled) {
  uint8_t const value = enabled ? 1 : 0;
  if (per_draw_.depth_test_enabled != value) {
    per_draw_.depth_test_enabled = value;
    dirty_ = true;
    RecordEvent(mnexus::RenderStateEventTag::kSetDepthTestEnabled);
  }
}

void RenderPipelineStateTracker::SetDepthWriteEnabled(bool enabled) {
  uint8_t const value = enabled ? 1 : 0;
  if (per_draw_.depth_write_enabled != value) {
    per_draw_.depth_write_enabled = value;
    dirty_ = true;
    RecordEvent(mnexus::RenderStateEventTag::kSetDepthWriteEnabled);
  }
}

void RenderPipelineStateTracker::SetDepthCompareOp(mnexus::CompareOp op) {
  uint8_t const value = static_cast<uint8_t>(op);
  if (per_draw_.depth_compare_op != value) {
    per_draw_.depth_compare_op = value;
    dirty_ = true;
    RecordEvent(mnexus::RenderStateEventTag::kSetDepthCompareOp);
  }
}

void RenderPipelineStateTracker::SetStencilTestEnabled(bool enabled) {
  uint8_t const value = enabled ? 1 : 0;
  if (per_draw_.stencil_test_enabled != value) {
    per_draw_.stencil_test_enabled = value;
    dirty_ = true;
    RecordEvent(mnexus::RenderStateEventTag::kSetStencilTestEnabled);
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
    RecordEvent(mnexus::RenderStateEventTag::kSetStencilFrontOps);
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
    RecordEvent(mnexus::RenderStateEventTag::kSetStencilBackOps);
  }
}

void RenderPipelineStateTracker::SetBlendEnabled(uint32_t attachment, bool enabled) {
  MBASE_ASSERT(attachment < per_attachment_.size());
  uint8_t const value = enabled ? 1 : 0;
  if (per_attachment_[attachment].blend_enabled != value) {
    per_attachment_[attachment].blend_enabled = value;
    dirty_ = true;
    RecordEvent(mnexus::RenderStateEventTag::kSetBlendEnabled);
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
    RecordEvent(mnexus::RenderStateEventTag::kSetBlendFactors);
  }
}

void RenderPipelineStateTracker::SetColorWriteMask(uint32_t attachment, mnexus::ColorWriteMask mask) {
  MBASE_ASSERT(attachment < per_attachment_.size());
  uint8_t const value = static_cast<uint8_t>(mask);
  if (per_attachment_[attachment].color_write_mask != value) {
    per_attachment_[attachment].color_write_mask = value;
    dirty_ = true;
    RecordEvent(mnexus::RenderStateEventTag::kSetColorWriteMask);
  }
}

void RenderPipelineStateTracker::SetRenderTargetConfig(
  mbase::SmallVector<mnexus::Format, 4> color_formats,
  mnexus::Format depth_stencil_format,
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

// ----------------------------------------------------------------------------------------------------
// Cache key / snapshot assembly
//

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

mnexus::RenderPipelineStateSnapshot RenderPipelineStateTracker::BuildSnapshot() const {
  using namespace mnexus;
  RenderPipelineStateSnapshot s;
  s.program = program_;

  // Per-draw fixed-function state (unpack uint8_t -> typed enums).
  s.primitive_topology = static_cast<PrimitiveTopology>(per_draw_.ia_primitive_topology);
  s.polygon_mode       = static_cast<PolygonMode>(per_draw_.raster_polygon_mode);
  s.cull_mode          = static_cast<CullMode>(per_draw_.raster_cull_mode);
  s.front_face         = static_cast<FrontFace>(per_draw_.raster_front_face);
  s.depth_test_enabled  = per_draw_.depth_test_enabled != 0;
  s.depth_write_enabled = per_draw_.depth_write_enabled != 0;
  s.depth_compare_op    = static_cast<CompareOp>(per_draw_.depth_compare_op);
  s.stencil_test_enabled = per_draw_.stencil_test_enabled != 0;
  s.stencil_front_fail_op       = static_cast<StencilOp>(per_draw_.stencil_front_fail_op);
  s.stencil_front_pass_op       = static_cast<StencilOp>(per_draw_.stencil_front_pass_op);
  s.stencil_front_depth_fail_op = static_cast<StencilOp>(per_draw_.stencil_front_depth_fail_op);
  s.stencil_front_compare_op    = static_cast<CompareOp>(per_draw_.stencil_front_compare_op);
  s.stencil_back_fail_op       = static_cast<StencilOp>(per_draw_.stencil_back_fail_op);
  s.stencil_back_pass_op       = static_cast<StencilOp>(per_draw_.stencil_back_pass_op);
  s.stencil_back_depth_fail_op = static_cast<StencilOp>(per_draw_.stencil_back_depth_fail_op);
  s.stencil_back_compare_op    = static_cast<CompareOp>(per_draw_.stencil_back_compare_op);

  // Per-attachment blend state.
  s.attachments.resize(per_attachment_.size());
  for (size_t i = 0; i < per_attachment_.size(); ++i) {
    auto const& src = per_attachment_[i];
    auto& dst = s.attachments[i];
    dst.blend_enabled = src.blend_enabled != 0;
    dst.src_color = static_cast<BlendFactor>(src.blend_src_color_factor);
    dst.dst_color = static_cast<BlendFactor>(src.blend_dst_color_factor);
    dst.color_op  = static_cast<BlendOp>(src.blend_color_blend_op);
    dst.src_alpha = static_cast<BlendFactor>(src.blend_src_alpha_factor);
    dst.dst_alpha = static_cast<BlendFactor>(src.blend_dst_alpha_factor);
    dst.alpha_op  = static_cast<BlendOp>(src.blend_alpha_blend_op);
    dst.write_mask = static_cast<ColorWriteMask>(src.color_write_mask);
  }

  // Vertex input.
  s.vertex_bindings = vertex_bindings_;
  s.vertex_attributes = vertex_attributes_;

  // Render target configuration.
  s.color_formats = color_formats_;
  s.depth_stencil_format = depth_stencil_format_;
  s.sample_count = sample_count_;

  return s;
}

// ----------------------------------------------------------------------------------------------------
// Text formatting
//

namespace {

void AppendLine(std::ostringstream& oss, char const* label, std::string_view value) {
  oss << label << ": " << value << '\n';
}

void AppendLine(std::ostringstream& oss, char const* label, bool value) {
  oss << label << ": " << (value ? "true" : "false") << '\n';
}

void AppendLine(std::ostringstream& oss, char const* label, uint32_t value) {
  oss << label << ": " << value << '\n';
}

void AppendHandleLine(std::ostringstream& oss, char const* label, uint64_t handle) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "0x%016llX",
    static_cast<unsigned long long>(handle));
  oss << label << ": " << buf << '\n';
}

} // namespace

std::string RenderPipelineStateTracker::FormatSnapshot(
  mnexus::RenderPipelineStateSnapshot const& s
) {
  using namespace mnexus;
  std::ostringstream oss;

  AppendHandleLine(oss, "program", s.program.Get());
  AppendLine(oss, "primitive_topology", ToString(s.primitive_topology));
  AppendLine(oss, "polygon_mode",       ToString(s.polygon_mode));
  AppendLine(oss, "cull_mode",          ToString(s.cull_mode));
  AppendLine(oss, "front_face",         ToString(s.front_face));
  AppendLine(oss, "depth_test_enabled",  s.depth_test_enabled);
  AppendLine(oss, "depth_write_enabled", s.depth_write_enabled);
  AppendLine(oss, "depth_compare_op",    ToString(s.depth_compare_op));
  AppendLine(oss, "stencil_test_enabled", s.stencil_test_enabled);
  oss << "stencil_front_ops: "
      << ToString(s.stencil_front_fail_op) << ", "
      << ToString(s.stencil_front_pass_op) << ", "
      << ToString(s.stencil_front_depth_fail_op) << ", "
      << ToString(s.stencil_front_compare_op) << '\n';
  oss << "stencil_back_ops: "
      << ToString(s.stencil_back_fail_op) << ", "
      << ToString(s.stencil_back_pass_op) << ", "
      << ToString(s.stencil_back_depth_fail_op) << ", "
      << ToString(s.stencil_back_compare_op) << '\n';

  for (size_t i = 0; i < s.attachments.size(); ++i) {
    auto const& a = s.attachments[i];
    oss << "attachment[" << i << "]: "
        << "blend=" << (a.blend_enabled ? "true" : "false")
        << " color={" << ToString(a.src_color) << ", " << ToString(a.dst_color) << ", " << ToString(a.color_op) << "}"
        << " alpha={" << ToString(a.src_alpha) << ", " << ToString(a.dst_alpha) << ", " << ToString(a.alpha_op) << "}"
        << " write_mask=" << ToString(a.write_mask) << '\n';
  }

  oss << "vertex_bindings: " << s.vertex_bindings.size() << '\n';
  for (size_t i = 0; i < s.vertex_bindings.size(); ++i) {
    auto const& vb = s.vertex_bindings[i];
    oss << "  [" << i << "] binding=" << vb.binding
        << " stride=" << vb.stride
        << " step_mode=" << ToString(vb.step_mode) << '\n';
  }

  oss << "vertex_attributes: " << s.vertex_attributes.size() << '\n';
  for (size_t i = 0; i < s.vertex_attributes.size(); ++i) {
    auto const& va = s.vertex_attributes[i];
    oss << "  [" << i << "] location=" << va.location
        << " binding=" << va.binding
        << " format=" << ToString(static_cast<MnFormat>(va.format))
        << " offset=" << va.offset << '\n';
  }

  oss << "color_formats:";
  for (auto fmt : s.color_formats) {
    oss << ' ' << ToString(static_cast<MnFormat>(fmt));
  }
  oss << '\n';

  oss << "depth_stencil_format: " << ToString(static_cast<MnFormat>(s.depth_stencil_format)) << '\n';
  AppendLine(oss, "sample_count", s.sample_count);

  return oss.str();
}

std::string RenderPipelineStateTracker::FormatDiff(
  mnexus::RenderPipelineStateSnapshot const& a,
  mnexus::RenderPipelineStateSnapshot const& b
) {
  using namespace mnexus;
  std::ostringstream oss;

  if (a.program != b.program) {
    char buf_a[32], buf_b[32];
    std::snprintf(buf_a, sizeof(buf_a), "0x%016llX", static_cast<unsigned long long>(a.program.Get()));
    std::snprintf(buf_b, sizeof(buf_b), "0x%016llX", static_cast<unsigned long long>(b.program.Get()));
    oss << "program: " << buf_a << " -> " << buf_b << '\n';
  }

  #define DIFF_ENUM(field, to_str) \
    if (a.field != b.field) { \
      oss << #field << ": " << to_str(a.field) << " -> " << to_str(b.field) << '\n'; \
    }

  #define DIFF_BOOL(field) \
    if (a.field != b.field) { \
      oss << #field << ": " << (a.field ? "true" : "false") << " -> " << (b.field ? "true" : "false") << '\n'; \
    }

  DIFF_ENUM(primitive_topology, ToString)
  DIFF_ENUM(polygon_mode,       ToString)
  DIFF_ENUM(cull_mode,          ToString)
  DIFF_ENUM(front_face,         ToString)
  DIFF_BOOL(depth_test_enabled)
  DIFF_BOOL(depth_write_enabled)
  DIFF_ENUM(depth_compare_op,    ToString)
  DIFF_BOOL(stencil_test_enabled)
  DIFF_ENUM(stencil_front_fail_op,       ToString)
  DIFF_ENUM(stencil_front_pass_op,       ToString)
  DIFF_ENUM(stencil_front_depth_fail_op, ToString)
  DIFF_ENUM(stencil_front_compare_op,    ToString)
  DIFF_ENUM(stencil_back_fail_op,       ToString)
  DIFF_ENUM(stencil_back_pass_op,       ToString)
  DIFF_ENUM(stencil_back_depth_fail_op, ToString)
  DIFF_ENUM(stencil_back_compare_op,    ToString)

  #undef DIFF_ENUM
  #undef DIFF_BOOL

  // Per-attachment diff.
  size_t const max_att = std::max(a.attachments.size(), b.attachments.size());
  for (size_t i = 0; i < max_att; ++i) {
    if (i >= a.attachments.size()) {
      oss << "attachment[" << i << "]: (added)\n";
      continue;
    }
    if (i >= b.attachments.size()) {
      oss << "attachment[" << i << "]: (removed)\n";
      continue;
    }
    auto const& aa = a.attachments[i];
    auto const& bb = b.attachments[i];
    bool changed = false;
    std::ostringstream att_oss;
    att_oss << "attachment[" << i << "]: ";
    if (aa.blend_enabled != bb.blend_enabled) {
      att_oss << "blend=" << (aa.blend_enabled ? "true" : "false") << "->" << (bb.blend_enabled ? "true" : "false") << ' ';
      changed = true;
    }
    if (aa.src_color != bb.src_color || aa.dst_color != bb.dst_color || aa.color_op != bb.color_op) {
      att_oss << "color={" << ToString(aa.src_color) << "," << ToString(aa.dst_color) << "," << ToString(aa.color_op) << "}"
              << "->{" << ToString(bb.src_color) << "," << ToString(bb.dst_color) << "," << ToString(bb.color_op) << "} ";
      changed = true;
    }
    if (aa.src_alpha != bb.src_alpha || aa.dst_alpha != bb.dst_alpha || aa.alpha_op != bb.alpha_op) {
      att_oss << "alpha={" << ToString(aa.src_alpha) << "," << ToString(aa.dst_alpha) << "," << ToString(aa.alpha_op) << "}"
              << "->{" << ToString(bb.src_alpha) << "," << ToString(bb.dst_alpha) << "," << ToString(bb.alpha_op) << "} ";
      changed = true;
    }
    if (aa.write_mask != bb.write_mask) {
      att_oss << "write_mask=" << ToString(aa.write_mask) << "->" << ToString(bb.write_mask);
      changed = true;
    }
    if (changed) {
      oss << att_oss.str() << '\n';
    }
  }

  // Vertex input diff (summary only).
  if (a.vertex_bindings.size() != b.vertex_bindings.size() ||
      a.vertex_attributes.size() != b.vertex_attributes.size()) {
    oss << "vertex_input: " << a.vertex_bindings.size() << " bindings, "
        << a.vertex_attributes.size() << " attributes -> "
        << b.vertex_bindings.size() << " bindings, "
        << b.vertex_attributes.size() << " attributes\n";
  }

  // Render target diff.
  if (a.color_formats.size() != b.color_formats.size() ||
      a.depth_stencil_format != b.depth_stencil_format ||
      a.sample_count != b.sample_count) {
    oss << "render_target: color_formats=" << a.color_formats.size() << "->" << b.color_formats.size()
        << " depth=" << ToString(static_cast<MnFormat>(a.depth_stencil_format))
        << "->" << ToString(static_cast<MnFormat>(b.depth_stencil_format))
        << " samples=" << a.sample_count << "->" << b.sample_count << '\n';
  }

  return oss.str();
}

// ----------------------------------------------------------------------------------------------------
// Reset
//

void RenderPipelineStateTracker::Reset() {
  dirty_ = true;
  program_ = mnexus::ProgramHandle::Invalid();
  per_draw_ = PerDrawFixedFunctionStaticState {};
  per_attachment_.clear();
  vertex_bindings_.clear();
  vertex_attributes_.clear();
  color_formats_.clear();
  depth_stencil_format_ = mnexus::Format::kUndefined;
  sample_count_ = 1;
}

} // namespace pipeline
