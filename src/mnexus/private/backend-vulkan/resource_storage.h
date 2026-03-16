#pragma once

// project headers --------------------------------------
#include "backend-vulkan/backend-vulkan-shader.h"

namespace mnexus_backend::vulkan {

struct ResourceStorage final {
  ShaderModuleResourcePool shader_modules;
  ProgramResourcePool programs;

  pipeline::TPipelineLayoutCache<VulkanPipelineLayoutPtr> pipeline_layout_cache;
};

} // namespace mnexus_backend::vulkan
