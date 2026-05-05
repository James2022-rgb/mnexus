// TU header --------------------------------------------
#include "backend-vulkan/device/vk-device.h"

// c++ headers ------------------------------------------
#include <algorithm>
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
#include "backend-vulkan/device/vk-queue.h"

#include "backend-vulkan/device/vk-device_helper.h"

namespace mnexus_backend::vulkan {

// ====================================================================================================
// VulkanDevice
//

class VulkanDevice final : public IVulkanDevice {
public:
  ~VulkanDevice() override = default;
  MBASE_DISALLOW_COPY_MOVE(VulkanDevice);

  void Shutdown() override {
    if (handle_ != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(handle_);
    }

    this->ProcessPendingDestroys();
    {
      mbase::LockGuard lock(pending_destroys_mutex_);

      MBASE_ASSERT_MSG(pending_destroys_.empty(), "Pending destroys remain after device idle (count: {})", pending_destroys_.size());
    }

    if (vma_allocator_ != VK_NULL_HANDLE) {
      vmaDestroyAllocator(vma_allocator_);
      vma_allocator_ = VK_NULL_HANDLE;
    }

    if (handle_ != VK_NULL_HANDLE) {
      for (auto& q : queues_) {
        if (q != nullptr) {
          q->Shutdown();
          q.reset();
        }
      }

      vkDestroyDevice(handle_, nullptr);
      handle_ = VK_NULL_HANDLE;
    }

    instance_.Shutdown();
    VulkanInstance::ShutdownVolk();
  }

  VulkanInstance const* instance() const override { return &instance_; }
  PhysicalDeviceDesc const& physical_device_desc() const override { return *physical_device_desc_; }
  VkDevice handle() const override { return handle_; }
  mnexus::QueueSelection const& queue_selection() const override { return queue_selection_; }
  VmaAllocator vma_allocator() const override { return vma_allocator_; }

  bool IsExtensionEnabled(char const* /*extension_name*/) const override {
    // FIXME: Implement `VulkanDevice::IsExtensionEnabled`.
    return false;
  }

  IVulkanDeferredDestroyer* GetDeferredDestroyer() const override { return &deferred_destroyer_; }

  IVulkanQueue* GetQueue(mnexus::QueueId const& queue_id) override {
    std::optional<uint32_t> opt_index = queue_index_map_.Find(queue_id);
    if (!opt_index.has_value()) {
      return nullptr;
    }
    return queues_[*opt_index].get();
  }

private:
  friend class IVulkanDevice; // For Create() to construct.

  explicit VulkanDevice(
    VulkanInstance instance,
    PhysicalDeviceDesc const& physical_device_desc,
    VkDevice handle,
    mnexus::QueueSelection queue_selection,
    QueueIndexMap queue_index_map,
    VmaAllocator vma_allocator
  ) :
    instance_(std::move(instance)),
    physical_device_desc_(std::make_unique<PhysicalDeviceDesc>(physical_device_desc)),
    handle_(handle),
    queue_selection_(queue_selection),
    queue_index_map_(queue_index_map),
    vma_allocator_(vma_allocator)
  {}

  VulkanInstance instance_;
  std::unique_ptr<PhysicalDeviceDesc> physical_device_desc_;
  VkDevice handle_ = VK_NULL_HANDLE;
  mnexus::QueueSelection queue_selection_;
  QueueIndexMap queue_index_map_;
  std::unique_ptr<IVulkanQueue> queues_[kMaxQueues] {};
  VmaAllocator vma_allocator_ = VK_NULL_HANDLE;

  // --- Deferred destruction (composition, not inheritance) ---

  class DeferredDestroyer final : public IVulkanDeferredDestroyer {
  public:
    explicit DeferredDestroyer(VulkanDevice& owner) : owner_(owner) {}

