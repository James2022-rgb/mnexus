// TU header --------------------------------------------
#include "backend-vulkan/vk-device.h"

// c++ headers ------------------------------------------
#include <algorithm>
#include <map>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------

namespace mnexus_backend::vulkan {

#define RESOLVE_QUEUE_INDEX(var_name, queue_id) \
  std::optional<uint32_t> opt_##var_name = queue_index_map_.Find(queue_id); \
  MBASE_ASSERT_MSG(opt_##var_name.has_value(), "Unknown QueueId ({}, {})", (queue_id).queue_family_index, (queue_id).queue_index); \
  uint32_t const var_name = *opt_##var_name

namespace {

// ----------------------------------------------------------------------------------------------------
// Queue family selection
//

mnexus::QueueSelection SelectQueueFamilies(
  PhysicalDeviceDesc const& physical_device_desc
) {
  using mnexus::QueueId;

  auto const queue_families = physical_device_desc.queue_families();
  bool const has_video_queue =
    physical_device_desc.CheckExtensionSupport(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME) != nullptr;

  // Build all potential (family_index, queue_index) pairs.
  std::vector<QueueId> potential_queue_ids;
  for (uint32_t fi = 0; fi < queue_families.size(); ++fi) {
    for (uint32_t qi = 0; qi < queue_families[fi].properties.queueCount; ++qi) {
      potential_queue_ids.emplace_back(QueueId(fi, qi));
    }
  }

  // A. Present-capable queue (mandatory): first family with GRAPHICS | COMPUTE, queue index 0.
  std::optional<QueueId> present_capable;
  for (uint32_t fi = 0; fi < queue_families.size(); ++fi) {
    constexpr VkQueueFlags kRequired = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    if ((queue_families[fi].properties.queueFlags & kRequired) == kRequired) {
      present_capable = QueueId(fi, 0);
      break;
    }
  }

  if (!present_capable.has_value()) {
    MBASE_LOG_ERROR("No queue family with GRAPHICS | COMPUTE found.");
    return {};
  }

  // B. Dedicated compute queue (optional).
  std::optional<QueueId> dedicated_compute;
  {
    int best_score = INT_MAX;
    for (auto const& qid : potential_queue_ids) {
      if (qid == *present_capable) continue;

      VkQueueFlags const flags = queue_families[qid.queue_family_index].properties.queueFlags;
      if (!(flags & VK_QUEUE_COMPUTE_BIT)) continue;

      int score;
      // Dedicated COMPUTE-only family.
      if ((flags & ~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT)) == VK_QUEUE_COMPUTE_BIT) {
        score = 0;
      // Separate family from present-capable.
      } else if (qid.queue_family_index != present_capable->queue_family_index) {
        score = 1;
      // Same family, different queue index.
      } else {
        score = 2;
      }

      if (score < best_score) {
        best_score = score;
        dedicated_compute = qid;
      }
    }
  }

  // C. Dedicated transfer queue (optional).
  std::optional<QueueId> dedicated_transfer;
  {
    int best_score = INT_MAX;
    for (auto const& qid : potential_queue_ids) {
      if (qid == *present_capable) continue;
      if (dedicated_compute.has_value() && qid == *dedicated_compute) continue;

      VkQueueFlags const flags = queue_families[qid.queue_family_index].properties.queueFlags;
      if (!(flags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))) continue;

      bool const separate_from_present =
        qid.queue_family_index != present_capable->queue_family_index;
      bool const separate_from_compute =
        !QueueId::InSameQueueFamily(qid, dedicated_compute);

      int score;
      // Dedicated TRANSFER-only family.
      if ((flags & ~(VK_QUEUE_COMPUTE_BIT | VK_QUEUE_SPARSE_BINDING_BIT)) == VK_QUEUE_TRANSFER_BIT) {
        score = 0;
      // Separate from both present-capable and compute.
      } else if (separate_from_present && separate_from_compute) {
        score = 1;
      } else {
        score = 2;
      }

      if (score < best_score) {
        best_score = score;
        dedicated_transfer = qid;
      }
    }
  }

  // D. Dedicated video decode queue (optional, requires VK_KHR_video_queue).
  std::optional<QueueId> dedicated_video_decode;
  if (has_video_queue) {
    int best_score = INT_MAX;
    for (auto const& qid : potential_queue_ids) {
      if (qid == *present_capable) continue;
      if (dedicated_compute.has_value() && qid == *dedicated_compute) continue;
      if (dedicated_transfer.has_value() && qid == *dedicated_transfer) continue;

      VkQueueFlags const flags = queue_families[qid.queue_family_index].properties.queueFlags;
      if (!(flags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)) continue;

      bool const separate_from_present =
        qid.queue_family_index != present_capable->queue_family_index;
      bool const separate_from_compute =
        !QueueId::InSameQueueFamily(qid, dedicated_compute);
      bool const separate_from_transfer =
        !QueueId::InSameQueueFamily(qid, dedicated_transfer);

      int score;
      // Dedicated VIDEO_DECODE-only family.
      if ((flags & ~VK_QUEUE_SPARSE_BINDING_BIT) == VK_QUEUE_VIDEO_DECODE_BIT_KHR) {
        score = 0;
      // Separate from all other selected queues.
      } else if (separate_from_present && separate_from_compute && separate_from_transfer) {
        score = 1;
      } else {
        score = 2;
      }

      if (score < best_score) {
        best_score = score;
        dedicated_video_decode = qid;
      }
    }
  }

  // E. Dedicated video encode queue (optional, requires VK_KHR_video_queue).
  std::optional<QueueId> dedicated_video_encode;
  if (has_video_queue) {
    int best_score = INT_MAX;
    for (auto const& qid : potential_queue_ids) {
      if (qid == *present_capable) continue;
      if (dedicated_compute.has_value() && qid == *dedicated_compute) continue;
      if (dedicated_transfer.has_value() && qid == *dedicated_transfer) continue;
      if (dedicated_video_decode.has_value() && qid == *dedicated_video_decode) continue;

      VkQueueFlags const flags = queue_families[qid.queue_family_index].properties.queueFlags;
      if (!(flags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR)) continue;

      bool const separate_from_present =
        qid.queue_family_index != present_capable->queue_family_index;
      bool const separate_from_compute =
        !QueueId::InSameQueueFamily(qid, dedicated_compute);
      bool const separate_from_transfer =
        !QueueId::InSameQueueFamily(qid, dedicated_transfer);
      bool const separate_from_decode =
        !QueueId::InSameQueueFamily(qid, dedicated_video_decode);

      int score;
      // Dedicated VIDEO_ENCODE-only family.
      if ((flags & ~VK_QUEUE_SPARSE_BINDING_BIT) == VK_QUEUE_VIDEO_ENCODE_BIT_KHR) {
        score = 0;
      // Separate from all other selected queues.
      } else if (separate_from_present && separate_from_compute &&
                 separate_from_transfer && separate_from_decode) {
        score = 1;
      } else {
        score = 2;
      }

      if (score < best_score) {
        best_score = score;
        dedicated_video_encode = qid;
      }
    }
  }

  MBASE_LOG_INFO("Queue selection: present-capable = (family {}, queue {})",
    present_capable->queue_family_index, present_capable->queue_index);
  if (dedicated_compute.has_value()) {
    MBASE_LOG_INFO("Queue selection: dedicated compute = (family {}, queue {})",
      dedicated_compute->queue_family_index, dedicated_compute->queue_index);
  } else {
    MBASE_LOG_INFO("Queue selection: dedicated compute = none");
  }
  if (dedicated_transfer.has_value()) {
    MBASE_LOG_INFO("Queue selection: dedicated transfer = (family {}, queue {})",
      dedicated_transfer->queue_family_index, dedicated_transfer->queue_index);
  } else {
    MBASE_LOG_INFO("Queue selection: dedicated transfer = none");
  }
  if (dedicated_video_decode.has_value()) {
    MBASE_LOG_INFO("Queue selection: dedicated video decode = (family {}, queue {})",
      dedicated_video_decode->queue_family_index, dedicated_video_decode->queue_index);
  } else {
    MBASE_LOG_INFO("Queue selection: dedicated video decode = none");
  }
  if (dedicated_video_encode.has_value()) {
    MBASE_LOG_INFO("Queue selection: dedicated video encode = (family {}, queue {})",
      dedicated_video_encode->queue_family_index, dedicated_video_encode->queue_index);
  } else {
    MBASE_LOG_INFO("Queue selection: dedicated video encode = none");
  }

  return mnexus::QueueSelection {
    .present_capable = *present_capable,
    .dedicated_compute = dedicated_compute,
    .dedicated_transfer = dedicated_transfer,
    .dedicated_video_decode = dedicated_video_decode,
    .dedicated_video_encode = dedicated_video_encode,
  };
}

// ----------------------------------------------------------------------------------------------------
// Queue create info construction
//

struct QueueFamilyRequest {
  uint32_t family_index;
  uint32_t queue_count;
  std::vector<float> priorities;
};

std::vector<QueueFamilyRequest> BuildQueueCreateInfos(
  mnexus::QueueSelection const& selection
) {
  // Accumulate max(queue_index + 1) per family.
  std::map<uint32_t, uint32_t> family_queue_count;

  auto register_queue = [&](mnexus::QueueId const& qid) {
    auto& count = family_queue_count[qid.queue_family_index];
    count = std::max(count, qid.queue_index + 1);
  };

  register_queue(selection.present_capable);
  if (selection.dedicated_compute.has_value()) {
    register_queue(*selection.dedicated_compute);
  }
  if (selection.dedicated_transfer.has_value()) {
    register_queue(*selection.dedicated_transfer);
  }
  if (selection.dedicated_video_decode.has_value()) {
    register_queue(*selection.dedicated_video_decode);
  }
  if (selection.dedicated_video_encode.has_value()) {
    register_queue(*selection.dedicated_video_encode);
  }

  std::vector<QueueFamilyRequest> result;
  for (auto const& [family_index, queue_count] : family_queue_count) {
    float const priority = 1.0f / static_cast<float>(queue_count);
    result.push_back(QueueFamilyRequest {
      .family_index = family_index,
      .queue_count = queue_count,
      .priorities = std::vector<float>(queue_count, priority),
    });
  }
  return result;
}

} // namespace

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::Create
//

