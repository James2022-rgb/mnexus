#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <vector>

// public project headers -------------------------------
#include "mbase/public/container.h"

#include "mnexus/public/types.h"

namespace mnexus {

/// Strongly-typed representation of the complete render pipeline state.
/// Used by the structured event log for full state reconstruction at any
/// recorded event point. Unlike the internal packed cache key, all fields
/// use their native enum types for human readability.
struct RenderPipelineStateSnapshot final {
  ProgramHandle program;

  // Per-draw fixed-function state.
  PrimitiveTopology primitive_topology = PrimitiveTopology::kTriangleList;
  PolygonMode       polygon_mode       = PolygonMode::kFill;
  CullMode          cull_mode          = CullMode::kNone;
  FrontFace         front_face         = FrontFace::kCounterClockwise;
  bool              depth_test_enabled  = false;
  bool              depth_write_enabled = false;
  CompareOp         depth_compare_op    = CompareOp::kAlways;
  bool              stencil_test_enabled = false;
  StencilOp         stencil_front_fail_op       = StencilOp::kKeep;
  StencilOp         stencil_front_pass_op       = StencilOp::kKeep;
  StencilOp         stencil_front_depth_fail_op = StencilOp::kKeep;
  CompareOp         stencil_front_compare_op    = CompareOp::kAlways;
  StencilOp         stencil_back_fail_op       = StencilOp::kKeep;
  StencilOp         stencil_back_pass_op       = StencilOp::kKeep;
  StencilOp         stencil_back_depth_fail_op = StencilOp::kKeep;
  CompareOp         stencil_back_compare_op    = CompareOp::kAlways;

  // Per-attachment blend state.
  struct AttachmentState final {
    bool           blend_enabled = false;
    BlendFactor    src_color = BlendFactor::kOne;
    BlendFactor    dst_color = BlendFactor::kZero;
    BlendOp        color_op  = BlendOp::kAdd;
    BlendFactor    src_alpha = BlendFactor::kOne;
    BlendFactor    dst_alpha = BlendFactor::kZero;
    BlendOp        alpha_op  = BlendOp::kAdd;
    ColorWriteMask write_mask = ColorWriteMask::kAll;
  };
  mbase::SmallVector<AttachmentState, 4> attachments;

  // Vertex input.
  mbase::SmallVector<VertexInputBindingDesc, 4>  vertex_bindings;
  mbase::SmallVector<VertexInputAttributeDesc, 8> vertex_attributes;

  // Render target configuration.
  mbase::SmallVector<Format, 4> color_formats;
  Format   depth_stencil_format = Format::kUndefined;
  uint32_t sample_count = 1;
};

// ---------------------------------------------------------------------------
// Cache inspection types
//

/// Aggregate diagnostics for the device's render pipeline cache.
struct RenderPipelineCacheDiagnosticsSnapshot final {
  uint64_t total_lookups = 0;
  uint64_t cache_hits = 0;
  uint64_t cache_misses = 0;
  uint64_t cached_pipeline_count = 0;

  [[nodiscard]] double HitRate() const {
    return total_lookups > 0
      ? static_cast<double>(cache_hits) / static_cast<double>(total_lookups)
      : 0.0;
  }
};

/// A single entry in the render pipeline cache snapshot.
struct RenderPipelineCacheEntry final {
  size_t hash = 0;
  RenderPipelineStateSnapshot state;
};

/// Complete snapshot of the render pipeline cache contents and diagnostics.
struct RenderPipelineCacheSnapshot final {
  RenderPipelineCacheDiagnosticsSnapshot diagnostics;
  std::vector<RenderPipelineCacheEntry> entries;
};

} // namespace mnexus