    void EnqueueDestroy(
      std::function<void()> destroy_func,
      ResourceSyncStamp::Snapshot snapshot
    ) override {
      if (snapshot.used_mask == 0) {
        destroy_func();
        return;
      }

      bool completed = true;
      for (uint32_t index = 0; index < kMaxQueues; ++index) {
        if ((snapshot.used_mask & (1u << index)) != 0) {
          uint64_t const last_used = snapshot.last_used[index];

          MBASE_ASSERT(owner_.queues_[index] != nullptr);
          uint64_t const completed_value = owner_.queues_[index]->GetCompletedValue();

          if (completed_value < last_used) {
            completed = false;
            break;
          }
        }
      }

      if (completed) {
        destroy_func();
      } else {
        owner_.EnqueuePendingDestroy(std::move(destroy_func), snapshot);
      }
    }

    void Process() override {
      owner_.ProcessPendingDestroys();
    }

  private:
    VulkanDevice& owner_;
  };
  mutable DeferredDestroyer deferred_destroyer_{*this};

  struct PendingDestroy {
    std::function<void()> destroy_func;
    ResourceSyncStamp::Snapshot snapshot;
  };

  void EnqueuePendingDestroy(std::function<void()> destroy_func, ResourceSyncStamp::Snapshot snapshot) {
    mbase::LockGuard lock(pending_destroys_mutex_);
    pending_destroys_.emplace_back(
      PendingDestroy {
        .destroy_func = std::move(destroy_func),
        .snapshot = snapshot,
      }
    );
  }

  void ProcessPendingDestroys() {
    mbase::LockGuard lock(pending_destroys_mutex_);

    for (size_t i = 0; i < pending_destroys_.size(); ) {
      PendingDestroy& entry = pending_destroys_[i];

      bool completed = true;
      for (uint32_t qi = 0; qi < kMaxQueues; ++qi) {
        if ((entry.snapshot.used_mask & (1u << qi)) != 0) {
          MBASE_ASSERT(queues_[qi] != nullptr);
          uint64_t const completed_value = queues_[qi]->GetCompletedValue();
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

  mbase::Lockable<std::mutex> pending_destroys_mutex_;
  std::vector<PendingDestroy> pending_destroys_ MBASE_GUARDED_BY(pending_destroys_mutex_);
};


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

  mnexus_backend::QueueIndexMap queue_index_map(selection);

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
    vma_allocator
  ));

  // Retrieve VkQueues, create timeline semaphores, and construct IVulkanQueue
  // entries. The deferred destroyer is hooked up to each queue so that
  // queue ops opportunistically drive deferred destruction.
  auto InitQueue = [&](mnexus::QueueId const& queue_id) -> bool {
    std::optional<uint32_t> opt_index = queue_index_map.Find(queue_id);
    MBASE_ASSERT(opt_index.has_value());
    uint32_t const compact_index = *opt_index;

    VkQueue vk_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(
      vk_device,
      queue_id.queue_family_index,
      queue_id.queue_index,
      &vk_queue
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
    VkSemaphore timeline_semaphore = VK_NULL_HANDLE;
    VkResult const sem_result = vkCreateSemaphore(vk_device, &sem_info, nullptr, &timeline_semaphore);
    if (sem_result != VK_SUCCESS) {
      MBASE_LOG_ERROR("vkCreateSemaphore (timeline) failed: {}", string_VkResult(sem_result));
      return false;
    }

    device->queues_[compact_index] = IVulkanQueue::Create(
      vk_device,
      vk_queue,
      timeline_semaphore,
      queue_id,
      compact_index,
      device->GetDeferredDestroyer()
    );
    return true;
  };

  // TODO: Use a finally-like scope guard here to clean up the device if any of these fail.

  if (!InitQueue(selection.present_capable)) {
    return nullptr;
  }
  if (selection.dedicated_compute.has_value() && !InitQueue(*selection.dedicated_compute)) {
    return nullptr;
  }
  if (selection.dedicated_transfer.has_value() && !InitQueue(*selection.dedicated_transfer)) {
    return nullptr;
  }
  if (selection.dedicated_video_decode.has_value() && !InitQueue(*selection.dedicated_video_decode)) {
    return nullptr;
  }
  if (selection.dedicated_video_encode.has_value() && !InitQueue(*selection.dedicated_video_encode)) {
    return nullptr;
  }

  return device;
}

} // namespace mnexus_backend::vulkan
