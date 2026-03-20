#pragma once

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// CommandEncoder
//
// Wraps a VkCommandBuffer and tracks Vulkan-side recording state.
//

class CommandEncoder final {
public:
  CommandEncoder() = default;
  explicit CommandEncoder(VkCommandBuffer command_buffer);

  [[nodiscard]] VkCommandBuffer command_buffer() const { return command_buffer_; }

  void End();

  // Compute
  void BindComputePipeline(VkPipeline pipeline, VkPipelineLayout layout);
  void DispatchCompute(uint32_t x, uint32_t y, uint32_t z);

private:
  VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;

  VkPipeline current_compute_pipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout current_pipeline_layout_ = VK_NULL_HANDLE;
};

} // namespace mnexus_backend::vulkan
