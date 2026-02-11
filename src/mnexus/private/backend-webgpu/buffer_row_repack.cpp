// TU header --------------------------------------------
#include "backend-webgpu/buffer_row_repack.h"

// c++ headers ------------------------------------------
#include <cstring>

// public project headers -------------------------------
#include "mbase/public/assert.h"

// project headers --------------------------------------
#include "backend-webgpu/builtin_shader.h"

namespace mnexus_backend::webgpu::buffer_row_repack {

namespace {

wgpu::ComputePipeline s_pipeline;

} // namespace

void Initialize(wgpu::Device const& wgpu_device) {
  wgpu::ShaderModule shader_module = builtin_shader::GetBufferRepackRowsCs();

  wgpu::ComputePipelineDescriptor desc {};
  desc.compute.module = shader_module;
  desc.compute.entryPoint = "main";
  s_pipeline = wgpu_device.CreateComputePipeline(&desc);
  MBASE_ASSERT_MSG(bool(s_pipeline), "Failed to create buffer_row_repack compute pipeline");
}

void Shutdown() {
  s_pipeline = nullptr;
}

wgpu::Buffer RepackRows(
  wgpu::Device const& wgpu_device,
  wgpu::CommandEncoder& command_encoder,
  wgpu::Buffer const& src_buffer,
  uint32_t src_offset,
  uint32_t src_bytes_per_row,
  uint32_t dst_bytes_per_row,
  uint32_t row_count
) {
  MBASE_ASSERT(src_bytes_per_row % 4 == 0);
  MBASE_ASSERT(dst_bytes_per_row % 4 == 0);
  MBASE_ASSERT(row_count > 0);

  // Create params uniform buffer (16 bytes, mapped at creation).
  uint32_t const params_data[4] = {
    src_offset,
    src_bytes_per_row,
    dst_bytes_per_row,
    row_count,
  };

  wgpu::BufferDescriptor params_buf_desc {};
  params_buf_desc.size = sizeof(params_data);
  params_buf_desc.usage = wgpu::BufferUsage::Uniform;
  params_buf_desc.mappedAtCreation = true;
  wgpu::Buffer params_buffer = wgpu_device.CreateBuffer(&params_buf_desc);
  std::memcpy(params_buffer.GetMappedRange(), params_data, sizeof(params_data));
  params_buffer.Unmap();

  // Create aligned temp buffer.
  uint64_t const temp_size = static_cast<uint64_t>(dst_bytes_per_row) * row_count;
  wgpu::BufferDescriptor temp_buf_desc {};
  temp_buf_desc.size = temp_size;
  temp_buf_desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
  wgpu::Buffer temp_buffer = wgpu_device.CreateBuffer(&temp_buf_desc);

  // Create bind group.
  wgpu::BindGroupLayout layout = s_pipeline.GetBindGroupLayout(0);

  uint64_t const src_bind_size = static_cast<uint64_t>(src_offset) + static_cast<uint64_t>(src_bytes_per_row) * row_count;

  wgpu::BindGroupEntry entries[3] {};
  entries[0].binding = 0;
  entries[0].buffer = params_buffer;
  entries[0].offset = 0;
  entries[0].size = sizeof(params_data);

  entries[1].binding = 1;
  entries[1].buffer = src_buffer;
  entries[1].offset = 0;
  entries[1].size = src_bind_size;

  entries[2].binding = 2;
  entries[2].buffer = temp_buffer;
  entries[2].offset = 0;
  entries[2].size = temp_size;

  wgpu::BindGroupDescriptor bg_desc {};
  bg_desc.layout = layout;
  bg_desc.entryCount = 3;
  bg_desc.entries = entries;
  wgpu::BindGroup bind_group = wgpu_device.CreateBindGroup(&bg_desc);

  // Record compute pass.
  wgpu::ComputePassEncoder pass = command_encoder.BeginComputePass();
  pass.SetPipeline(s_pipeline);
  pass.SetBindGroup(0, bind_group);

  uint32_t const words_per_row = src_bytes_per_row / 4;
  uint32_t const workgroup_x = (words_per_row + 63) / 64;
  pass.DispatchWorkgroups(workgroup_x, row_count, 1);
  pass.End();

  return temp_buffer;
}

} // namespace mnexus_backend::webgpu::buffer_row_repack
