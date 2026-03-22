// TU header --------------------------------------------
#include "backend-vulkan/descriptor_set_binder.h"

// c++ headers ------------------------------------------
#include <cstdint>

#include <algorithm>
#include <limits>

// public project headers -------------------------------
#include "mbase/public/log.h"
#include "mbase/public/trap.h"

// project headers --------------------------------------
#include "backend-vulkan/backend-vulkan-shader.h"
#include "backend-vulkan/descriptor_set_allocator.h"

namespace mnexus_backend::vulkan {

void DescriptorSetBinder::AssumePipelineLayout(
  VkPipelineLayout pipeline_layout,
  VulkanDescriptorSetLayout const* descriptor_set_layouts,
  uint32_t descriptor_set_count
) {
  if (current_pipeline_layout_ == pipeline_layout) {
    // Identical pipeline layout; no sets disturbed.
    current_descriptor_set_layouts_ = descriptor_set_layouts;
    current_descriptor_set_count_ = descriptor_set_count;
    return;
  }

  // Find the first set with an incompatible descriptor set layout.
  uint32_t first_disturbed = kMaxSets;
  uint32_t const old_set_count = current_descriptor_set_count_;
  uint32_t const new_set_count = descriptor_set_count;
  uint32_t const min_set_count = std::min(old_set_count, new_set_count);

  for (uint32_t set_index = 0; set_index < min_set_count; ++set_index) {
    VkDescriptorSetLayout old_layout_handle = current_descriptor_set_layouts_
      ? current_descriptor_set_layouts_[set_index].handle() : VK_NULL_HANDLE;
    VkDescriptorSetLayout new_layout_handle = descriptor_set_layouts[set_index].handle();

    if (old_layout_handle != new_layout_handle) {
      first_disturbed = set_index;
      break;
    }
  }

  if (first_disturbed == kMaxSets && old_set_count != new_set_count) {
    // Set counts differ but shared layouts match -- first new/missing set is disturbed.
    first_disturbed = min_set_count;
  }

  if (first_disturbed == kMaxSets) {
    // Pipeline layouts differ but all descriptor set layouts match.
    // The difference should be in push constants -- all sets are disturbed.
    first_disturbed = 0;
  }

  for (uint32_t set_index = first_disturbed; set_index < kMaxSets; ++set_index) {
    // Set is disturbed and needs to be rebound.
    set_rebinding_needed_.set(set_index);

    if (set_index < new_set_count) {
      VkDescriptorSetLayout old_layout_handle =
        (current_descriptor_set_layouts_ && set_index < old_set_count)
        ? current_descriptor_set_layouts_[set_index].handle() : VK_NULL_HANDLE;
      VkDescriptorSetLayout new_layout_handle = descriptor_set_layouts[set_index].handle();

      if (old_layout_handle != new_layout_handle) {
        // Descriptor set layout changed -- content must be rewritten.
        set_reallocation_needed_.set(set_index);
        set_write_descs_[set_index].AssumeDescriptorSetLayout(descriptor_set_layouts[set_index]);
      }
    }
  }

  current_pipeline_layout_ = pipeline_layout;
  current_descriptor_set_layouts_ = descriptor_set_layouts;
  current_descriptor_set_count_ = descriptor_set_count;
}

void DescriptorSetBinder::SetBuffer(
  uint32_t set, uint32_t binding, uint32_t array_element,
  VkDescriptorType descriptor_type,
  uint64_t handle_id,
  VkBuffer vk_buffer_handle, VkDeviceSize offset, VkDeviceSize range
) {
  if (set >= kMaxSets) {
    MBASE_LOG_ERROR(
      "DescriptorSetBinder: set index {} exceeds max of {}. SetBuffer(({}, {}, {}), {}, {}, {}, {})",
      set, kMaxSets - 1, set, binding, array_element, string_VkDescriptorType(descriptor_type), handle_id, offset, range
    );
    return;
  }

  set_explicit_flag_[set] = false;

  DescriptorSetWriteDesc::ReallocationNeeded const reallocation_needed =
    set_write_descs_[set].SetBufferDescriptorValue(binding, array_element, descriptor_type, handle_id, vk_buffer_handle, offset, range);
  set_reallocation_needed_[set] = set_reallocation_needed_[set] || reallocation_needed.value;
}

void DescriptorSetBinder::CmdBindDescriptorSets(
  VkCommandBuffer command_buffer,
  VkPipelineBindPoint bind_point,
  VkDevice device,
  IDescriptorSetAllocator* ds_allocator
) {
  if (set_reallocation_needed_.none() && set_rebinding_needed_.none()) {
    return;
  }

  uint32_t first_set_to_rebind = std::numeric_limits<uint32_t>::max();
  uint32_t inclusive_last_set_to_rebind = 0;
  uint32_t sets_to_rebind_count = 0;

  mbase::StaticVector<VkDescriptorSet, kMaxSets> vk_descriptor_sets(current_descriptor_set_count_);
  mbase::SmallVector<uint32_t, 8> dense_dynamic_offsets;

  for (uint32_t set_index = 0; set_index < current_descriptor_set_count_; ++set_index) {
    DescriptorSetWriteDesc& set_write_desc = set_write_descs_[set_index];

    if (set_write_desc.GetDescriptorCount() == 0) {
      // Mark for non-use.
      vk_descriptor_sets[set_index] = VK_NULL_HANDLE;
      continue;
    }

    if (set_reallocation_needed_[set_index]) {
      set_write_desc.ComputeHash();

      VulkanDescriptorSetLayout const& dsl = current_descriptor_set_layouts_[set_index];
      VulkanDescriptorSetPtr new_set = ds_allocator->Allocate(device, dsl, set_write_desc);

      if (new_set) {
        bound_descriptor_sets_[set_index] = std::move(new_set);
      }
    }

    if (set_reallocation_needed_[set_index] || set_rebinding_needed_[set_index]) {
      first_set_to_rebind = std::min(first_set_to_rebind, set_index);
      inclusive_last_set_to_rebind = std::max(inclusive_last_set_to_rebind, set_index);
      ++sets_to_rebind_count;

      vk_descriptor_sets[set_index] = bound_descriptor_sets_[set_index]
        ? bound_descriptor_sets_[set_index]->handle() : VK_NULL_HANDLE;

      if (!set_explicit_flag_[set_index]) {
        std::span<uint32_t const> const dynamic_offsets = set_write_desc.GetDenseDescriptorDynamicOffsets();

        if (!dynamic_offsets.empty()) {
          dense_dynamic_offsets.append_memcpyable_unsafe(dynamic_offsets.begin(), dynamic_offsets.end());
        }
      } else {
        // TODO: Implement dynamic offsets handling for explicit sets.
        mbase::Trap();
      }
    }

    set_reallocation_needed_[set_index] = false;
    set_rebinding_needed_[set_index] = false;
  }

  if (sets_to_rebind_count == 0) {
    return;
  }

  if (inclusive_last_set_to_rebind - first_set_to_rebind == sets_to_rebind_count - 1) {
    // Contiguous -- single call.
    vkCmdBindDescriptorSets(
      command_buffer,
      bind_point,
      current_pipeline_layout_,
      first_set_to_rebind, sets_to_rebind_count,
      vk_descriptor_sets.data() + first_set_to_rebind,
      static_cast<uint32_t>(dense_dynamic_offsets.size()), dense_dynamic_offsets.data()
    );
  } else {
    // Sparse -- individual calls with per-set dynamic offset tracking.
    uint32_t first_dynamic_offset_index = 0;

    for (uint32_t set_index = first_set_to_rebind; set_index <= inclusive_last_set_to_rebind; ++set_index) {
      if (vk_descriptor_sets[set_index] == VK_NULL_HANDLE) {
        continue;
      }

      uint32_t const dynamic_descriptor_count =
        static_cast<uint32_t>(set_write_descs_[set_index].GetDenseDescriptorDynamicOffsets().size());

      vkCmdBindDescriptorSets(
        command_buffer,
        bind_point,
        current_pipeline_layout_,
        set_index, 1,
        &vk_descriptor_sets[set_index],
        dynamic_descriptor_count, dense_dynamic_offsets.data() + first_dynamic_offset_index
      );

      first_dynamic_offset_index += dynamic_descriptor_count;
    }
  }
}

} // namespace mnexus_backend::vulkan
