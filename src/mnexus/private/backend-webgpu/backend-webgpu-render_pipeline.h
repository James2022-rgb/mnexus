#pragma once

// project headers --------------------------------------
#include "backend-webgpu/include_dawn.h"
#include "backend-webgpu/backend-webgpu-shader.h"

#include "pipeline/render_pipeline_cache_key.h"

namespace mnexus_backend::webgpu {

/// Creates a `wgpu::RenderPipeline` from a `RenderPipelineCacheKey`.
/// Looks up the program's pipeline layout and shader modules from the resource pools.
wgpu::RenderPipeline CreateWgpuRenderPipelineFromCacheKey(
  wgpu::Device const& wgpu_device,
  pipeline::RenderPipelineCacheKey const& key,
  ProgramResourcePool const& program_pool,
  ShaderModuleResourcePool const& shader_module_pool
);

} // namespace mnexus_backend::webgpu
