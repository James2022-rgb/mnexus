#pragma once

// c++ headers ------------------------------------------
#include <memory>

// project headers --------------------------------------
#include "backend-iface/backend-iface.h"

namespace mnexus_backend::webgpu {

class IBackendWebGpu : public IBackend {
public:
  static std::unique_ptr<IBackendWebGpu> Create();

  ~IBackendWebGpu() override = default;

protected:
  IBackendWebGpu() = default;
};

} // namespace mnexus_backend::webgpu
