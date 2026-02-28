#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <atomic>
#include <shared_mutex>
#include <unordered_map>

// public project headers -------------------------------
#include "mbase/public/tsa.h"

// project headers --------------------------------------
#include "pipeline/render_pipeline_cache_key.h"
#include "pipeline/render_pipeline_diagnostics.h"

namespace pipeline {

/// Thread-safe pipeline cache keyed by `RenderPipelineCacheKey`.
/// Backends instantiate with their pipeline type (e.g. `wgpu::RenderPipeline`).
template<typename TPipeline>
class TRenderPipelineCache final {
public:
  /// Looks up `key` in the cache. On hit, returns the cached pipeline and
  /// sets `*out_cache_hit = true`. On miss, calls `factory(key)` to create
  /// a new pipeline, inserts it, sets `*out_cache_hit = false`, and returns it.
  /// The factory is only invoked while the exclusive lock is held, so at most one
  /// thread creates a pipeline for any given key.
  template<typename TFactory>
  TPipeline FindOrInsert(RenderPipelineCacheKey const& key, TFactory&& factory,
                         bool* out_cache_hit) MBASE_EXCLUDES(mutex_) {
    // Fast path: shared lock for concurrent reads.
    {
      mbase::SharedLockGuard shared_lock(mutex_);
      total_lookups_.fetch_add(1, std::memory_order_relaxed);
      auto it = cache_.find(key);
      if (it != cache_.end()) {
        cache_hits_.fetch_add(1, std::memory_order_relaxed);
        *out_cache_hit = true;
        return it->second;
      }
    }

    // Slow path: exclusive lock, double-check, then create.
    mbase::LockGuard exclusive_lock(mutex_);
    auto [it, inserted] = cache_.emplace(key, TPipeline{});
    if (!inserted) {
      // Another thread inserted between our shared unlock and exclusive lock.
      cache_hits_.fetch_add(1, std::memory_order_relaxed);
      *out_cache_hit = true;
      return it->second;
    }
    cache_misses_.fetch_add(1, std::memory_order_relaxed);
    *out_cache_hit = false;
    it->second = factory(key);
    return it->second;
  }

  [[nodiscard]] RenderPipelineCacheDiagnostics GetDiagnostics() const MBASE_EXCLUDES(mutex_) {
    mbase::SharedLockGuard lock(mutex_);
    return RenderPipelineCacheDiagnostics {
      .total_lookups = total_lookups_.load(std::memory_order_relaxed),
      .cache_hits = cache_hits_.load(std::memory_order_relaxed),
      .cache_misses = cache_misses_.load(std::memory_order_relaxed),
      .cached_pipeline_count = static_cast<uint64_t>(cache_.size()),
    };
  }

  void Clear() MBASE_EXCLUDES(mutex_) {
    mbase::LockGuard lock(mutex_);
    cache_.clear();
    total_lookups_.store(0, std::memory_order_relaxed);
    cache_hits_.store(0, std::memory_order_relaxed);
    cache_misses_.store(0, std::memory_order_relaxed);
  }

  [[nodiscard]] size_t size() const MBASE_EXCLUDES(mutex_) {
    mbase::SharedLockGuard lock(mutex_);
    return cache_.size();
  }

private:
  mutable mbase::SharedLockable<std::shared_mutex> mutex_;
  std::unordered_map<RenderPipelineCacheKey, TPipeline, RenderPipelineCacheKey::Hasher>
    cache_ MBASE_GUARDED_BY(mutex_);

  // Diagnostics counters (atomic, lock-free update).
  std::atomic<uint64_t> total_lookups_ = 0;
  std::atomic<uint64_t> cache_hits_ = 0;
  std::atomic<uint64_t> cache_misses_ = 0;
};

} // namespace pipeline
