// TU header --------------------------------------------
#include "backend-webgpu/shader_module.h"

// c++ headers ------------------------------------------
#include <string>

// project headers --------------------------------------
#include "shader/wgsl.h"

// public project headers -------------------------------
#include "mbase/public/platform.h"
#include "mbase/public/assert.h"
#include "mbase/public/log.h"

namespace mnexus_backend::webgpu {

wgpu::ShaderModule CreateWgpuShaderModule(
  wgpu::Device const& wgpu_device,
  mnexus::ShaderModuleDesc const& shader_module_desc
) {
  MBASE_ASSERT_MSG(
    shader_module_desc.source_language == mnexus::ShaderSourceLanguage::kSpirV,
    "Only SPIR-V is supported in CreateWgpuShaderModule"
  );

  wgpu::ShaderModuleDescriptor wgpu_shader_module_desc {};

#if MNEXUS_INTERNAL_USE_DAWN
  // Native with Dawn: Can pass SPIR-V directly to Dawn
  wgpu::ShaderSourceSPIRV shader_source_spirv {};

  if (shader_module_desc.source_language == mnexus::ShaderSourceLanguage::kSpirV) {
    shader_source_spirv.code = reinterpret_cast<uint32_t const*>(shader_module_desc.code_ptr);
    shader_source_spirv.codeSize = shader_module_desc.code_size_in_bytes / sizeof(uint32_t);

    wgpu_shader_module_desc.nextInChain = &shader_source_spirv;
  }
  else {
    MBASE_LOG_ERROR("Unsupported shader source language: {}", static_cast<uint32_t>(shader_module_desc.source_language));
    mbase::Trap();
  }
#else // MNEXUS_INTERNAL_USE_DAWN
  // Emscripten: WGSL only (SPIR-V must be converted to WGSL via Tint)
  wgpu::ShaderSourceWGSL shader_source_wgsl {};
  std::string wgsl_storage; // Storage for converted WGSL (must outlive the descriptor)

# if MNEXUS_INTERNAL_USE_TINT
  // Convert SPIR-V to WGSL using Tint
  auto const* spirv = reinterpret_cast<uint32_t const*>(shader_module_desc.code_ptr);
  auto spirv_word_count = shader_module_desc.code_size_in_bytes / sizeof(uint32_t);
  auto wgsl = shader::ConvertSpirvToWgsl(spirv, spirv_word_count);
  MBASE_ASSERT_MSG(wgsl.has_value(), "Failed to convert SPIR-V to WGSL");
  wgsl_storage = std::move(*wgsl);

  shader_source_wgsl.code = wgpu::StringView{ wgsl_storage.data(), wgsl_storage.size() };
  wgpu_shader_module_desc.nextInChain = &shader_source_wgsl;

  // NOTE: Logging the full WGSL text here can crash on Emscripten due to
  // heap corruption from Slang runtime. Log only the size.
  MBASE_LOG_TRACE("Converted SPIR-V to WGSL ({} bytes)", wgsl_storage.size());
# else
  MBASE_ASSERT_MSG(false, "SPIR-V input requires Tint support");
  mbase::Trap();
# endif

#endif // MNEXUS_INTERNAL_USE_DAWN

  return wgpu_device.CreateShaderModule(&wgpu_shader_module_desc);
}

} // namespace mnexus_backend::webgpu
