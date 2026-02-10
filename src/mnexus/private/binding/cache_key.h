#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <unordered_map>

// public project headers -------------------------------
#include "mbase/public/container.h"
#include "mbase/public/hash.h"

#include "mnexus/public/types.h"

namespace binding {

/// Hashable, equality-comparable descriptor identifying a bind group's contents.
/// Used as a key in `TBindGroupCache` to avoid redundant bind group creation.
struct BindGroupCacheKey final {
  struct Entry final {
    uint32_t binding = 0;
    uint32_t array_element = 0;
    mnexus::BindGroupLayoutEntryType type = mnexus::BindGroupLayoutEntryType::kUniformBuffer;
    uint64_t resource_handle = 0;
    uint64_t offset = 0;
    uint64_t size = 0;
  };

  /// Identity of the pipeline layout (for bind group layout compatibility).
  uint64_t pipeline_identity = 0;
  uint32_t group_index = 0;
  mbase::SmallVector<Entry, 4> entries;

  [[nodiscard]] size_t ComputeHash() const {
    mbase::Hasher hasher;
    hasher.Do(pipeline_identity);
    hasher.Do(group_index);
    hasher.Do(static_cast<uint32_t>(entries.size()));
    for (auto const& e : entries) {
      hasher.Do(e.binding, e.array_element);
      hasher.Do(static_cast<uint32_t>(e.type));
      hasher.Do(e.resource_handle, e.offset, e.size);
    }
    return static_cast<size_t>(hasher.Finish());
  }

  [[nodiscard]] bool operator==(BindGroupCacheKey const& other) const {
    if (pipeline_identity != other.pipeline_identity) return false;
    if (group_index != other.group_index) return false;
    if (entries.size() != other.entries.size()) return false;
    for (size_t i = 0; i < entries.size(); ++i) {
      auto const& a = entries[i];
      auto const& b = other.entries[i];
      if (a.binding != b.binding) return false;
      if (a.array_element != b.array_element) return false;
      if (a.type != b.type) return false;
      if (a.resource_handle != b.resource_handle) return false;
      if (a.offset != b.offset) return false;
      if (a.size != b.size) return false;
    }
    return true;
  }

  struct Hasher final {
    size_t operator()(BindGroupCacheKey const& key) const {
      return key.ComputeHash();
    }
  };
};

/// Template cache for backend-specific bind group objects.
/// Backends instantiate with their bind group type (e.g. `wgpu::BindGroup`).
template<typename TBindGroup>
class TBindGroupCache final {
public:
  TBindGroup* Find(BindGroupCacheKey const& key) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void Insert(BindGroupCacheKey key, TBindGroup bind_group) {
    cache_.emplace(std::move(key), std::move(bind_group));
  }

  void Clear() {
    cache_.clear();
  }

  [[nodiscard]] size_t size() const { return cache_.size(); }

private:
  std::unordered_map<BindGroupCacheKey, TBindGroup, BindGroupCacheKey::Hasher> cache_;
};

} // namespace binding
