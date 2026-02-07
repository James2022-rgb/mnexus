#pragma once

// c++ headers ------------------------------------------

// public project headers -------------------------------
#include "mnexus/public/mnexus.h"

namespace mnexus_backend {

class IBackend {
public:
  virtual ~IBackend() = default;

  // ----------------------------------------------------------------------------------------------
  // Surface lifecycle.

  virtual void OnDisplayChanged() = 0;

  virtual void OnSurfaceDestroyed() = 0;
  virtual void OnSurfaceRecreated(mnexus::SurfaceSourceDesc const& surface_source_desc) = 0;

  // ----------------------------------------------------------------------------------------------
  // Presentation.

  virtual void OnPresentPrologue() = 0;
  virtual void OnPresentEpilogue() = 0;

  // ----------------------------------------------------------------------------------------------
  // Device.

  virtual mnexus::IDevice* GetDevice() = 0;

protected:
  IBackend() = default;
};

} // namespace mnexus_backend
