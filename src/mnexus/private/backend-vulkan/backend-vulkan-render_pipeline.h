#pragma once

// project headers --------------------------------------
#include "pipeline/render_pipeline_cache_key.h"

#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/object/vk-object-render_pipeline.h"

#include "backend-vulkan/device/fwd.h"

namespace mnexus_backend::vulkan {

/// Creates a `VulkanRenderPipeline` from a `RenderPipelineCacheKey`.
/// Resolves the program's shader modules and pipeline layout via the
/// supplied resource pools, then calls vkCreateGraphicsPipelines.
/// The pipeline uses VK_DYNAMIC_STATE_VIEWPORT and VK_DYNAMIC_STATE_SCISSOR;
/// all other state is baked into the pipeline.
VulkanRenderPipelinePtr CreateVulkanRenderPipelineFromCacheKey(
  IVulkanDevice& vk_device,
  pipeline::RenderPipelineCacheKey const& key,
  ProgramResourcePool const& program_pool,
  ShaderModuleResourcePool const& shader_module_pool
);

} // namespace mnexus_backend::vulkan
