// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-render_pipeline.h"

// c++ headers ------------------------------------------
#include <vector>

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "resource_pool/generational_pool.h"

#include "backend-vulkan/device/vk-device.h"
#include "backend-vulkan/resource/types_bridge.h"

namespace mnexus_backend::vulkan {

namespace {

/// Convention for render programs:
///   `program_cold.shader_module_handles[0]` = vertex shader
///   `program_cold.shader_module_handles[1]` = fragment shader (optional)
/// Matches the WebGPU backend.
constexpr uint32_t kVertexShaderIndex = 0;
constexpr uint32_t kFragmentShaderIndex = 1;

} // namespace

VulkanRenderPipelinePtr CreateVulkanRenderPipelineFromCacheKey(
  IVulkanDevice& vk_device,
  pipeline::RenderPipelineCacheKey const& key,
  ProgramResourcePool const& program_pool,
  ShaderModuleResourcePool const& shader_module_pool
) {
  // Resolve program -> shader modules + pipeline layout.
  auto const program_pool_handle = resource_pool::ResourceHandle::FromU64(key.program.Get());
  auto [program_hot, program_cold, program_lock] = program_pool.GetConstRefWithSharedLockGuard(program_pool_handle);

  VkPipelineLayout const vk_pipeline_layout = program_hot.vk_pipeline_layout;

  MBASE_ASSERT_MSG(
    program_cold.shader_module_handles.size() >= 1 && program_cold.shader_module_handles.size() <= 2,
    "Render program must have 1 or 2 shader modules (vertex, or vertex+fragment)"
  );

  // Resolve vertex shader module handle. The shared lock is released at the
  // end of the inner scope; the VkShaderModule handle stays valid as long as
  // its owning generational-pool entry is not erased -- which the program
  // (locked above) keeps alive via shader_module_handles.
  VkShaderModule vs_module_handle = VK_NULL_HANDLE;
  {
    auto const vs_pool_handle = resource_pool::ResourceHandle::FromU64(
      program_cold.shader_module_handles[kVertexShaderIndex].Get()
    );
    auto [vs_hot, vs_lock] = shader_module_pool.GetHotConstRefWithSharedLockGuard(vs_pool_handle);
    vs_module_handle = vs_hot.vk_shader_module.handle();
  }

  // Resolve fragment shader module (optional).
  VkShaderModule fs_module_handle = VK_NULL_HANDLE;
  if (program_cold.shader_module_handles.size() > kFragmentShaderIndex) {
    auto const fs_pool_handle = resource_pool::ResourceHandle::FromU64(
      program_cold.shader_module_handles[kFragmentShaderIndex].Get()
    );
    auto [fs_hot, fs_lock] = shader_module_pool.GetHotConstRefWithSharedLockGuard(fs_pool_handle);
    fs_module_handle = fs_hot.vk_shader_module.handle();
  }

  // Shader stages.
  mbase::SmallVector<VkPipelineShaderStageCreateInfo, 2> stages;
  stages.emplace_back(VkPipelineShaderStageCreateInfo {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = vs_module_handle,
    .pName = "main",
    .pSpecializationInfo = nullptr,
  });
  if (fs_module_handle != VK_NULL_HANDLE) {
    stages.emplace_back(VkPipelineShaderStageCreateInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fs_module_handle,
      .pName = "main",
      .pSpecializationInfo = nullptr,
    });
  }

  // Vertex input.
  mbase::SmallVector<VkVertexInputBindingDescription, 4> vk_vertex_bindings;
  vk_vertex_bindings.reserve(key.vertex_bindings.size());
  for (auto const& b : key.vertex_bindings) {
    vk_vertex_bindings.emplace_back(ToVkVertexInputBindingDescription(b));
  }

  mbase::SmallVector<VkVertexInputAttributeDescription, 8> vk_vertex_attributes;
  vk_vertex_attributes.reserve(key.vertex_attributes.size());
  for (auto const& a : key.vertex_attributes) {
    vk_vertex_attributes.emplace_back(ToVkVertexInputAttributeDescription(a));
  }

