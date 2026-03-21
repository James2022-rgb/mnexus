// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-shader.h"

// c++ headers ------------------------------------------
#include <vector>

// project headers --------------------------------------
#include "backend-vulkan/shader_module.h"

#include "pipeline/pipeline_layout_cache_key.h"

namespace mnexus_backend::vulkan {

container::ResourceHandle EmplaceShaderModuleResourcePool(
  ShaderModuleResourcePool& out_pool,
  VulkanDevice const& device,
  mnexus::ShaderModuleDesc const& shader_module_desc
) {
  // Assert that the shader code is SPIR-V; this backend only supports SPIR-V shaders, and we rely on being able to reflect the shader code, which is only really possible with SPIR-V.
  MBASE_ASSERT_MSG(
    shader_module_desc.source_language == mnexus::ShaderSourceLanguage::kSpirV,
    "Only SPIR-V is supported in the Vulkan backend."
  );

  // TODO: Shader Source Language Capability

  VkShaderModule vk_shader_module_handle = CreateVkShaderModule(
    device,
    shader_module_desc
  );

  if (vk_shader_module_handle == VK_NULL_HANDLE) {
    MBASE_LOG_ERROR("Failed to create Vulkan shader module!");
    return container::ResourceHandle::Null();
  }

  std::optional<shader::ShaderModuleReflection> opt_reflection =
    shader::ShaderModuleReflection::CreateFromSpirv(
      reinterpret_cast<uint32_t const*>(shader_module_desc.code_ptr),
      shader_module_desc.code_size_in_bytes / sizeof(uint32_t)
    );

  if (!opt_reflection.has_value()) {
    MBASE_LOG_ERROR("Failed to reflect SPIR-V shader module!");
    return container::ResourceHandle::Null();
  }

  VkDevice vk_device = device.handle();
  ShaderModuleHot hot {
    .vk_shader_module = VulkanShaderModule(
      vk_shader_module_handle,
      [vk_device, vk_shader_module_handle] { vkDestroyShaderModule(vk_device, vk_shader_module_handle, nullptr); },
      device.deferred_destroyer()
    ),
  };
  ShaderModuleCold cold {
    .reflection = std::move(*opt_reflection)
  };

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

container::ResourceHandle EmplaceProgramResourcePool(
  ProgramResourcePool& out_pool,
  VulkanDevice const& device,
  mnexus::ProgramDesc const& program_desc,
  ShaderModuleResourcePool const& shader_module_pool,
  pipeline::TPipelineLayoutCache<VulkanPipelineLayoutPtr>& pipeline_layout_cache
) {
  // Phase 1: Merge bind group layouts from all shader modules.
  shader::MergedPipelineLayout merged_pipeline_layout;

  for (uint32_t shader_module_index = 0; shader_module_index < program_desc.shader_modules.size(); ++shader_module_index) {
    auto const shader_module_pool_handle = container::ResourceHandle::FromU64(
      program_desc.shader_modules[shader_module_index].Get()
    );

    auto [shader_module_cold, lock] = shader_module_pool.GetColdConstRefWithSharedLockGuard(
      shader_module_pool_handle
    );

    if (!merged_pipeline_layout.Merge(shader_module_cold.reflection)) {
      MBASE_LOG_ERROR("Failed to merge bind group layouts for shader module index {}", shader_module_index);
      return container::ResourceHandle::Null();
    }
  }

  mbase::ArrayProxy<shader::BindGroupLayout const> merged_layouts = merged_pipeline_layout.GetBindGroupLayouts();

  // Phase 2: Look up or create the pipeline layout via cache.
  pipeline::PipelineLayoutCacheKey layout_key = pipeline::BuildPipelineLayoutCacheKey(merged_layouts);

  VulkanPipelineLayoutPtr pipeline_layout_ptr = pipeline_layout_cache.FindOrInsert(
    layout_key,
    [&](pipeline::PipelineLayoutCacheKey const&) -> VulkanPipelineLayoutPtr {
      // Convert merged layouts to Vulkan descriptor set layouts.
      mbase::SmallVector<VulkanDescriptorSetLayout, 4> dsls;
      std::vector<VkDescriptorSetLayout> raw_dsls;
      raw_dsls.reserve(merged_layouts.size());

      for (uint32_t set_index = 0; set_index < merged_layouts.size(); ++set_index) {
        shader::BindGroupLayout const& merged_bgl = merged_layouts[set_index];

        mbase::SmallVector<VkDescriptorSetLayoutBinding, 4> vk_bindings;
        vk_bindings.reserve(merged_bgl.entries.size());

        for (uint32_t entry_index = 0; entry_index < merged_bgl.entries.size(); ++entry_index) {
          shader::BindGroupLayoutEntry const& entry = merged_bgl.entries[entry_index];

          VkDescriptorSetLayoutBinding vk_binding {};
          vk_binding.binding = entry.binding;
          vk_binding.descriptorCount = entry.count;
          vk_binding.stageFlags = VK_SHADER_STAGE_ALL;
          vk_binding.pImmutableSamplers = nullptr;

          switch (entry.type) {
            case mnexus::BindGroupLayoutEntryType::kUniformBuffer:
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
              break;
            case mnexus::BindGroupLayoutEntryType::kStorageBuffer:
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
              break;
            case mnexus::BindGroupLayoutEntryType::kSampledTexture:
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
              break;
            case mnexus::BindGroupLayoutEntryType::kSampler:
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
              break;
            case mnexus::BindGroupLayoutEntryType::kStorageTexture:
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
              break;
            case mnexus::BindGroupLayoutEntryType::kCombinedTextureSampler:
              vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
              break;
            default:
              MBASE_LOG_ERROR("Unsupported BindGroupLayoutEntryType: {}", static_cast<uint32_t>(entry.type));
              mbase::Trap();
              break;
          }

          vk_bindings.emplace_back(vk_binding);
        }

        VkDescriptorSetLayoutCreateInfo dsl_create_info {};
        dsl_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dsl_create_info.bindingCount = static_cast<uint32_t>(vk_bindings.size());
        dsl_create_info.pBindings = vk_bindings.data();

        VkDescriptorSetLayout vk_dsl = VK_NULL_HANDLE;
        VkResult result = vkCreateDescriptorSetLayout(device.handle(), &dsl_create_info, nullptr, &vk_dsl);
        if (result != VK_SUCCESS) {
          MBASE_LOG_ERROR("vkCreateDescriptorSetLayout failed: {}", static_cast<int32_t>(result));
          return nullptr;
        }

        VkDevice dev = device.handle();
        dsls.emplace_back(
          VulkanDescriptorSetLayout(
            vk_dsl,
            [dev, vk_dsl] { vkDestroyDescriptorSetLayout(dev, vk_dsl, nullptr); },
            device.deferred_destroyer(),
            vk_bindings
          )
        );
        raw_dsls.push_back(vk_dsl);
      }

      // Create VkPipelineLayout from descriptor set layouts.
      VkPipelineLayoutCreateInfo pl_create_info {};
      pl_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      pl_create_info.setLayoutCount = static_cast<uint32_t>(raw_dsls.size());
      pl_create_info.pSetLayouts = raw_dsls.data();
      pl_create_info.pushConstantRangeCount = 0;
      pl_create_info.pPushConstantRanges = nullptr;

      VkPipelineLayout vk_pl = VK_NULL_HANDLE;
      VkResult result = vkCreatePipelineLayout(device.handle(), &pl_create_info, nullptr, &vk_pl);
      if (result != VK_SUCCESS) {
        MBASE_LOG_ERROR("vkCreatePipelineLayout failed: {}", static_cast<int32_t>(result));
        return nullptr;
      }

      // Construct VulkanPipelineLayout with handle + destroy callback, then attach DSLs.
      VkDevice dev = device.handle();
      auto layout = std::make_shared<VulkanPipelineLayout>(
        vk_pl,
        [dev, vk_pl] { vkDestroyPipelineLayout(dev, vk_pl, nullptr); },
        device.deferred_destroyer()
      );
      layout->descriptor_set_layouts = std::move(dsls);
      return layout;
    }
  );

  if (!pipeline_layout_ptr || !pipeline_layout_ptr->IsValid()) {
    return container::ResourceHandle::Null();
  }

  // Phase 3: Emplace into pool and return handle.
  ProgramHot hot {
    .vk_pipeline_layout = pipeline_layout_ptr->handle(),
    .pipeline_layout_ref = pipeline_layout_ptr,
    .sync_stamp {},
  };

  ProgramCold cold {};
  cold.shader_module_handles.reserve(program_desc.shader_modules.size());
  for (uint32_t i = 0; i < program_desc.shader_modules.size(); ++i) {
    cold.shader_module_handles.emplace_back(program_desc.shader_modules[i]);
  }

  return out_pool.Emplace(
    std::forward_as_tuple(std::move(hot)),
    std::forward_as_tuple(std::move(cold))
  );
}

} // namespace mnexus_backend::vulkan
