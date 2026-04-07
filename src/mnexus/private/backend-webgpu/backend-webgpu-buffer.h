#pragma once

// c++ headers ------------------------------------------
#include <mutex>

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"
#include "backend-webgpu/include_dawn.h"

namespace mnexus_backend::webgpu {

struct BufferHot final {
  wgpu::Buffer wgpu_buffer;
};
struct BufferCold final {
  mnexus::BufferDesc desc;
};

using BufferResourcePool = resource_pool::TResourceGenerationalPool<BufferHot, BufferCold, mnexus::kResourceTypeBuffer>;

/// ## Remarks
/// WebGPU requires that buffers with map read or map write access have a size
/// that is a multiple of 4 bytes. If this constraint is violated, the function
/// logs an error and returns nullptr.
wgpu::Buffer CreateWgpuBuffer(
  wgpu::Device const& wgpu_device,
  mnexus::BufferDesc const& buffer_desc
);

} // namespace mnexus_backend::webgpu
