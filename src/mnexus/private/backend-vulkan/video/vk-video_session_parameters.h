#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/object/vk-object.h"
#include "backend-vulkan/video/vk-video_session.h"  // for VideoSessionResourcePool

namespace mnexus_backend::vulkan {

class IVulkanDevice;

// ----------------------------------------------------------------------------------------------------
// VulkanVideoSessionParameters
//
// Thin RAII wrapper around `VkVideoSessionParametersKHR`.
//

class VulkanVideoSessionParameters final : public TVulkanObjectBase<VkVideoSessionParametersKHR> {
public:
  VulkanVideoSessionParameters() = default;
  VulkanVideoSessionParameters(
    VkVideoSessionParametersKHR handle,
    std::function<void()> destroy_func,
    IVulkanDeferredDestroyer* deferred_destroyer
  ) :
    TVulkanObjectBase(handle, std::move(destroy_func), deferred_destroyer)
  {}
};

// ----------------------------------------------------------------------------------------------------
// Resource pool slot types
//

struct VideoSessionParametersHot final {
  VulkanVideoSessionParameters vk_video_session_parameters;

  void Stamp(uint32_t queue_compact_index, uint64_t serial) {
    this->vk_video_session_parameters.sync_stamp().Stamp(queue_compact_index, serial);
  }
};

struct VideoSessionParametersCold final {
  /// Originating session handle, kept for diagnostic / lifetime traceability.
  resource_pool::ResourceHandle session_handle = resource_pool::ResourceHandle::Null();
};

using VideoSessionParametersResourcePool = resource_pool::TResourceGenerationalPool<
  VideoSessionParametersHot, VideoSessionParametersCold, mnexus::kResourceTypeVideoSessionParameters>;

// ----------------------------------------------------------------------------------------------------
// Factory
//

/// Parses VPS / SPS / PPS NAL units (via vidsynt), translates them into
/// `StdVideoH265*` structs, and creates a `VkVideoSessionParametersKHR`
/// associated with `desc.session`.
///
/// Returns `ResourceHandle::Null()` on failure.
resource_pool::ResourceHandle EmplaceVideoSessionParametersResourcePoolDecodeH265(
  VideoSessionParametersResourcePool& out_pool,
  IVulkanDevice& vk_device,
  VideoSessionResourcePool const& session_pool,
  mnexus::VideoSessionParametersDecodeH265Desc const& desc
);

} // namespace mnexus_backend::vulkan
