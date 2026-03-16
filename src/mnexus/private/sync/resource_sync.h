#pragma once

// c++ headers ------------------------------------------
#include <atomic>
#include <cstdint>

#include <optional>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/assert.h"

#include "mnexus/public/types.h"

namespace mnexus_backend {

static constexpr uint32_t kMaxQueues = 5;

// ----------------------------------------------------------------------------------------------------
// ResourceSyncStamp
//
// Per-resource synchronization stamp for deferred destruction.
// To be embedded as a member of the resource's Hot struct.
//

struct ResourceSyncStamp final {
  std::atomic<uint64_t> last_used[kMaxQueues] {};
  std::atomic<uint32_t> used_mask {0};

  ResourceSyncStamp() = default;
  ~ResourceSyncStamp() = default;

  ResourceSyncStamp(ResourceSyncStamp const& other);
  ResourceSyncStamp& operator=(ResourceSyncStamp const& other);

  ResourceSyncStamp(ResourceSyncStamp&& other);
  ResourceSyncStamp& operator=(ResourceSyncStamp&& other);

  /// Record that this resource was used in a submission on the given queue.
  void Stamp(uint32_t queue_compact_index, uint64_t submit_serial);

  /// Reset to the initial state (no usage on any queue).
  void Reset();

  /// Snapshot the current state into a non-atomic copy for retire point capture.
  struct Snapshot {
    uint64_t last_used[kMaxQueues] {};
    uint32_t used_mask = 0;
  };
  [[nodiscard]] Snapshot TakeSnapshot() const;
};

// ----------------------------------------------------------------------------------------------------
// QueueIndexMap
//
// Maps QueueId (family_index, queue_index) to a compact index [0..N).
// Built once at device creation from QueueSelection.
//

class QueueIndexMap final {
public:
  QueueIndexMap() = default;
  ~QueueIndexMap() = default;
  MBASE_DEFAULT_COPY_MOVE(QueueIndexMap);

  /// Build from a QueueSelection. Assigns compact indices 0..N-1.
  explicit QueueIndexMap(mnexus::QueueSelection const& selection);

  /// Returns the compact index for the given QueueId, or nullopt if not mapped.
  [[nodiscard]] std::optional<uint32_t> Find(mnexus::QueueId const& queue_id) const;

  /// Returns the number of mapped queues.
  [[nodiscard]] uint32_t Count() const { return count_; }

private:
  struct Entry {
    mnexus::QueueId queue_id;
    uint32_t compact_index = 0;
  };
  uint32_t count_ = 0;
  Entry entries_[kMaxQueues] {};
};

} // namespace mnexus_backend