std::unique_ptr<VulkanDevice> VulkanDevice::Create(
  VulkanInstance const& instance,
  VulkanDeviceDesc const& desc
) {
  // Select queue families.
  mnexus::QueueSelection const selection = SelectQueueFamilies(*desc.physical_device_desc);

  // Build VkDeviceQueueCreateInfo array.
  std::vector<QueueFamilyRequest> const queue_requests = BuildQueueCreateInfos(selection);

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  for (auto const& req : queue_requests) {
    VkDeviceQueueCreateInfo qci {};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = req.family_index;
    qci.queueCount       = req.queue_count;
    qci.pQueuePriorities = req.priorities.data();
    queue_create_infos.push_back(qci);
  }

  // Device extensions.
  std::vector<char const*> device_extensions;
  if (!desc.headless) {
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }

  // VK_KHR_timeline_semaphore is mandatory for the mnexus Vulkan backend.
  if (desc.physical_device_desc->CheckExtensionSupport(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == nullptr) {
    MBASE_LOG_ERROR("VK_KHR_timeline_semaphore is not supported by the physical device.");
    return nullptr;
  }
  device_extensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);

  // VK_KHR_synchronization2 is mandatory for the mnexus Vulkan backend.
  if (desc.physical_device_desc->CheckExtensionSupport(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) == nullptr) {
    MBASE_LOG_ERROR("VK_KHR_synchronization2 is not supported by the physical device.");
    return nullptr;
  }
  device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);

  // Features.
  VkPhysicalDeviceFeatures device_features {};

  VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_semaphore_features {};
  timeline_semaphore_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR;
  timeline_semaphore_features.timelineSemaphore = VK_TRUE;

  VkPhysicalDeviceSynchronization2Features sync2_features {};
  sync2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
  sync2_features.pNext = &timeline_semaphore_features;
  sync2_features.synchronization2 = VK_TRUE;

  VkPhysicalDeviceVulkan11Features device_features_11 {};
  device_features_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  device_features_11.pNext = &sync2_features;

  VkDeviceCreateInfo info {};
  info.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.pNext                = &device_features_11;
  info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
  info.pQueueCreateInfos    = queue_create_infos.data();
  info.enabledLayerCount    = 0;
  info.ppEnabledLayerNames  = nullptr;
  info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
  info.ppEnabledExtensionNames = device_extensions.data();
  info.pEnabledFeatures = &device_features;

  VkDevice vk_device = VK_NULL_HANDLE;
  VkResult const result = vkCreateDevice(desc.physical_device_desc->handle(), &info, nullptr, &vk_device);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateDevice failed \"{}\".", string_VkResult(result));
    return nullptr;
  }

  // Retrieve VkQueues and create timeline semaphores.
  mnexus_backend::QueueIndexMap queue_index_map(selection);
  VulkanQueueState queue_states[mnexus_backend::kMaxQueues] {};

  auto InitQueueState = [&](mnexus::QueueId const& queue_id) -> bool {
    std::optional<uint32_t> opt_index = queue_index_map.Find(queue_id);
    MBASE_ASSERT(opt_index.has_value());
    uint32_t const index = *opt_index;

    vkGetDeviceQueue(
      vk_device,
      queue_id.queue_family_index,
      queue_id.queue_index,
      &queue_states[index].vk_queue
    );

    VkSemaphoreTypeCreateInfoKHR type_info {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
      .pNext = nullptr,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR,
      .initialValue = 0,
    };
    VkSemaphoreCreateInfo sem_info {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &type_info,
      .flags = 0,
    };
    VkResult sem_result = vkCreateSemaphore(vk_device, &sem_info, nullptr, &queue_states[index].timeline_semaphore);
    if (sem_result != VK_SUCCESS) {
      MBASE_LOG_ERROR("vkCreateSemaphore (timeline) failed: {}", string_VkResult(sem_result));
      return false;
    }
    return true;
  };

  // TODO: Use a finally-like scope guard here to clean up the device if any of these fail.

  if (!InitQueueState(selection.present_capable)) {
    vkDestroyDevice(vk_device, nullptr);
    return nullptr;
  }
  if (selection.dedicated_compute.has_value() && !InitQueueState(*selection.dedicated_compute)) {
    vkDestroyDevice(vk_device, nullptr);
    return nullptr;
  }
  if (selection.dedicated_transfer.has_value() && !InitQueueState(*selection.dedicated_transfer)) {
    vkDestroyDevice(vk_device, nullptr);
    return nullptr;
  }
  if (selection.dedicated_video_decode.has_value() && !InitQueueState(*selection.dedicated_video_decode)) {
    vkDestroyDevice(vk_device, nullptr);
    return nullptr;
  }
  if (selection.dedicated_video_encode.has_value() && !InitQueueState(*selection.dedicated_video_encode)) {
    vkDestroyDevice(vk_device, nullptr);
    return nullptr;
  }

  VmaAllocator vma_allocator = VK_NULL_HANDLE;
  {
    VmaVulkanFunctions vulkan_functions {};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo vma_info {
      .flags = 0,
      .physicalDevice = desc.physical_device_desc->handle(),
      .device = vk_device,
      .preferredLargeHeapBlockSize = 0,
      .pAllocationCallbacks = nullptr,
      .pDeviceMemoryCallbacks = nullptr,
      .pHeapSizeLimit = nullptr,
      .pVulkanFunctions = &vulkan_functions,
      .instance = instance.handle(),
      .vulkanApiVersion = 0,
      .pTypeExternalMemoryHandleTypes = nullptr,
    };

    
    VkResult const vma_result = vmaCreateAllocator(&vma_info, &vma_allocator);
    if (vma_result != VK_SUCCESS) {
      MBASE_LOG_ERROR("vmaCreateAllocator failed: {}", string_VkResult(vma_result));
      vkDestroyDevice(vk_device, nullptr);
      return nullptr;
    }
  }

  auto device = std::unique_ptr<VulkanDevice>(new VulkanDevice(
    &instance,
    *desc.physical_device_desc,
    vk_device,
    selection,
    queue_index_map,
    queue_states,
    queue_index_map.Count(),
    vma_allocator
  ));

  // Initialize staging infrastructure.
  device->staging_buffer_pool_.Initialize(device.get());
  device->transient_command_pool_.Initialize(device.get(), selection.present_capable.queue_family_index);
  device->thread_command_pool_registry_.Initialize(device.get(), selection.present_capable.queue_family_index);

  return device;
}

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::EnqueueDestroy
//

