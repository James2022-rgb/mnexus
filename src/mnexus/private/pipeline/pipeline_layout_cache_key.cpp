// TU header --------------------------------------------
#include "pipeline/pipeline_layout_cache_key.h"

// public project headers -------------------------------
#include "shader/reflection.h"

namespace pipeline {

size_t PipelineLayoutCacheKey::ComputeHash() const {
  mbase::Hasher hasher;

  hasher.Do(static_cast<uint32_t>(groups.size()));
  for (auto const& group : groups) {
    hasher.Do(group.set);
    hasher.Do(static_cast<uint32_t>(group.entries.size()));
    for (auto const& entry : group.entries) {
      hasher.Do(entry.binding);
      hasher.Do(static_cast<uint32_t>(entry.type));
      hasher.Do(entry.count);
      hasher.Do(static_cast<uint8_t>(entry.writable));
    }
  }

  return static_cast<size_t>(hasher.Finish());
}

bool PipelineLayoutCacheKey::operator==(PipelineLayoutCacheKey const& other) const {
  if (groups.size() != other.groups.size()) return false;

  for (size_t g = 0; g < groups.size(); ++g) {
    auto const& a = groups[g];
    auto const& b = other.groups[g];

    if (a.set != b.set) return false;
    if (a.entries.size() != b.entries.size()) return false;

    for (size_t e = 0; e < a.entries.size(); ++e) {
      auto const& ea = a.entries[e];
      auto const& eb = b.entries[e];
      if (ea.binding != eb.binding) return false;
      if (ea.type != eb.type) return false;
      if (ea.count != eb.count) return false;
      if (ea.writable != eb.writable) return false;
    }
  }

  return true;
}

PipelineLayoutCacheKey BuildPipelineLayoutCacheKey(
  mbase::ArrayProxy<shader::BindGroupLayout const> bind_group_layouts
) {
  PipelineLayoutCacheKey key;
  key.groups.reserve(bind_group_layouts.size());

  for (uint32_t i = 0; i < bind_group_layouts.size(); ++i) {
    shader::BindGroupLayout const& src = bind_group_layouts[i];

    PipelineLayoutCacheKey::Group group;
    group.set = src.set;
    group.entries.reserve(src.entries.size());

    for (uint32_t j = 0; j < src.entries.size(); ++j) {
      shader::BindGroupLayoutEntry const& src_entry = src.entries[j];
      group.entries.emplace_back(PipelineLayoutCacheKey::Entry {
        .binding = src_entry.binding,
        .type = src_entry.type,
        .count = src_entry.count,
        .writable = src_entry.writable,
      });
    }

    key.groups.emplace_back(std::move(group));
  }

  return key;
}

} // namespace pipeline
