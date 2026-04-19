#pragma once

// c++ headers ------------------------------------------
#include <cstdint>
#include <cstring>

#include <span>

// public project headers -------------------------------
#include "mbase/public/access.h"
#include "mbase/public/container.h"
#include "mbase/public/type_safety.h"
#include "mbase/public/type_util.h"

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

namespace mnexus_backend::vulkan {

class VulkanDescriptorSetLayout;

// ----------------------------------------------------------------------------------------------------
// Descriptor write types
//

struct DescriptorBufferValue final {
  VkBuffer buffer;
  VkDeviceSize offset;
  VkDeviceSize range;
};

struct DescriptorWriteDesc final {
  uint32_t binding;
  uint32_t array_element;
  VkDescriptorType descriptor_type;
  union {
    DescriptorBufferValue buffer;
    // TODO: DescriptorCombinedImageSamplerValue combined_image_sampler;
  } value;
};

// For hash (cache key) -- parallel array with DescriptorWriteDesc.
// Trivially constructible + no padding for reliable memcmp/hash.
struct DescriptorHashedState final {
  uint64_t value_0; // handle_id (BufferHandle.Get(), etc.)
  uint64_t value_1; // offset (or sampler id)
  uint64_t value_2; // range (or 0)
};

MBASE_STATIC_ASSERT_NO_PADDING(DescriptorHashedState, value_2);
static_assert(sizeof(DescriptorHashedState) == 24);

// ----------------------------------------------------------------------------------------------------
// DescriptorSetWriteDesc
//
// Dense descriptor write state per set, with hash-based change detection.
//

class DescriptorSetWriteDesc final {
public:
  using ReallocationNeeded = mbase::TypesafeBool<struct ReallocationNeededTag>;
  using RebindingNeeded = mbase::TypesafeBool<struct RebindingNeededTag>;

  DescriptorSetWriteDesc() = default;
  ~DescriptorSetWriteDesc() = default;
  MBASE_DEFAULT_COPY_MOVE(DescriptorSetWriteDesc);

  /// Rebuilds binding index tables and resizes dense arrays for the new layout.
  void AssumeDescriptorSetLayout(VulkanDescriptorSetLayout const& set_layout);

  /// Writes a buffer descriptor value (uniform or storage).
  ReallocationNeeded SetBufferDescriptorValue(
    uint32_t binding, uint32_t array_element,
    VkDescriptorType descriptor_type,
    uint64_t handle_id,
    VkBuffer vk_buffer, VkDeviceSize offset, VkDeviceSize range
  );

  // TODO: SetCombinedImageSamplerDescriptorValue
  // TODO: SetDynamicBufferDescriptorValue

  size_t ComputeHash();

  [[nodiscard]] size_t GetCachedHash() const { return cached_hash_; }

  [[nodiscard]] uint32_t GetDescriptorCount() const { return descriptor_count_; }

  [[nodiscard]] std::span<DescriptorWriteDesc const> GetDenseDescriptorWriteDescs() const {
    return dense_descriptor_write_descs_;
  }
  [[nodiscard]] std::span<uint32_t const> GetDenseDescriptorDynamicOffsets() const {
    return dense_descriptor_dynamic_offsets_;
  }

  friend bool operator==(DescriptorSetWriteDesc const&, DescriptorSetWriteDesc const&);

private:
  [[nodiscard]] uint32_t GetBindingDescriptorIndex(uint32_t binding, uint32_t array_element) const;
  [[nodiscard]] uint32_t GetBindingDynamicOffsetIndex(uint32_t binding, uint32_t array_element) const;

  DescriptorWriteDesc& GetDescriptorWriteDesc(uint32_t descriptor_index);

  ReallocationNeeded SetHashedState(uint32_t descriptor_index, DescriptorHashedState const& value);
  RebindingNeeded SetDynamicOffset(uint32_t binding, uint32_t array_element, uint32_t value);

  uint32_t descriptor_count_ = 0;

  mbase::SmallVector<uint32_t, 4> binding_first_descriptor_index_table_;
  mbase::SmallVector<uint32_t, 4> binding_first_dynamic_offset_index_table_;

  mbase::SmallVector<DescriptorWriteDesc, 4> dense_descriptor_write_descs_;
  mbase::SmallVector<DescriptorHashedState, 4> dense_descriptor_hashed_states_;
  mbase::SmallVector<uint32_t, 4> dense_descriptor_dynamic_offsets_;

  size_t mutable cached_hash_ = 0;
  bool mutable hash_dirty_ = true;
};

} // namespace mnexus_backend::vulkan
