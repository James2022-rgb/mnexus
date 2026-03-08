// TU header --------------------------------------------
#include "backend-vulkan/vk-instance.h"

// c++ headers ------------------------------------------
#include <algorithm>

// public project headers -------------------------------
#include "mbase/public/log.h"
#include "mbase/public/trap.h"

#define CONFIG_TRAP_ON_VULKAN_ERROR 1

namespace mnexus_backend::vulkan {

namespace {

VkBool32 VKAPI_CALL VulkanDebugUtilsMessengerCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
  void* pUserData
) {
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    MBASE_LOG_ERROR("[Vulkan DebugUtils] {}: {}", string_VkDebugUtilsMessageTypeFlagsEXT(messageType), pCallbackData->pMessage);
#if CONFIG_TRAP_ON_VULKAN_ERROR
    mbase::Trap();
#endif
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    MBASE_LOG_WARN("[Vulkan DebugUtils] {}: {}", string_VkDebugUtilsMessageTypeFlagsEXT(messageType), pCallbackData->pMessage);
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    MBASE_LOG_INFO("[Vulkan DebugUtils] {}: {}", string_VkDebugUtilsMessageTypeFlagsEXT(messageType), pCallbackData->pMessage);
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    MBASE_LOG_DEBUG("[Vulkan DebugUtils] {}: {}", string_VkDebugUtilsMessageTypeFlagsEXT(messageType), pCallbackData->pMessage);
  }
  return VK_FALSE;
}

}

bool VulkanInstance::InitializeVolk() {
  VkResult const result = volkInitialize();
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("volkInitialize() failed: {}", string_VkResult(result));
    return false;
  }
  return true;
}

void VulkanInstance::ShutdownVolk() {
  volkFinalize();
}

void VulkanInstance::CheckCapabilities() {
  {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);

    available_layers_.resize(count);
    vkEnumerateInstanceLayerProperties(&count, available_layers_.data());
  }
  {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

    available_extensions_.resize(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, available_extensions_.data());
  }
}

VkExtensionProperties const* VulkanInstance::QueryExtensionSupport(std::string_view extension_name) const {
  auto it = std::find_if(
    std::begin(available_extensions_), std::end(available_extensions_),
    [&](VkExtensionProperties const& v) {
      return v.extensionName == extension_name;
    }
  );
  if (it != std::end(available_extensions_)) {
    return &(*it);
  }
  return nullptr;
}

namespace {

template<class TNewContainer, class TContainer>
TNewContainer TransformStringToPointer(TContainer const& container) {
  TNewContainer result;
  std::transform(
    std::begin(container), std::end(container), std::back_inserter(result),
    [](typename TContainer::value_type const& v) -> typename TContainer::value_type::value_type const* {
      return v.c_str();
    }
  );
  return result;
}

}

bool VulkanInstance::Initialize(
  char const* app_name,
  uint32_t api_version,
  mbase::ArrayProxy<std::string const> layers,
  mbase::ArrayProxy<std::string const> extensions,
  mbase::ArrayProxy<VkValidationFeatureEnableEXT const> enabled_validation_features,
  mbase::ArrayProxy<VkValidationFeatureDisableEXT const> disabled_validation_features
) {
  enabled_layers_ = { layers.begin(), layers.end() };
  enabled_extensions_ = { extensions.begin(), extensions.end() };

  VkApplicationInfo app_info {};
  app_info.sType               = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName    = app_name;
  app_info.applicationVersion  = 0;
  app_info.pEngineName         = "mnexus";
  app_info.engineVersion       = 0;
  app_info.apiVersion          = api_version;

  

  VkDebugUtilsMessengerCreateInfoEXT* debug_utils_messenger_create_info_ptr = nullptr;
  VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info {};
  if (this->QueryExtensionSupport(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != nullptr) {
    debug_utils_messenger_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .pNext = nullptr,
      .flags = 0,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = VulkanDebugUtilsMessengerCallback,
      .pUserData = nullptr,
    };

    debug_utils_messenger_create_info_ptr = &debug_utils_messenger_create_info;

    enabled_extensions_.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  // Deduplicate layers and extensions.
  {
    std::sort(enabled_layers_.begin(), enabled_layers_.end());
    enabled_layers_.erase(std::unique(enabled_layers_.begin(), enabled_layers_.end()), enabled_layers_.end());

    std::sort(enabled_extensions_.begin(), enabled_extensions_.end());
    enabled_extensions_.erase(std::unique(enabled_extensions_.begin(), enabled_extensions_.end()), enabled_extensions_.end());
  }

  auto enabled_layer_pointers = TransformStringToPointer<std::vector<char const*>>(enabled_layers_);
  auto enabled_extension_pointers = TransformStringToPointer<std::vector<char const*>>(enabled_extensions_);

  std::optional<VkValidationFeaturesEXT> validation_features;
  if (!enabled_validation_features.empty() || !disabled_validation_features.empty()) {
    validation_features.emplace();
    validation_features->sType                          = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validation_features->pNext                          = nullptr;
    validation_features->enabledValidationFeatureCount  = uint32_t(enabled_validation_features.size());
    validation_features->pEnabledValidationFeatures     = enabled_validation_features.data();
    validation_features->disabledValidationFeatureCount = uint32_t(disabled_validation_features.size());
    validation_features->pDisabledValidationFeatures    = disabled_validation_features.data();
  }

  VkInstanceCreateInfo info {};
  info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  info.pNext                   = debug_utils_messenger_create_info_ptr;
  info.flags                   = 0;
  info.pApplicationInfo        = &app_info;
  info.enabledLayerCount       = uint32_t(enabled_layer_pointers.size());
  info.ppEnabledLayerNames     = enabled_layer_pointers.data();
  info.enabledExtensionCount   = uint32_t(enabled_extension_pointers.size());
  info.ppEnabledExtensionNames = enabled_extension_pointers.data();

  // pNext chain: VkInstanceCreateInfo -> VkValidationFeaturesEXT
  if (validation_features.has_value()) {
    info.pNext = &validation_features.value();
  }

  VkResult result = vkCreateInstance(&info, nullptr, &handle_);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateInstance failed \"{}\".", string_VkResult(result));

    MBASE_LOG_ERROR("Enabled instance layers ():{}", enabled_layers_.size());
    for (auto const& value : enabled_layers_) {
      MBASE_LOG_ERROR("  {}", value);
    }

    return false;
  }

  volkLoadInstance(handle_);

  result = vkCreateDebugUtilsMessengerEXT(handle_, &debug_utils_messenger_create_info, nullptr, &vk_debug_utils_messenger_);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateDebugUtilsMessengerEXT failed: {}", string_VkResult(result));
  }

  return result == VK_SUCCESS;
}

void VulkanInstance::Shutdown() {
  if (vk_debug_utils_messenger_ != VK_NULL_HANDLE) {
    vkDestroyDebugUtilsMessengerEXT(handle_, vk_debug_utils_messenger_, nullptr);
    vk_debug_utils_messenger_ = VK_NULL_HANDLE;
  }

  vkDestroyInstance(handle_, nullptr);
  handle_ = VK_NULL_HANDLE;
}

bool VulkanInstance::CheckExtensionEnabled(std::string_view extension_name) const {
  return std::any_of(
    std::begin(enabled_extensions_), std::end(enabled_extensions_),
    [&](std::string const& v) {
      return v == extension_name;
    }
  );
}

} // namespace mnexus_backend::vulkan
