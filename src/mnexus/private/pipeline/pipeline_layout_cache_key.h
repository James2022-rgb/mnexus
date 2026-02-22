#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

// public project headers -------------------------------
#include "mbase/public/array_proxy.h"
#include "mbase/public/container.h"
#include "mbase/public/hash.h"

#include "mnexus/public/types.h"

// forward declarations ---------------------------------
namespace shader { struct BindGroupLayout; }

namespace pipeline {

/// Hashable, equality-comparable key identifying a unique pipeline layout configuration.
/// Used as a key in `TPipelineLayoutCache` to avoid redundant layout creation.
struct PipelineLayoutCacheKey final {
  struct Entry final {
    uint32_t binding = 0;
    mnexus::BindGroupLayoutEntryType type = mnexus::BindGroupLayoutEntryType::kUniformBuffer;
    uint32_t count = 1;
    bool writable = false;
  };

  struct Group final {
    uint32_t set = 0;
    mbase::SmallVector<Entry, 2> entries;  // sorted by binding
  };

  mbase::SmallVector<Group, 4> groups;  // sorted by set

  [[nodiscard]] size_t ComputeHash() const;
  [[nodiscard]] bool operator==(PipelineLayoutCacheKey const& other) const;

  struct Hasher final {
    size_t operator()(PipelineLayoutCacheKey const& key) const {
      return key.ComputeHash();
    }
  };
};

/// Build a `PipelineLayoutCacheKey` from merged bind group layouts (shader reflection output).
PipelineLayoutCacheKey BuildPipelineLayoutCacheKey(
  mbase::ArrayProxy<shader::BindGroupLayout const> bind_group_layouts
);

} // namespace pipeline
