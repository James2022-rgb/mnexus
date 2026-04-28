#pragma once

// c++ headers ------------------------------------------
#include <functional>
#include <utility>
#include <memory>
#include <atomic>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/accessor.h"

// project headers --------------------------------------
#include "sync/resource_sync.h"

#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/object/vk-deferred_destroyer.h"

namespace mnexus_backend::vulkan {

inline std::atomic<uint32_t> s_next_serial = 1;

// ----------------------------------------------------------------------------------------------------
// TVulkanObjectBase<T>
//
// Bundles a Vulkan handle with a ResourceSyncStamp and a destroy callback.
// Works something like an RAII wrapper, but the destructor delegates to the deferred destruction system instead of immediately destroying the Vulkan handle.
//

// FIXME: `TVulkanObjectBase` should be a lot smaller so as not to pollute the cache lines of the hot path.
// The `ResourceSyncStamp` is the main culprit here, the former of which is 64 bytes on MSVC x64 and the latter of which is 48 bytes in our current implementation.
template<class T>
class TVulkanObjectBase {
public:
  using DestroyFunc = std::function<void()>;

  [[nodiscard]] bool IsValid() const { return handle_ != VK_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const { return this->IsValid(); }

  MBASE_ACCESSOR_GETV(T, handle);
  MBASE_ACCESSOR_GETV(uint32_t, serial);

  MBASE_ACCESSOR_GETR(ResourceSyncStamp, sync_stamp);
  MBASE_ACCESSOR_GETCR(ResourceSyncStamp, sync_stamp);

protected:
  TVulkanObjectBase() = default;

  TVulkanObjectBase(T handle, DestroyFunc destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
    serial_(s_next_serial.fetch_add(1, std::memory_order_relaxed)),
    handle_(handle),
    destroy_func_(std::make_unique<DestroyFunc>(std::move(destroy_func))),
    deferred_destroyer_(deferred_destroyer)
  {
  }

  ~TVulkanObjectBase() {
    if (deferred_destroyer_ != nullptr) {
      deferred_destroyer_->EnqueueDestroy(*destroy_func_, sync_stamp_.TakeSnapshot());
    }
  }

  MBASE_DISALLOW_COPY(TVulkanObjectBase);

  TVulkanObjectBase(TVulkanObjectBase&& other) noexcept :
    handle_(other.handle_),
    destroy_func_(std::move(other.destroy_func_)),
    deferred_destroyer_(other.deferred_destroyer_),
    sync_stamp_(std::move(other.sync_stamp_))
  {
    other.handle_ = VK_NULL_HANDLE;
    other.deferred_destroyer_ = nullptr;
  }

  TVulkanObjectBase& operator=(TVulkanObjectBase&& other) noexcept {
    if (this != &other) {
      handle_ = other.handle_;
      destroy_func_ = std::move(other.destroy_func_);
      deferred_destroyer_ = other.deferred_destroyer_;
      sync_stamp_ = std::move(other.sync_stamp_);
      other.handle_ = VK_NULL_HANDLE;
      other.deferred_destroyer_ = nullptr;
    }
    return *this;
  }

private:
  uint32_t serial_ = 0;
  T handle_ = VK_NULL_HANDLE;
  std::unique_ptr<DestroyFunc> destroy_func_;
  IVulkanDeferredDestroyer* deferred_destroyer_ = nullptr;
  ResourceSyncStamp sync_stamp_;
};

} // namespace mnexus_backend::vulkan
