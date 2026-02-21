#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/array_proxy.h"
#include "mbase/public/container.h"

#include "mnexus/public/types.h"

namespace binding {

struct BoundBuffer final {
  mnexus::BufferHandle buffer;
  uint64_t offset = 0;
  uint64_t size = 0;
};

struct BoundTexture final {
  mnexus::TextureHandle texture;
  mnexus::TextureSubresourceRange subresource_range;
};

struct BoundSampler final {
  mnexus::SamplerHandle sampler;
};

struct BoundEntry final {
  uint32_t binding = 0;
  uint32_t array_element = 0;
  mnexus::BindGroupLayoutEntryType type = mnexus::BindGroupLayoutEntryType::kUniformBuffer;
  BoundBuffer buffer;    // Valid when type == kUniformBuffer or kStorageBuffer.
  BoundTexture texture;  // Valid when type == kSampledTexture.
  BoundSampler sampler;  // Valid when type == kSampler.
};

/// Tracks the current bind group state across all groups.
/// Each group is independently dirty-tracked.
class BindGroupStateTracker final {
public:
  void SetBuffer(
    uint32_t group, uint32_t binding, uint32_t array_element,
    mnexus::BindGroupLayoutEntryType type,
    mnexus::BufferHandle buffer, uint64_t offset, uint64_t size
  );

  void SetTexture(
    uint32_t group, uint32_t binding, uint32_t array_element,
    mnexus::BindGroupLayoutEntryType type,
    mnexus::TextureHandle texture,
    mnexus::TextureSubresourceRange const& subresource_range
  );

  void SetSampler(
    uint32_t group, uint32_t binding, uint32_t array_element,
    mnexus::SamplerHandle sampler
  );

  [[nodiscard]] bool IsGroupDirty(uint32_t group) const;
  [[nodiscard]] mbase::ArrayProxy<BoundEntry const> GetGroupEntries(uint32_t group) const;
  void MarkGroupClean(uint32_t group);
  void Reset();

private:
  static constexpr uint32_t kMaxGroups = 4;

  struct Group final {
    /// Entries sorted by (binding, array_element).
    mbase::SmallVector<BoundEntry, 4> entries;
    bool dirty = false;
  };

  /// Upsert `entry` into the group, keep sorted, mark dirty.
  void UpsertEntry(uint32_t group, BoundEntry entry);

  Group groups_[kMaxGroups];
};

} // namespace binding
