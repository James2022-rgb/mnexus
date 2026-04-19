#pragma once

// c++ headers ------------------------------------------
#include <vector>
#include <string>
#include <string_view>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/accessor.h"
#include "mbase/public/array_proxy.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

namespace mnexus_backend::vulkan {

class VulkanInstance final {
public:
  VulkanInstance() = default;
  ~VulkanInstance() = default;
  MBASE_DISALLOW_COPY_DEFAULT_MOVE(VulkanInstance);

  static bool InitializeVolk();
  static void ShutdownVolk();

  /// *MUST* be called after `volkInitialize()` and before `QueryExtensionSupport`.
  void CheckCapabilities();
  [[nodiscard]] VkExtensionProperties const* QueryExtensionSupport(std::string_view extension_name) const;

  /// `VK_EXT_debug_utils` is automatically enabled if available.
  bool Initialize(
    char const* app_name,
    uint32_t api_version,
    mbase::ArrayProxy<std::string const> layers,
    mbase::ArrayProxy<std::string const> extensions,
    mbase::ArrayProxy<VkValidationFeatureEnableEXT const> enabled_validation_features,
    mbase::ArrayProxy<VkValidationFeatureDisableEXT const> disabled_validation_features
  );
  void Shutdown();

  [[nodiscard]] bool CheckExtensionEnabled(std::string_view extension_name) const;

  MBASE_ACCESSOR_GETV(VkInstance, handle);

private:
  VkInstance handle_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT vk_debug_utils_messenger_ = VK_NULL_HANDLE;

  std::vector<VkLayerProperties> available_layers_;
  std::vector<VkExtensionProperties> available_extensions_;

  std::vector<std::string> enabled_layers_;
  std::vector<std::string> enabled_extensions_;
};

} // namespace mnexus_backend::vulkan
