#pragma once

// c++ headers ------------------------------------------
#include <functional>
#include <utility>

// public project headers -------------------------------
#include "mbase/public/access.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"
#include "sync/resource_sync.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// TVulkanObjectBase<T>
//
// Bundles a Vulkan handle with a ResourceSyncStamp and a destroy callback.
// This is NOT an RAII wrapper -- the destructor does NOT destroy the Vulkan handle.
// The destroy callback is stored for the deferred destruction system to invoke
// at the right time (after GPU completion).
//

template<class T>
class TVulkanObjectBase {
public:
  [[nodiscard]] T handle() const { return handle_; }
  [[nodiscard]] bool IsValid() const { return handle_ != VK_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const { return this->IsValid(); }

  ResourceSyncStamp& sync_stamp() { return sync_stamp_; }
  ResourceSyncStamp const& sync_stamp() const { return sync_stamp_; }

  /// Returns the destroy callback (for deferred destruction system to invoke).
  [[nodiscard]] std::function<void()> const& destroy_func() const { return destroy_func_; }

protected:
  TVulkanObjectBase() = default;

  TVulkanObjectBase(T handle, std::function<void()> destroy_func) :
    handle_(handle),
    destroy_func_(std::move(destroy_func))
  {
  }

  ~TVulkanObjectBase() = default;

  MBASE_DISALLOW_COPY(TVulkanObjectBase);

  TVulkanObjectBase(TVulkanObjectBase&& other) noexcept :
    handle_(other.handle_),
    destroy_func_(std::move(other.destroy_func_)),
    sync_stamp_(std::move(other.sync_stamp_))
  {
    other.handle_ = VK_NULL_HANDLE;
  }

  TVulkanObjectBase& operator=(TVulkanObjectBase&& other) noexcept {
    if (this != &other) {
      handle_ = other.handle_;
      destroy_func_ = std::move(other.destroy_func_);
      sync_stamp_ = std::move(other.sync_stamp_);
      other.handle_ = VK_NULL_HANDLE;
    }
    return *this;
  }

private:
  T handle_ = VK_NULL_HANDLE;
  std::function<void()> destroy_func_;
  ResourceSyncStamp sync_stamp_;
};

} // namespace mnexus_backend::vulkan
