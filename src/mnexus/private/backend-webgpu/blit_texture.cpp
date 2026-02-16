// TU header --------------------------------------------
#include "backend-webgpu/blit_texture.h"

// c++ headers ------------------------------------------
#include <cstring>

#include <algorithm>
#include <mutex>
#include <sstream>
#include <unordered_map>

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/tsa.h"

// project headers --------------------------------------
#include "backend-webgpu/backend-webgpu-texture.h"
#include "backend-webgpu/builtin_shader.h"

namespace mnexus_backend::webgpu::blit_texture {

namespace {

wgpu::Sampler s_sampler_nearest;
wgpu::Sampler s_sampler_linear;
wgpu::Device s_device; // Cached for lazy pipeline creation.

// Per-format render pipelines (lazy init).
// The shader is format-agnostic; only the ColorTargetState format differs.
mbase::Lockable<std::mutex> s_pipelines_mutex;
std::unordered_map<wgpu::TextureFormat, wgpu::RenderPipeline> s_pipelines MBASE_GUARDED_BY(s_pipelines_mutex);

wgpu::RenderPipeline GetPipeline(wgpu::TextureFormat dst_format) MBASE_REQUIRES(s_pipelines_mutex) {
  auto it = s_pipelines.find(dst_format);
  if (it != s_pipelines.end()) {
    return it->second;
  }

  wgpu::ColorTargetState color_target {};
  color_target.format = dst_format;
  color_target.writeMask = wgpu::ColorWriteMask::All;

  wgpu::FragmentState fragment_state {};
  fragment_state.module = builtin_shader::GetBlitTexture2dFs();
  fragment_state.targetCount = 1;
  fragment_state.targets = &color_target;

  std::ostringstream label_stream;
  label_stream << "BlitTexture2D [" << dst_format << "]";
  std::string const label = label_stream.str();

  wgpu::RenderPipelineDescriptor pipeline_desc {};
  pipeline_desc.label = label.c_str();
  pipeline_desc.vertex.module = builtin_shader::GetFullScreenQuadVs();
  pipeline_desc.fragment = &fragment_state;
  pipeline_desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  pipeline_desc.primitive.cullMode = wgpu::CullMode::None;

  wgpu::RenderPipeline pipeline = s_device.CreateRenderPipeline(&pipeline_desc);
  MBASE_ASSERT_MSG(bool(pipeline), "Failed to create blit render pipeline");

  s_pipelines[dst_format] = pipeline;
  return pipeline;
}

} // namespace

void Initialize(wgpu::Device const& wgpu_device) {
  // Create samplers.
  {
    wgpu::SamplerDescriptor desc {};
    desc.minFilter = wgpu::FilterMode::Nearest;
    desc.magFilter = wgpu::FilterMode::Nearest;
    desc.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
    desc.addressModeU = wgpu::AddressMode::ClampToEdge;
    desc.addressModeV = wgpu::AddressMode::ClampToEdge;
    desc.addressModeW = wgpu::AddressMode::ClampToEdge;
    s_sampler_nearest = wgpu_device.CreateSampler(&desc);
  }
  {
    wgpu::SamplerDescriptor desc {};
    desc.minFilter = wgpu::FilterMode::Linear;
    desc.magFilter = wgpu::FilterMode::Linear;
    desc.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
    desc.addressModeU = wgpu::AddressMode::ClampToEdge;
    desc.addressModeV = wgpu::AddressMode::ClampToEdge;
    desc.addressModeW = wgpu::AddressMode::ClampToEdge;
    s_sampler_linear = wgpu_device.CreateSampler(&desc);
  }

  // Cache device for lazy pipeline creation.
  s_device = wgpu_device;
}

void Shutdown() {
  s_sampler_nearest = nullptr;
  s_sampler_linear = nullptr;
  {
    mbase::LockGuard lock(s_pipelines_mutex);
    s_pipelines.clear();
  }
  s_device = nullptr;
}

void BlitTexture2D(
  wgpu::Device const& wgpu_device,
  wgpu::CommandEncoder& command_encoder,
  wgpu::Texture const& src_texture,
  wgpu::TextureFormat src_format,
  mnexus::TextureSubresourceRange const& src_subresource,
  uint32_t src_offset_x, uint32_t src_offset_y,
  uint32_t src_extent_w, uint32_t src_extent_h,
  wgpu::Texture const& dst_texture,
  wgpu::TextureFormat dst_format,
  mnexus::TextureSubresourceRange const& dst_subresource,
  uint32_t dst_offset_x, uint32_t dst_offset_y,
  uint32_t dst_extent_w, uint32_t dst_extent_h,
  mnexus::Filter filter
) {
  // Compute source UV range from pixel offsets/extents and src mip-level dimensions.
  // This does not access shared state, so compute before taking the lock.
  uint32_t const src_mip_w = std::max(1u, src_texture.GetWidth() >> src_subresource.base_mip_level);
  uint32_t const src_mip_h = std::max(1u, src_texture.GetHeight() >> src_subresource.base_mip_level);
  float const params_data[4] = {
    src_offset_x / float(src_mip_w),                    // src_uv_min.x
    src_offset_y / float(src_mip_h),                    // src_uv_min.y
    (src_offset_x + src_extent_w) / float(src_mip_w),   // src_uv_max.x
    (src_offset_y + src_extent_h) / float(src_mip_h),   // src_uv_max.y
  };

  wgpu::RenderPipeline pipeline;
  {
    mbase::LockGuard lock(s_pipelines_mutex);
    pipeline = GetPipeline(dst_format);
  }

  wgpu::Sampler const& sampler = (filter == mnexus::Filter::kLinear)
    ? s_sampler_linear
    : s_sampler_nearest;

  // Create params uniform buffer (16 bytes, mapped at creation).
  wgpu::BufferDescriptor params_buf_desc {};
  params_buf_desc.size = sizeof(params_data);
  params_buf_desc.usage = wgpu::BufferUsage::Uniform;
  params_buf_desc.mappedAtCreation = true;
  wgpu::Buffer params_buffer = wgpu_device.CreateBuffer(&params_buf_desc);
  std::memcpy(params_buffer.GetMappedRange(), params_data, sizeof(params_data));
  params_buffer.Unmap();

  // Create texture views.
  wgpu::TextureViewDescriptor src_view_desc = MakeWgpuTextureViewDesc(
    src_format,
    wgpu::TextureViewDimension::e2D,
    src_subresource,
    wgpu::TextureAspect::All
  );
  wgpu::TextureView src_view = src_texture.CreateView(&src_view_desc);

  wgpu::TextureViewDescriptor dst_view_desc = MakeWgpuTextureViewDesc(
    dst_format,
    wgpu::TextureViewDimension::e2D,
    dst_subresource,
    wgpu::TextureAspect::All
  );
  wgpu::TextureView dst_view = dst_texture.CreateView(&dst_view_desc);

  // Create bind group (3 entries: params, src_view, sampler).
  wgpu::BindGroupLayout layout = pipeline.GetBindGroupLayout(0);

  wgpu::BindGroupEntry entries[3] {};
  entries[0].binding = 0;
  entries[0].buffer = params_buffer;
  entries[0].offset = 0;
  entries[0].size = sizeof(params_data);

  entries[1].binding = 1;
  entries[1].textureView = src_view;

  entries[2].binding = 2;
  entries[2].sampler = sampler;

  wgpu::BindGroupDescriptor bg_desc {};
  bg_desc.layout = layout;
  bg_desc.entryCount = 3;
  bg_desc.entries = entries;
  wgpu::BindGroup bind_group = wgpu_device.CreateBindGroup(&bg_desc);

  // Record render pass with dst texture as color attachment.
  wgpu::RenderPassColorAttachment color_attachment {};
  color_attachment.view = dst_view;
  color_attachment.loadOp = wgpu::LoadOp::Load;
  color_attachment.storeOp = wgpu::StoreOp::Store;

  wgpu::RenderPassDescriptor rp_desc {};
  rp_desc.colorAttachmentCount = 1;
  rp_desc.colorAttachments = &color_attachment;

  wgpu::RenderPassEncoder pass = command_encoder.BeginRenderPass(&rp_desc);
  pass.SetPipeline(pipeline);
  pass.SetBindGroup(0, bind_group);
  pass.SetViewport(
    static_cast<float>(dst_offset_x),
    static_cast<float>(dst_offset_y),
    static_cast<float>(dst_extent_w),
    static_cast<float>(dst_extent_h),
    0.0f, 1.0f
  );
  pass.Draw(3);
  pass.End();
}

} // namespace mnexus_backend::webgpu::blit_texture
