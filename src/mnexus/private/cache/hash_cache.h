#pragma once

// c++ headers ------------------------------------------
#include <shared_mutex>
#include <unordered_map>

// public project headers -------------------------------
#include "mbase/public/tsa.h"

/// Thread-safe hash-and-cache with double-checked locking.
///
/// `TKey` must be hashable via `THasher` and equality-comparable.
/// `TValue` must be default-constructible and move-assignable.
///
/// On lookup miss, `FindOrInsert` calls a user-provided factory under
/// an exclusive lock, guaranteeing at most one creation per key.
template<typename TKey, typename TValue, typename THasher>
class THashCache final {
public:
  /// Looks up `key` in the cache. On hit, returns the cached value.
  /// On miss, calls `factory(key)` to create a new value, inserts it, and returns it.
  template<typename TFactory>
  TValue FindOrInsert(TKey const& key, TFactory&& factory) MBASE_EXCLUDES(mutex_) {
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
    auto [it, inserted] = cache_.emplace(key, TValue{});
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
  std::unordered_map<TKey, TValue, THasher> cache_ MBASE_GUARDED_BY(mutex_);
};
