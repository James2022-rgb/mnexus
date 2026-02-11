#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-webgpu/include_dawn.h"

namespace mnexus_backend::webgpu {

/// Create a wgpu::ShaderModule from a ShaderModuleDesc.
/// On native Dawn, SPIR-V is passed directly.
/// On Emscripten (with Tint), SPIR-V is converted to WGSL first.
wgpu::ShaderModule CreateWgpuShaderModule(
  wgpu::Device const& wgpu_device,
  mnexus::ShaderModuleDesc const& shader_module_desc
);

} // namespace mnexus_backend::webgpu
