#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <vector>

// public project headers -------------------------------
#include "mnexus/public/render_pipeline_state_snapshot.h"

namespace mnexus {

// ----------------------------------------------------------------------------------------------------
// Event tags
//

enum class RenderStateEventTag : uint8_t {
  kBeginRenderPass,
  kEndRenderPass,
  kSetProgram,
  kSetVertexInputLayout,
  kSetPrimitiveTopology,
  kSetPolygonMode,
  kSetCullMode,
  kSetFrontFace,
  kSetDepthTestEnabled,
  kSetDepthWriteEnabled,
  kSetDepthCompareOp,
  kSetStencilTestEnabled,
  kSetStencilFrontOps,
  kSetStencilBackOps,
  kSetBlendEnabled,
  kSetBlendFactors,
  kSetColorWriteMask,
  kPsoResolved,
  kDraw,
  kDrawIndexed,
};

// ----------------------------------------------------------------------------------------------------
// Event
//

/// A single recorded render-state event.
/// Carries a full pipeline state snapshot so that the complete state is
/// directly available at any event index without replay.
struct RenderStateEvent final {
  RenderStateEventTag tag;
  RenderPipelineStateSnapshot state;

  // PSO metadata (valid when tag == kPsoResolved).
  size_t pso_hash  = 0;
  bool   cache_hit = false;
};

// ----------------------------------------------------------------------------------------------------
// Event log
//

/// Per-command-list structured event log for render pipeline state changes.
/// Recording is opt-in: when disabled, `Record` / `RecordPso` are no-ops.
class RenderStateEventLog final {
public:
  void SetEnabled(bool enabled);
  [[nodiscard]] bool IsEnabled() const;

  void Record(RenderStateEventTag tag,
              RenderPipelineStateSnapshot const& state);
  void RecordPso(RenderPipelineStateSnapshot const& state,
                 size_t hash, bool cache_hit);

  [[nodiscard]] uint32_t GetCount() const;
  [[nodiscard]] RenderStateEvent const& GetEvent(uint32_t index) const;
  void Clear();

private:
  bool enabled_ = false;
  std::vector<RenderStateEvent> events_;
};

} // namespace mnexus
