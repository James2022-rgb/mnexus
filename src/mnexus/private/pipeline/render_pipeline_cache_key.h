#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

// public project headers -------------------------------
#include "mbase/public/container.h"
#include "mbase/public/hash.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "pipeline/render_pipeline_fixed_function.h"

namespace pipeline {

/// Hashable, equality-comparable key identifying a unique render pipeline configuration.
/// Used as a key in `TRenderPipelineCache` to avoid redundant pipeline creation.
struct RenderPipelineCacheKey final {
  mnexus::ProgramHandle program;
  PerDrawFixedFunctionStaticState per_draw;
  mbase::SmallVector<PerAttachmentFixedFunctionStaticState, 4> per_attachment;
  mbase::SmallVector<mnexus::VertexInputBindingDesc, 4> vertex_bindings;
  mbase::SmallVector<mnexus::VertexInputAttributeDesc, 8> vertex_attributes;
  mbase::SmallVector<MnFormat, 4> color_formats;
  MnFormat depth_stencil_format = MnFormat::kUndefined;
  uint32_t sample_count = 1;

  [[nodiscard]] size_t ComputeHash() const;
  [[nodiscard]] bool operator==(RenderPipelineCacheKey const& other) const;

  struct Hasher final {
    size_t operator()(RenderPipelineCacheKey const& key) const {
      return key.ComputeHash();
    }
  };
};

} // namespace pipeline
