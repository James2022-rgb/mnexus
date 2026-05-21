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

  // ---- Decode-only fields ----------------------------------------------

  /// `VK_QUERY_TYPE_RESULT_STATUS_ONLY_KHR` query pool with a single slot,
  /// chained with this session's `VkVideoProfileInfoKHR`. Each
  /// `DecodeVideoH265` call wraps `vkCmdDecodeVideoKHR` in
  /// `vkCmdBeginQuery`/`vkCmdEndQuery` against this pool. The next
  /// `BeginVideoCoding` call reads the previous frame's result via
  /// `vkGetQueryPoolResults` (with `WAIT` so it blocks for the prior
  /// submission) and logs it before resetting the pool. Diagnostics-only.
  /// Destroyed alongside the `VkVideoSessionKHR` via the session's
  /// deferred-destroy lambda. `VK_NULL_HANDLE` on encode sessions.
  VkQueryPool result_status_query_pool = VK_NULL_HANDLE;

  /// `true` once a `vkCmdEndQuery` has been recorded against the pool but
  /// the result has not yet been read back. Mutable through the const&
  /// returned by the resource pool's shared lock guard. Single-threaded
  /// access pattern (test bed records and reads on the same thread), so
  /// a plain `bool` is fine -- `std::atomic<bool>` would make the whole
  /// struct non-movable and break resource-pool emplacement.
  mutable bool result_status_pending = false;

  // ---- Encode-only fields ----------------------------------------------

  /// `VK_QUERY_TYPE_VIDEO_ENCODE_FEEDBACK_KHR` query pool with a single
  /// slot, chained with this session's `VkVideoProfileInfoKHR`. Each
  /// `EncodeVideoH265` call wraps `vkCmdEncodeVideoKHR` in
  /// `vkCmdBeginQuery`/`vkCmdEndQuery` against this pool. The next
  /// `BeginVideoCoding` call reads the prior bytes-written value via
  /// `vkGetQueryPoolResults` and caches it in
  /// `last_encoded_bytes_written`. `VK_NULL_HANDLE` on decode sessions.
  VkQueryPool encode_feedback_query_pool = VK_NULL_HANDLE;

  /// `true` once `vkCmdEndQuery` has been recorded against the encode
  /// feedback pool but the result has not yet been drained. Same mutable
  /// rationale as `result_status_pending`.
  mutable bool encode_feedback_pending = false;

  /// Most recently observed value of
  /// `VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BYTES_WRITTEN_BIT_KHR` for this
  /// session. Updated by `BeginVideoCoding` when it drains the previous
  /// `EncodeVideoH265`'s feedback query. Surfaced through
  /// `IDevice::GetLastEncodedBytesWritten`. Zero until the first drain.
  mutable uint64_t last_encoded_bytes_written = 0;

  /// `true` once `last_encoded_bytes_written` carries a valid (drained)
  /// result. `GetLastEncodedBytesWritten` returns `MnBoolFalse` until
  /// then.
  mutable bool last_encoded_bytes_written_valid = false;

  /// `true` after the first encode operation in a coding scope needs to
  /// emit a `vkCmdControlVideoCodingKHR` with the rate-control DISABLED
  /// (CQP) mode. Set by the encode session factory, cleared the first
  /// time `BeginVideoCoding` records the control op for this session.
  mutable bool encode_needs_rate_control_init = false;

  void Stamp(uint32_t queue_compact_index, uint64_t serial) {
    this->vk_video_session.sync_stamp().Stamp(queue_compact_index, serial);
  }
};

/// Holds the original IDevice-facing desc for either a decode or encode
/// session. Exactly one of the two optionals is populated, depending on
/// which factory created the entry. Pure diagnostic state today --
/// nothing in the hot path reads it.
struct VideoSessionCold final {
  std::optional<mnexus::VideoSessionDecodeH265Desc> decode_desc;
  std::optional<mnexus::VideoSessionEncodeH265Desc> encode_desc;
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

/// Creates a `VkVideoSessionKHR` for H.265 encode and emplaces it in the
/// (same) pool. Mirrors the decode factory: queries memory binding
/// requirements, allocates `VkDeviceMemory` per binding, binds in one
/// call. Additionally creates a single-slot
/// `VK_QUERY_TYPE_VIDEO_ENCODE_FEEDBACK_KHR` query pool chained with the
/// session's profile info (required for reading back the per-frame
/// encoded byte count).
///
/// Returns `ResourceHandle::Null()` on failure (logs the cause via mbase log).
resource_pool::ResourceHandle EmplaceVideoSessionResourcePoolEncodeH265(
  VideoSessionResourcePool& out_pool,
  IVulkanDevice& vk_device,
  mnexus::VideoSessionEncodeH265Desc const& desc
);

} // namespace mnexus_backend::vulkan
