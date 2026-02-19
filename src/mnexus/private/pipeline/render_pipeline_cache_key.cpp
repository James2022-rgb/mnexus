// TU header --------------------------------------------
#include "pipeline/render_pipeline_cache_key.h"

// c++ headers ------------------------------------------
#include <cstring>

namespace pipeline {

size_t RenderPipelineCacheKey::ComputeHash() const {
  mbase::Hasher hasher;

  // Program handle.
  hasher.Do(program.Get());

  // Per-draw state (trivially copyable, memhash).
  hasher.DoBytes(&per_draw, sizeof(per_draw));

  // Per-attachment states.
  hasher.Do(static_cast<uint32_t>(per_attachment.size()));
  for (auto const& att : per_attachment) {
    hasher.DoBytes(&att, sizeof(att));
  }

  // Vertex bindings.
  hasher.Do(static_cast<uint32_t>(vertex_bindings.size()));
  for (auto const& vb : vertex_bindings) {
    hasher.Do(vb.binding, vb.stride);
    hasher.Do(static_cast<uint8_t>(vb.step_mode));
  }

  // Vertex attributes.
  hasher.Do(static_cast<uint32_t>(vertex_attributes.size()));
  for (auto const& va : vertex_attributes) {
    hasher.Do(va.location, va.binding);
    hasher.Do(static_cast<uint32_t>(va.format));
    hasher.Do(va.offset);
  }

  // Render target formats.
  hasher.Do(static_cast<uint32_t>(color_formats.size()));
  for (auto const& fmt : color_formats) {
    hasher.Do(static_cast<uint32_t>(fmt));
  }

  hasher.Do(static_cast<uint32_t>(depth_stencil_format));
  hasher.Do(sample_count);

  return static_cast<size_t>(hasher.Finish());
}

bool RenderPipelineCacheKey::operator==(RenderPipelineCacheKey const& other) const {
  if (program != other.program) return false;
  if (std::memcmp(&per_draw, &other.per_draw, sizeof(per_draw)) != 0) return false;

  if (per_attachment.size() != other.per_attachment.size()) return false;
  for (size_t i = 0; i < per_attachment.size(); ++i) {
    if (std::memcmp(&per_attachment[i], &other.per_attachment[i], sizeof(per_attachment[i])) != 0) return false;
  }

  if (vertex_bindings.size() != other.vertex_bindings.size()) return false;
  for (size_t i = 0; i < vertex_bindings.size(); ++i) {
    auto const& a = vertex_bindings[i];
    auto const& b = other.vertex_bindings[i];
    if (a.binding != b.binding || a.stride != b.stride || a.step_mode != b.step_mode) return false;
  }

  if (vertex_attributes.size() != other.vertex_attributes.size()) return false;
  for (size_t i = 0; i < vertex_attributes.size(); ++i) {
    auto const& a = vertex_attributes[i];
    auto const& b = other.vertex_attributes[i];
    if (a.location != b.location || a.binding != b.binding || a.format != b.format || a.offset != b.offset) return false;
  }

  if (color_formats.size() != other.color_formats.size()) return false;
  for (size_t i = 0; i < color_formats.size(); ++i) {
    if (color_formats[i] != other.color_formats[i]) return false;
  }

  if (depth_stencil_format != other.depth_stencil_format) return false;
  if (sample_count != other.sample_count) return false;

  return true;
}

} // namespace pipeline