void VulkanDevice::EnqueueDestroy(
  std::function<void()> destroy_func,
  ResourceSyncStamp::Snapshot /*snapshot*/
) {
  // TODO: Check completed serials against snapshot and defer if needed.
  destroy_func();
}

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::QueueGetCompletedValue
//

uint64_t VulkanDevice::QueueGetCompletedValue(mnexus::QueueId const& queue_id) {
  RESOLVE_QUEUE_INDEX(index, queue_id);

  uint64_t value = 0;
  vkGetSemaphoreCounterValueKHR(handle_, queue_states_[index].timeline_semaphore, &value);
  return value;
}

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::QueueWaitSubmitSerial
//

void VulkanDevice::QueueWaitSubmitSerial(mnexus::QueueId const& queue_id, uint64_t value) {
  if (value == 0) {
    return;
  }

  RESOLVE_QUEUE_INDEX(index, queue_id);

  VulkanQueueState& qs = queue_states_[index];
  VkSemaphoreWaitInfoKHR wait_info {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .semaphoreCount = 1,
    .pSemaphores = &qs.timeline_semaphore,
    .pValues = &value,
  };
  vkWaitSemaphoresKHR(handle_, &wait_info, UINT64_MAX);
}

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::QueueWaitIdle
//

uint64_t VulkanDevice::QueueWaitIdle(mnexus::QueueId const& queue_id) {
  RESOLVE_QUEUE_INDEX(index, queue_id);

  uint64_t const last_submitted = queue_states_[index].next_submit_serial.load(std::memory_order_acquire) - 1;
  this->QueueWaitSubmitSerial(queue_id, last_submitted);
  return last_submitted;
}

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::QueueAdvanceTimeline
//

