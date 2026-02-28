// TU header --------------------------------------------
#include "mnexus/public/render_state_event_log.h"

// public project headers -------------------------------
#include "mbase/public/assert.h"

namespace mnexus {

void RenderStateEventLog::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

bool RenderStateEventLog::IsEnabled() const {
  return enabled_;
}

void RenderStateEventLog::Record(RenderStateEventTag tag,
                                 RenderPipelineStateSnapshot const& state) {
  if (!enabled_) return;
  events_.push_back(RenderStateEvent { .tag = tag, .state = state });
}

void RenderStateEventLog::RecordPso(RenderPipelineStateSnapshot const& state,
                                    size_t hash, bool cache_hit) {
  if (!enabled_) return;
  events_.push_back(RenderStateEvent {
    .tag = RenderStateEventTag::kPsoResolved,
    .state = state,
    .pso_hash = hash,
    .cache_hit = cache_hit,
  });
}

uint32_t RenderStateEventLog::GetCount() const {
  return static_cast<uint32_t>(events_.size());
}

RenderStateEvent const& RenderStateEventLog::GetEvent(uint32_t index) const {
  MBASE_ASSERT(index < events_.size());
  return events_[index];
}

void RenderStateEventLog::Clear() {
  events_.clear();
}

} // namespace mnexus
