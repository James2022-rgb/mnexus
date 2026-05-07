#pragma once

// public project headers -------------------------------
#include "mbase/public/array_proxy.h"

#include "mnexus/public/mnexus.h"

// project headers --------------------------------------
#include "resource_pool/resource_generational_pool.h"

#include "backend-vulkan/command/fwd.h"
#include "backend-vulkan/descriptor/fwd.h"
#include "backend-vulkan/device/fwd.h"
#include "backend-vulkan/object/fwd.h"
#include "backend-vulkan/resource/fwd.h"

namespace mnexus_backend::vulkan {

// ----------------------------------------------------------------------------------------------------
// IMnexusCommandListVulkan
//
// Vulkan-side `mnexus::ICommandList` implementation. Implementation is
// hidden in the .cpp file.
//

class IMnexusCommandListVulkan : public mnexus::ICommandList {
public:
  static IMnexusCommandListVulkan* Create(
    IVulkanDevice* vk_device,
    IDescriptorSetAllocator* ds_allocator,
    ResourceStorage* resource_storage,
    uint32_t queue_family_index
  );

  /// Clean up internal state and delete this object. The owned per-list
  /// `VkCommandPool` is enqueued for deferred destruction.
  virtual void Shutdown() = 0;

  [[nodiscard]] virtual ICommandEncoder& encoder() = 0;
  [[nodiscard]] virtual VulkanCommandPool& vk_command_pool() = 0;
  [[nodiscard]] virtual mbase::ArrayProxy<resource_pool::ResourceHandle const> GetReferencedResources() const = 0;

protected:
  IMnexusCommandListVulkan() = default;
};

} // namespace mnexus_backend::vulkan
