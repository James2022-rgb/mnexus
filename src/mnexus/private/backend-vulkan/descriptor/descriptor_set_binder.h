#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <array>
#include <bitset>

// public project headers -------------------------------
#include "mbase/public/container.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"
#include "backend-vulkan/descriptor/descriptor_set_allocator.h"
#include "backend-vulkan/descriptor/descriptor_set_write.h"

namespace mnexus_backend::vulkan {

class IDescriptorSetAllocator;
class VulkanDescriptorSetLayout;

// ----------------------------------------------------------------------------------------------------
// DescriptorSetBinder
//
// Tracks per-set descriptor binding state with dirty flags.
// On dispatch/draw, resolves dirty sets via IDescriptorSetAllocator hash-and-cache.
//

class DescriptorSetBinder final {
public:
  static constexpr uint32_t kMaxSets = 4;

  DescriptorSetBinder() = default;

  /// Called when a new pipeline is bound.
  void AssumePipelineLayout(
    VkPipelineLayout pipeline_layout,
    VulkanDescriptorSetLayout const* descriptor_set_layouts,
    uint32_t descriptor_set_count
  );

  /// Set a buffer binding (uniform or storage).
  void SetBuffer(
    uint32_t set, uint32_t binding, uint32_t array_element,
    VkDescriptorType descriptor_type,
    uint64_t handle_id,
    VkBuffer vk_buffer_handle, VkDeviceSize offset, VkDeviceSize range
  );

  /// Resolve dirty sets and emit vkCmdBindDescriptorSets.
  void CmdBindDescriptorSets(
    VkCommandBuffer command_buffer,
    VkPipelineBindPoint bind_point,
    VkDevice device,
    IDescriptorSetAllocator* ds_allocator
  );

private:
  VkPipelineLayout current_pipeline_layout_ = VK_NULL_HANDLE;
  VulkanDescriptorSetLayout const* current_descriptor_set_layouts_ = nullptr;
  uint32_t current_descriptor_set_count_ = 0;

  std::array<DescriptorSetWriteDesc, kMaxSets> set_write_descs_;

  // TODO: Explicit descriptor set binding API.
  std::bitset<kMaxSets> set_explicit_flag_;
  std::bitset<kMaxSets> set_reallocation_needed_; // Content changed -> need new descriptor set.
  std::bitset<kMaxSets> set_rebinding_needed_;    // Layout changed -> need vkCmdBindDescriptorSets.
  std::array<VulkanDescriptorSetPtr, kMaxSets> bound_descriptor_sets_;
};

} // namespace mnexus_backend::vulkan
