#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <string>

// public project headers -------------------------------
#include "mbase/public/container.h"

#include "mnexus/public/render_pipeline_state_snapshot.h"
#include "mnexus/public/render_state_event_log.h"
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "pipeline/render_pipeline_cache_key.h"
#include "pipeline/render_pipeline_fixed_function.h"

namespace pipeline {

/// Tracks mutable render pipeline state on a command list.
/// When any state changes, the dirty flag is set. At Draw time, if dirty,
/// `BuildCacheKey()` assembles a `RenderPipelineCacheKey` for pipeline lookup/creation.
class RenderPipelineStateTracker final {
public:
  // -----------------------------------------------------------------------
  // Program

  void SetProgram(mnexus::ProgramHandle program);

  // -----------------------------------------------------------------------
  // Vertex input

  void SetVertexInputLayout(
    mbase::SmallVector<mnexus::VertexInputBindingDesc, 4> bindings,
    mbase::SmallVector<mnexus::VertexInputAttributeDesc, 8> attributes
  );

  // -----------------------------------------------------------------------
  // Fixed-function state

  void SetPrimitiveTopology(mnexus::PrimitiveTopology topology);
  void SetPolygonMode(mnexus::PolygonMode mode);
  void SetCullMode(mnexus::CullMode cull_mode);
  void SetFrontFace(mnexus::FrontFace front_face);

  // Depth
  void SetDepthTestEnabled(bool enabled);
  void SetDepthWriteEnabled(bool enabled);
  void SetDepthCompareOp(mnexus::CompareOp op);

  // Stencil
  void SetStencilTestEnabled(bool enabled);
  void SetStencilFrontOps(mnexus::StencilOp fail, mnexus::StencilOp pass,
                          mnexus::StencilOp depth_fail, mnexus::CompareOp compare);
  void SetStencilBackOps(mnexus::StencilOp fail, mnexus::StencilOp pass,
                         mnexus::StencilOp depth_fail, mnexus::CompareOp compare);

  // Per-attachment blend
  void SetBlendEnabled(uint32_t attachment, bool enabled);
  void SetBlendFactors(uint32_t attachment,
                       mnexus::BlendFactor src_color, mnexus::BlendFactor dst_color, mnexus::BlendOp color_op,
                       mnexus::BlendFactor src_alpha, mnexus::BlendFactor dst_alpha, mnexus::BlendOp alpha_op);
  void SetColorWriteMask(uint32_t attachment, mnexus::ColorWriteMask mask);

  // -----------------------------------------------------------------------
  // Render target configuration (called by backend at BeginRenderPass)

  void SetRenderTargetConfig(
    mbase::SmallVector<mnexus::Format, 4> color_formats,
    mnexus::Format depth_stencil_format,
    uint32_t sample_count
  );

  // -----------------------------------------------------------------------
  // Dirty tracking

  [[nodiscard]] bool IsDirty() const { return dirty_; }

  void MarkClean() { dirty_ = false; }

  // -----------------------------------------------------------------------
  // Cache key assembly

  [[nodiscard]] RenderPipelineCacheKey BuildCacheKey() const;

  // -----------------------------------------------------------------------
  // Public snapshot

  /// Builds a strongly-typed snapshot from the current internal state.
  [[nodiscard]] mnexus::RenderPipelineStateSnapshot BuildSnapshot() const;

  // -----------------------------------------------------------------------
  // Text formatting

  /// Formats a snapshot as multi-line human-readable text.
  [[nodiscard]] static std::string FormatSnapshot(
    mnexus::RenderPipelineStateSnapshot const& snapshot);

  /// Formats the difference between two snapshots as human-readable text.
  /// Returns an empty string when the snapshots are identical.
  [[nodiscard]] static std::string FormatDiff(
    mnexus::RenderPipelineStateSnapshot const& before,
    mnexus::RenderPipelineStateSnapshot const& after);

  // -----------------------------------------------------------------------
  // Event log

  /// Sets the event log to record state changes into. Pass nullptr to disable.
  void SetEventLog(mnexus::RenderStateEventLog* log);

  // -----------------------------------------------------------------------
  // Reset

  void Reset();

private:
  void RecordEvent(mnexus::RenderStateEventTag tag);

  bool dirty_ = true;
  mnexus::RenderStateEventLog* event_log_ = nullptr;

  mnexus::ProgramHandle program_;
  PerDrawFixedFunctionStaticState per_draw_;
  mbase::SmallVector<PerAttachmentFixedFunctionStaticState, 4> per_attachment_;
  mbase::SmallVector<mnexus::VertexInputBindingDesc, 4> vertex_bindings_;
  mbase::SmallVector<mnexus::VertexInputAttributeDesc, 8> vertex_attributes_;

  // Render target configuration (set at BeginRenderPass).
  mbase::SmallVector<mnexus::Format, 4> color_formats_;
  mnexus::Format depth_stencil_format_ = mnexus::Format::kUndefined;
  uint32_t sample_count_ = 1;
};

} // namespace pipeline
