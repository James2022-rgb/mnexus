#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <atomic>
#include <shared_mutex>
#include <unordered_map>

// public project headers -------------------------------
#include "mbase/public/tsa.h"

// project headers --------------------------------------
#include "pipeline/pipeline_layout_cache_key.h"

namespace pipeline {

/// Thread-safe pipeline layout cache keyed by `PipelineLayoutCacheKey`.
/// Backends instantiate with their layout type (e.g. `wgpu::PipelineLayout`).
template<typename TLayout>
class TPipelineLayoutCache final {
public:
  /// Looks up `key` in the cache. On hit, returns the cached layout.
  /// On miss, calls `factory(key)` to create a new layout, inserts it, and returns it.
  /// The factory is only invoked while the exclusive lock is held, so at most one
  /// thread creates a layout for any given key.
  template<typename TFactory>
  TLayout FindOrInsert(PipelineLayoutCacheKey const& key, TFactory&& factory) MBASE_EXCLUDES(mutex_) {
    // Fast path: shared lock for concurrent reads.
    {
      mbase::SharedLockGuard shared_lock(mutex_);
      auto it = cache_.find(key);
      if (it != cache_.end()) {
        return it->second;
      }
    }

    // Slow path: exclusive lock, double-check, then create.
    mbase::LockGuard exclusive_lock(mutex_);
    auto [it, inserted] = cache_.emplace(key, TLayout{});
    if (!inserted) {
      return it->second;
    }
    it->second = factory(key);
    return it->second;
  }

  void Clear() MBASE_EXCLUDES(mutex_) {
    mbase::LockGuard lock(mutex_);
    cache_.clear();
  }

  [[nodiscard]] size_t size() const MBASE_EXCLUDES(mutex_) {
    mbase::SharedLockGuard lock(mutex_);
    return cache_.size();
  }

private:
  mutable mbase::SharedLockable<std::shared_mutex> mutex_;
  std::unordered_map<PipelineLayoutCacheKey, TLayout, PipelineLayoutCacheKey::Hasher>
    cache_ MBASE_GUARDED_BY(mutex_);
};

} // namespace pipeline
