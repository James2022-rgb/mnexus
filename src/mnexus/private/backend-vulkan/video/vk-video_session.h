#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/object/vk-object.h"

namespace mnexus_backend::vulkan {

class IVulkanDevice;

// ----------------------------------------------------------------------------------------------------
// VulkanVideoSession
//
// Thin RAII wrapper around `VkVideoSessionKHR`. Destruction goes through the
// deferred destroyer; the destroy callback also frees any backing memory
// allocations bound to the session.
//

class VulkanVideoSession final : public TVulkanObjectBase<VkVideoSessionKHR> {
public:
  VulkanVideoSession() = default;
  VulkanVideoSession(
    VkVideoSessionKHR handle,
    std::function<void()> destroy_func,
    IVulkanDeferredDestroyer* deferred_destroyer
  ) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {}
};

// ----------------------------------------------------------------------------------------------------
// Resource pool slot types
//

struct VideoSessionHot final {
  VulkanVideoSession vk_video_session;

  void Stamp(uint32_t queue_compact_index, uint64_t serial) {
    this->vk_video_session.sync_stamp().Stamp(queue_compact_index, serial);
  }
};

struct VideoSessionCold final {
  mnexus::VideoSessionDecodeH265Desc desc;
};

using VideoSessionResourcePool = resource_pool::TResourceGenerationalPool<
  VideoSessionHot, VideoSessionCold, mnexus::kResourceTypeVideoSession>;

// ----------------------------------------------------------------------------------------------------
// Factory
//

/// Creates a `VkVideoSessionKHR` for H.265 decode and emplaces it in the pool.
/// Allocates any required backing memory (VkVideoSession-specific binding
/// requirements are satisfied with manually allocated `VkDeviceMemory` since
/// VMA does not support video session memory).
///
/// Returns `ResourceHandle::Null()` on failure (logs the cause via mbase log).
resource_pool::ResourceHandle EmplaceVideoSessionResourcePoolDecodeH265(
  VideoSessionResourcePool& out_pool,
  IVulkanDevice& vk_device,
  mnexus::VideoSessionDecodeH265Desc const& desc
);

} // namespace mnexus_backend::vulkan
