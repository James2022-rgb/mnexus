// TU header --------------------------------------------
#include "backend-vulkan/device/vk-device.h"

// c++ headers ------------------------------------------
#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/accessor.h"
#include "mbase/public/log.h"
#include "mbase/public/tsa.h"

// project headers --------------------------------------
#include "sync/resource_sync.h"

#include "backend-vulkan/depend/vulkan_vma.h"
#include "backend-vulkan/device/vk-deferred_destroyer.h"
#include "backend-vulkan/device/vk-physical_device.h"

#include "backend-vulkan/device/vk-device_helper.h"

namespace mnexus_backend::vulkan {

// ====================================================================================================
// VulkanDevice
//

struct VulkanQueueState final {
  VkQueue vk_queue = VK_NULL_HANDLE;
  VkSemaphore timeline_semaphore = VK_NULL_HANDLE;
  std::atomic<uint64_t> next_submit_serial {1}; // Valid serials start at 1.
};

class VulkanDevice final : public IVulkanDevice {
public:
  ~VulkanDevice() override = default;
  MBASE_DISALLOW_COPY_MOVE(VulkanDevice);

  void Shutdown() override;

  VulkanInstance const* instance() const override { return &instance_; }
  PhysicalDeviceDesc const& physical_device_desc() const override { return *physical_device_desc_; }
  VkDevice handle() const override { return handle_; }
  mnexus::QueueSelection const& queue_selection() const override { return queue_selection_; }
  VmaAllocator vma_allocator() const override { return vma_allocator_; }

  bool IsExtensionEnabled(char const* extension_name) const override {
    // FIXME: Implement `VulkanDevice::IsExtensionEnabled`.
    return false;
  }

  IVulkanDeferredDestroyer* GetDeferredDestroyer() const override { return &deferred_destroyer_; }

  uint64_t QueueGetCompletedValue(mnexus::QueueId const& queue_id) override;
  void QueueWaitSubmitSerial(mnexus::QueueId const& queue_id, uint64_t value) override;
  uint64_t QueueWaitIdle(mnexus::QueueId const& queue_id) override;
  uint64_t QueueAdvanceTimeline(mnexus::QueueId const& queue_id) override;
  uint64_t QueueSubmitSingle(mnexus::QueueId const& queue_id, VkCommandBuffer command_buffer) override;
  uint64_t QueuePresentSwapchainImage(
    mnexus::QueueId const& queue_id,
    uint32_t wait_semaphore_count,
    VkSemaphore const* wait_semaphores,
    uint64_t const* wait_values,
    VkSemaphore present_binary_semaphore,
    VkSwapchainKHR swapchain,
    uint32_t image_index
  ) override;
  uint64_t QueuePresentSwapchainImage(
    mnexus::QueueId const& queue_id,
    uint64_t wait_serial,
    VkSemaphore present_binary_semaphore,
    VkSwapchainKHR swapchain,
    uint32_t image_index
  ) override;

private:
  friend class IVulkanDevice; // For Create() to construct.

  explicit VulkanDevice(
    VulkanInstance instance,
    PhysicalDeviceDesc const& physical_device_desc,
    VkDevice handle,
    mnexus::QueueSelection queue_selection,
    QueueIndexMap queue_index_map,
    VulkanQueueState const* queue_states,
    uint32_t queue_count,
    VmaAllocator vma_allocator
  ) :
    instance_(std::move(instance)),
    physical_device_desc_(std::make_unique<PhysicalDeviceDesc>(physical_device_desc)),
    queue_selection_(queue_selection),
    handle_(handle),
    queue_index_map_(queue_index_map),
    vma_allocator_(vma_allocator)
  {
    for (uint32_t i = 0; i < queue_count; ++i) {
      queue_states_[i].vk_queue = queue_states[i].vk_queue;
      queue_states_[i].timeline_semaphore = queue_states[i].timeline_semaphore;
      queue_states_[i].next_submit_serial.store(queue_states[i].next_submit_serial.load(std::memory_order_seq_cst), std::memory_order_seq_cst);
    }
  }

