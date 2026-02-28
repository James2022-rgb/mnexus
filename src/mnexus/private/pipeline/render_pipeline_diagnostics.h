#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

namespace pipeline {

/// Diagnostics counters for the render pipeline cache.
/// Readable via `IDevice::GetRenderPipelineCacheDiagnostics()`.
struct RenderPipelineCacheDiagnostics final {
  uint64_t total_lookups = 0;
  uint64_t cache_hits = 0;
  uint64_t cache_misses = 0;
  uint64_t cached_pipeline_count = 0;

  [[nodiscard]] double HitRate() const {
    return total_lookups > 0
      ? static_cast<double>(cache_hits) / static_cast<double>(total_lookups)
      : 0.0;
  }
};

} // namespace pipeline
