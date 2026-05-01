// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-command_list.h"

// c++ headers ------------------------------------------
#include <array>
#include <string>
#include <vector>

// public project headers -------------------------------
#include "mbase/public/log.h"

#include "mnexus/public/render_state_event_log.h"

// project headers --------------------------------------
#include "impl/impl_macros.h"

#include "pipeline/render_pipeline_state_tracker.h"

#include "backend-vulkan/backend-vulkan-render_pipeline.h"
#include "backend-vulkan/command/command_encoder.h"
#include "backend-vulkan/command/image_layout_tracker.h"
#include "backend-vulkan/object/vk-object-command_pool.h"
#include "backend-vulkan/object/vk-object-render_pipeline.h"

#include "backend-vulkan/device/vk-device.h"
#include "backend-vulkan/resource/resource_storage.h"
#include "backend-vulkan/resource/types_bridge.h"

namespace mnexus_backend::vulkan {

namespace {

VkCommandPool CreateCommandPool(IVulkanDevice* vk_device) {
  uint32_t const queue_family_index = vk_device->queue_selection().present_capable.queue_family_index;
  VkCommandPoolCreateInfo pool_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
    .queueFamilyIndex = queue_family_index,
  };
  VkCommandPool vk_pool = VK_NULL_HANDLE;
  VkResult const result = vkCreateCommandPool(vk_device->handle(), &pool_info, nullptr, &vk_pool);
  if (result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkCreateCommandPool failed: {}", string_VkResult(result));
  }
  return vk_pool;
}

VkCommandBuffer AllocateAndBeginCommandBuffer(IVulkanDevice* vk_device, VkCommandPool vk_pool) {
  VkCommandBufferAllocateInfo alloc_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,
    .commandPool = vk_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  VkCommandBuffer vk_cb = VK_NULL_HANDLE;
  VkResult const alloc_result = vkAllocateCommandBuffers(vk_device->handle(), &alloc_info, &vk_cb);
  if (alloc_result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkAllocateCommandBuffers failed: {}", string_VkResult(alloc_result));
    return VK_NULL_HANDLE;
  }

  VkCommandBufferBeginInfo begin_info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = nullptr,
  };
  VkResult const begin_result = vkBeginCommandBuffer(vk_cb, &begin_info);
  if (begin_result != VK_SUCCESS) {
    MBASE_LOG_ERROR("vkBeginCommandBuffer failed: {}", string_VkResult(begin_result));
  }
  return vk_cb;
}

VulkanCommandPool MakeVulkanCommandPool(IVulkanDevice* vk_device, VkCommandPool vk_pool) {
  VkDevice const vk_device_handle = vk_device->handle();
  return VulkanCommandPool(
    vk_pool,
    [vk_device_handle, vk_pool] { vkDestroyCommandPool(vk_device_handle, vk_pool, nullptr); },
    vk_device->GetDeferredDestroyer()
  );
}

class MnexusCommandListVulkan final : public IMnexusCommandListVulkan {
public:
  MnexusCommandListVulkan(
    IVulkanDevice* vk_device,
    IDescriptorSetAllocator* ds_allocator,
    ResourceStorage* resource_storage
  ) :
    vk_device_(vk_device),
    vk_command_pool_(MakeVulkanCommandPool(vk_device, CreateCommandPool(vk_device))),
    encoder_(ICommandEncoder::Create(CommandEncoderDesc {
      .vk_cb_handle = AllocateAndBeginCommandBuffer(vk_device, vk_command_pool_.handle()),
      .vk_device = vk_device,
      .ds_allocator = ds_allocator,
      .resource_storage = resource_storage,
    })),
    resource_storage_(resource_storage)
  {}
  ~MnexusCommandListVulkan() override {
    if (encoder_ != nullptr) {
      encoder_->Shutdown();
      encoder_ = nullptr;
    }
  }
  MBASE_DISALLOW_COPY_MOVE(MnexusCommandListVulkan);

