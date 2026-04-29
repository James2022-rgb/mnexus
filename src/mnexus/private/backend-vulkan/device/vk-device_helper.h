#pragma once

// c++ headers ------------------------------------------
#include <vector>

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

namespace mnexus_backend::vulkan {

class PhysicalDeviceDesc;

// ----------------------------------------------------------------------------------------------------
// Queue family selection
//

/// Selects queue families from the physical device for present, compute, transfer,
/// and video encode/decode roles. Returns a default-constructed QueueSelection
/// (with empty present_capable) on failure.
mnexus::QueueSelection SelectQueueFamilies(PhysicalDeviceDesc const& physical_device_desc);

// ----------------------------------------------------------------------------------------------------
// Queue create info construction
//

struct QueueFamilyRequest final {
  uint32_t family_index;
  uint32_t queue_count;
  std::vector<float> priorities;
};

/// Aggregates `selection` into per-family queue counts and uniform priorities,
/// suitable for populating `VkDeviceQueueCreateInfo` array.
std::vector<QueueFamilyRequest> BuildQueueCreateInfos(mnexus::QueueSelection const& selection);

} // namespace mnexus_backend::vulkan
