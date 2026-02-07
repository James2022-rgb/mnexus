// TU header --------------------------------------------
#include "backend-webgpu/backend-webgpu-shader.h"

// c++ headers ------------------------------------------
#include <optional>
#include <string>
#include <vector>

// external headers -------------------------------------
#if MNEXUS_INTERNAL_USE_TINT
# include <tint/tint.h>
#endif

// public project headers -------------------------------
#include "mbase/public/platform.h"
#include "mbase/public/assert.h"

namespace mnexus_backend::webgpu {

//
// ShaderModule
//

void InitializeShaderSubsystem() {
#if MNEXUS_INTERNAL_USE_TINT
  tint::Initialize();
#endif
}

void ShutdownShaderSubsystem() {
#if MNEXUS_INTERNAL_USE_TINT
  tint::Shutdown();
#endif
}

namespace {

// SPIR-V to WGSL conversion via Tint (available on both native Dawn and Emscripten with Tint)
#if MNEXUS_INTERNAL_USE_TINT
std::optional<std::string> ConvertSpirvToWgsl(std::vector<uint32_t> const& spirv) {
  tint::wgsl::writer::Options wgsl_options;
  tint::Result<std::string> result = tint::SpirvToWgsl(spirv, wgsl_options);
  if (result != tint::Success) {
    return std::nullopt;
  }
  return std::move(result.Move());
}

std::optional<std::string> ConvertSpirvToWgsl(uint32_t const* spirv, uint32_t spirv_word_count) {
  std::vector<uint32_t> spirv_vector(spirv, spirv + spirv_word_count);
  return ConvertSpirvToWgsl(spirv_vector);
}
#endif // MNEXUS_INTERNAL_USE_TINT

wgpu::ShaderModule CreateWgpuShaderModule(
  wgpu::Device const& wgpu_device,
  mnexus::ShaderModuleDesc const& shader_module_desc
) {
  MBASE_ASSERT_MSG(
    shader_module_desc.source_language == mnexus::ShaderSourceLanguage::kSpirV,
    "Only SPIR-V is supported in CreateWgpuShaderModule2"
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
  auto wgsl = ConvertSpirvToWgsl(spirv, spirv_word_count);
  MBASE_ASSERT_MSG(wgsl.has_value(), "Failed to convert SPIR-V to WGSL");
  wgsl_storage = std::move(*wgsl);

  shader_source_wgsl.code = wgpu::StringView{ wgsl_storage.data(), wgsl_storage.size() };
  wgpu_shader_module_desc.nextInChain = &shader_source_wgsl;

  MBASE_LOG_TRACE("Converted SPIR-V to WGSL:\n{}", wgsl_storage);
# else
  MBASE_ASSERT_MSG(false, "SPIR-V input requires Tint support");
  mbase::Trap();
# endif

#endif // MNEXUS_INTERNAL_USE_DAWN

  return wgpu_device.CreateShaderModule(&wgpu_shader_module_desc);
}

} // namespace

container::ResourceHandle EmplaceShaderModuleResourcePool(
  ShaderModuleResourcePool& out_pool,
  wgpu::Device const& wgpu_device,
  mnexus::ShaderModuleDesc const& shader_module_desc
) {
  // Assert that the shader code is SPIR-V; we rely on being able to reflect the shader code, which is only really possible with SPIR-V.
  MBASE_ASSERT_MSG(
    shader_module_desc.source_language == mnexus::ShaderSourceLanguage::kSpirV,
    "Only SPIR-V is supported in EmplaceShaderModuleResourcePool"
  );

  wgpu::ShaderModule wgpu_shader_module = CreateWgpuShaderModule(
    wgpu_device,
    shader_module_desc
  );

  if (!wgpu_shader_module) {
    return container::ResourceHandle::Null();
  }

  std::optional<shader::ShaderModuleReflection> opt_reflection =
    shader::ShaderModuleReflection::CreateFromSpirv(
      reinterpret_cast<uint32_t const*>(shader_module_desc.code_ptr),
      shader_module_desc.code_size_in_bytes / sizeof(uint32_t)
    );

  if (!opt_reflection.has_value()) {
    MBASE_LOG_ERROR("Failed to reflect SPIR-V shader module!");
    return container::ResourceHandle::Null();
  }

  ShaderModuleHot hot {
    .wgpu_shader_module = std::move(wgpu_shader_module)
  };
  ShaderModuleCold cold {
    .reflection = std::move(*opt_reflection)
  };

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

//
// Program
//

container::ResourceHandle EmplaceProgramResourcePool(
  ProgramResourcePool& out_pool,
  wgpu::Device const& wgpu_device,
  mnexus::ProgramDesc const& program_desc,
  ShaderModuleResourcePool const& shader_module_pool,
  std::function<container::ResourceHandle(mnexus::ShaderModuleHandle)> get_shader_module_pool_handle
) {
  // Phase 1: Merge bind group layouts from all shader modules.
  shader::MergedPipelineLayout merged_pipeline_layout;

  for (uint32_t shader_module_index = 0; shader_module_index < program_desc.shader_modules.size(); ++shader_module_index) {
    container::ResourceHandle shader_module_pool_handle = get_shader_module_pool_handle(
      program_desc.shader_modules[shader_module_index]
    );

    auto [shader_module_cold, lock] = shader_module_pool.GetColdConstRefWithSharedLockGuard(
      shader_module_pool_handle
    );

    if (!merged_pipeline_layout.Merge(shader_module_cold.reflection)) {
      MBASE_LOG_ERROR("Failed to merge bind group layouts for shader module index {}", shader_module_index);
      return container::ResourceHandle::Null();
    }
  }

  mbase::ArrayProxy<shader::BindGroupLayout const> merged_layouts = merged_pipeline_layout.GetBindGroupLayouts();

  // Phase 2: Convert merged layouts to WebGPU bind group layouts.

  std::vector<wgpu::BindGroupLayout> wgpu_bind_group_layouts;
  wgpu_bind_group_layouts.reserve(merged_layouts.size());

  for (uint32_t set_index = 0; set_index < merged_layouts.size(); ++set_index) {
    shader::BindGroupLayout const& merged_bgl = merged_layouts[set_index];

    mbase::SmallVector<wgpu::BindGroupLayoutEntry, 4> wgpu_entries;
    wgpu_entries.reserve(merged_bgl.entries.size());

    for (uint32_t entry_index = 0; entry_index < merged_bgl.entries.size(); ++entry_index) {
      shader::BindGroupLayoutEntry const& entry = merged_bgl.entries[entry_index];

      wgpu::BindGroupLayoutEntry& wgpu_entry = wgpu_entries.emplace_back();

      wgpu_entry.binding = entry.binding;
      wgpu_entry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Compute;

      switch (entry.type) {
        case mnexus::BindGroupLayoutEntryType::kUniformBuffer:
          wgpu_entry.buffer.type = wgpu::BufferBindingType::Uniform;
          break;
        case mnexus::BindGroupLayoutEntryType::kStorageBuffer:
          if (entry.writable) {
            wgpu_entry.buffer.type = wgpu::BufferBindingType::Storage;
            wgpu_entry.visibility = wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Compute;
          } else {
            wgpu_entry.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
          }
          break;
        case mnexus::BindGroupLayoutEntryType::kSampledTexture:
          wgpu_entry.texture.sampleType = wgpu::TextureSampleType::Float;
          wgpu_entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
          wgpu_entry.texture.multisampled = false;
          break;
        case mnexus::BindGroupLayoutEntryType::kSampler:
          wgpu_entry.sampler.type = wgpu::SamplerBindingType::Filtering;
          break;
        case mnexus::BindGroupLayoutEntryType::kStorageTexture:
          wgpu_entry.storageTexture.access = wgpu::StorageTextureAccess::ReadWrite;
          wgpu_entry.storageTexture.format = wgpu::TextureFormat::RGBA8Unorm;
          wgpu_entry.storageTexture.viewDimension = wgpu::TextureViewDimension::e2D;
          break;
        case mnexus::BindGroupLayoutEntryType::kCombinedTextureSampler:
          wgpu_entry.texture.sampleType = wgpu::TextureSampleType::Float;
          wgpu_entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
          wgpu_entry.texture.multisampled = false;
          wgpu_entry.sampler.type = wgpu::SamplerBindingType::Filtering;
          break;
        default:
          MBASE_LOG_ERROR("Unsupported BindGroupLayoutEntryType in EmplaceProgramResourcePool: {}", static_cast<uint32_t>(entry.type));
          mbase::Trap();
          break;
      }
    }

    wgpu::BindGroupLayoutDescriptor wgpu_bgl_desc {};
    wgpu_bgl_desc.entryCount = wgpu_entries.size();
    wgpu_bgl_desc.entries = wgpu_entries.data();
    wgpu_bind_group_layouts.push_back(wgpu_device.CreateBindGroupLayout(&wgpu_bgl_desc));
  }

  // Phase 3: Create PipelineLayout, emplace into pool, and return handle.
  wgpu::PipelineLayoutDescriptor pipeline_layout_desc {};
  pipeline_layout_desc.bindGroupLayoutCount = wgpu_bind_group_layouts.size();
  pipeline_layout_desc.bindGroupLayouts = wgpu_bind_group_layouts.data();
  wgpu::PipelineLayout pipeline_layout = wgpu_device.CreatePipelineLayout(&pipeline_layout_desc);

  ProgramHot hot { .wgpu_pipeline_layout = std::move(pipeline_layout) };
  ProgramCold cold {};

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

} // namespace mnexus_backend::webgpu
