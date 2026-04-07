#pragma once

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"
#include "backend-webgpu/include_dawn.h"
#include "backend-webgpu/backend-webgpu-shader.h"

#include "pipeline/render_pipeline_cache_key.h"

namespace mnexus_backend::webgpu {

struct RenderPipelineHot final {
  wgpu::RenderPipeline wgpu_render_pipeline;
};
struct RenderPipelineCold final {
};

using RenderPipelineResourcePool = resource_pool::TResourceGenerationalPool<RenderPipelineHot, RenderPipelineCold, mnexus::kResourceTypeRenderPipeline>;

/// Creates a `wgpu::RenderPipeline` from a `RenderPipelineCacheKey`.
/// Looks up the program's pipeline layout and shader modules from the resource pools.
wgpu::RenderPipeline CreateWgpuRenderPipelineFromCacheKey(
  wgpu::Device const& wgpu_device,
  pipeline::RenderPipelineCacheKey const& key,
  ProgramResourcePool const& program_pool,
  ShaderModuleResourcePool const& shader_module_pool
);

} // namespace mnexus_backend::webgpu
