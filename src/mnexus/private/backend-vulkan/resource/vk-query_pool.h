#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/depend/vulkan.h"

namespace mnexus_backend::vulkan {

class IVulkanDevice;

// ----------------------------------------------------------------------------------------------------
// Resource pool slot types for `VK_QUERY_TYPE_TIMESTAMP` query pools.
//
// Currently only timestamp queries are exposed via the public API; the
// existing per-video-session `RESULT_STATUS_ONLY_KHR` pool stays
// internal to `VideoSessionHot`.
//

struct QueryPoolHot final {
  VkQueryPool vk_query_pool = VK_NULL_HANDLE;

  void Stamp(uint32_t /*queue_compact_index*/, uint64_t /*serial*/) {
    // No timeline tracking on query pools today (the public reads are
    // all non-blocking and tolerate "not yet completed").
  }
};

struct QueryPoolCold final {
  uint32_t query_count       = 0;
  /// `VkPhysicalDeviceLimits::timestampPeriod` (= ns per tick).
  /// Cached at creation so `GetTimestampQueryResults` doesn't have to
  /// re-query the physical device.
  float    timestamp_period_ns = 1.0f;
};

using QueryPoolResourcePool = resource_pool::TResourceGenerationalPool<
  QueryPoolHot, QueryPoolCold, mnexus::kResourceTypeQueryPool>;

// ----------------------------------------------------------------------------------------------------
// Factory
//

/// Creates a `VK_QUERY_TYPE_TIMESTAMP` query pool with `query_count`
/// slots and emplaces it in `out_pool`. Returns `ResourceHandle::Null()`
/// on failure (logs the cause).
resource_pool::ResourceHandle EmplaceTimestampQueryPool(
  QueryPoolResourcePool& out_pool,
  IVulkanDevice&         vk_device,
  uint32_t               query_count
);

} // namespace mnexus_backend::vulkan
