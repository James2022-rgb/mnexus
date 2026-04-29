#pragma once

// project headers --------------------------------------
#include "backend-vulkan/depend/vulkan.h"

#include "backend-vulkan/device/fwd.h"

// public project headers -------------------------------
#include "mnexus/public/types.h"

namespace mnexus_backend::vulkan {

/// Create a VkShaderModule from a ShaderModuleDesc.
/// Only SPIR-V is supported.
VkShaderModule CreateVkShaderModule(
  IVulkanDevice const& device,
  mnexus::ShaderModuleDesc const& shader_module_desc
);

} // namespace mnexus_backend::vulkan
