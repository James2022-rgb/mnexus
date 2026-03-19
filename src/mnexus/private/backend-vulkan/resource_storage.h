#pragma once

// project headers --------------------------------------
#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/backend-vulkan-compute_pipeline.h"

namespace mnexus_backend::vulkan {

struct ResourceStorage final {
  ShaderModuleResourcePool shader_modules;
  ProgramResourcePool programs;
  ComputePipelineResourcePool compute_pipelines;

  pipeline::TPipelineLayoutCache<VulkanPipelineLayoutPtr> pipeline_layout_cache;
};

} // namespace mnexus_backend::vulkan
