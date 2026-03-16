#pragma once

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

// public project headers -------------------------------
#include "mnexus/public/types.h"

namespace mnexus_backend::vulkan {

class VulkanDevice;

/// Create a VkShaderModule from a ShaderModuleDesc.
/// Only SPIR-V is supported.
VkShaderModule CreateVkShaderModule(
  VulkanDevice const& device,
  mnexus::ShaderModuleDesc const& shader_module_desc
);

} // namespace mnexus_backend::vulkan
