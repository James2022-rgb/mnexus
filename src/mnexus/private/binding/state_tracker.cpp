// TU header --------------------------------------------
#include "binding/state_tracker.h"

// c++ headers ------------------------------------------
#include <algorithm>

// public project headers -------------------------------
#include "mbase/public/assert.h"

namespace binding {

void BindGroupStateTracker::SetBuffer(
  uint32_t group, uint32_t binding, uint32_t array_element,
  mnexus::BindGroupLayoutEntryType type,
  mnexus::BufferHandle buffer, uint64_t offset, uint64_t size
) {
  MBASE_ASSERT(group < kMaxGroups);

  Group& g = groups_[group];

  // Find existing entry with matching (binding, array_element).
  auto it = std::find_if(
    g.entries.begin(), g.entries.end(),
    [binding, array_element](BoundEntry const& e) {
      return e.binding == binding && e.array_element == array_element;
    }
  );

  BoundEntry entry {
    .binding = binding,
    .array_element = array_element,
    .type = type,
    .buffer = BoundBuffer {
      .buffer = buffer,
      .offset = offset,
      .size = size,
    },
  };

  if (it != g.entries.end()) {
    *it = entry;
  } else {
    g.entries.emplace_back(entry);

    // Keep sorted by (binding, array_element).
    std::sort(
      g.entries.begin(), g.entries.end(),
      [](BoundEntry const& a, BoundEntry const& b) {
        if (a.binding != b.binding) return a.binding < b.binding;
        return a.array_element < b.array_element;
      }
    );
  }

  g.dirty = true;
}

bool BindGroupStateTracker::IsGroupDirty(uint32_t group) const {
  MBASE_ASSERT(group < kMaxGroups);
  return groups_[group].dirty;
}

mbase::ArrayProxy<BoundEntry const> BindGroupStateTracker::GetGroupEntries(uint32_t group) const {
  MBASE_ASSERT(group < kMaxGroups);
  Group const& g = groups_[group];
  return { g.entries.data(), static_cast<uint32_t>(g.entries.size()) };
}

void BindGroupStateTracker::MarkGroupClean(uint32_t group) {
  MBASE_ASSERT(group < kMaxGroups);
  groups_[group].dirty = false;
}

void BindGroupStateTracker::Reset() {
  for (uint32_t i = 0; i < kMaxGroups; ++i) {
    groups_[i].entries.clear();
    groups_[i].dirty = false;
  }
}

} // namespace binding
