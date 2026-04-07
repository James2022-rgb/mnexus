#pragma once

// c++ headers ------------------------------------------
#include <functional>

// public project headers -------------------------------
#include "mbase/public/container.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-webgpu/include_dawn.h"

#include "pipeline/pipeline_layout_cache.h"
#include "shader/reflection.h"

namespace mnexus_backend::webgpu {

//
// ShaderModule
//

void InitializeShaderSubsystem();
void ShutdownShaderSubsystem();

struct ShaderModuleHot final {
  wgpu::ShaderModule wgpu_shader_module;
};
struct ShaderModuleCold final {
  shader::ShaderModuleReflection reflection;
};

using ShaderModuleResourcePool = resource_pool::TResourceGenerationalPool<ShaderModuleHot, ShaderModuleCold, mnexus::kResourceTypeShaderModule>;

resource_pool::ResourceHandle EmplaceShaderModuleResourcePool(
  ShaderModuleResourcePool& out_pool,
  wgpu::Device const& wgpu_device,
  mnexus::ShaderModuleDesc const& shader_module_desc
);

//
// Program
//

struct ProgramHot final {
  wgpu::PipelineLayout wgpu_pipeline_layout;
};
struct ProgramCold final {
  mbase::SmallVector<mnexus::ShaderModuleHandle, 2> shader_module_handles;
};

using ProgramResourcePool = resource_pool::TResourceGenerationalPool<ProgramHot, ProgramCold, mnexus::kResourceTypeProgram>;

resource_pool::ResourceHandle EmplaceProgramResourcePool(
  ProgramResourcePool& out_pool,
  wgpu::Device const& wgpu_device,
  mnexus::ProgramDesc const& program_desc,
  ShaderModuleResourcePool const& shader_module_pool,
  std::function<resource_pool::ResourceHandle(mnexus::ShaderModuleHandle)> get_shader_module_pool_handle,
  pipeline::TPipelineLayoutCache<wgpu::PipelineLayout>& pipeline_layout_cache
);

} // namespace mnexus_backend::webgpu
