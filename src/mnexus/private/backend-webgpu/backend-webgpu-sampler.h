#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "container/resource_generational_pool.h"
#include "backend-webgpu/include_dawn.h"

namespace mnexus_backend::webgpu {

struct SamplerHot final {
  wgpu::Sampler wgpu_sampler;
};
struct SamplerCold final {
  mnexus::SamplerDesc desc;
};

using SamplerResourcePool = container::TResourceGenerationalPool<SamplerHot, SamplerCold>;

} // namespace mnexus_backend::webgpu