  VkPipelineVertexInputStateCreateInfo vertex_input_state {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .vertexBindingDescriptionCount = static_cast<uint32_t>(vk_vertex_bindings.size()),
    .pVertexBindingDescriptions = vk_vertex_bindings.data(),
    .vertexAttributeDescriptionCount = static_cast<uint32_t>(vk_vertex_attributes.size()),
    .pVertexAttributeDescriptions = vk_vertex_attributes.data(),
  };

  // Input assembly.
  pipeline::PerDrawFixedFunctionStaticState const& pd = key.per_draw;
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .topology = ToVkPrimitiveTopology(static_cast<mnexus::PrimitiveTopology>(pd.ia_primitive_topology)),
    .primitiveRestartEnable = VK_FALSE,
  };

  // Viewport state (counts only; actual values are dynamic).
  VkPipelineViewportStateCreateInfo viewport_state {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewportCount = 1,
    .pViewports = nullptr,
    .scissorCount = 1,
    .pScissors = nullptr,
  };

  // Rasterization.
  VkPipelineRasterizationStateCreateInfo rasterization_state {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = ToVkPolygonMode(static_cast<mnexus::PolygonMode>(pd.raster_polygon_mode)),
    .cullMode = ToVkCullMode(static_cast<mnexus::CullMode>(pd.raster_cull_mode)),
    .frontFace = ToVkFrontFace(static_cast<mnexus::FrontFace>(pd.raster_front_face)),
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = 0.0f,
    .depthBiasClamp = 0.0f,
    .depthBiasSlopeFactor = 0.0f,
    .lineWidth = 1.0f,
  };

  // Multisample.
  VkPipelineMultisampleStateCreateInfo multisample_state {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .rasterizationSamples = static_cast<VkSampleCountFlagBits>(key.sample_count),
    .sampleShadingEnable = VK_FALSE,
    .minSampleShading = 0.0f,
    .pSampleMask = nullptr,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE,
  };

  // Depth-stencil.
  VkPipelineDepthStencilStateCreateInfo depth_stencil_state {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthTestEnable = pd.depth_test_enabled ? VK_TRUE : VK_FALSE,
    .depthWriteEnable = pd.depth_write_enabled ? VK_TRUE : VK_FALSE,
    .depthCompareOp = ToVkCompareOp(static_cast<mnexus::CompareOp>(pd.depth_compare_op)),
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable = pd.stencil_test_enabled ? VK_TRUE : VK_FALSE,
    .front = VkStencilOpState {
      .failOp = ToVkStencilOp(static_cast<mnexus::StencilOp>(pd.stencil_front_fail_op)),
      .passOp = ToVkStencilOp(static_cast<mnexus::StencilOp>(pd.stencil_front_pass_op)),
      .depthFailOp = ToVkStencilOp(static_cast<mnexus::StencilOp>(pd.stencil_front_depth_fail_op)),
      .compareOp = ToVkCompareOp(static_cast<mnexus::CompareOp>(pd.stencil_front_compare_op)),
      .compareMask = 0xFF,
      .writeMask = 0xFF,
      .reference = 0,
    },
    .back = VkStencilOpState {
      .failOp = ToVkStencilOp(static_cast<mnexus::StencilOp>(pd.stencil_back_fail_op)),
      .passOp = ToVkStencilOp(static_cast<mnexus::StencilOp>(pd.stencil_back_pass_op)),
      .depthFailOp = ToVkStencilOp(static_cast<mnexus::StencilOp>(pd.stencil_back_depth_fail_op)),
      .compareOp = ToVkCompareOp(static_cast<mnexus::CompareOp>(pd.stencil_back_compare_op)),
      .compareMask = 0xFF,
      .writeMask = 0xFF,
      .reference = 0,
    },
    .minDepthBounds = 0.0f,
    .maxDepthBounds = 1.0f,
  };

  // Color blend.
  mbase::SmallVector<VkPipelineColorBlendAttachmentState, 4> color_blend_attachments;
  color_blend_attachments.reserve(key.color_formats.size());
  for (size_t i = 0; i < key.color_formats.size(); ++i) {
    pipeline::PerAttachmentFixedFunctionStaticState const& att =
      i < key.per_attachment.size()
        ? key.per_attachment[i]
        : pipeline::PerAttachmentFixedFunctionStaticState {};

    color_blend_attachments.emplace_back(VkPipelineColorBlendAttachmentState {
      .blendEnable = att.blend_enabled ? VK_TRUE : VK_FALSE,
      .srcColorBlendFactor = ToVkBlendFactor(static_cast<mnexus::BlendFactor>(att.blend_src_color_factor)),
      .dstColorBlendFactor = ToVkBlendFactor(static_cast<mnexus::BlendFactor>(att.blend_dst_color_factor)),
      .colorBlendOp = ToVkBlendOp(static_cast<mnexus::BlendOp>(att.blend_color_blend_op)),
      .srcAlphaBlendFactor = ToVkBlendFactor(static_cast<mnexus::BlendFactor>(att.blend_src_alpha_factor)),
      .dstAlphaBlendFactor = ToVkBlendFactor(static_cast<mnexus::BlendFactor>(att.blend_dst_alpha_factor)),
      .alphaBlendOp = ToVkBlendOp(static_cast<mnexus::BlendOp>(att.blend_alpha_blend_op)),
      .colorWriteMask = ToVkColorComponentFlags(static_cast<mnexus::ColorWriteMask>(att.color_write_mask)),
    });
  }

  VkPipelineColorBlendStateCreateInfo color_blend_state {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = static_cast<uint32_t>(color_blend_attachments.size()),
    .pAttachments = color_blend_attachments.data(),
    .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f },
  };

  // Dynamic state.
  static constexpr VkDynamicState kDynamicStates[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo dynamic_state {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .dynamicStateCount = static_cast<uint32_t>(std::size(kDynamicStates)),
    .pDynamicStates = kDynamicStates,
  };

  // Dynamic rendering pipeline rendering info.
  mbase::SmallVector<VkFormat, 4> color_attachment_formats;
  color_attachment_formats.reserve(key.color_formats.size());
  for (auto const f : key.color_formats) {
    color_attachment_formats.emplace_back(ToVkFormat(f));
  }
  VkFormat const depth_format = ToVkFormat(key.depth_stencil_format);

  VkPipelineRenderingCreateInfoKHR rendering_create_info {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
    .pNext = nullptr,
    .viewMask = 0,
    .colorAttachmentCount = static_cast<uint32_t>(color_attachment_formats.size()),
    .pColorAttachmentFormats = color_attachment_formats.data(),
    .depthAttachmentFormat = depth_format,
    .stencilAttachmentFormat = pd.stencil_test_enabled ? depth_format : VK_FORMAT_UNDEFINED,
  };

  // Pipeline create info.
  VkGraphicsPipelineCreateInfo pipeline_create_info {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &rendering_create_info,
    .flags = 0,
    .stageCount = static_cast<uint32_t>(stages.size()),
    .pStages = stages.data(),
    .pVertexInputState = &vertex_input_state,
    .pInputAssemblyState = &input_assembly_state,
    .pTessellationState = nullptr,
    .pViewportState = &viewport_state,
    .pRasterizationState = &rasterization_state,
    .pMultisampleState = &multisample_state,
    .pDepthStencilState = &depth_stencil_state,
    .pColorBlendState = &color_blend_state,
    .pDynamicState = &dynamic_state,
    .layout = vk_pipeline_layout,
    .renderPass = VK_NULL_HANDLE,
    .subpass = 0,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1,
  };

  VkPipeline vk_pipeline_handle = VK_NULL_HANDLE;
  VkResult const result = vkCreateGraphicsPipelines(
    vk_device.handle(),
    VK_NULL_HANDLE,
    1,
    &pipeline_create_info,
    nullptr,
    &vk_pipeline_handle
  );
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateGraphicsPipelines failed: {}", static_cast<int32_t>(result));
    return nullptr;
  }

  VkDevice const vk_device_handle = vk_device.handle();
  return std::make_shared<VulkanRenderPipeline>(
    vk_pipeline_handle,
    [vk_device_handle, vk_pipeline_handle] { vkDestroyPipeline(vk_device_handle, vk_pipeline_handle, nullptr); },
    vk_device.GetDeferredDestroyer()
  );
}

} // namespace mnexus_backend::vulkan
