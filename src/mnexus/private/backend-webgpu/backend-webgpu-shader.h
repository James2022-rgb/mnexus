#pragma once

// c++ headers ------------------------------------------
#include <functional>

// public project headers -------------------------------
#include "mbase/public/container.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "container/resource_generational_pool.h"

#include "backend-webgpu/include_dawn.h"

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

using ShaderModuleResourcePool = container::TResourceGenerationalPool<ShaderModuleHot, ShaderModuleCold>;

container::ResourceHandle EmplaceShaderModuleResourcePool(
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

using ProgramResourcePool = container::TResourceGenerationalPool<ProgramHot, ProgramCold>;

container::ResourceHandle EmplaceProgramResourcePool(
  ProgramResourcePool& out_pool,
  wgpu::Device const& wgpu_device,
  mnexus::ProgramDesc const& program_desc,
  ShaderModuleResourcePool const& shader_module_pool,
  std::function<container::ResourceHandle(mnexus::ShaderModuleHandle)> get_shader_module_pool_handle
);

} // namespace mnexus_backend::webgpu
