#pragma once

// project headers --------------------------------------
#include "backend-webgpu/include_dawn.h"

namespace mnexus_backend::webgpu::builtin_shader {

void Initialize(wgpu::Device const& wgpu_device);
void Shutdown();

wgpu::ShaderModule GetFullScreenQuadVs();
wgpu::ShaderModule GetBlit2dColorFs();
wgpu::ShaderModule GetBufferRepackRowsCs();

} // namespace mnexus_backend::webgpu::builtin_shader
