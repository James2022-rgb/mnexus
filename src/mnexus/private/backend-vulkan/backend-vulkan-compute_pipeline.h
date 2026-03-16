#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "container/resource_generational_pool.h"

namespace mnexus_backend::vulkan {

struct ComputePipelineHot final {
};

struct ComputePipelineCold final {
};

using ComputePipelineResourcePool = container::TResourceGenerationalPool<ComputePipelineHot, ComputePipelineCold>;

} // namespace mnexus_backend::vulkan
