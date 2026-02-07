#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "container/resource_generational_pool.h"
#include "backend-webgpu/include_dawn.h"

namespace mnexus_backend::webgpu {

struct ComputePipelineHot final {
  wgpu::ComputePipeline wgpu_compute_pipeline;
};
struct ComputePipelineCold final {
  
};

using ComputePipelineResourcePool = container::TResourceGenerationalPool<ComputePipelineHot, ComputePipelineCold>;

wgpu::ComputePipeline CreateWgpuComputePipeline(
  wgpu::Device const& wgpu_device,
  wgpu::ShaderModule const& wgpu_shader_module
);

} // namespace mnexus_backend::webgpu
