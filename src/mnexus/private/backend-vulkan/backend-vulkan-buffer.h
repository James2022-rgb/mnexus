#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "container/resource_generational_pool.h"

namespace mnexus_backend::vulkan {

struct BufferHot final {
};

struct BufferCold final {
  mnexus::BufferDesc desc;
};

using BufferResourcePool = container::TResourceGenerationalPool<BufferHot, BufferCold>;

} // namespace mnexus_backend::vulkan
