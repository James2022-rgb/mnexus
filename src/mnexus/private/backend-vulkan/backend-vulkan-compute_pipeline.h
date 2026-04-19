#pragma once

// public project headers -------------------------------
#include "mbase/public/accessor.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/object/vk-object-compute_pipeline.h"

namespace mnexus_backend::vulkan {

class ComputePipelineHot final {
public:
  explicit ComputePipelineHot(VulkanComputePipeline vk_compute_pipeline, VulkanPipelineLayoutPtr pipeline_layout_ref) :
    vk_compute_pipeline_(std::move(vk_compute_pipeline)),
    pipeline_layout_ref_(std::move(pipeline_layout_ref))
  {
  }
  ~ComputePipelineHot() = default;
  MBASE_DISALLOW_COPY_DEFAULT_MOVE(ComputePipelineHot);

  MBASE_ACCESSOR_GETCR(VulkanComputePipeline, vk_compute_pipeline);
  MBASE_ACCESSOR_GETCR(VulkanPipelineLayoutPtr, pipeline_layout_ref);

  void Stamp(uint32_t queue_compact_index, uint64_t serial);

private:
  VulkanComputePipeline vk_compute_pipeline_;
  VulkanPipelineLayoutPtr pipeline_layout_ref_;
};

class ComputePipelineCold final {
public:
  explicit ComputePipelineCold(mnexus::ProgramHandle program_handle, mnexus::ShaderModuleHandle shader_module_handle) :
    program_handle_(program_handle),
    shader_module_handle_(shader_module_handle)
  {
  }
  ~ComputePipelineCold() = default;
  MBASE_DISALLOW_COPY_DEFAULT_MOVE(ComputePipelineCold);
  
  MBASE_ACCESSOR_GETV(mnexus::ProgramHandle, program_handle);
  MBASE_ACCESSOR_GETV(mnexus::ShaderModuleHandle, shader_module_handle);

private:
  mnexus::ProgramHandle program_handle_;
  mnexus::ShaderModuleHandle shader_module_handle_;
};

using ComputePipelineResourcePool = resource_pool::TResourceGenerationalPool<ComputePipelineHot, ComputePipelineCold, mnexus::kResourceTypeComputePipeline>;

resource_pool::ResourceHandle EmplaceComputePipelineResourcePool(
  ComputePipelineResourcePool& out_pool,
  IVulkanDevice const& vk_device,
  mnexus::ProgramHandle program_handle,
  ProgramResourcePool const& program_pool,
  ShaderModuleResourcePool const& shader_module_pool
);

} // namespace mnexus_backend::vulkan
