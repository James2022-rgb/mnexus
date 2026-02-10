#pragma once

// project headers --------------------------------------
#include "binding/state_tracker.h"

#include "backend-webgpu/include_dawn.h"
#include "backend-webgpu/backend-webgpu-buffer.h"

namespace mnexus_backend::webgpu {

/// Resolves dirty bind groups from the state tracker and sets them on the compute pass.
/// For each dirty group, creates a `wgpu::BindGroup` from the bound entries and calls `SetBindGroup`.
void ResolveAndSetBindGroups(
  wgpu::Device const& wgpu_device,
  wgpu::ComputePassEncoder& compute_pass,
  wgpu::ComputePipeline const& compute_pipeline,
  binding::BindGroupStateTracker& state_tracker,
  BufferResourcePool const& buffer_pool
);

} // namespace mnexus_backend::webgpu