  VulkanInstance instance_;
  std::unique_ptr<PhysicalDeviceDesc> physical_device_desc_;
  VkDevice handle_ = VK_NULL_HANDLE;
  mnexus::QueueSelection queue_selection_;
  QueueIndexMap queue_index_map_;
  VulkanQueueState queue_states_[kMaxQueues] {};
  VmaAllocator vma_allocator_ = VK_NULL_HANDLE;

  // --- Deferred destruction (composition, not inheritance) ---

  class DeferredDestroyer final : public IVulkanDeferredDestroyer {
  public:
    explicit DeferredDestroyer(VulkanDevice& owner) : owner_(owner) {}
    void EnqueueDestroy(
      std::function<void()> destroy_func,
      ResourceSyncStamp::Snapshot snapshot
    ) override;
  private:
    VulkanDevice& owner_;
  };
  mutable DeferredDestroyer deferred_destroyer_{*this};

  struct PendingDestroy {
    std::function<void()> destroy_func;
    ResourceSyncStamp::Snapshot snapshot;
  };

  void EnqueuePendingDestroy(std::function<void()> destroy_func, ResourceSyncStamp::Snapshot snapshot);
  void ProcessPendingDestroys();

  mbase::Lockable<std::mutex> pending_destroys_mutex_;
  std::vector<PendingDestroy> pending_destroys_ MBASE_GUARDED_BY(pending_destroys_mutex_);
};

#define RESOLVE_QUEUE_INDEX(var_name, queue_id) \
  std::optional<uint32_t> opt_##var_name = queue_index_map_.Find(queue_id); \
  MBASE_ASSERT_MSG(opt_##var_name.has_value(), "Unknown QueueId ({}, {})", (queue_id).queue_family_index, (queue_id).queue_index); \
  uint32_t const var_name = *opt_##var_name


// ----------------------------------------------------------------------------------------------------
// VulkanDevice::Create
//

std::unique_ptr<IVulkanDevice> IVulkanDevice::Create(
  VulkanInstance instance,
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
  if (desc.physical_device_desc->QueryExtensionSupport(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == nullptr) {
    MBASE_LOG_ERROR("VK_KHR_timeline_semaphore is not supported by the physical device.");
    return nullptr;
  }
  device_extensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);

  // VK_KHR_synchronization2 is mandatory for the mnexus Vulkan backend.
  if (desc.physical_device_desc->QueryExtensionSupport(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) == nullptr) {
    MBASE_LOG_ERROR("VK_KHR_synchronization2 is not supported by the physical device.");
    return nullptr;
  }
  device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);

  bool has_create_renderpass2 = desc.physical_device_desc->QueryExtensionSupport(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME) != nullptr;
  if (has_create_renderpass2) {
    device_extensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
  }

  bool has_depth_stencil_resolve = desc.physical_device_desc->QueryExtensionSupport(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME) != nullptr;
  if (has_depth_stencil_resolve) {
    device_extensions.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
  }

  bool has_dynamic_rendering = desc.physical_device_desc->QueryExtensionSupport(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) != nullptr;
  if (has_dynamic_rendering) {
    device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
  }

  // De-duplicate device extension names.
  {
    std::sort(device_extensions.begin(), device_extensions.end());
    auto last = std::unique(device_extensions.begin(), device_extensions.end());
    device_extensions.erase(last, device_extensions.end());
  }

  // Features.
  VkPhysicalDeviceFeatures device_features{};

  void** pp_next = nullptr;

  VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_semaphore_features{};
  timeline_semaphore_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR;
  timeline_semaphore_features.timelineSemaphore = VK_TRUE;
  pp_next = &timeline_semaphore_features.pNext;

  VkPhysicalDeviceSynchronization2Features sync2_features{};
  sync2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
  sync2_features.synchronization2 = VK_TRUE;
  *pp_next = &sync2_features;
  pp_next = &sync2_features.pNext;

  VkPhysicalDeviceVulkan11Features device_features_11{};
  device_features_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  *pp_next = &device_features_11;
  pp_next = &device_features_11.pNext;

  VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features{};
  if (has_dynamic_rendering) {
    dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamic_rendering_features.dynamicRendering = VK_TRUE;
    *pp_next = &dynamic_rendering_features;
    pp_next = &dynamic_rendering_features.pNext;
  }

  VkDeviceCreateInfo info {};
  info.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.pNext                = &timeline_semaphore_features;
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
    std::move(instance),
    *desc.physical_device_desc,
    vk_device,
    selection,
    queue_index_map,
    queue_states,
    queue_index_map.Count(),
    vma_allocator
  ));

  return device;
}

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::DeferredDestroyer::EnqueueDestroy
//

