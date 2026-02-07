// TU header --------------------------------------------
#include "shader/reflection.h"

// c++ headers ------------------------------------------
#include <algorithm>
#include <cstring>

// external headers -------------------------------------
#include <spirv_reflect.h>

namespace shader {

static_assert(sizeof(ShaderModuleReflection::SpvReflectShaderModuleStorage) >= sizeof(SpvReflectShaderModule), "Storage size is insufficient for SpvReflectShaderModule");

namespace {

static mnexus::BindGroupLayoutEntryType ConvertDescriptorType(SpvReflectDescriptorType type) {
  switch (type) {
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      return mnexus::BindGroupLayoutEntryType::kUniformBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      return mnexus::BindGroupLayoutEntryType::kStorageBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      return mnexus::BindGroupLayoutEntryType::kSampledTexture;
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      return mnexus::BindGroupLayoutEntryType::kCombinedTextureSampler;
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
      return mnexus::BindGroupLayoutEntryType::kSampler;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      return mnexus::BindGroupLayoutEntryType::kStorageTexture;
    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
      return mnexus::BindGroupLayoutEntryType::kAccelerationStructure;
    default:
      return mnexus::BindGroupLayoutEntryType::kUniformBuffer;
  }
}

} // namespace

ShaderModuleReflection::ShaderModuleReflection(SpvReflectShaderModule* copy_src) {
  std::memset(reflect_shader_module_storage_, 0, sizeof(reflect_shader_module_storage_));
  reflect_shader_module_ = reinterpret_cast<SpvReflectShaderModule*>(reflect_shader_module_storage_);
  *reflect_shader_module_ = *copy_src;

  // Populate bind_group_layouts_ from descriptor sets.
  for (uint32_t i = 0; i < reflect_shader_module_->descriptor_set_count; ++i) {
    SpvReflectDescriptorSet const& descriptor_set = reflect_shader_module_->descriptor_sets[i];

    BindGroupLayout layout;
    layout.set = descriptor_set.set;

    for (uint32_t j = 0; j < descriptor_set.binding_count; ++j) {
      SpvReflectDescriptorBinding const& descriptor_binding = *descriptor_set.bindings[j];

      BindGroupLayoutEntry entry;
      entry.binding = descriptor_binding.binding;
      entry.type = ConvertDescriptorType(descriptor_binding.descriptor_type);
      entry.count = descriptor_binding.count;
      entry.writable = !(descriptor_binding.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);

      layout.entries.emplace_back(entry);
    }

    // Sort entries by binding.
    std::sort(layout.entries.begin(), layout.entries.end(),
      [](BindGroupLayoutEntry const& a, BindGroupLayoutEntry const& b) {
        return a.binding < b.binding;
      }
    );

    bind_group_layouts_.emplace_back(std::move(layout));
  }

  // Sort bind_group_layouts_ by set.
  std::sort(bind_group_layouts_.begin(), bind_group_layouts_.end(),
    [](BindGroupLayout const& a, BindGroupLayout const& b) {
      return a.set < b.set;
    }
  );
}

ShaderModuleReflection::ShaderModuleReflection(ShaderModuleReflection&& other) noexcept
    : bind_group_layouts_(std::move(other.bind_group_layouts_)) {
  std::memcpy(reflect_shader_module_storage_, other.reflect_shader_module_storage_, sizeof(reflect_shader_module_storage_));
  reflect_shader_module_ = reinterpret_cast<SpvReflectShaderModule*>(reflect_shader_module_storage_);
  other.reflect_shader_module_ = nullptr;
}

ShaderModuleReflection& ShaderModuleReflection::operator=(ShaderModuleReflection&& other) noexcept {
  if (this != &other) {
    std::memcpy(reflect_shader_module_storage_, other.reflect_shader_module_storage_, sizeof(reflect_shader_module_storage_));
    reflect_shader_module_ = reinterpret_cast<SpvReflectShaderModule*>(reflect_shader_module_storage_);
    other.reflect_shader_module_ = nullptr;
    bind_group_layouts_ = std::move(other.bind_group_layouts_);
  }
  return *this;
}

std::optional<ShaderModuleReflection> ShaderModuleReflection::CreateFromSpirv(
  uint32_t const* MBASE_NOT_NULL spirv_data,
  uint32_t spirv_word_count
) {
  SpvReflectShaderModule module;
  SpvReflectResult const result = spvReflectCreateShaderModule2(
    SPV_REFLECT_MODULE_FLAG_NONE,
    spirv_word_count * sizeof(uint32_t),
    spirv_data,
    &module
  );

  if (result != SPV_REFLECT_RESULT_SUCCESS) {
    return std::nullopt;
  }

  return ShaderModuleReflection(&module);
}

bool MergedPipelineLayout::Merge(ShaderModuleReflection const& reflection) {
  mbase::ArrayProxy<BindGroupLayout const> bind_group_layouts = reflection.GetBindGroupLayouts();

  for (uint32_t bgl_index = 0; bgl_index < bind_group_layouts.size(); ++bgl_index) {
    BindGroupLayout const& src_bgl = bind_group_layouts[bgl_index];

    // Find or insert the set in bind_group_layouts_ (sorted by set).
    auto set_it = std::lower_bound(
      bind_group_layouts_.begin(), bind_group_layouts_.end(), src_bgl.set,
      [](BindGroupLayout const& lhs, uint32_t set) { return lhs.set < set; }
    );

    if (set_it == bind_group_layouts_.end() || set_it->set != src_bgl.set) {
      BindGroupLayout new_bgl;
      new_bgl.set = src_bgl.set;
      set_it = bind_group_layouts_.insert(set_it, &new_bgl, &new_bgl + 1);
    }

    // Merge entries into this set (entries are sorted by binding).
    for (uint32_t entry_index = 0; entry_index < src_bgl.entries.size(); ++entry_index) {
      BindGroupLayoutEntry const& src_entry = src_bgl.entries[entry_index];

      auto entry_it = std::lower_bound(
        set_it->entries.begin(), set_it->entries.end(), src_entry.binding,
        [](BindGroupLayoutEntry const& lhs, uint32_t binding) { return lhs.binding < binding; }
      );

      if (entry_it != set_it->entries.end() && entry_it->binding == src_entry.binding) {
        // Binding already exists; check for conflict.
        if (entry_it->type != src_entry.type || entry_it->count != src_entry.count) {
          return false;
        }
        // Same type and count — merge writable flag (writable if any stage writes).
        entry_it->writable = entry_it->writable || src_entry.writable;
      } else {
        // Insert in sorted position.
        set_it->entries.insert(entry_it, &src_entry, &src_entry + 1);
      }
    }
  }

  return true;
}

} // namespace shader
