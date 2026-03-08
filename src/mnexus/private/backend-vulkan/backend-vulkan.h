#pragma once

// c++ headers ------------------------------------------
#include <memory>

// project headers --------------------------------------
#include "backend-iface/backend-iface.h"

namespace mnexus_backend::vulkan {

struct BackendVulkanCreateDesc {
  bool headless = false;
  char const* app_name = "app";
};

class IBackendVulkan : public IBackend {
public:
  static std::unique_ptr<IBackendVulkan> Create(BackendVulkanCreateDesc const& desc);

  ~IBackendVulkan() override = default;

protected:
  IBackendVulkan() = default;
};

} // namespace mnexus_backend::vulkan
