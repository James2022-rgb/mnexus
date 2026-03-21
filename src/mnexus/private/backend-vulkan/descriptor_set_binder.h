#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <array>
#include <bitset>

// public project headers -------------------------------
#include "mbase/public/container.h"

#include "mnexus/public/types.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

namespace mnexus_backend::vulkan {

class DescriptorSetAllocator;
class VulkanDescriptorSetLayout;
struct ResourceStorage;

// ----------------------------------------------------------------------------------------------------
// DescriptorSetBinder
//
// Tracks per-set descriptor binding state with dirty flags.
// On dispatch/draw, resolves dirty sets into VkDescriptorSets via DescriptorSetAllocator.
//

class DescriptorSetBinder final {
public:
  static constexpr uint32_t kMaxSets = 4;

  DescriptorSetBinder() = default;

  /// Called when a new pipeline is bound.
  /// Marks disturbed sets for reallocation/rebinding.
  void AssumePipelineLayout(
    VkPipelineLayout pipeline_layout,
    VulkanDescriptorSetLayout const* descriptor_set_layouts,
    uint32_t descriptor_set_count
  );

  /// Set a buffer binding (uniform or storage).
  void SetBuffer(
    uint32_t set, uint32_t binding, uint32_t array_element,
    VkDescriptorType descriptor_type,
    VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range
  );

  /// Resolve dirty sets and emit vkCmdBindDescriptorSets.
  void CmdBindDescriptorSets(
    VkCommandBuffer command_buffer,
    VkPipelineBindPoint bind_point,
    VkDevice device,
    DescriptorSetAllocator& allocator
  );

private:
  struct BufferBinding {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    VkDeviceSize range = 0;
  };

  struct BindingEntry {
    uint32_t binding = 0;
    uint32_t array_element = 0;
    VkDescriptorType descriptor_type {};
    BufferBinding buffer;
    // TODO: ImageBinding, SamplerBinding for texture/sampler support.
  };

  struct SetState {
    mbase::SmallVector<BindingEntry, 4> entries;
  };

  VkPipelineLayout current_pipeline_layout_ = VK_NULL_HANDLE;
  VulkanDescriptorSetLayout const* current_descriptor_set_layouts_ = nullptr;
  uint32_t current_descriptor_set_count_ = 0;

  std::array<SetState, kMaxSets> sets_;
  std::bitset<kMaxSets> set_dirty_;
  std::array<VkDescriptorSet, kMaxSets> bound_descriptor_sets_ {};
};

} // namespace mnexus_backend::vulkan
