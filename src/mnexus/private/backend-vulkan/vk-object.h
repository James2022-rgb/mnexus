#pragma once

// c++ headers ------------------------------------------
#include <functional>
#include <utility>
#include <memory>

// public project headers -------------------------------
#include "mbase/public/access.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/vk-deferred_destroyer.h"
#include "sync/resource_sync.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// TVulkanObjectBase<T>
//
// Bundles a Vulkan handle with a ResourceSyncStamp and a destroy callback.
// Works something like an RAII wrapper, but the destructor delegates to the deferred destruction system instead of immediately destroying the Vulkan handle.
//

// FIXME: `TVulkanObjectBase` should be a lot smaller so as not to pollute the cache lines of the hot path.
// The `std::function<void()>` and the `ResourceSyncStamp` are the main culprit here, the former of which is 64 bytes on MSVC x64 and the latter of which is 48 bytes in our current implementation.
template<class T>
class TVulkanObjectBase {
public:
  using DestroyFunc = std::function<void()>;

  [[nodiscard]] T handle() const { return handle_; }
  [[nodiscard]] bool IsValid() const { return handle_ != VK_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const { return this->IsValid(); }

  ResourceSyncStamp& sync_stamp() { return sync_stamp_; }
  ResourceSyncStamp const& sync_stamp() const { return sync_stamp_; }

protected:
  TVulkanObjectBase() = default;

  TVulkanObjectBase(T handle, DestroyFunc destroy_func, IVulkanDeferredDestroyer* deferred_destroyer) :
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
  T handle_ = VK_NULL_HANDLE;
  std::unique_ptr<DestroyFunc> destroy_func_;
  IVulkanDeferredDestroyer* deferred_destroyer_ = nullptr;
  ResourceSyncStamp sync_stamp_;
};

} // namespace mnexus_backend::vulkan
