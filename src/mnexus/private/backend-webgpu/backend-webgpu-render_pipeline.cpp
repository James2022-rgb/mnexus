// TU header --------------------------------------------
#include "backend-webgpu/backend-webgpu-render_pipeline.h"

// c++ headers ------------------------------------------
#include <vector>

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "container/generational_pool.h"

#include "backend-webgpu/types_bridge.h"

#include "pipeline/render_pipeline_fixed_function.h"

namespace mnexus_backend::webgpu {

wgpu::RenderPipeline CreateWgpuRenderPipelineFromCacheKey(
  wgpu::Device const& wgpu_device,
  pipeline::RenderPipelineCacheKey const& key,
  ProgramResourcePool const& program_pool,
  ShaderModuleResourcePool const& shader_module_pool
) {
  // Look up program resources.
  auto program_pool_handle = container::ResourceHandle::FromU64(key.program.Get());
  auto [program_hot, program_cold, program_lock] = program_pool.GetConstRefWithSharedLockGuard(program_pool_handle);

  wgpu::PipelineLayout const& pipeline_layout = program_hot.wgpu_pipeline_layout;

  // Resolve shader modules (vertex + fragment).
  MBASE_ASSERT_MSG(
    program_cold.shader_module_handles.size() >= 1 && program_cold.shader_module_handles.size() <= 2,
    "Render program must have 1 or 2 shader modules (vertex, or vertex+fragment)"
  );

  // First shader module = vertex.
  auto vs_pool_handle = container::ResourceHandle::FromU64(program_cold.shader_module_handles[0].Get());
  auto [vs_hot, vs_lock] = shader_module_pool.GetHotConstRefWithSharedLockGuard(vs_pool_handle);

  // Second shader module = fragment (optional).
  wgpu::ShaderModule fs_module;
  if (program_cold.shader_module_handles.size() >= 2) {
    auto fs_pool_handle = container::ResourceHandle::FromU64(program_cold.shader_module_handles[1].Get());
    auto [fs_hot, fs_lock] = shader_module_pool.GetHotConstRefWithSharedLockGuard(fs_pool_handle);
    fs_module = fs_hot.wgpu_shader_module;
  }

  // Build vertex buffer layouts.
  std::vector<wgpu::VertexBufferLayout> wgpu_vertex_buffers;
  std::vector<std::vector<wgpu::VertexAttribute>> wgpu_attributes_per_binding;

  wgpu_vertex_buffers.reserve(key.vertex_bindings.size());
  wgpu_attributes_per_binding.resize(key.vertex_bindings.size());

  // Group attributes by binding.
  for (auto const& attr : key.vertex_attributes) {
    for (size_t i = 0; i < key.vertex_bindings.size(); ++i) {
      if (key.vertex_bindings[i].binding == attr.binding) {
        wgpu_attributes_per_binding[i].emplace_back(
          wgpu::VertexAttribute {
            .format = ToWgpuVertexFormat(attr.format),
            .offset = attr.offset,
            .shaderLocation = attr.location,
          }
        );
        break;
      }
    }
  }

  for (size_t i = 0; i < key.vertex_bindings.size(); ++i) {
    auto const& binding = key.vertex_bindings[i];
    wgpu_vertex_buffers.emplace_back(
      wgpu::VertexBufferLayout {
        .stepMode = ToWgpuVertexStepMode(binding.step_mode),
        .arrayStride = binding.stride,
        .attributeCount = wgpu_attributes_per_binding[i].size(),
        .attributes = wgpu_attributes_per_binding[i].data(),
      }
    );
  }

  // Vertex state.
  wgpu::VertexState vertex_state {
    .module = vs_hot.wgpu_shader_module,
    .entryPoint = "main",
    .bufferCount = wgpu_vertex_buffers.size(),
    .buffers = wgpu_vertex_buffers.data(),
  };

  // Primitive state.
  auto const& pd = key.per_draw;
  wgpu::PrimitiveState primitive_state {
    .topology = ToWgpuPrimitiveTopology(static_cast<mnexus::PrimitiveTopology>(pd.ia_primitive_topology)),
    .frontFace = ToWgpuFrontFace(static_cast<mnexus::FrontFace>(pd.raster_front_face)),
    .cullMode = ToWgpuCullMode(static_cast<mnexus::CullMode>(pd.raster_cull_mode)),
  };

  // Depth stencil state (if depth format is specified).
  std::optional<wgpu::DepthStencilState> depth_stencil_state;
  if (key.depth_stencil_format != MnFormat::kUndefined) {
    depth_stencil_state = wgpu::DepthStencilState {
      .format = ToWgpuTextureFormat(key.depth_stencil_format),
      .depthWriteEnabled = pd.depth_write_enabled != 0,
      .depthCompare = pd.depth_test_enabled
        ? ToWgpuCompareFunction(static_cast<mnexus::CompareOp>(pd.depth_compare_op))
        : wgpu::CompareFunction::Always,
    };
  }

  // Color targets.
  std::vector<wgpu::ColorTargetState> color_targets;
  std::vector<wgpu::BlendState> blend_states;
  color_targets.reserve(key.color_formats.size());
  blend_states.reserve(key.color_formats.size());

  for (size_t i = 0; i < key.color_formats.size(); ++i) {
    pipeline::PerAttachmentFixedFunctionStaticState const& att =
      i < key.per_attachment.size()
        ? key.per_attachment[i]
        : pipeline::PerAttachmentFixedFunctionStaticState {};

    wgpu::BlendState blend {
      .color = {
        .operation = ToWgpuBlendOperation(static_cast<mnexus::BlendOp>(att.blend_color_blend_op)),
        .srcFactor = ToWgpuBlendFactor(static_cast<mnexus::BlendFactor>(att.blend_src_color_factor)),
        .dstFactor = ToWgpuBlendFactor(static_cast<mnexus::BlendFactor>(att.blend_dst_color_factor)),
      },
      .alpha = {
        .operation = ToWgpuBlendOperation(static_cast<mnexus::BlendOp>(att.blend_alpha_blend_op)),
        .srcFactor = ToWgpuBlendFactor(static_cast<mnexus::BlendFactor>(att.blend_src_alpha_factor)),
        .dstFactor = ToWgpuBlendFactor(static_cast<mnexus::BlendFactor>(att.blend_dst_alpha_factor)),
      },
    };
    blend_states.emplace_back(blend);

    color_targets.emplace_back(
      wgpu::ColorTargetState {
        .format = ToWgpuTextureFormat(key.color_formats[i]),
        .blend = att.blend_enabled ? &blend_states.back() : nullptr,
        .writeMask = ToWgpuColorWriteMask(static_cast<mnexus::ColorWriteMask>(att.color_write_mask)),
      }
    );
  }

  // Fragment state.
  std::optional<wgpu::FragmentState> fragment_state;
  if (fs_module) {
    fragment_state = wgpu::FragmentState {
      .module = fs_module,
      .entryPoint = "main",
      .targetCount = color_targets.size(),
      .targets = color_targets.data(),
    };
  }

  // Multisample state.
  wgpu::MultisampleState multisample_state {
    .count = key.sample_count,
  };

  // Create the render pipeline.
  wgpu::RenderPipelineDescriptor desc {
    .layout = pipeline_layout,
    .vertex = vertex_state,
    .primitive = primitive_state,
    .depthStencil = depth_stencil_state.has_value() ? &*depth_stencil_state : nullptr,
    .multisample = multisample_state,
    .fragment = fragment_state.has_value() ? &*fragment_state : nullptr,
  };

  wgpu::RenderPipeline wgpu_pipeline = wgpu_device.CreateRenderPipeline(&desc);

  if (!wgpu_pipeline) {
    MBASE_LOG_ERROR("Failed to create wgpu::RenderPipeline from cache key");
  }

  return wgpu_pipeline;
}

} // namespace mnexus_backend::webgpu
