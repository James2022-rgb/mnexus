#pragma once

// c++ headers ------------------------------------------
#include <functional>

// project headers --------------------------------------
#include "sync/resource_sync.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// IVulkanDeferredDestroyer
//
// Interface for enqueuing Vulkan objects for deferred destruction.
// The destroy callback will be invoked once all queues in the snapshot
// have completed past their respective retire serials.
//

class IVulkanDeferredDestroyer {
public:
  virtual ~IVulkanDeferredDestroyer() = default;

  virtual void EnqueueDestroy(
    std::function<void()> destroy_func,
    ResourceSyncStamp::Snapshot snapshot
  ) = 0;
};

} // namespace mnexus_backend::vulkan
