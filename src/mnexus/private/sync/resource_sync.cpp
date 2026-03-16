// TU header --------------------------------------------
#include "sync/resource_sync.h"

namespace mnexus_backend {

// ----------------------------------------------------------------------------------------------------
// ResourceSyncStamp
//

ResourceSyncStamp::ResourceSyncStamp(ResourceSyncStamp const& other) {
  for (uint32_t i = 0; i < kMaxQueues; ++i) {
    last_used[i].store(other.last_used[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
  }
  used_mask.store(other.used_mask.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

ResourceSyncStamp& ResourceSyncStamp::operator=(ResourceSyncStamp const& other) {
  for (uint32_t i = 0; i < kMaxQueues; ++i) {
    last_used[i].store(other.last_used[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
  }
  used_mask.store(other.used_mask.load(std::memory_order_relaxed), std::memory_order_relaxed);
  return *this;
}

ResourceSyncStamp::ResourceSyncStamp(ResourceSyncStamp&& other) {
  for (uint32_t i = 0; i < kMaxQueues; ++i) {
    last_used[i].store(other.last_used[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
  }
  used_mask.store(other.used_mask.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

ResourceSyncStamp& ResourceSyncStamp::operator=(ResourceSyncStamp&& other) {
  for (uint32_t i = 0; i < kMaxQueues; ++i) {
    last_used[i].store(other.last_used[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
  }
  used_mask.store(other.used_mask.load(std::memory_order_relaxed), std::memory_order_relaxed);
  return *this;
}

void ResourceSyncStamp::Stamp(uint32_t queue_compact_index, uint64_t submit_serial) {
  MBASE_ASSERT(queue_compact_index < kMaxQueues);
  // relaxed: the subsequent release on used_mask orders this store.
  last_used[queue_compact_index].store(submit_serial, std::memory_order_relaxed);
  // release: synchronizes-with the acquire load in TakeSnapshot(),
  // guaranteeing that the last_used store above is visible to the reader.
  used_mask.fetch_or(1u << queue_compact_index, std::memory_order_release);
}

void ResourceSyncStamp::Reset() {
  for (uint32_t i = 0; i < kMaxQueues; ++i) {
    last_used[i].store(0, std::memory_order_relaxed);
  }
  used_mask.store(0, std::memory_order_relaxed);
}

ResourceSyncStamp::Snapshot ResourceSyncStamp::TakeSnapshot() const {
  Snapshot snap;
  // acquire: synchronizes-with the release fetch_or in Stamp().
  // All last_used stores sequenced-before the release are visible after this.
  snap.used_mask = used_mask.load(std::memory_order_acquire);
  for (uint32_t i = 0; i < kMaxQueues; ++i) {
    // relaxed: ordered by the acquire on used_mask above.
    snap.last_used[i] = last_used[i].load(std::memory_order_relaxed);
  }
  return snap;
}

// ----------------------------------------------------------------------------------------------------
// QueueIndexMap
//

QueueIndexMap::QueueIndexMap(mnexus::QueueSelection const& selection) {
  uint32_t next = 0;

  entries_[next] = Entry { selection.present_capable, next };
  ++next;

  if (selection.dedicated_compute.has_value()) {
    entries_[next] = Entry { *selection.dedicated_compute, next };
    ++next;
  }
  if (selection.dedicated_transfer.has_value()) {
    entries_[next] = Entry { *selection.dedicated_transfer, next };
    ++next;
  }
  if (selection.dedicated_video_decode.has_value()) {
    entries_[next] = Entry { *selection.dedicated_video_decode, next };
    ++next;
  }
  if (selection.dedicated_video_encode.has_value()) {
    entries_[next] = Entry { *selection.dedicated_video_encode, next };
    ++next;
  }

  count_ = next;
}

std::optional<uint32_t> QueueIndexMap::Find(mnexus::QueueId const& queue_id) const {
  for (uint32_t i = 0; i < count_; ++i) {
    if (entries_[i].queue_id == queue_id) {
      return entries_[i].compact_index;
    }
  }
  return std::nullopt;
}

} // namespace mnexus_backend
