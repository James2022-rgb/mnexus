// TU header --------------------------------------------
#include "binding/state_tracker.h"

// c++ headers ------------------------------------------
#include <algorithm>

// public project headers -------------------------------
#include "mbase/public/assert.h"

namespace binding {

void BindGroupStateTracker::UpsertEntry(uint32_t group, BoundEntry entry) {
  MBASE_ASSERT(group < kMaxGroups);

  Group& g = groups_[group];

  auto it = std::find_if(
    g.entries.begin(), g.entries.end(),
    [&entry](BoundEntry const& e) {
      return e.binding == entry.binding && e.array_element == entry.array_element;
    }
  );

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

void BindGroupStateTracker::SetBuffer(
  uint32_t group, uint32_t binding, uint32_t array_element,
  mnexus::BindGroupLayoutEntryType type,
  mnexus::BufferHandle buffer, uint64_t offset, uint64_t size
) {
  UpsertEntry(group, BoundEntry {
    .binding = binding,
    .array_element = array_element,
    .type = type,
    .buffer = BoundBuffer {
      .buffer = buffer,
      .offset = offset,
      .size = size,
    },
    .texture = {},
    .sampler = {},
  });
}

void BindGroupStateTracker::SetTexture(
  uint32_t group, uint32_t binding, uint32_t array_element,
  mnexus::BindGroupLayoutEntryType type,
  mnexus::TextureHandle texture,
  mnexus::TextureSubresourceRange const& subresource_range
) {
  UpsertEntry(group, BoundEntry {
    .binding = binding,
    .array_element = array_element,
    .type = type,
    .buffer = {},
    .texture = BoundTexture {
      .texture = texture,
      .subresource_range = subresource_range,
    },
    .sampler = {},
  });
}

void BindGroupStateTracker::SetSampler(
  uint32_t group, uint32_t binding, uint32_t array_element,
  mnexus::SamplerHandle sampler
) {
  UpsertEntry(group, BoundEntry {
    .binding = binding,
    .array_element = array_element,
    .type = mnexus::BindGroupLayoutEntryType::kSampler,
    .buffer = {},
    .texture = {},
    .sampler = BoundSampler {
      .sampler = sampler,
    },
  });
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
