#pragma once

// c++ headers ------------------------------------------
#include <tuple>
#include <shared_mutex>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/tsa.h"

// project headers --------------------------------------
#include "container/generational_pool.h"

namespace container {

using ResourceHandle = container::GenerationalHandle;

template<class THot, class TCold>
class TResourceGenerationalPool {
public:
  using SharedMutexType = mbase::SharedLockable<std::shared_mutex>;
  using SharedLockGuardType = mbase::SharedLockGuard<SharedMutexType>;

  TResourceGenerationalPool() = default;
  ~TResourceGenerationalPool() = default;
  MBASE_DISALLOW_COPY_MOVE(TResourceGenerationalPool);

  template <class... HotArgs, class... ColdArgs>
  GenerationalHandle Emplace(
    std::tuple<HotArgs...> hot_args,
    std::tuple<ColdArgs...> cold_args
  ) MBASE_EXCLUDES(mutex_) {
    mbase::LockGuard lock(mutex_);
    return inner_.Emplace(
      std::piecewise_construct,
      std::move(hot_args),
      std::move(cold_args)
    );
  }

  bool Erase(GenerationalHandle handle) MBASE_EXCLUDES(mutex_) {
    mbase::LockGuard lock(mutex_);
    return inner_.Erase(handle);
  }

  std::pair<THot const&, SharedLockGuardType> GetHotConstRefWithSharedLockGuard(
    GenerationalHandle handle
  ) const MBASE_EXCLUDES(mutex_) MBASE_NO_THREAD_SAFETY_ANALYSIS {
    mbase::SharedLockGuard lock(mutex_);
    THot const& hot_ref = inner_.HotRef(handle);
    return { hot_ref, std::move(lock) };
  }

  std::tuple<THot const&, TCold const&, SharedLockGuardType> GetConstRefWithSharedLockGuard(
    GenerationalHandle handle
  ) const MBASE_EXCLUDES(mutex_) MBASE_NO_THREAD_SAFETY_ANALYSIS {
    mbase::SharedLockGuard lock(mutex_);
    THot const&  hot_ref  = inner_.HotRef(handle);
    TCold const& cold_ref = inner_.ColdRef(handle);
    return { hot_ref, cold_ref, std::move(lock) };
  }
  std::tuple<THot&, TCold&, SharedLockGuardType> GetRefWithSharedLockGuard(
    GenerationalHandle handle
  ) MBASE_EXCLUDES(mutex_) MBASE_NO_THREAD_SAFETY_ANALYSIS {
    mbase::SharedLockGuard lock(mutex_);
    THot&  hot_ref  = inner_.HotRef(handle);
    TCold& cold_ref = inner_.ColdRef(handle);
    return { hot_ref, cold_ref, std::move(lock) };
  }

  std::pair<THot const&, TCold const&> LockSharedAndGetConstRef(GenerationalHandle handle) const MBASE_ACQUIRE_SHARED(mutex_) {
    mutex_.lock_shared();
    THot const&  hot_ref  = inner_.HotRef(handle);
    TCold const& cold_ref = inner_.ColdRef(handle);
    return { hot_ref, cold_ref };
  }

  std::pair<THot&, TCold&> LockSharedAndGetRef(GenerationalHandle handle) MBASE_ACQUIRE_SHARED(mutex_) {
    mutex_.lock_shared();
    THot&  hot_ref  = inner_.HotRef(handle);
    TCold& cold_ref = inner_.ColdRef(handle);
    return { hot_ref, cold_ref };
  }

  std::pair<TCold const&, SharedLockGuardType> GetColdConstRefWithSharedLockGuard(
    GenerationalHandle handle
  ) const MBASE_EXCLUDES(mutex_) MBASE_NO_THREAD_SAFETY_ANALYSIS {
    mbase::SharedLockGuard lock(mutex_);
    TCold const& cold_ref = inner_.ColdRef(handle);
    return { cold_ref, std::move(lock) };
  }

  THot& LockSharedAndGetRefHot(GenerationalHandle handle) MBASE_ACQUIRE_SHARED(mutex_) {
    mutex_.lock_shared();
    THot& hot_ref = inner_.HotRef(handle);
    return hot_ref;
  }

  TCold const& LockSharedAndGetConstRefCold(GenerationalHandle handle) const MBASE_ACQUIRE_SHARED(mutex_) {
    mutex_.lock_shared();
    TCold const& cold_ref = inner_.ColdRef(handle);
    return cold_ref;
  }

  void UnlockShared() const MBASE_RELEASE_SHARED(mutex_) {
    mutex_.unlock_shared();
  }

private:
  mbase::SharedLockable<std::shared_mutex> mutable mutex_;
  container::GenerationalPool<THot, TCold> inner_;
};

} // namespace container
