#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/object/vk-object.h"
#include "backend-vulkan/video/vk-video_session.h"  // for VideoSessionResourcePool, VidsyntHevcContextPtr

// vidsynt forward decls (defs in <vidsynt.h>, included only in .cpp).
extern "C" {
struct VidsyntHevcSequenceParameterSet;
struct VidsyntHevcPictureParameterSet;
}

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

  /// vidsynt context that owns the parsed `parsed_sps` / `parsed_pps`
  /// pointers (and the parsed VPS, slice headers, etc. that may be added
  /// during decode). Persisted here so the parsed structs stay alive for
  /// the lifetime of this `VideoSessionParameters` and can be looked up
  /// at `DecodeVideoH265` time.
  VidsyntHevcContextPtr vidsynt_ctx;

  /// Borrowed pointers into `vidsynt_ctx`'s arena. Used at decode time to
  /// (1) call `vidsynt_hevc_context_set_active_sps/pps` on the session's
  /// vidsynt context before parsing the picture's slice header, and
  /// (2) feed `vidsynt_hevc_poc_compute`.
  VidsyntHevcSequenceParameterSet const* parsed_sps = nullptr;
  VidsyntHevcPictureParameterSet const*  parsed_pps = nullptr;
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