void VulkanDevice::DeferredDestroyer::EnqueueDestroy(
  std::function<void()> destroy_func,
  ResourceSyncStamp::Snapshot snapshot
) {
  auto& device = owner_;

  if (snapshot.used_mask == 0) {
    destroy_func();
    return;
  }

  bool completed = true;
  for (uint32_t index = 0; index < kMaxQueues; ++index) {
    if ((snapshot.used_mask & (1u << index)) != 0) {
      uint64_t const last_used = snapshot.last_used[index];

      uint64_t completed_value = 0;
      vkGetSemaphoreCounterValueKHR(device.handle_, device.queue_states_[index].timeline_semaphore, &completed_value);

      if (completed_value < last_used) {
        completed = false;
        break;
      }
    }
  }

  if (completed) {
    destroy_func();
  } else {
    device.EnqueuePendingDestroy(std::move(destroy_func), snapshot);
  }
}

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::EnqueuePendingDestroy (private)
//

void VulkanDevice::EnqueuePendingDestroy(
  std::function<void()> destroy_func,
  ResourceSyncStamp::Snapshot snapshot
) {
  mbase::LockGuard lock(pending_destroys_mutex_);
  pending_destroys_.emplace_back(
    PendingDestroy {
      .destroy_func = std::move(destroy_func),
      .snapshot = snapshot,
    }
  );
}

void VulkanDevice::ProcessPendingDestroys() {
  mbase::LockGuard lock(pending_destroys_mutex_);

  for (size_t i = 0; i < pending_destroys_.size(); ) {
    PendingDestroy& entry = pending_destroys_[i];

    bool completed = true;
    for (uint32_t qi = 0; qi < kMaxQueues; ++qi) {
      if ((entry.snapshot.used_mask & (1u << qi)) != 0) {
        uint64_t completed_value = 0;
        vkGetSemaphoreCounterValueKHR(handle_, queue_states_[qi].timeline_semaphore, &completed_value);
        if (completed_value < entry.snapshot.last_used[qi]) {
          completed = false;
          break;
        }
      }
    }

    if (completed) {
      entry.destroy_func();
      // Swap with last and pop (order doesn't matter).
      entry = std::move(pending_destroys_.back());
      pending_destroys_.pop_back();
    } else {
      ++i;
    }
  }
}

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::QueueGetCompletedValue
//

uint64_t VulkanDevice::QueueGetCompletedValue(mnexus::QueueId const& queue_id) {
  RESOLVE_QUEUE_INDEX(index, queue_id);

  uint64_t completed_value = 0;
  vkGetSemaphoreCounterValueKHR(handle_, queue_states_[index].timeline_semaphore, &completed_value);
  return completed_value;
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

  this->ProcessPendingDestroys();
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
  uint64_t const serial = queue_states_[index].next_submit_serial.fetch_add(1, std::memory_order_acq_rel);

  this->ProcessPendingDestroys();

  return serial;
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

  this->ProcessPendingDestroys();

  return serial;
}

