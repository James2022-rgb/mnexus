// TU header --------------------------------------------
#include "backend-vulkan/resource/vk-query_pool.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/device/vk-device.h"
#include "backend-vulkan/device/vk-physical_device.h"

namespace mnexus_backend::vulkan {

resource_pool::ResourceHandle EmplaceTimestampQueryPool(
  QueryPoolResourcePool& out_pool,
  IVulkanDevice&         vk_device,
  uint32_t               query_count
) {
  if (query_count == 0) {
    MBASE_LOG_ERROR("EmplaceTimestampQueryPool: query_count must be > 0.");
    return resource_pool::ResourceHandle::Null();
  }

  VkPhysicalDeviceProperties const& props = vk_device.physical_device_desc().properties();
  if (props.limits.timestampPeriod == 0.0f) {
    MBASE_LOG_ERROR("EmplaceTimestampQueryPool: device does not support timestamp queries (timestampPeriod == 0).");
    return resource_pool::ResourceHandle::Null();
  }

  VkQueryPoolCreateInfo const create_info {
    .sType              = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    .pNext              = nullptr,
    .flags              = 0,
    .queryType          = VK_QUERY_TYPE_TIMESTAMP,
    .queryCount         = query_count,
    .pipelineStatistics = 0,
  };
  VkQueryPool vk_query_pool = VK_NULL_HANDLE;
  VkResult const result = vkCreateQueryPool(vk_device.handle(), &create_info, nullptr, &vk_query_pool);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateQueryPool (TIMESTAMP) failed: {}", static_cast<int>(result));
    return resource_pool::ResourceHandle::Null();
  }

  QueryPoolHot hot   { .vk_query_pool = vk_query_pool };
  QueryPoolCold cold {
    .query_count         = query_count,
    .timestamp_period_ns = props.limits.timestampPeriod,
  };
  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

} // namespace mnexus_backend::vulkan
