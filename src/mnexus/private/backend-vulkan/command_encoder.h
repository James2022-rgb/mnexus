#pragma once

// public project headers -------------------------------
#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/descriptor_set_binder.h"

namespace mnexus_backend::vulkan {

class DescriptorSetAllocator;
class VulkanDescriptorSetLayout;
struct ResourceStorage;

// ----------------------------------------------------------------------------------------------------
// CommandEncoder
//
// Wraps a VkCommandBuffer and tracks Vulkan-side recording state.
//

class CommandEncoder final {
public:
  CommandEncoder() = default;
  CommandEncoder(VkCommandBuffer command_buffer, VkDevice device, DescriptorSetAllocator* allocator, ResourceStorage* resource_storage);

  [[nodiscard]] VkCommandBuffer command_buffer() const { return command_buffer_; }

  void End();

  // Compute
  void BindComputePipeline(VkPipeline pipeline, VkPipelineLayout layout,
                           VulkanDescriptorSetLayout const* descriptor_set_layouts, uint32_t descriptor_set_count);
  void DispatchCompute(uint32_t x, uint32_t y, uint32_t z);

  // Binding
  void BindBuffer(uint32_t set, uint32_t binding, uint32_t array_element,
                  VkDescriptorType descriptor_type,
                  VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

private:
  void ResolveDescriptorSets(VkPipelineBindPoint bind_point);

  VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;
  DescriptorSetAllocator* allocator_ = nullptr;
  ResourceStorage* resource_storage_ = nullptr;

  VkPipeline current_compute_pipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout current_pipeline_layout_ = VK_NULL_HANDLE;

  DescriptorSetBinder descriptor_set_binder_;
};

} // namespace mnexus_backend::vulkan
