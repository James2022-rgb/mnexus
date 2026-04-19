// TU header --------------------------------------------
#include "backend-vulkan/command/command_encoder.h"

// public project headers -------------------------------
#include "mbase/public/assert.h"

namespace mnexus_backend::vulkan {

CommandEncoder::CommandEncoder(
  VkCommandBuffer command_buffer,
  VkDevice device,
  IDescriptorSetAllocator* ds_allocator,
  ResourceStorage* resource_storage
) :
  command_buffer_(command_buffer),
  vk_device_(device),
  ds_allocator_(ds_allocator),
  resource_storage_(resource_storage)
{
}

void CommandEncoder::End() {
  vkEndCommandBuffer(command_buffer_);
}

void CommandEncoder::BindComputePipeline(
  VkPipeline pipeline,
  VkPipelineLayout layout,
  VulkanDescriptorSetLayout const* descriptor_set_layouts,
  uint32_t descriptor_set_count
) {
  current_compute_pipeline_ = pipeline;
  current_pipeline_layout_ = layout;
  vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  descriptor_set_binder_.AssumePipelineLayout(layout, descriptor_set_layouts, descriptor_set_count);
}

void CommandEncoder::DispatchCompute(uint32_t x, uint32_t y, uint32_t z) {
  this->ResolveDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE);
  vkCmdDispatch(command_buffer_, x, y, z);
}

void CommandEncoder::BindBuffer(
  uint32_t set, uint32_t binding, uint32_t array_element,
  VkDescriptorType descriptor_type, uint64_t handle_id,
  VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range
) {
  descriptor_set_binder_.SetBuffer(set, binding, array_element, descriptor_type, handle_id, buffer, offset, range);
}

void CommandEncoder::ResolveDescriptorSets(VkPipelineBindPoint bind_point) {
  MBASE_ASSERT(ds_allocator_ != nullptr);
  descriptor_set_binder_.CmdBindDescriptorSets(command_buffer_, bind_point, vk_device_, ds_allocator_);
}

} // namespace mnexus_backend::vulkan