uint64_t VulkanDevice::QueuePresentSwapchainImage(
  mnexus::QueueId const& queue_id,
  uint32_t wait_semaphore_count,
  VkSemaphore const* wait_semaphores,
  uint64_t const* wait_values,
  VkSemaphore present_binary_semaphore,
  VkSwapchainKHR swapchain,
  uint32_t image_index
) {
  RESOLVE_QUEUE_INDEX(index, queue_id);

  VulkanQueueState& qs = queue_states_[index];
  uint64_t const serial = qs.next_submit_serial.fetch_add(1, std::memory_order_acq_rel);

  // vkQueuePresentKHR does not support timeline semaphores. To advance the
  // queue timeline we insert a command-less vkQueueSubmit2KHR that:
  //   - waits on the caller's timeline semaphores
  //   - signals the timeline semaphore with the new serial
  //   - signals a binary semaphore for present to wait on
  //
  // See https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html for why a single binary semaphore is NOT sufficient.

  // Build wait infos from the caller's timeline semaphores.
  std::vector<VkSemaphoreSubmitInfoKHR> wait_infos(wait_semaphore_count);
  for (uint32_t i = 0; i < wait_semaphore_count; ++i) {
    wait_infos[i] = {
      .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
      .pNext     = nullptr,
      .semaphore = wait_semaphores[i],
      .value     = wait_values[i],
      .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .deviceIndex = 0,
    };
  }

  // Signal both the timeline (serial tracking) and binary (for present).
  VkSemaphoreSubmitInfoKHR signal_infos[2] {
    {
      .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
      .pNext     = nullptr,
      .semaphore = qs.timeline_semaphore,
      .value     = serial,
      .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .deviceIndex = 0,
    },
    {
      .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
      .pNext     = nullptr,
      .semaphore = present_binary_semaphore,
      .value     = 0, // binary semaphore
      .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .deviceIndex = 0,
    },
  };

  VkSubmitInfo2KHR submit_info {
    .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
    .pNext                    = nullptr,
    .flags                    = 0,
    .waitSemaphoreInfoCount   = wait_semaphore_count,
    .pWaitSemaphoreInfos      = wait_infos.data(),
    .commandBufferInfoCount   = 0,
    .pCommandBufferInfos      = nullptr,
    .signalSemaphoreInfoCount = 2,
    .pSignalSemaphoreInfos    = signal_infos,
  };

  VkResult result = vkQueueSubmit2KHR(qs.vk_queue, 1, &submit_info, VK_NULL_HANDLE);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkQueueSubmit2KHR (pre-present) failed: {}", string_VkResult(result));
    return 0;
  }

  // Present, waiting on the binary semaphore.
  VkPresentInfoKHR present_info {
    .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext              = nullptr,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores    = &present_binary_semaphore,
    .swapchainCount     = 1,
    .pSwapchains        = &swapchain,
    .pImageIndices      = &image_index,
    .pResults           = nullptr,
  };

  result = vkQueuePresentKHR(qs.vk_queue, &present_info);
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    MBASE_LOG_ERROR("vkQueuePresentKHR failed: {}", string_VkResult(result));
  }

  this->ProcessPendingDestroys();

  return serial;
}

uint64_t VulkanDevice::QueuePresentSwapchainImage(
  mnexus::QueueId const& queue_id,
  uint64_t wait_serial,
  VkSemaphore present_binary_semaphore,
  VkSwapchainKHR swapchain,
  uint32_t image_index
) {
  RESOLVE_QUEUE_INDEX(index, queue_id);

  VulkanQueueState& qs = queue_states_[index];
  return this->QueuePresentSwapchainImage(
    queue_id, 1, &qs.timeline_semaphore, &wait_serial, present_binary_semaphore, swapchain, image_index
  );
}

// ----------------------------------------------------------------------------------------------------
// VulkanDevice::Shutdown
//

void VulkanDevice::Shutdown() {
  if (handle_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(handle_);
  }

  this->ProcessPendingDestroys();
  MBASE_ASSERT_MSG(pending_destroys_.empty(), "Pending destroys remain after device idle (count: {})", pending_destroys_.size());

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

  instance_.Shutdown();
  VulkanInstance::ShutdownVolk();
}

} // namespace mnexus_backend::vulkan
