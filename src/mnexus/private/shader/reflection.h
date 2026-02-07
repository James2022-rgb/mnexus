#pragma once

// c++ headers ------------------------------------------
#include <cstddef>
#include <cstdint>

#include <vector>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/type_util.h"
#include "mbase/public/container.h"
#include "mbase/public/array_proxy.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------

// forward declarations ---------------------------------
struct SpvReflectShaderModule;

namespace shader {

struct BindGroupLayoutEntry final {
  uint32_t binding = 0;
  mnexus::BindGroupLayoutEntryType type = mnexus::BindGroupLayoutEntryType::kUniformBuffer;
  uint32_t count = 1;
  /// Whether the resource is writable (applicable to storage buffers/textures).
  /// Determined by the absence of the NonWritable SPIR-V decoration.
  bool writable = false;
};

struct BindGroupLayout final {
  uint32_t set = 0;
  /// `BindGroupLayoutEntry`s, sorted by `binding`.
  mbase::SmallVector<BindGroupLayoutEntry, 2> entries;
};

class ShaderModuleReflection final {
public:
  using SpvReflectShaderModuleStorage = std::byte[1216];

  ~ShaderModuleReflection() = default;

  MBASE_DISALLOW_COPY(ShaderModuleReflection);
  ShaderModuleReflection(ShaderModuleReflection&& other) noexcept;
  ShaderModuleReflection& operator=(ShaderModuleReflection&& other) noexcept;

  static std::optional<ShaderModuleReflection> CreateFromSpirv(
    uint32_t const* MBASE_NOT_NULL spirv_data,
    uint32_t spirv_word_count
  );

  mbase::ArrayProxy<BindGroupLayout const> GetBindGroupLayouts() const {
    return bind_group_layouts_;
  }

private:
  explicit ShaderModuleReflection(SpvReflectShaderModule* MBASE_NOT_NULL copy_src);

  SpvReflectShaderModuleStorage reflect_shader_module_storage_;
  SpvReflectShaderModule* MBASE_NOT_NULL reflect_shader_module_ = nullptr;

  mbase::SmallVector<BindGroupLayout, 2> bind_group_layouts_;
};

/// Incrementally merges bind group layouts from multiple shader modules.
///
/// Usage:
///   MergedPipelineLayout merged;
///   merged.Merge(vertex_reflection);   // returns true on success
///   merged.Merge(fragment_reflection); // returns true on success
///   auto const& layouts = merged.GetBindGroupLayouts();
class MergedPipelineLayout final {
public:
  /// Merge the bind group layouts from a shader module reflection.
  /// Returns `false` if a conflicting binding is detected.
  bool Merge(ShaderModuleReflection const& reflection);

  mbase::ArrayProxy<BindGroupLayout const> GetBindGroupLayouts() const {
    return bind_group_layouts_;
  }

private:
  /// `BindGroupLayout`s, sorted by `set`.
  mbase::SmallVector<BindGroupLayout, 4> bind_group_layouts_;
};

} // namespace shader
