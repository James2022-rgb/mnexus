// TU header --------------------------------------------
#include "backend-vulkan/descriptor_set_binder.h"

// c++ headers ------------------------------------------
#include <limits>
#include <vector>

// project headers --------------------------------------
#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/descriptor_set_allocator.h"

namespace mnexus_backend::vulkan {

void DescriptorSetBinder::AssumePipelineLayout(
  VkPipelineLayout pipeline_layout,
  VulkanDescriptorSetLayout const* descriptor_set_layouts,
  uint32_t descriptor_set_count
) {
  if (current_pipeline_layout_ != pipeline_layout) {
    // Pipeline layout changed — mark all sets dirty.
    for (uint32_t i = 0; i < descriptor_set_count; ++i) {
      set_dirty_.set(i);
    }
  }

  current_pipeline_layout_ = pipeline_layout;
  current_descriptor_set_layouts_ = descriptor_set_layouts;
  current_descriptor_set_count_ = descriptor_set_count;
}

void DescriptorSetBinder::SetBuffer(
  uint32_t set, uint32_t binding, uint32_t array_element,
  VkDescriptorType descriptor_type,
  VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range
) {
  if (set >= kMaxSets) {
    return;
  }

  SetState& state = sets_[set];

  // Upsert: find existing entry or update in-place.
  for (auto& entry : state.entries) {
    if (entry.binding == binding && entry.array_element == array_element) {
      entry.descriptor_type = descriptor_type;
      entry.buffer = { buffer, offset, range };
      set_dirty_.set(set);
      return;
    }
  }

  state.entries.emplace_back(
    BindingEntry {
      .binding = binding,
      .array_element = array_element,
      .descriptor_type = descriptor_type,
      .buffer = { buffer, offset, range },
    }
  );
  set_dirty_.set(set);
}

void DescriptorSetBinder::CmdBindDescriptorSets(
  VkCommandBuffer command_buffer,
  VkPipelineBindPoint bind_point,
  VkDevice device,
  DescriptorSetAllocator& allocator
) {
  uint32_t first_set_to_rebind = std::numeric_limits<uint32_t>::max();
  uint32_t inclusive_last_set_to_rebind = 0;
  uint32_t sets_to_rebind_count = 0;

  // Phase 1: Allocate + update dirty sets.
  for (uint32_t set_index = 0; set_index < current_descriptor_set_count_; ++set_index) {
    if (!set_dirty_.test(set_index)) {
      continue;
    }

    SetState& state = sets_[set_index];
    if (state.entries.empty()) {
      set_dirty_.reset(set_index);
      continue;
    }

    VulkanDescriptorSetLayout const& dsl = current_descriptor_set_layouts_[set_index];

    // Allocate a descriptor set for this layout.
    VkDescriptorSet vk_set = allocator.Allocate(dsl);
    if (vk_set == VK_NULL_HANDLE) {
      continue;
    }

    // Build VkWriteDescriptorSet + VkDescriptorBufferInfo arrays.
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    writes.reserve(state.entries.size());
    buffer_infos.reserve(state.entries.size());

    for (auto const& entry : state.entries) {
      buffer_infos.emplace_back(
        VkDescriptorBufferInfo {
          .buffer = entry.buffer.buffer,
          .offset = entry.buffer.offset,
          .range = entry.buffer.range,
        }
      );

      writes.emplace_back(
        VkWriteDescriptorSet {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = nullptr,
          .dstSet = vk_set,
          .dstBinding = entry.binding,
          .dstArrayElement = entry.array_element,
          .descriptorCount = 1,
          .descriptorType = entry.descriptor_type,
          .pImageInfo = nullptr,
          .pBufferInfo = &buffer_infos.back(),
          .pTexelBufferView = nullptr,
        }
      );
    }

    vkUpdateDescriptorSets(
      device,
      static_cast<uint32_t>(writes.size()),
      writes.data(),
      0,
      nullptr
    );

    bound_descriptor_sets_[set_index] = vk_set;

    first_set_to_rebind = std::min(first_set_to_rebind, set_index);
    inclusive_last_set_to_rebind = std::max(inclusive_last_set_to_rebind, set_index);
    ++sets_to_rebind_count;

    set_dirty_.reset(set_index);
  }

  if (sets_to_rebind_count == 0) {
    return;
  }

  // Phase 2: Emit vkCmdBindDescriptorSets.
  if (inclusive_last_set_to_rebind - first_set_to_rebind == sets_to_rebind_count - 1) {
    // Sets are contiguous — single call.
    vkCmdBindDescriptorSets(
      command_buffer,
      bind_point,
      current_pipeline_layout_,
      first_set_to_rebind, sets_to_rebind_count,
      &bound_descriptor_sets_[first_set_to_rebind],
      0, nullptr
    );
  } else {
    // Sets are sparse — individual calls.
    for (uint32_t set_index = first_set_to_rebind; set_index <= inclusive_last_set_to_rebind; ++set_index) {
      if (bound_descriptor_sets_[set_index] == VK_NULL_HANDLE) {
        continue;
      }

      vkCmdBindDescriptorSets(
        command_buffer,
        bind_point,
        current_pipeline_layout_,
        set_index, 1,
        &bound_descriptor_sets_[set_index],
        0, nullptr
      );
    }
  }
}

} // namespace mnexus_backend::vulkan
