#pragma once

namespace mnexus_backend::vulkan {

//
// vk-deferred_destroyer.h
//

class IVulkanDeferredDestroyer;

//
// vk-device.h
//

struct VulkanDeviceDesc;
class IVulkanDevice;

//
// vk-device_helper.h
//

struct QueueFamilyRequest;

//
// vk-instance.h
//

class VulkanInstance;

//
// vk-physical_device.h
//

class PhysicalDeviceDesc;

//
// vk-queue.h
//

class IVulkanQueue;

//
// vk-staging.h
//

struct StagingBuffer;
class StagingBufferPool;
class TransientCommandPool;

} // namespace mnexus_backend::vulkan
