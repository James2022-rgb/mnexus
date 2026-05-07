// TU header --------------------------------------------
#include "backend-vulkan/video/vk-video_session.h"

// c++ headers ------------------------------------------
#include <optional>
#include <utility>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/device/vk-device.h"
#include "backend-vulkan/device/vk-physical_device.h"
#include "backend-vulkan/resource/types_bridge.h"

// external headers -------------------------------------
#include "vidsynt.h"

namespace mnexus_backend::vulkan {

void VidsyntHevcContextDeleter::operator()(VidsyntHevcContext* p) const noexcept {
  if (p != nullptr) {
    vidsynt_hevc_context_free(p);
  }
}

namespace {

StdVideoH265ProfileIdc ToStdVideoH265ProfileIdc(mnexus::VideoH265Profile profile) {
  switch (profile) {
  case mnexus::VideoH265Profile::kMain:   return STD_VIDEO_H265_PROFILE_IDC_MAIN;
  case mnexus::VideoH265Profile::kMain10: return STD_VIDEO_H265_PROFILE_IDC_MAIN_10;
  }
  return STD_VIDEO_H265_PROFILE_IDC_INVALID;
}

VkVideoComponentBitDepthFlagsKHR ToVkVideoComponentBitDepth(mnexus::VideoBitDepth bit_depth) {
  switch (bit_depth) {
  case mnexus::VideoBitDepth::k8:  return VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
  case mnexus::VideoBitDepth::k10: return VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
  }
  return VK_VIDEO_COMPONENT_BIT_DEPTH_INVALID_KHR;
}

/// Resolve the internal H.265 decode caps slot probed by `PhysicalDeviceDesc::Query`.
VideoDecodeH265Properties const* SelectInternalDecodeH265Slot(
  VideoDecodeH265Capabilities const& caps,
  mnexus::VideoH265Profile profile,
  mnexus::VideoBitDepth bit_depth
) {
  if (profile == mnexus::VideoH265Profile::kMain && bit_depth == mnexus::VideoBitDepth::k8) {
    return caps.main.has_value() ? &*caps.main : nullptr;
  }
  if (profile == mnexus::VideoH265Profile::kMain10 && bit_depth == mnexus::VideoBitDepth::k8) {
    return caps.main10_8bit.has_value() ? &*caps.main10_8bit : nullptr;
  }
  if (profile == mnexus::VideoH265Profile::kMain10 && bit_depth == mnexus::VideoBitDepth::k10) {
    return caps.main10_10bit.has_value() ? &*caps.main10_10bit : nullptr;
  }
  // (Main, 10-bit) is invalid -- Main is 8-bit only.
  return nullptr;
}

/// Pick a memory type for a `VkVideoSessionMemoryRequirementsKHR` binding.
/// Prefer DEVICE_LOCAL when available; if no DEVICE_LOCAL type matches the
/// `memoryTypeBits` mask (some drivers report bindings that only fit
/// host-visible types), fall back to the first matching type. Per the
/// Vulkan spec, any bit set in `memoryTypeBits` is a valid choice.
std::optional<uint32_t> PickVideoSessionMemoryType(
  VkPhysicalDeviceMemoryProperties const& mem_props,
  uint32_t memory_type_bits
) {
  // Pass 1: DEVICE_LOCAL.
  for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
    if ((memory_type_bits & (1u << i)) == 0) continue;
    if ((mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
      return i;
    }
  }
  // Pass 2: any matching type.
  for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
    if ((memory_type_bits & (1u << i)) != 0) {
      return i;
    }
  }
  return std::nullopt;
}

} // anonymous namespace

