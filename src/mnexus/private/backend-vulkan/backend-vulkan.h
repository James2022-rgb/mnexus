#pragma once

// c++ headers ------------------------------------------
#include <memory>

// project headers --------------------------------------
#include "backend-iface/backend-iface.h"

namespace mnexus_backend::vulkan {

class IBackendVulkan : public IBackend {
public:
  static std::unique_ptr<IBackendVulkan> Create();

  ~IBackendVulkan() override = default;

protected:
  IBackendVulkan() = default;
};

} // namespace mnexus_backend::vulkan
