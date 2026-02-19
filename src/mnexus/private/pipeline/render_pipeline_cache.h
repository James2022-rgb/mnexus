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
///
/// - `Find()` takes a shared lock (concurrent reads from multiple command lists).
/// - `Insert()` takes an exclusive lock (cache miss → pipeline creation).
template<typename TPipeline>
class TRenderPipelineCache final {
public:
  /// Returns a pointer to the cached pipeline, or nullptr on miss.
  TPipeline* Find(RenderPipelineCacheKey const& key) MBASE_EXCLUDES(mutex_) {
    mbase::SharedLockGuard lock(mutex_);
    total_lookups_.fetch_add(1, std::memory_order_relaxed);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      cache_hits_.fetch_add(1, std::memory_order_relaxed);
      return &it->second;
    }
    cache_misses_.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
  }

  /// Inserts a new pipeline into the cache. Caller must ensure the key is not already present.
  void Insert(RenderPipelineCacheKey key, TPipeline pipeline) MBASE_EXCLUDES(mutex_) {
    mbase::LockGuard lock(mutex_);
    cache_.emplace(std::move(key), std::move(pipeline));
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
