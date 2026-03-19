// TU header --------------------------------------------
#include "backend-webgpu/backend-webgpu-compute_pipeline.h"

// public project headers -------------------------------

namespace mnexus_backend::webgpu {

wgpu::ComputePipeline CreateWgpuComputePipeline(
  wgpu::Device const& wgpu_device,
  wgpu::PipelineLayout const& wgpu_pipeline_layout,
  wgpu::ShaderModule const& wgpu_shader_module
) {
  wgpu::ComputePipelineDescriptor wgpu_compute_pipeline_desc {};

  wgpu_compute_pipeline_desc.layout = wgpu_pipeline_layout;

  wgpu_compute_pipeline_desc.compute = wgpu::ComputeState {
    .nextInChain = nullptr,
    .module = wgpu_shader_module,
    .entryPoint = "main",
    .constantCount = 0,
    .constants = nullptr
  };

  return wgpu_device.CreateComputePipeline(&wgpu_compute_pipeline_desc);
}

} // namespace mnexus_backend::webgpu
