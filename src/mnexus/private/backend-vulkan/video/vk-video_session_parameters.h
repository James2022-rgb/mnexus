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

  /// Annex B concatenation of the encoded VPS + SPS + PPS bytes generated
  /// by the driver via `vkGetEncodedVideoSessionParametersKHR`. Populated
  /// only by the encode factory; left empty for decode parameters (decode
  /// already has the bytes in `mnexus::VideoSessionParametersDecodeH265Desc`,
  /// no need to cache).
  std::vector<uint8_t> encoded_vps_sps_pps_bytes;
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

/// Authors `StdVideoH265*` VPS / SPS / PPS structs from `desc`, creates
/// a `VkVideoSessionParametersKHR` for the encode session referenced by
/// `desc.session`, and reads back the driver-encoded VPS / SPS / PPS
/// NAL bytes via `vkGetEncodedVideoSessionParametersKHR`. The bytes are
/// cached in `VideoSessionParametersCold::encoded_vps_sps_pps_bytes`,
/// retrievable via `IDevice::GetEncodedVideoSessionParametersBytes`.
///
/// Returns `ResourceHandle::Null()` on failure.
resource_pool::ResourceHandle EmplaceVideoSessionParametersResourcePoolEncodeH265(
  VideoSessionParametersResourcePool& out_pool,
  IVulkanDevice& vk_device,
  VideoSessionResourcePool const& session_pool,
  mnexus::VideoSessionParametersEncodeH265Desc const& desc
);

} // namespace mnexus_backend::vulkan
