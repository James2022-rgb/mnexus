// TU header --------------------------------------------
#include "backend-vulkan/vk-physical_device.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------

namespace mnexus_backend::vulkan {

PhysicalDeviceDesc PhysicalDeviceDesc::Query(VulkanInstance const& instance, VkPhysicalDevice vk_physical_device) {
  PhysicalDeviceDesc result;

  result.instance_ = instance.handle();
  result.handle_ = vk_physical_device;

  // VkExtensionProperties
  {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(vk_physical_device, nullptr, &count, nullptr);
    result.extensions_.resize(count);
    vkEnumerateDeviceExtensionProperties(vk_physical_device, nullptr, &count, result.extensions_.data());
  }

  if (instance.CheckExtensionEnabled(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
    {
      VkPhysicalDeviceMultiviewFeaturesKHR multiview_features {};
      VkPhysicalDevice16BitStorageFeaturesKHR sixteen_bit_storage_features {};
      VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing_features {};
      VkPhysicalDeviceBufferDeviceAddressFeaturesKHR buffer_device_address_features {};
      VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features {};
      VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features {};
      VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features {};

      VkPhysicalDeviceFeatures2KHR features2 {};
      features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
      features2.pNext = nullptr;

      void** next = &features2.pNext;

      #define ADD_FEATURE_QUERY_PNEXT_CHAIN(result_var, features_var, stype) \
        result_var.emplace();         \
        \
        auto& features = features_var; \
        features.sType = stype;       \
        features.pNext = nullptr;     \
        \
        *next = &features;            \
        next = &features.pNext;

      if (result.QueryExtensionSupport(VK_KHR_MULTIVIEW_EXTENSION_NAME)) {
        // pNextChain: VkPhysicalDeviceMultiviewFeaturesKHR
        ADD_FEATURE_QUERY_PNEXT_CHAIN(result.multiview_desc_, multiview_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR);
      }

      if (result.QueryExtensionSupport(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME)) {
        result.depth_stencil_resolve_desc_.emplace();
      }

      if (result.QueryExtensionSupport(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME)) {
        if (result.QueryExtensionSupport(VK_KHR_16BIT_STORAGE_EXTENSION_NAME)) {
          // pNextChain: VkPhysicalDevice16BitStorageFeaturesKHR
          ADD_FEATURE_QUERY_PNEXT_CHAIN(result.sixteen_bit_storage_desc_, sixteen_bit_storage_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR);
        }
      }

      if (result.QueryExtensionSupport(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
        // pNext chain: VkPhysicalDeviceDescriptorIndexingFeaturesEXT
        ADD_FEATURE_QUERY_PNEXT_CHAIN(result.descriptor_indexing_desc_, descriptor_indexing_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT);
      }

      if (result.QueryExtensionSupport(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
        // pNext chain: VkPhysicalDeviceBufferDeviceAddressFeaturesKHR
        ADD_FEATURE_QUERY_PNEXT_CHAIN(result.buffer_device_address_desc_, buffer_device_address_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR);
      }

      if (result.QueryExtensionSupport(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)) {
        // pNext chain: VkPhysicalDeviceAccelerationStructureFeaturesKHR
        ADD_FEATURE_QUERY_PNEXT_CHAIN(result.acceleration_structure_desc_, acceleration_structure_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);
      }

      if (result.QueryExtensionSupport(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)) {
        // pNext chain: VkPhysicalDeviceRayTracingPipelineFeaturesKHR
        ADD_FEATURE_QUERY_PNEXT_CHAIN(result.ray_tracing_pipeline_desc_, ray_tracing_pipeline_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR);
      }

      if (result.QueryExtensionSupport(VK_KHR_RAY_QUERY_EXTENSION_NAME)) {
        // pNext chain: VkPhysicalDeviceRayQueryFeaturesKHR
        ADD_FEATURE_QUERY_PNEXT_CHAIN(result.ray_query_desc_, ray_query_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR);
      }

      #undef ADD_FEATURE_QUERY_PNEXT_CHAIN

      vkGetPhysicalDeviceFeatures2KHR(vk_physical_device, &features2);

      #define COPY_AND_FIXUP_FEATURE_QUERY_RESULT(result_var, features_var) \
        if (result_var.has_value()) {                \
          result_var->features       = features_var; \
          result_var->features.pNext = nullptr;      \
        }

      result.features_ = features2.features;
      COPY_AND_FIXUP_FEATURE_QUERY_RESULT(result.multiview_desc_, multiview_features);
      COPY_AND_FIXUP_FEATURE_QUERY_RESULT(result.sixteen_bit_storage_desc_, sixteen_bit_storage_features);
      COPY_AND_FIXUP_FEATURE_QUERY_RESULT(result.descriptor_indexing_desc_, descriptor_indexing_features);
      COPY_AND_FIXUP_FEATURE_QUERY_RESULT(result.buffer_device_address_desc_, buffer_device_address_features);
      COPY_AND_FIXUP_FEATURE_QUERY_RESULT(result.acceleration_structure_desc_, acceleration_structure_features);
      COPY_AND_FIXUP_FEATURE_QUERY_RESULT(result.ray_tracing_pipeline_desc_, ray_tracing_pipeline_features);
      COPY_AND_FIXUP_FEATURE_QUERY_RESULT(result.ray_query_desc_, ray_query_features);

      #undef COPY_AND_FIXUP_FEATURE_QUERY_RESULT
    }
    {
      VkPhysicalDeviceProperties2KHR properties2 {};
      properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
      properties2.pNext = nullptr;

      void** next = &properties2.pNext;

      #define ADD_PROPERTIES_QUERY_PNEXT_CHAIN(result_var, stype) \
        auto& properties = result_var->properties; \
        properties.sType = stype;   \
        properties.pNext = nullptr; \
        \
        *next = &properties; \
        next  = &properties.pNext;

      if (result.QueryExtensionSupport(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
        // pNext chain: VkPhysicalDeviceDriverProperties
        result.driver_properties_desc_.emplace();
        ADD_PROPERTIES_QUERY_PNEXT_CHAIN(result.driver_properties_desc_, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR);
      }
      if (result.QueryExtensionSupport(VK_KHR_MULTIVIEW_EXTENSION_NAME)) {
        // pNext chain: VkPhysicalDeviceMultiviewPropertiesKHR
        ADD_PROPERTIES_QUERY_PNEXT_CHAIN(result.multiview_desc_, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHR);
      }

      if (result.QueryExtensionSupport(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME)) {
        // pNext chain: VkPhysicalDeviceDepthStencilResolvePropertiesKHR
        ADD_PROPERTIES_QUERY_PNEXT_CHAIN(result.depth_stencil_resolve_desc_, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES_KHR);
      }

      if (result.QueryExtensionSupport(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
        // pNext chain: VkPhysicalDeviceDescriptorIndexingPropertiesEXT
        ADD_PROPERTIES_QUERY_PNEXT_CHAIN(result.descriptor_indexing_desc_, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT);
      }

      if (result.QueryExtensionSupport(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)) {
        // pNext chain: VkPhysicalDeviceAccelerationStructurePropertiesKHR
        ADD_PROPERTIES_QUERY_PNEXT_CHAIN(result.acceleration_structure_desc_, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR);
      }

      if (result.QueryExtensionSupport(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)) {
        // pNext chain: VkPhysicalDeviceRayTracingPipelinePropertiesKHR
        ADD_PROPERTIES_QUERY_PNEXT_CHAIN(result.ray_tracing_pipeline_desc_, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR);
      }

      #undef ADD_PROPERTIES_QUERY_PNEXT_CHAIN

      vkGetPhysicalDeviceProperties2KHR(vk_physical_device, &properties2);

      #define FIXUP_PROPERTIES_QUERY_RESULT(result_var) \
        if (result_var.has_value()) { \
          result_var->properties.pNext = nullptr; \
        }

      result.properties_ = properties2.properties;
      FIXUP_PROPERTIES_QUERY_RESULT(result.driver_properties_desc_);
      FIXUP_PROPERTIES_QUERY_RESULT(result.multiview_desc_);
      FIXUP_PROPERTIES_QUERY_RESULT(result.depth_stencil_resolve_desc_);
      FIXUP_PROPERTIES_QUERY_RESULT(result.descriptor_indexing_desc_);
      FIXUP_PROPERTIES_QUERY_RESULT(result.acceleration_structure_desc_);
      FIXUP_PROPERTIES_QUERY_RESULT(result.ray_tracing_pipeline_desc_);

      #undef FIXUP_PROPERTIES_QUERY_RESULT
    }
  }
  else {
    vkGetPhysicalDeviceFeatures(vk_physical_device, &result.features_);
    vkGetPhysicalDeviceProperties(vk_physical_device, &result.properties_);
  }

  vkGetPhysicalDeviceMemoryProperties(vk_physical_device, &result.memory_properties_);

  {
    bool const has_video_queue = result.QueryExtensionSupport(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME) != nullptr;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(vk_physical_device, &count, nullptr);

    std::vector<VkQueueFamilyVideoPropertiesKHR> video_props(count);
    std::vector<VkQueueFamilyProperties2> props2(count);
    for (uint32_t i = 0; i < count; ++i) {
      video_props[i] = {};
      video_props[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
      video_props[i].pNext = nullptr;

      props2[i] = {};
      props2[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
      props2[i].pNext = has_video_queue ? &video_props[i] : nullptr;
    }

    vkGetPhysicalDeviceQueueFamilyProperties2(vk_physical_device, &count, props2.data());

    result.queue_families_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
      result.queue_families_[i].properties = props2[i].queueFamilyProperties;
      result.queue_families_[i].video_properties = video_props[i];
      result.queue_families_[i].video_properties.pNext = nullptr;
    }
  }

  return result;
}

bool PhysicalDeviceDesc::QuerySurfaceSupport(uint32_t queue_family_index, VkSurfaceKHR surface) const {
  VkBool32 result = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(handle_, queue_family_index, surface, &result);
  return result == VK_TRUE;
}

VkExtensionProperties const* PhysicalDeviceDesc::QueryExtensionSupport(std::string_view extension_name) const {
  auto it = std::find_if(
    std::begin(extensions_), std::end(extensions_),
    [&](VkExtensionProperties const& v) {
      return v.extensionName == extension_name;
    }
  );
  if (it != std::end(extensions_)) {
    return &(*it);
  }
  return nullptr;
}

std::optional<PhysicalDeviceDesc> SelectPhysicalDevice(
  VulkanInstance const& instance,
  mbase::ArrayProxy<std::string const> extensions,
  mbase::ArrayProxy<char const* const> non_mandatory_extensions
) {
  std::vector<VkPhysicalDevice> physical_devices;
  {
    uint32_t physical_devices_count = 0;
    vkEnumeratePhysicalDevices(instance.handle(), &physical_devices_count, nullptr);
    physical_devices.resize(physical_devices_count);
    vkEnumeratePhysicalDevices(instance.handle(), &physical_devices_count, physical_devices.data());
  }

  std::vector<PhysicalDeviceDesc> physical_device_descs;
  for (const auto& elem : physical_devices) {
    physical_device_descs.emplace_back(PhysicalDeviceDesc::Query(instance, elem));
  }

  for (auto const& physical_device_desc : physical_device_descs) {
    auto const& properties = physical_device_desc.properties();

    MBASE_LOG_INFO("--------------------------------------------------");
    MBASE_LOG_INFO("Driver Version:{}", properties.driverVersion);
    MBASE_LOG_INFO("Device Name:{}", properties.deviceName);
    MBASE_LOG_INFO("Device Type:{}", string_VkPhysicalDeviceType(properties.deviceType));
    MBASE_LOG_INFO("API Version:{}.{}.{}", (properties.apiVersion >> 22) & 0x3FF, (properties.apiVersion >> 12) & 0x3FF, properties.apiVersion & 0xFF);

    {
      auto const& mem_props = physical_device_desc.memory_properties();

      MBASE_LOG_INFO("{} Memory Heaps:", mem_props.memoryHeapCount);
      for (uint32_t i = 0; i < mem_props.memoryHeapCount; ++i) {
        const auto& elem = mem_props.memoryHeaps[i];
        MBASE_LOG_INFO("  Memory Heap:{}", i);
        MBASE_LOG_INFO("    Size:{} ({}GiB)", elem.size, elem.size / 1024 / 1024 / 1024);
        MBASE_LOG_INFO("    Heap Flags:{}", string_VkMemoryHeapFlags(elem.flags).c_str());
      }

      MBASE_LOG_INFO("{} Memory Types:", mem_props.memoryTypeCount);
      for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        const auto& elem = mem_props.memoryTypes[i];
        MBASE_LOG_INFO("  Memory Type:{}", i);
        MBASE_LOG_INFO("    Memory Index:{}", elem.heapIndex);
        MBASE_LOG_INFO("    Memory Properties:{}", string_VkMemoryPropertyFlags(elem.propertyFlags).c_str());
      }
    }

    {
      auto const& queue_families = physical_device_desc.queue_families();
      MBASE_LOG_INFO("{} queue families:", queue_families.size());
      for (const auto& elem : queue_families) {
        MBASE_LOG_INFO("  Queue family {}:", std::distance(&*std::begin(queue_families), &elem));
        MBASE_LOG_INFO("    {} queues", elem.properties.queueCount);
        MBASE_LOG_INFO("    Supported operations:{}", string_VkQueueFlags(elem.properties.queueFlags).c_str());
        const auto& min_image_transfer_granularity = elem.properties.minImageTransferGranularity;
        MBASE_LOG_INFO("    Min Image Transfer Granularity:(W:{}, H:{}, D:{})", min_image_transfer_granularity.width, min_image_transfer_granularity.height, min_image_transfer_granularity.depth);
      }
    }
  }

  // TODO: perform proper selection.
  PhysicalDeviceDesc const& selected = physical_device_descs.front();

  // Check if the physical device supports all the extensions necessary.
  uint32_t unsupported_extensions = 0;
  for (const auto& desired : extensions) {
    if (std::all_of(
        std::begin(selected.extensions()), std::end(selected.extensions()),
        [&](VkExtensionProperties const& props) {
          return std::string(props.extensionName) != desired;
        })) {
      MBASE_LOG_ERROR("Mandatory device extension {} not supported!", desired);
      ++unsupported_extensions;
    }
  }

  // Error out if any of the necessary extensions is not available.
  if (unsupported_extensions > 0) {
    return std::nullopt;
  }

#if 1
  // Check and log if the physical device supports any of the non-mandatory extensions.
  for (const auto& desired : non_mandatory_extensions) {
    if (std::all_of(
        std::begin(selected.extensions()), std::end(selected.extensions()),
        [&](VkExtensionProperties const& props) {
          return std::string(props.extensionName) != desired;
        })) {
      MBASE_LOG_WARN("Non-mandarory device extension {} not supported!", desired);
    }
  }
#endif

  return selected;
}

} // namespace mnexus_backend::vulkan