uint64_t VulkanDevice::QueueAdvanceTimeline(mnexus::QueueId const& queue_id) {
  RESOLVE_QUEUE_INDEX(index, queue_id);
  return queue_states_[index].next_submit_serial.fetch_add(1, std::memory_order_acq_rel);
}

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::QueueSubmitSingle
//

uint64_t VulkanDevice::QueueSubmitSingle(mnexus::QueueId const& queue_id, VkCommandBuffer command_buffer) {
  RESOLVE_QUEUE_INDEX(index, queue_id);

  VulkanQueueState& qs = queue_states_[index];
  uint64_t const serial = qs.next_submit_serial.fetch_add(1, std::memory_order_acq_rel);

  VkCommandBufferSubmitInfoKHR cmd_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
    .pNext = nullptr,
    .commandBuffer = command_buffer,
    .deviceMask = 0,
  };

  VkSemaphoreSubmitInfoKHR signal_info {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
    .pNext = nullptr,
    .semaphore = qs.timeline_semaphore,
    .value = serial,
    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
    .deviceIndex = 0,
  };

  VkSubmitInfo2KHR submit_info {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
    .pNext = nullptr,
    .flags = 0,
    .waitSemaphoreInfoCount = 0,
    .pWaitSemaphoreInfos = nullptr,
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &cmd_info,
    .signalSemaphoreInfoCount = 1,
    .pSignalSemaphoreInfos = &signal_info,
  };

  VkResult const result = vkQueueSubmit2KHR(qs.vk_queue, 1, &submit_info, VK_NULL_HANDLE);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkQueueSubmit2KHR failed: {}", string_VkResult(result));
  }

  return serial;
}

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::Shutdown
//

void VulkanDevice::Shutdown() {
  thread_command_pool_registry_.Shutdown();
  transient_command_pool_.Shutdown();
  staging_buffer_pool_.Shutdown();

  if (vma_allocator_ != VK_NULL_HANDLE) {
    vmaDestroyAllocator(vma_allocator_);
    vma_allocator_ = VK_NULL_HANDLE;
  }

  if (handle_ != VK_NULL_HANDLE) {
    for (auto& qs : queue_states_) {
      if (qs.timeline_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(handle_, qs.timeline_semaphore, nullptr);
        qs.timeline_semaphore = VK_NULL_HANDLE;
      }
    }

    vkDestroyDevice(handle_, nullptr);
    handle_ = VK_NULL_HANDLE;
  }
}

} // namespace mnexus_backend::vulkan
