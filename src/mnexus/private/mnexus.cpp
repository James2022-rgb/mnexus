// TU header --------------------------------------------
#include "mnexus/public/mnexus.h"

// platform detection header ----------------------------
#include "mbase/public/platform.h"

// c++ headers ------------------------------------------

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/log.h"
#include "mbase/public/access.h"

// project headers --------------------------------------
#if MNEXUS_ENABLE_BACKEND_WGPU
# include "backend-webgpu/backend-webgpu.h"
#endif

namespace mnexus {

class Nexus final : public INexus {
public:
  explicit Nexus(std::unique_ptr<mnexus_backend::IBackend> backend, bool headless) :
    backend_(std::move(backend)),
    headless_(headless)
  {
  }
  ~Nexus() override = default;

  MNEXUS_NO_THROW void MNEXUS_CALL Destroy() override {
    delete this;
  }

  // ----------------------------------------------------------------------------------------------
  // Surface lifecycle.

  MNEXUS_NO_THROW void MNEXUS_CALL OnDisplayChanged() override {
    if (headless_) return;
    backend_->OnDisplayChanged();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL OnSurfaceDestroyed() override {
    MBASE_ASSERT_MSG(!headless_, "OnSurfaceDestroyed() must not be called on a headless INexus instance");
    backend_->OnSurfaceDestroyed();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL OnSurfaceRecreated(SurfaceSourceDesc const& surface_source_desc) override {
    MBASE_ASSERT_MSG(!headless_, "OnSurfaceRecreated() must not be called on a headless INexus instance");
    backend_->OnSurfaceRecreated(surface_source_desc);
  }

  // ----------------------------------------------------------------------------------------------
  // Presentation.

  MNEXUS_NO_THROW void MNEXUS_CALL OnPresentPrologue() override {
    MBASE_ASSERT_MSG(!headless_, "OnPresentPrologue() must not be called on a headless INexus instance");
    backend_->OnPresentPrologue();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL OnPresentEpilogue() override {
    MBASE_ASSERT_MSG(!headless_, "OnPresentEpilogue() must not be called on a headless INexus instance");
    backend_->OnPresentEpilogue();
  }

  // ----------------------------------------------------------------------------------------------
  // Device.

  MNEXUS_NO_THROW IDevice* MNEXUS_CALL GetDevice() override {
    return backend_->GetDevice();
  }

private:
  std::unique_ptr<mnexus_backend::IBackend> backend_;
  bool headless_ = false;
};

INexus* INexus::Create(NexusDesc const& desc) {
  // TODO: Select backend here.

#if MNEXUS_ENABLE_BACKEND_WGPU
  auto backend = mnexus_backend::webgpu::IBackendWebGpu::Create();
#else
# error No backend is enabled in mnexus!
#endif

  if (!backend) return nullptr;
  return new Nexus(std::move(backend), desc.headless);
}

} // namespace mnexus
