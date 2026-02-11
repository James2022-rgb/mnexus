// TU header --------------------------------------------
#include "backend-webgpu/builtin_shader.h"

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-webgpu/shader_module.h"
#include "builtin_shader/full_screen_quad_spv.h"
#include "builtin_shader/blit_2d_color_spv.h"

namespace mnexus_backend::webgpu::builtin_shader {

namespace {

wgpu::ShaderModule s_full_screen_quad_vs;
wgpu::ShaderModule s_blit_2d_color_fs;

wgpu::ShaderModule CreateFromSpirv(
  wgpu::Device const& wgpu_device,
  uint8_t const* spirv_data,
  uint32_t spirv_size_in_bytes
) {
  mnexus::ShaderModuleDesc desc {};
  desc.source_language = mnexus::ShaderSourceLanguage::kSpirV;
  desc.code_ptr = reinterpret_cast<uint64_t>(spirv_data);
  desc.code_size_in_bytes = spirv_size_in_bytes;
  return CreateWgpuShaderModule(wgpu_device, desc);
}

} // namespace

void Initialize(wgpu::Device const& wgpu_device) {
  s_full_screen_quad_vs = CreateFromSpirv(
    wgpu_device,
    ::builtin_shader::kFullScreenQuadSpv,
    ::builtin_shader::kFullScreenQuadSpvSize
  );
  MBASE_ASSERT_MSG(bool(s_full_screen_quad_vs), "Failed to create builtin shader: full_screen_quad");

  s_blit_2d_color_fs = CreateFromSpirv(
    wgpu_device,
    ::builtin_shader::kBlit2dColorSpv,
    ::builtin_shader::kBlit2dColorSpvSize
  );
  MBASE_ASSERT_MSG(bool(s_blit_2d_color_fs), "Failed to create builtin shader: blit_2d_color");
}

void Shutdown() {
  s_blit_2d_color_fs = nullptr;
  s_full_screen_quad_vs = nullptr;
}

wgpu::ShaderModule GetFullScreenQuadVs() {
  MBASE_ASSERT_MSG(bool(s_full_screen_quad_vs), "Builtin shaders not initialized");
  return s_full_screen_quad_vs;
}

wgpu::ShaderModule GetBlit2dColorFs() {
  MBASE_ASSERT_MSG(bool(s_blit_2d_color_fs), "Builtin shaders not initialized");
  return s_blit_2d_color_fs;
}

} // namespace mnexus_backend::webgpu::builtin_shader
