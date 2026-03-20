// TU header --------------------------------------------
#include "backend-vulkan/command_encoder.h"

namespace mnexus_backend::vulkan {

CommandEncoder::CommandEncoder(VkCommandBuffer command_buffer) :
  command_buffer_(command_buffer)
{
}

void CommandEncoder::End() {
  vkEndCommandBuffer(command_buffer_);
}

void CommandEncoder::BindComputePipeline(VkPipeline pipeline, VkPipelineLayout layout) {
  current_compute_pipeline_ = pipeline;
  current_pipeline_layout_ = layout;
  vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

void CommandEncoder::DispatchCompute(uint32_t x, uint32_t y, uint32_t z) {
  vkCmdDispatch(command_buffer_, x, y, z);
}

} // namespace mnexus_backend::vulkan
