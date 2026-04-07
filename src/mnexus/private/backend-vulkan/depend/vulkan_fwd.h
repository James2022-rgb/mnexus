#pragma once

// ----------------------------------------------------------------------------------------------------
// Forward declarations and handle typedefs for Vulkan types.
// Include this instead of vulkan.h when only handle types are needed (e.g. in interface headers).
//

struct VkInstance_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkQueue_T;
struct VkSemaphore_T;
struct VkFence_T;
struct VkShaderModule_T;
struct VkCommandBuffer_T;
struct VkSwapchainKHR_T;

typedef VkInstance_T*       VkInstance;
typedef VkPhysicalDevice_T* VkPhysicalDevice;
typedef VkDevice_T*         VkDevice;
typedef VkQueue_T*          VkQueue;
typedef VkSemaphore_T*      VkSemaphore;
typedef VkFence_T*          VkFence;
typedef VkShaderModule_T*   VkShaderModule;
typedef VkCommandBuffer_T*  VkCommandBuffer;
typedef VkSwapchainKHR_T*   VkSwapchainKHR;
