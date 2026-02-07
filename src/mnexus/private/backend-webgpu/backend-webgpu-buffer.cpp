// TU header --------------------------------------------
#include "backend-webgpu/backend-webgpu-buffer.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-webgpu/types_bridge.h"

namespace mnexus_backend::webgpu {

wgpu::Buffer CreateWgpuBuffer(
  wgpu::Device const& wgpu_device,
  mnexus::BufferDesc const& buffer_desc
) {
  if (buffer_desc.size_in_bytes == 0) {
    MBASE_LOG_ERROR("Cannot create a buffer with size 0 bytes");
    return nullptr;
  }

  wgpu::BufferDescriptor wgpu_buffer_desc {};
  wgpu_buffer_desc.size = buffer_desc.size_in_bytes;
  wgpu_buffer_desc.usage = ToWgpuBufferUsage(buffer_desc.usage);
  wgpu_buffer_desc.mappedAtCreation = false;

  return wgpu_device.CreateBuffer(&wgpu_buffer_desc);
}

} // namespace mnexus_backend::webgpu
