// TU header --------------------------------------------
#include "backend-vulkan/resource/shader_module.h"

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/log.h"

// project headers --------------------------------------
#include "backend-vulkan/device/vk-device.h"

namespace mnexus_backend::vulkan {

VkShaderModule CreateVkShaderModule(
  IVulkanDevice const& device,
  mnexus::ShaderModuleDesc const& shader_module_desc
) {
  MBASE_ASSERT_MSG(
    shader_module_desc.source_language == mnexus::ShaderSourceLanguage::kSpirV,
    "Only SPIR-V is supported in the Vulkan backend."
  );
  MBASE_ASSERT_MSG(shader_module_desc.code_ptr != 0, "Shader code pointer must not be null.");
  MBASE_ASSERT_MSG(shader_module_desc.code_size_in_bytes > 0, "Shader code size must be greater than zero.");
  MBASE_ASSERT_MSG(shader_module_desc.code_size_in_bytes % sizeof(uint32_t) == 0, "SPIR-V code size must be a multiple of 4.");

  VkShaderModuleCreateInfo create_info {};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = shader_module_desc.code_size_in_bytes;
  create_info.pCode = reinterpret_cast<uint32_t const*>(shader_module_desc.code_ptr);

  VkShaderModule shader_module = VK_NULL_HANDLE;
  VkResult const result = vkCreateShaderModule(device.handle(), &create_info, nullptr, &shader_module);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateShaderModule failed: {}", static_cast<int32_t>(result));
    return VK_NULL_HANDLE;
  }

  return shader_module;
}

} // namespace mnexus_backend::vulkan