  void Shutdown() override {
    delete this;
  }

  ICommandEncoder& encoder() override { return *encoder_; }
  VulkanCommandPool& vk_command_pool() override { return vk_command_pool_; }
  mbase::ArrayProxy<resource_pool::ResourceHandle const> GetReferencedResources() const override {
    return referenced_resources_;
  }

  // --------------------------------------------------------------------------------------------------
  // mnexus::ICommandList implementation
  //

  MNEXUS_NO_THROW void MNEXUS_CALL End() override {
    // Transition all tracked images back to their default layouts before finalizing.
    image_layout_tracker_.TransitionAllToDefaults();
    image_layout_tracker_.FlushPendingTransitions(pending_pipeline_barrier_);
    pending_pipeline_barrier_.FlushAndClear(encoder_->vk_cb_handle());

    encoder_->End();
  }

  //
  // Diagnostics
  //

  MNEXUS_NO_THROW mnexus::RenderStateEventLog& MNEXUS_CALL GetStateEventLog() override {
    return render_state_event_log_;
  }

  //
  // Debug Markers
  //

  MNEXUS_NO_THROW void MNEXUS_CALL PushDebugGroup(
    mnexus::container::ArrayProxy<char const> name, float const* color
  ) override {
    if (vkCmdBeginDebugUtilsLabelEXT != nullptr) {
      // `VkDebugUtilsLabelEXT` expects a null-terminated string, but `name` may not be null-terminated. Create a temporary null-terminated string for this call.
      std::string name_str(name.data(), name.size());

      VkDebugUtilsLabelEXT label_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = nullptr,
        .pLabelName = name_str.c_str(),
        .color = { 0.f, 0.f, 0.f, 0.f }
      };

      std::array<float, 4> default_color = { 0.f, 0.f, 0.f, 1.f };
      if (color != nullptr) {
        std::copy_n(color, 4, label_info.color);
      } else {
        std::copy_n(default_color.data(), 4, label_info.color);
      }

      vkCmdBeginDebugUtilsLabelEXT(encoder_->vk_cb_handle(), &label_info);
    }
  }

  MNEXUS_NO_THROW void MNEXUS_CALL PopDebugGroup() override {
    if (vkCmdEndDebugUtilsLabelEXT != nullptr) {
      vkCmdEndDebugUtilsLabelEXT(encoder_->vk_cb_handle());
    }
  }

  //
  // Transfer
  //

  MNEXUS_NO_THROW void MNEXUS_CALL ClearTexture(
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ClearValue const& clear_value
  ) override {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    VulkanImage const& vk_image = hot.GetVkImage();
    VkImage const vk_image_handle = vk_image.handle();
    mnexus::TextureDesc const& desc = cold.GetTextureDesc();
    VkFormat const vk_format = ToVkFormat(desc.format);

    // Register and transition target subresources to TRANSFER_DST.
    image_layout_tracker_.RegisterImage(
      vk_image_handle,
      ToVkImageUsageFlags(desc.usage, vk_format),
      vk_format,
      desc.mip_level_count,
      desc.array_layer_count
    );

    image_layout_tracker_.TransitionRangeToTransferDst(
      vk_image_handle,
      { .mip_level = subresource_range.base_mip_level, .array_layer = subresource_range.base_array_layer },
      subresource_range.mip_level_count,
      subresource_range.array_layer_count
    );

    image_layout_tracker_.FlushPendingTransitions(pending_pipeline_barrier_);
    pending_pipeline_barrier_.FlushAndClear(encoder_->vk_cb_handle());

    encoder_->CmdClearImageSubresourceRange(vk_image, subresource_range, clear_value);

    referenced_resources_.push_back(pool_handle);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL CopyBufferToTexture(
    mnexus::BufferHandle src_buffer_handle,
    uint32_t src_buffer_offset,
    mnexus::TextureHandle dst_texture_handle,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Extent3d const& copy_extent
  ) override {
    MBASE_ASSERT(dst_subresource_range.mip_level_count == 1);

    // Resolve source buffer.
    auto const src_pool_handle = resource_pool::ResourceHandle::FromU64(src_buffer_handle.Get());
    auto [src_hot, src_lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(src_pool_handle);

    // Resolve destination texture.
    auto const dst_pool_handle = resource_pool::ResourceHandle::FromU64(dst_texture_handle.Get());
    auto [dst_hot, dst_cold, dst_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(dst_pool_handle);

    VulkanImage const& vk_image = dst_hot.GetVkImage();
    VkImage const vk_image_handle = vk_image.handle();
    mnexus::TextureDesc const& dst_desc = dst_cold.GetTextureDesc();
    VkFormat const vk_format = ToVkFormat(dst_desc.format);

    // Register the image and transition the target subresource to TRANSFER_DST_OPTIMAL.
    image_layout_tracker_.RegisterImage(
      vk_image_handle,
      ToVkImageUsageFlags(dst_desc.usage, vk_format),
      vk_format,
      dst_desc.mip_level_count,
      dst_desc.array_layer_count
    );

    image_layout_tracker_.TransitionRangeToTransferDst(
      vk_image_handle,
      { .mip_level = dst_subresource_range.base_mip_level, .array_layer = dst_subresource_range.base_array_layer },
      dst_subresource_range.mip_level_count,
      dst_subresource_range.array_layer_count
    );

    // Flush the layout transition barrier before the copy.
    image_layout_tracker_.FlushPendingTransitions(pending_pipeline_barrier_);
    pending_pipeline_barrier_.FlushAndClear(encoder_->vk_cb_handle());

    encoder_->CmdCopyBufferToImageSubresource(
      src_hot.vk_buffer,
      src_buffer_offset,
      dst_hot.GetVkImage(),
      dst_subresource_range,
      copy_extent
    );

    // Track referenced resources for submit-time stamping.
    referenced_resources_.push_back(src_pool_handle);
    referenced_resources_.push_back(dst_pool_handle);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL CopyTextureToBuffer(
    mnexus::TextureHandle /*src_texture_handle*/,
    mnexus::TextureSubresourceRange const& /*src_subresource_range*/,
    mnexus::BufferHandle /*dst_buffer_handle*/,
    uint32_t /*dst_buffer_offset*/,
    mnexus::Extent3d const& /*copy_extent*/
  ) override {
    STUB_NOT_IMPLEMENTED();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL BlitTexture(
    mnexus::TextureHandle /*src_texture_handle*/,
    mnexus::TextureSubresourceRange const& /*src_subresource_range*/,
    mnexus::Offset3d const& /*src_offset*/,
    mnexus::Extent3d const& /*src_extent*/,
    mnexus::TextureHandle /*dst_texture_handle*/,
    mnexus::TextureSubresourceRange const& /*dst_subresource_range*/,
    mnexus::Offset3d const& /*dst_offset*/,
    mnexus::Extent3d const& /*dst_extent*/,
    mnexus::Filter /*filter*/
  ) override {
    STUB_NOT_IMPLEMENTED();
  }

  //
  // Compute
  //

  MNEXUS_NO_THROW void MNEXUS_CALL BindExplicitComputePipeline(
    mnexus::ComputePipelineHandle compute_pipeline_handle
  ) override {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(compute_pipeline_handle.Get());
    auto [hot, cold, lock] = resource_storage_->compute_pipelines.GetConstRefWithSharedLockGuard(pool_handle);

    VulkanPipelineLayoutPtr const& pipeline_layout_ref = hot.pipeline_layout_ref();

    encoder_->BindComputePipeline(
      hot.vk_compute_pipeline().handle(),
      pipeline_layout_ref->handle(),
      pipeline_layout_ref->descriptor_set_layouts.data(),
      static_cast<uint32_t>(pipeline_layout_ref->descriptor_set_layouts.size())
    );

    // Track referenced resources for submit-time stamping.
    referenced_resources_.push_back(pool_handle);
    referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(cold.program_handle().Get()));
    referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(cold.shader_module_handle().Get()));
  }

  MNEXUS_NO_THROW void MNEXUS_CALL DispatchCompute(
    uint32_t workgroup_count_x,
    uint32_t workgroup_count_y,
    uint32_t workgroup_count_z
  ) override {
    encoder_->CmdDispatchCompute(workgroup_count_x, workgroup_count_y, workgroup_count_z);
  }

  //
  // Resource Binding
  //

  MNEXUS_NO_THROW void MNEXUS_CALL BindUniformBuffer(
    mnexus::BindingId const& id,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  ) override {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);
    encoder_->BindBuffer(
      id.group, id.binding, id.array_element,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      buffer_handle.Get(), hot.vk_buffer.handle(), offset, size
    );
    referenced_resources_.push_back(pool_handle);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL BindStorageBuffer(
    mnexus::BindingId const& id,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  ) override {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);
    encoder_->BindBuffer(
      id.group, id.binding, id.array_element,
      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      buffer_handle.Get(), hot.vk_buffer.handle(), offset, size
    );
    referenced_resources_.push_back(pool_handle);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL BindSampledTexture(
    mnexus::BindingId const& /*id*/,
    mnexus::TextureHandle /*texture_handle*/,
    mnexus::TextureSubresourceRange const& /*subresource_range*/
  ) override {
    STUB_NOT_IMPLEMENTED();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL BindSampler(
    mnexus::BindingId const& /*id*/,
    mnexus::SamplerHandle /*sampler_handle*/
  ) override {
    STUB_NOT_IMPLEMENTED();
  }

  //
  // Explicit Pipeline Binding
  //

  MNEXUS_NO_THROW void MNEXUS_CALL BindExplicitRenderPipeline(
    mnexus::RenderPipelineHandle /*render_pipeline_handle*/
  ) override {
    STUB_NOT_IMPLEMENTED();
  }

  //
  // Render Pass
  //

  MNEXUS_NO_THROW void MNEXUS_CALL BeginRenderPass(
    mnexus::RenderPassDesc const& desc
  ) override {
    DynamicRenderPassDesc dyn_rp_desc{};

    mbase::SmallVector<mnexus::Format, 4> color_formats;
    mnexus::Format depth_stencil_format = mnexus::Format::kUndefined;
    VkExtent2D render_area { 0, 0 };

    for (mnexus::ColorAttachmentDesc const& attachment_desc : desc.color_attachments) {
      auto const pool_handle = resource_pool::ResourceHandle::FromU64(attachment_desc.texture.Get());
      referenced_resources_.push_back(pool_handle);

      auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

      VulkanImage const& vk_image = hot.GetVkImage();
      VkExtent3D const& extent = vk_image.extent();
      render_area.width  = std::max(render_area.width, extent.width);
      render_area.height = std::max(render_area.height, extent.height);

      color_formats.emplace_back(FromVkFormat(vk_image.vk_format()));

      RenderTargetDesc& rt_desc = dyn_rp_desc.color_attachments.emplace_back();
      rt_desc.vk_image = &vk_image;
      rt_desc.subresource_range = attachment_desc.subresource_range;
      if (attachment_desc.load_op == mnexus::LoadOp::kClear) {
        rt_desc.clear_value = RenderTargetClearValue {
          .f32 = {
            attachment_desc.clear_value.color.r,
            attachment_desc.clear_value.color.g,
            attachment_desc.clear_value.color.b,
            attachment_desc.clear_value.color.a,
          },
        };
      }
    }

    if (desc.depth_stencil_attachment != nullptr && desc.depth_stencil_attachment->texture.IsValid()) {
      auto const pool_handle = resource_pool::ResourceHandle::FromU64(desc.depth_stencil_attachment->texture.Get());
      referenced_resources_.push_back(pool_handle);

      auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

      VulkanImage const& vk_image = hot.GetVkImage();
      VkExtent3D const& extent = vk_image.extent();
      render_area.width  = std::max(render_area.width, extent.width);
      render_area.height = std::max(render_area.height, extent.height);

      depth_stencil_format = FromVkFormat(vk_image.vk_format());

      RenderTargetDesc& rt_desc = dyn_rp_desc.depth_stencil_attachment.emplace();
      rt_desc.vk_image = &vk_image;
      rt_desc.subresource_range = desc.depth_stencil_attachment->subresource_range;
      if (desc.depth_stencil_attachment->depth_load_op == mnexus::LoadOp::kClear) {
        rt_desc.clear_value = RenderTargetClearValue {
          .f32 = {
            desc.depth_stencil_attachment->depth_clear_value,
          },
        };
      }
      if (desc.depth_stencil_attachment->stencil_load_op == mnexus::LoadOp::kClear) {
        rt_desc.stencil_clear_value = RenderTargetClearValue {
            .u32 = {
              desc.depth_stencil_attachment->stencil_clear_value,
            },
        };
      }
    }

    encoder_->CmdBeginRendering(dyn_rp_desc);

    // Configure state tracker with render target info; the next Draw will
    // build the cache key using this.
    render_pipeline_state_tracker_.SetRenderTargetConfig(
      std::move(color_formats),
      depth_stencil_format,
      1 // sample_count (always 1 for now)
    );

    // Push the default viewport / scissor matching the render area. Callers
    // can overwrite later via SetViewport / SetScissor before Draw.
    encoder_->CmdSetViewport(VkViewport {
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(render_area.width),
      .height = static_cast<float>(render_area.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
    });
    encoder_->CmdSetScissor(VkRect2D {
      .offset = VkOffset2D { .x = 0, .y = 0 },
      .extent = render_area,
    });
  }

  MNEXUS_NO_THROW void MNEXUS_CALL EndRenderPass() override {
    encoder_->CmdEndRendering();
  }

  //
  // Render State (auto-generation path)
  //

  MNEXUS_NO_THROW void MNEXUS_CALL BindRenderProgram(
    mnexus::ProgramHandle program_handle
  ) override {
    referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(program_handle.Get()));
    render_pipeline_state_tracker_.SetProgram(program_handle);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetVertexInputLayout(
    mnexus::container::ArrayProxy<mnexus::VertexInputBindingDesc const> bindings,
    mnexus::container::ArrayProxy<mnexus::VertexInputAttributeDesc const> attributes
  ) override {
    mbase::SmallVector<mnexus::VertexInputBindingDesc, 4> bindings_vec;
    bindings_vec.reserve(bindings.size());
    for (uint32_t i = 0; i < bindings.size(); ++i) {
      bindings_vec.emplace_back(bindings[i]);
    }

    mbase::SmallVector<mnexus::VertexInputAttributeDesc, 8> attributes_vec;
    attributes_vec.reserve(attributes.size());
    for (uint32_t i = 0; i < attributes.size(); ++i) {
      attributes_vec.emplace_back(attributes[i]);
    }

    render_pipeline_state_tracker_.SetVertexInputLayout(
      std::move(bindings_vec),
      std::move(attributes_vec)
    );
  }

  MNEXUS_NO_THROW void MNEXUS_CALL BindVertexBuffer(
    uint32_t binding,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset
  ) override {
    if (binding >= bound_vertex_buffers_.size()) {
      bound_vertex_buffers_.resize(binding + 1);
    }
    bound_vertex_buffers_[binding] = BoundVertexBuffer {
      .handle = buffer_handle,
      .offset = offset,
    };
    referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(buffer_handle.Get()));
  }

  MNEXUS_NO_THROW void MNEXUS_CALL BindIndexBuffer(
    mnexus::BufferHandle /*buffer_handle*/,
    uint64_t /*offset*/,
    mnexus::IndexType /*index_type*/
  ) override {
    STUB_NOT_IMPLEMENTED();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetPrimitiveTopology(
    mnexus::PrimitiveTopology topology
  ) override {
    render_pipeline_state_tracker_.SetPrimitiveTopology(topology);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetPolygonMode(
    mnexus::PolygonMode mode
  ) override {
    render_pipeline_state_tracker_.SetPolygonMode(mode);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetCullMode(
    mnexus::CullMode cull_mode
  ) override {
    render_pipeline_state_tracker_.SetCullMode(cull_mode);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetFrontFace(
    mnexus::FrontFace front_face
  ) override {
    render_pipeline_state_tracker_.SetFrontFace(front_face);
  }

  // Depth

  MNEXUS_NO_THROW void MNEXUS_CALL SetDepthTestEnabled(bool enabled) override {
    render_pipeline_state_tracker_.SetDepthTestEnabled(enabled);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetDepthWriteEnabled(bool enabled) override {
    render_pipeline_state_tracker_.SetDepthWriteEnabled(enabled);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetDepthCompareOp(
    mnexus::CompareOp op
  ) override {
    render_pipeline_state_tracker_.SetDepthCompareOp(op);
  }

  // Stencil

  MNEXUS_NO_THROW void MNEXUS_CALL SetStencilTestEnabled(bool enabled) override {
    render_pipeline_state_tracker_.SetStencilTestEnabled(enabled);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetStencilFrontOps(
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  ) override {
    render_pipeline_state_tracker_.SetStencilFrontOps(fail, pass, depth_fail, compare);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetStencilBackOps(
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  ) override {
    render_pipeline_state_tracker_.SetStencilBackOps(fail, pass, depth_fail, compare);
  }

  // Per-attachment blend

  MNEXUS_NO_THROW void MNEXUS_CALL SetBlendEnabled(
    uint32_t attachment, bool enabled
  ) override {
    render_pipeline_state_tracker_.SetBlendEnabled(attachment, enabled);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetBlendFactors(
    uint32_t attachment,
    mnexus::BlendFactor src_color, mnexus::BlendFactor dst_color, mnexus::BlendOp color_op,
    mnexus::BlendFactor src_alpha, mnexus::BlendFactor dst_alpha, mnexus::BlendOp alpha_op
  ) override {
    render_pipeline_state_tracker_.SetBlendFactors(
      attachment, src_color, dst_color, color_op,
      src_alpha, dst_alpha, alpha_op
    );
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetColorWriteMask(
    uint32_t attachment, mnexus::ColorWriteMask mask
  ) override {
    render_pipeline_state_tracker_.SetColorWriteMask(attachment, mask);
  }

  //
  // Draw
  //

  MNEXUS_NO_THROW void MNEXUS_CALL Draw(
    uint32_t vertex_count, uint32_t instance_count,
    uint32_t first_vertex, uint32_t first_instance
  ) override {
    this->FlushRenderPipeline();
    this->FlushVertexBuffers();
    encoder_->CmdDraw(vertex_count, instance_count, first_vertex, first_instance);
  }

  MNEXUS_NO_THROW void MNEXUS_CALL DrawIndexed(
    uint32_t /*index_count*/, uint32_t /*instance_count*/,
    uint32_t /*first_index*/, int32_t /*vertex_offset*/, uint32_t /*first_instance*/
  ) override {
    STUB_NOT_IMPLEMENTED();
  }

  //
  // Viewport / Scissor
  //

  MNEXUS_NO_THROW void MNEXUS_CALL SetViewport(
    float /*x*/, float /*y*/, float /*width*/, float /*height*/,
    float /*min_depth*/, float /*max_depth*/
  ) override {
    STUB_NOT_IMPLEMENTED();
  }

  MNEXUS_NO_THROW void MNEXUS_CALL SetScissor(
    int32_t /*x*/, int32_t /*y*/, uint32_t /*width*/, uint32_t /*height*/
  ) override {
    STUB_NOT_IMPLEMENTED();
  }

private:
  struct BoundVertexBuffer {
    mnexus::BufferHandle handle = mnexus::BufferHandle::Invalid();
    uint64_t offset = 0;
  };

  /// Resolves the dirty render pipeline state to a VkPipeline (via cache),
  /// then binds it on the encoder. No-op if not dirty.
  void FlushRenderPipeline() {
    if (!render_pipeline_state_tracker_.IsDirty()) {
      return;
    }
    pipeline::RenderPipelineCacheKey key = render_pipeline_state_tracker_.BuildCacheKey();
    render_pipeline_state_tracker_.MarkClean();

    bool cache_hit = false;
    VulkanRenderPipelinePtr pipeline = resource_storage_->render_pipeline_cache.FindOrInsert(
      key,
      [this](pipeline::RenderPipelineCacheKey const& k) -> VulkanRenderPipelinePtr {
        return CreateVulkanRenderPipelineFromCacheKey(
          *vk_device_,
          k,
          resource_storage_->programs,
          resource_storage_->shader_modules
        );
      },
      &cache_hit
    );
    if (pipeline == nullptr) {
      MBASE_LOG_ERROR("FlushRenderPipeline: failed to acquire VkPipeline");
      return;
    }

    // Look up the program's pipeline layout (lives next to the program in the pool).
    auto const program_pool_handle = resource_pool::ResourceHandle::FromU64(key.program.Get());
    auto [program_hot, program_cold, program_lock] =
      resource_storage_->programs.GetConstRefWithSharedLockGuard(program_pool_handle);

    VulkanPipelineLayoutPtr const& pipeline_layout_ref = program_hot.pipeline_layout_ref;

    encoder_->BindRenderPipeline(
      pipeline->handle(),
      pipeline_layout_ref->handle(),
      pipeline_layout_ref->descriptor_set_layouts.data(),
      static_cast<uint32_t>(pipeline_layout_ref->descriptor_set_layouts.size())
    );
  }

  /// Pushes all currently-bound vertex buffers to the encoder.
  void FlushVertexBuffers() {
    for (uint32_t i = 0; i < bound_vertex_buffers_.size(); ++i) {
      BoundVertexBuffer const& bvb = bound_vertex_buffers_[i];
      if (!bvb.handle.IsValid()) {
        continue;
      }
      auto const buffer_pool_handle = resource_pool::ResourceHandle::FromU64(bvb.handle.Get());
      auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(buffer_pool_handle);
      encoder_->CmdBindVertexBuffer(i, hot.vk_buffer.handle(), bvb.offset);
    }
  }

  IVulkanDevice* vk_device_ = nullptr;
  VulkanCommandPool vk_command_pool_;
  ICommandEncoder* encoder_ = nullptr;
  ResourceStorage* resource_storage_ = nullptr;
  std::vector<resource_pool::ResourceHandle> referenced_resources_;
  ImageLayoutTracker image_layout_tracker_;
  PendingPipelineBarrier pending_pipeline_barrier_;
  mnexus::RenderStateEventLog render_state_event_log_;

  pipeline::RenderPipelineStateTracker render_pipeline_state_tracker_;
  mbase::SmallVector<BoundVertexBuffer, 4> bound_vertex_buffers_;
};

} // namespace

// ----------------------------------------------------------------------------------------------------
// IMnexusCommandListVulkan::Create
//

IMnexusCommandListVulkan* IMnexusCommandListVulkan::Create(
  IVulkanDevice* vk_device,
  IDescriptorSetAllocator* ds_allocator,
  ResourceStorage* resource_storage
) {
  return new MnexusCommandListVulkan(vk_device, ds_allocator, resource_storage);
}

} // namespace mnexus_backend::vulkan
