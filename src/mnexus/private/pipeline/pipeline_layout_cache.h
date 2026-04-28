#pragma once

// project headers --------------------------------------
#include "cache/hash_cache.h"
#include "pipeline/pipeline_layout_cache_key.h"

namespace pipeline {

/// Thread-safe pipeline layout cache keyed by `PipelineLayoutCacheKey`.
/// Backends instantiate with their layout type (e.g. `wgpu::PipelineLayout`).
template<typename TLayout>
using TPipelineLayoutCache = THashCache<PipelineLayoutCacheKey, TLayout, PipelineLayoutCacheKey::Hasher>;

} // namespace pipeline