resource_pool::ResourceHandle EmplaceVideoSessionResourcePoolDecodeH265(
  VideoSessionResourcePool& out_pool,
  IVulkanDevice& vk_device,
  mnexus::VideoSessionDecodeH265Desc const& desc
) {
  // Required device extensions (enabled when the H.265 capability probe in
  // PhysicalDeviceDesc::Query() succeeded; see vk-device.cpp).
  if (!vk_device.IsExtensionEnabled(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME)
   || !vk_device.IsExtensionEnabled(VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME)
   || !vk_device.IsExtensionEnabled(VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME)) {
    MBASE_LOG_ERROR("CreateVideoSessionDecodeH265: required Vulkan Video extensions not enabled.");
    return resource_pool::ResourceHandle::Null();
  }

  // Need a video decode queue family.
  auto const& selection = vk_device.queue_selection();
  if (!selection.dedicated_video_decode.has_value()) {
    MBASE_LOG_ERROR("CreateVideoSessionDecodeH265: no video decode queue family was selected.");
    return resource_pool::ResourceHandle::Null();
  }

  // Look up the internal capabilities slot to get the matching `stdHeaderVersion`
  // (a `VkExtensionProperties` reported per profile by the driver).
  auto const& opt_caps = vk_device.physical_device_desc().video_coding_capabilities();
  if (!opt_caps.has_value()) {
    MBASE_LOG_ERROR("CreateVideoSessionDecodeH265: no video coding capabilities probed for this device.");
    return resource_pool::ResourceHandle::Null();
  }
  VideoDecodeH265Properties const* slot =
    SelectInternalDecodeH265Slot(opt_caps->decode_h265, desc.profile, desc.bit_depth);
  if (slot == nullptr) {
    MBASE_LOG_ERROR("CreateVideoSessionDecodeH265: requested (profile, bit_depth) combination is not supported by the device.");
    return resource_pool::ResourceHandle::Null();
  }

  // Build VkVideoProfileInfoKHR + codec-specific chain.
  VkVideoComponentBitDepthFlagsKHR const bit_depth_flag = ToVkVideoComponentBitDepth(desc.bit_depth);
  VkVideoDecodeH265ProfileInfoKHR h265_profile_info {
    .sType         = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR,
    .pNext         = nullptr,
    .stdProfileIdc = ToStdVideoH265ProfileIdc(desc.profile),
  };
  VkVideoProfileInfoKHR profile_info {
    .sType               = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,
    .pNext               = &h265_profile_info,
    .videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR,
    .chromaSubsampling   = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
    .lumaBitDepth        = bit_depth_flag,
    .chromaBitDepth      = bit_depth_flag,
  };

  VkVideoSessionCreateInfoKHR create_info {
    .sType                          = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR,
    .pNext                          = nullptr,
    .queueFamilyIndex               = selection.dedicated_video_decode->queue_family_index,
    .flags                          = 0,
    .pVideoProfile                  = &profile_info,
    .pictureFormat                  = ToVkFormat(desc.picture_format),
    .maxCodedExtent                 = VkExtent2D { desc.max_coded_extent.width, desc.max_coded_extent.height },
    .referencePictureFormat         = ToVkFormat(desc.reference_picture_format),
    .maxDpbSlots                    = desc.max_dpb_slots,
    .maxActiveReferencePictures     = desc.max_active_reference_pictures,
    .pStdHeaderVersion              = &slot->coding_capabilities.stdHeaderVersion,
  };

  VkDevice const vk_device_handle = vk_device.handle();
  VkVideoSessionKHR vk_session = VK_NULL_HANDLE;
  VkResult result = vkCreateVideoSessionKHR(vk_device_handle, &create_info, nullptr, &vk_session);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateVideoSessionKHR failed: {}", string_VkResult(result));
    return resource_pool::ResourceHandle::Null();
  }

  // Query and satisfy memory binding requirements. VkVideoSession may need
  // multiple distinct allocations (one per `memoryBindIndex`), so we allocate
  // one VkDeviceMemory per requirement and bind in a single call. VMA does not
  // support video session memory, so allocate via vkAllocateMemory directly.
  uint32_t requirement_count = 0;
  vkGetVideoSessionMemoryRequirementsKHR(vk_device_handle, vk_session, &requirement_count, nullptr);

  std::vector<VkVideoSessionMemoryRequirementsKHR> requirements(requirement_count);
  for (auto& req : requirements) {
    req.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;
    req.pNext = nullptr;
  }
  vkGetVideoSessionMemoryRequirementsKHR(vk_device_handle, vk_session, &requirement_count, requirements.data());

  std::vector<VkDeviceMemory> allocated_memories;
  std::vector<VkBindVideoSessionMemoryInfoKHR> bind_infos;
  allocated_memories.reserve(requirement_count);
  bind_infos.reserve(requirement_count);

  auto cleanup_on_failure = [&] {
    for (VkDeviceMemory mem : allocated_memories) {
      vkFreeMemory(vk_device_handle, mem, nullptr);
    }
    vkDestroyVideoSessionKHR(vk_device_handle, vk_session, nullptr);
  };

  auto const& mem_props = vk_device.physical_device_desc().memory_properties();
  for (auto const& req : requirements) {
    std::optional<uint32_t> const opt_type_idx = PickVideoSessionMemoryType(
      mem_props,
      req.memoryRequirements.memoryTypeBits
    );
    if (!opt_type_idx.has_value()) {
      MBASE_LOG_ERROR("CreateVideoSessionDecodeH265: no memory type satisfies binding {} (memoryTypeBits=0x{:x}).",
        req.memoryBindIndex, req.memoryRequirements.memoryTypeBits);
      cleanup_on_failure();
      return resource_pool::ResourceHandle::Null();
    }

    VkMemoryAllocateInfo alloc_info {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext           = nullptr,
      .allocationSize  = req.memoryRequirements.size,
      .memoryTypeIndex = *opt_type_idx,
    };
    VkDeviceMemory memory = VK_NULL_HANDLE;
    result = vkAllocateMemory(vk_device_handle, &alloc_info, nullptr, &memory);
    if (result != VK_SUCCESS) {
      MBASE_LOG_ERROR("vkAllocateMemory for video session binding {} failed: {}", req.memoryBindIndex, string_VkResult(result));
      cleanup_on_failure();
      return resource_pool::ResourceHandle::Null();
    }
    allocated_memories.push_back(memory);

    bind_infos.push_back(VkBindVideoSessionMemoryInfoKHR {
      .sType           = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR,
      .pNext           = nullptr,
      .memoryBindIndex = req.memoryBindIndex,
      .memory          = memory,
      .memoryOffset    = 0,
      .memorySize      = req.memoryRequirements.size,
    });
  }

  result = vkBindVideoSessionMemoryKHR(
    vk_device_handle, vk_session,
    static_cast<uint32_t>(bind_infos.size()), bind_infos.data()
  );
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkBindVideoSessionMemoryKHR failed: {}", string_VkResult(result));
    cleanup_on_failure();
    return resource_pool::ResourceHandle::Null();
  }

  // Wrap with deferred destruction. The destroy lambda owns the allocations
  // and runs once the GPU is done with the session.
  VulkanVideoSession vk_video_session(
    vk_session,
    [vk_device_handle, vk_session, allocated = std::move(allocated_memories)] {
      for (VkDeviceMemory mem : allocated) {
        vkFreeMemory(vk_device_handle, mem, nullptr);
      }
      vkDestroyVideoSessionKHR(vk_device_handle, vk_session, nullptr);
    },
    vk_device.GetDeferredDestroyer()
  );

  // Create a long-lived vidsynt context + POC computer for slice header
  // parsing and POC computation during decode. The POC computer is allocated
  // through the context, so freeing the context (via the unique_ptr deleter)
  // also frees the POC computer; we don't need a separate handle for it.
  VidsyntHevcContextPtr vidsynt_ctx{ vidsynt_hevc_context_new() };
  if (vidsynt_ctx == nullptr) {
    MBASE_LOG_ERROR("CreateVideoSessionDecodeH265: vidsynt_hevc_context_new returned null.");
    cleanup_on_failure();
    return resource_pool::ResourceHandle::Null();
  }
  VidsyntHevcPocComputer* poc_computer = vidsynt_hevc_poc_computer_new(vidsynt_ctx.get());
  if (poc_computer == nullptr) {
    MBASE_LOG_ERROR("CreateVideoSessionDecodeH265: vidsynt_hevc_poc_computer_new returned null.");
    cleanup_on_failure();
    return resource_pool::ResourceHandle::Null();
  }

  VideoSessionHot hot {
    .vk_video_session = std::move(vk_video_session),
    .vidsynt_ctx      = std::move(vidsynt_ctx),
    .poc_computer     = poc_computer,
  };
  VideoSessionCold cold { .desc = desc };

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

} // namespace mnexus_backend::vulkan
