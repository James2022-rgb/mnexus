// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-compute_pipeline.h"

// public project headers -------------------------------
#include "mbase/public/log.h"

namespace mnexus_backend::vulkan {

bool CreateVulkanComputePipeline(
  VulkanComputePipeline& out_vk_compute_pipeline,
  VulkanDevice const& vk_device,
  VulkanShaderModule const& vk_shader_module,
  VulkanPipelineLayout const& vk_pipeline_layout
) {
  VkComputePipelineCreateInfo info {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .stage = VkPipelineShaderStageCreateInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = vk_shader_module.handle(),
      .pName = "main",
      .pSpecializationInfo = nullptr,
    },
    .layout = vk_pipeline_layout.handle(),
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1,
  };

  VkPipeline vk_pipeline_handle = VK_NULL_HANDLE;
  VkResult const result = vkCreateComputePipelines(
    vk_device.handle(),
    VK_NULL_HANDLE,
    1,
    &info,
    nullptr,
    &vk_pipeline_handle
  );
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateComputePipelines failed: {}", static_cast<int32_t>(result));
    return false;
  }

  VkDevice vk_device_handle = vk_device.handle();
  out_vk_compute_pipeline = VulkanComputePipeline(
    vk_pipeline_handle,
    [vk_device_handle, vk_pipeline_handle] { vkDestroyPipeline(vk_device_handle, vk_pipeline_handle, nullptr); },
    vk_device.deferred_destroyer()
  );
  return true;
}

container::ResourceHandle EmplaceComputePipelineResourcePool(
  ComputePipelineResourcePool& out_pool,
  VulkanDevice const& vk_device,
  mnexus::ProgramHandle program_handle,
  ProgramResourcePool const& program_pool,
  ShaderModuleResourcePool const& shader_module_pool
) {
  auto const program_pool_handle = container::ResourceHandle::FromU64(program_handle.Get());
  auto [program_hot, program_cold, program_lock] = program_pool.GetConstRefWithSharedLockGuard(
    program_pool_handle
  );

  mnexus::ShaderModuleHandle const shader_module_handle = program_cold.shader_module_handles[0];
  auto const shader_module_pool_handle = container::ResourceHandle::FromU64(shader_module_handle.Get());

  auto [shader_module_hot, shader_module_lock] = shader_module_pool.GetHotConstRefWithSharedLockGuard(
    shader_module_pool_handle
  );

  VulkanComputePipeline vk_compute_pipeline;
  bool const success = CreateVulkanComputePipeline(
    vk_compute_pipeline,
    vk_device,
    shader_module_hot.vk_shader_module,
    *program_hot.pipeline_layout_ref.get()
  );
  if (!success) {
    return container::ResourceHandle::Null();
  }

  ComputePipelineHot hot {
    .vk_compute_pipeline = std::move(vk_compute_pipeline),
    .vk_pipeline_layout = program_hot.pipeline_layout_ref->handle(),
    .pipeline_layout_ref = program_hot.pipeline_layout_ref,
  };
  ComputePipelineCold cold {
    .program_handle = program_handle,
    .shader_module_handle = shader_module_handle,
  };

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

} // namespace mnexus_backend::vulkan
