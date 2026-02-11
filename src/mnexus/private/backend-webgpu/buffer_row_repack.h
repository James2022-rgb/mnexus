#pragma once

// project headers --------------------------------------
#include "backend-webgpu/include_dawn.h"

#include <cstdint>

namespace mnexus_backend::webgpu::buffer_row_repack {

void Initialize(wgpu::Device const& wgpu_device);
void Shutdown();

/// Repacks buffer rows from tight packing to 256-byte-aligned row pitch
/// using an internal compute pass recorded on the given command encoder.
/// Returns a temporary buffer with aligned rows and CopySrc usage.
/// Requires: src_bytes_per_row % 4 == 0.
wgpu::Buffer RepackRows(
  wgpu::Device const& wgpu_device,
  wgpu::CommandEncoder& command_encoder,
  wgpu::Buffer const& src_buffer,
  uint32_t src_offset,
  uint32_t src_bytes_per_row,
  uint32_t dst_bytes_per_row,
  uint32_t row_count
);

} // namespace mnexus_backend::webgpu::buffer_row_repack
