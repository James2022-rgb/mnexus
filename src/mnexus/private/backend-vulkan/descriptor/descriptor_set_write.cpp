// TU header --------------------------------------------
#include "backend-vulkan/descriptor/descriptor_set_write.h"

// c++ headers ------------------------------------------
#include <cstring>

#include <utility>

// public project headers -------------------------------
#include "mbase/public/hash.h"

// project headers --------------------------------------
#include "backend-vulkan/backend-vulkan-shader.h"

namespace mnexus_backend::vulkan {

void DescriptorSetWriteDesc::AssumeDescriptorSetLayout(VulkanDescriptorSetLayout const& set_layout) {
  descriptor_count_ = 0;

  binding_first_descriptor_index_table_.clear();
  binding_first_dynamic_offset_index_table_.clear();

  uint32_t binding_first_descriptor_index = 0;
  uint32_t binding_first_dynamic_offset_index = 0;

  for (auto const& binding : set_layout.bindings) {
    descriptor_count_ += binding.descriptorCount;

    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
      binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
      binding_first_dynamic_offset_index_table_.emplace_back(binding_first_dynamic_offset_index);
      binding_first_dynamic_offset_index += binding.descriptorCount;
    } else {
      // Not a dynamic descriptor -- unused slot.
      binding_first_dynamic_offset_index_table_.emplace_back(0xFFFFFFFF);
    }

    if (binding.descriptorCount > 0) {
      binding_first_descriptor_index_table_.emplace_back(binding_first_descriptor_index);
      binding_first_descriptor_index += binding.descriptorCount;
    } else {
      // Empty binding -- unused slot.
      binding_first_descriptor_index_table_.emplace_back(0xFFFFFFFF);
    }
  }

  dense_descriptor_write_descs_.resize(binding_first_descriptor_index);
  dense_descriptor_hashed_states_.resize(binding_first_descriptor_index);
  dense_descriptor_dynamic_offsets_.resize(binding_first_dynamic_offset_index);

  hash_dirty_ = true;
}

DescriptorSetWriteDesc::ReallocationNeeded DescriptorSetWriteDesc::SetBufferDescriptorValue(
  uint32_t binding, uint32_t array_element,
  VkDescriptorType descriptor_type,
  uint64_t handle_id,
  VkBuffer vk_buffer, VkDeviceSize offset, VkDeviceSize range
) {
  uint32_t const descriptor_index = this->GetBindingDescriptorIndex(binding, array_element);

  DescriptorWriteDesc& write_desc = this->GetDescriptorWriteDesc(descriptor_index);
  write_desc.binding = binding;
  write_desc.array_element = array_element;
  write_desc.descriptor_type = descriptor_type;
  write_desc.value.buffer = { vk_buffer, offset, range };

  DescriptorHashedState const hashed_state {
    .value_0 = handle_id,
    .value_1 = static_cast<uint64_t>(offset),
    .value_2 = static_cast<uint64_t>(range),
  };
  return this->SetHashedState(descriptor_index, hashed_state);
}

size_t DescriptorSetWriteDesc::ComputeHash() {
  if (std::exchange(hash_dirty_, false)) {
    mbase::HasherSizeT hasher;
    hasher.DoBytes(dense_descriptor_hashed_states_.data(), dense_descriptor_hashed_states_.size_in_bytes());
    cached_hash_ = hasher.Finish();
  }
  return cached_hash_;
}

bool operator==(DescriptorSetWriteDesc const& lhs, DescriptorSetWriteDesc const& rhs) {
  // We rely on DescriptorHashedState being a memcmp-able POD type.
  return
    lhs.dense_descriptor_hashed_states_.size_in_bytes() == rhs.dense_descriptor_hashed_states_.size_in_bytes() &&
    memcmp(
      lhs.dense_descriptor_hashed_states_.data(),
      rhs.dense_descriptor_hashed_states_.data(),
      lhs.dense_descriptor_hashed_states_.size_in_bytes()
    ) == 0;
}

uint32_t DescriptorSetWriteDesc::GetBindingDescriptorIndex(uint32_t binding, uint32_t array_element) const {
  return binding_first_descriptor_index_table_[binding] + array_element;
}

uint32_t DescriptorSetWriteDesc::GetBindingDynamicOffsetIndex(uint32_t binding, uint32_t array_element) const {
  return binding_first_dynamic_offset_index_table_[binding] + array_element;
}

DescriptorWriteDesc& DescriptorSetWriteDesc::GetDescriptorWriteDesc(uint32_t descriptor_index) {
  return dense_descriptor_write_descs_[descriptor_index];
}

DescriptorSetWriteDesc::ReallocationNeeded DescriptorSetWriteDesc::SetHashedState(uint32_t descriptor_index, DescriptorHashedState const& value) {
  // We rely on DescriptorHashedState being a memcmp-able POD type.
  DescriptorHashedState& element = dense_descriptor_hashed_states_[descriptor_index];
  if (memcmp(&element, &value, sizeof(DescriptorHashedState)) != 0) {
    element = value;
    hash_dirty_ = true;
    return ReallocationNeeded(true);
  }
  return ReallocationNeeded(false);
}

DescriptorSetWriteDesc::RebindingNeeded DescriptorSetWriteDesc::SetDynamicOffset(uint32_t binding, uint32_t array_element, uint32_t value) {
  uint32_t& element = dense_descriptor_dynamic_offsets_[this->GetBindingDynamicOffsetIndex(binding, array_element)];
  return RebindingNeeded(std::exchange(element, value) != value);
}

} // namespace mnexus_backend::vulkan
