#pragma once

// c++ headers ------------------------------------------
#include <optional>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/accessor.h"
#include "mbase/public/array_proxy.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/vk-instance.h"

namespace mnexus_backend::vulkan {

// VK_KHR_driver_properties, core in Vulkan 1.2
// VK_KHR_multiview, core in Vulkan 1.1
// VK_KHR_depth_stencil_resolve, core in Vulkan 1.2
// VK_KHR_16bit_storage , core in Vulkan 1.2
// VK_EXT_descriptor_indexing, core in Vulkan 1.2
// VK_KHR_buffer_device_address , core in Vulkan 1.2
// VK_KHR_acceleration_structure
// VK_KHR_ray_tracing_pipeline
// VK_KHR_ray_query

struct PhysialDeviceDriverPropertiesDesc final {
  VkPhysicalDeviceDriverPropertiesKHR properties {};
};
struct PhysicalDeviceMultiviewDesc final {
  VkPhysicalDeviceMultiviewFeaturesKHR features {};
  VkPhysicalDeviceMultiviewPropertiesKHR properties {};
};
struct PhysicalDeviceDepthStencilResolveDesc final {
  VkPhysicalDeviceDepthStencilResolvePropertiesKHR properties {};
};
struct PhysicalDevice16bitStorageDesc final {
  VkPhysicalDevice16BitStorageFeaturesKHR features {};
};
struct PhysicalDeviceDescriptorIndexingDesc final {
  VkPhysicalDeviceDescriptorIndexingFeaturesEXT features {};
  VkPhysicalDeviceDescriptorIndexingPropertiesEXT properties {};
};
struct PhysicalDeviceBufferDeviceAddressDesc final {
  VkPhysicalDeviceBufferDeviceAddressFeaturesKHR features {};
};
struct PhysicalDeviceAccelerationStructureDesc final {
  VkPhysicalDeviceAccelerationStructureFeaturesKHR features {};
  VkPhysicalDeviceAccelerationStructurePropertiesKHR properties {};
};
struct PhysicalDeviceRayTracingPipelineDesc final {
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR features {};
  VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties {};
};
struct PhysicalDeviceRayQueryDesc final {
  VkPhysicalDeviceRayQueryFeaturesKHR features {};
};

struct QueueFamilyProperties final {
  VkQueueFamilyProperties properties {};
  VkQueueFamilyVideoPropertiesKHR video_properties {};
};

class PhysicalDeviceDesc final {
public:
  PhysicalDeviceDesc() = default;
  ~PhysicalDeviceDesc() = default;
  MBASE_DEFAULT_COPY_MOVE(PhysicalDeviceDesc);

  static PhysicalDeviceDesc Query(VulkanInstance const& instance, VkPhysicalDevice vk_physical_device);

  MBASE_ACCESSOR_GETV(VkInstance, instance);
  MBASE_ACCESSOR_GETV(VkPhysicalDevice, handle);

  MBASE_ACCESSOR_GETCR(VkPhysicalDeviceFeatures, features);
  MBASE_ACCESSOR_GETCR(VkPhysicalDeviceProperties, properties);
  MBASE_ACCESSOR_GETCR(VkPhysicalDeviceMemoryProperties, memory_properties);

  MBASE_ACCESSOR_GETCR_OPTIONAL(PhysialDeviceDriverPropertiesDesc, driver_properties_desc);
  MBASE_ACCESSOR_GETCR_OPTIONAL(PhysicalDeviceMultiviewDesc, multiview_desc);
  MBASE_ACCESSOR_GETCR_OPTIONAL(PhysicalDeviceDepthStencilResolveDesc, depth_stencil_resolve_desc);
  MBASE_ACCESSOR_GETCR_OPTIONAL(PhysicalDevice16bitStorageDesc, sixteen_bit_storage_desc);
  MBASE_ACCESSOR_GETCR_OPTIONAL(PhysicalDeviceDescriptorIndexingDesc, descriptor_indexing_desc);
  MBASE_ACCESSOR_GETCR_OPTIONAL(PhysicalDeviceBufferDeviceAddressDesc, buffer_device_address_desc);
  MBASE_ACCESSOR_GETCR_OPTIONAL(PhysicalDeviceAccelerationStructureDesc, acceleration_structure_desc);
  MBASE_ACCESSOR_GETCR_OPTIONAL(PhysicalDeviceRayTracingPipelineDesc, ray_tracing_pipeline_desc);
  MBASE_ACCESSOR_GETCR_OPTIONAL(PhysicalDeviceRayQueryDesc, ray_query_desc);

  MBASE_ACCESSOR_ARRAY_PROXY(VkExtensionProperties, extensions);
  MBASE_ACCESSOR_ARRAY_PROXY(QueueFamilyProperties, queue_families);

  [[nodiscard]] bool QuerySurfaceSupport(uint32_t queue_family_index, VkSurfaceKHR surface) const;
  [[nodiscard]] VkExtensionProperties const* QueryExtensionSupport(std::string_view extension_name) const;
private:
  VkInstance instance_ = VK_NULL_HANDLE;

  VkPhysicalDevice handle_ = VK_NULL_HANDLE;

  std::vector<VkExtensionProperties> extensions_;

  VkPhysicalDeviceFeatures features_ {};
  VkPhysicalDeviceProperties properties_ {};
  VkPhysicalDeviceMemoryProperties memory_properties_ {};

  std::optional<PhysialDeviceDriverPropertiesDesc> driver_properties_desc_;
  std::optional<PhysicalDeviceMultiviewDesc> multiview_desc_;
  std::optional<PhysicalDeviceDepthStencilResolveDesc> depth_stencil_resolve_desc_;
  std::optional<PhysicalDevice16bitStorageDesc> sixteen_bit_storage_desc_;
  std::optional<PhysicalDeviceDescriptorIndexingDesc> descriptor_indexing_desc_;
  std::optional<PhysicalDeviceBufferDeviceAddressDesc> buffer_device_address_desc_;
  std::optional<PhysicalDeviceAccelerationStructureDesc> acceleration_structure_desc_;
  std::optional<PhysicalDeviceRayTracingPipelineDesc> ray_tracing_pipeline_desc_;
  std::optional<PhysicalDeviceRayQueryDesc> ray_query_desc_;

  std::vector<QueueFamilyProperties> queue_families_;
};

std::optional<PhysicalDeviceDesc> SelectPhysicalDevice(
  VulkanInstance const& instance,
  mbase::ArrayProxy<std::string const> extensions,
  mbase::ArrayProxy<char const* const> non_mandatory_extensions
);

} // namespace mnexus_backend::vulkan
