#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"
#include "backend-webgpu/include_dawn.h"

namespace mnexus_backend::webgpu {

struct SamplerHot final {
  wgpu::Sampler wgpu_sampler;
};
struct SamplerCold final {
  mnexus::SamplerDesc desc;
};

using SamplerResourcePool = resource_pool::TResourceGenerationalPool<SamplerHot, SamplerCold, mnexus::kResourceTypeSampler>;

} // namespace mnexus_backend::webgpu
