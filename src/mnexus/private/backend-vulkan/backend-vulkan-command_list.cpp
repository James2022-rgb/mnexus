// TU header --------------------------------------------
#include "backend-vulkan/backend-vulkan-command_list.h"

// c++ headers ------------------------------------------
#include <array>
#include <optional>
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

VkCommandPool CreateCommandPool(IVulkanDevice* vk_device, uint32_t queue_family_index) {
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
    ResourceStorage* resource_storage,
    uint32_t queue_family_index
  ) :
    vk_device_(vk_device),
    vk_command_pool_(MakeVulkanCommandPool(vk_device, CreateCommandPool(vk_device, queue_family_index))),
    encoder_(ICommandEncoder::Create(CommandEncoderDesc {
      .vk_cb_handle = AllocateAndBeginCommandBuffer(vk_device, vk_command_pool_.handle()),
      .vk_device = vk_device,
      .ds_allocator = ds_allocator,
      .resource_storage = resource_storage,
    })),
    resource_storage_(resource_storage)
  {
    render_pipeline_state_tracker_.SetEventLog(&render_state_event_log_);
  }
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

  IMPL_VAPI(void, End) {
    // Transition all tracked images back to their default layouts before finalizing.
    image_layout_tracker_.TransitionAllToDefaults();
    this->FlushPipelineBarrier();

    encoder_->End();
  }

  //
  // Diagnostics
  //

  IMPL_VAPI(mnexus::RenderStateEventLog&, GetStateEventLog) {
    return render_state_event_log_;
  }

  //
  // Debug Markers
  //

  IMPL_VAPI(void, PushDebugGroup,
    mnexus::container::ArrayProxy<char const> name, float const* color
  ) {
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

  IMPL_VAPI(void, PopDebugGroup) {
    if (vkCmdEndDebugUtilsLabelEXT != nullptr) {
      vkCmdEndDebugUtilsLabelEXT(encoder_->vk_cb_handle());
    }
  }

  //
  // Pipeline Barriers
  //

  IMPL_VAPI(void, TextureBarrier,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ResourceBarrierStageFlags dst_stage_flags,
    mnexus::ResourceBarrierState dst_state
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    VulkanImage const& vk_image = hot.GetVkImage();
    VkImage const vk_image_handle = vk_image.handle();
    mnexus::TextureDesc const& desc = cold.GetTextureDesc();
    VkFormat const vk_format = ToVkFormat(desc.format);

    image_layout_tracker_.RegisterImage(
      vk_image_handle,
      ToVkImageUsageFlags(desc.usage, vk_format),
      vk_format,
      desc.mip_level_count,
      desc.array_layer_count
    );

    VkPipelineStageFlags2KHR const vk_stage_mask = ToVkPipelineStageFlags2(dst_stage_flags);
    VkAccessFlags2KHR const vk_access_mask = ToVkAccessFlags2(dst_state, dst_stage_flags);
    VkImageLayout const vk_layout = ToVkImageLayout(dst_state);

    for (uint32_t mip = 0; mip < subresource_range.mip_level_count; ++mip) {
      for (uint32_t layer = 0; layer < subresource_range.array_layer_count; ++layer) {
        image_layout_tracker_.Transition(
          vk_image_handle,
          { .mip_level = subresource_range.base_mip_level + mip,
            .array_layer = subresource_range.base_array_layer + layer },
          vk_stage_mask,
          vk_access_mask,
          vk_layout
        );
      }
    }

    referenced_resources_.push_back(pool_handle);
  }

  //
  // Transfer
  //

  IMPL_VAPI(void, ClearTexture,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ClearValue const& clear_value
  ) {
    // The caller is responsible for putting the target subresource into
    // ResourceBarrierState::kTransferDst beforehand via TextureBarrier.
    this->FlushPipelineBarrier();

    auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    encoder_->CmdClearImageSubresourceRange(hot.GetVkImage(), subresource_range, clear_value);

    referenced_resources_.push_back(pool_handle);
  }

  IMPL_VAPI(void, CopyBufferToTexture,
    mnexus::BufferHandle src_buffer_handle,
    uint32_t src_buffer_offset,
    mnexus::TextureHandle dst_texture_handle,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Extent3d const& copy_extent
  ) {
    MBASE_ASSERT(dst_subresource_range.mip_level_count == 1);

    // The caller is responsible for putting the destination subresource
    // into ResourceBarrierState::kTransferDst beforehand via TextureBarrier.
    this->FlushPipelineBarrier();

    auto const src_pool_handle = resource_pool::ResourceHandle::FromU64(src_buffer_handle.Get());
    auto [src_hot, src_lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(src_pool_handle);

    auto const dst_pool_handle = resource_pool::ResourceHandle::FromU64(dst_texture_handle.Get());
    auto [dst_hot, dst_cold, dst_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(dst_pool_handle);

    encoder_->CmdCopyBufferToImageSubresource(
      src_hot.vk_buffer,
      src_buffer_offset,
      dst_hot.GetVkImage(),
      dst_subresource_range,
      copy_extent
    );

    referenced_resources_.push_back(src_pool_handle);
    referenced_resources_.push_back(dst_pool_handle);
  }

  IMPL_VAPI(void, CopyTextureToBuffer,
    mnexus::TextureHandle /*src_texture_handle*/,
    mnexus::TextureSubresourceRange const& /*src_subresource_range*/,
    mnexus::BufferHandle /*dst_buffer_handle*/,
    uint32_t /*dst_buffer_offset*/,
    mnexus::Extent3d const& /*copy_extent*/
  ) {
    STUB_NOT_IMPLEMENTED();
  }

  IMPL_VAPI(void, BlitTexture,
    mnexus::TextureHandle src_texture_handle,
    mnexus::TextureSubresourceRange const& src_subresource_range,
    mnexus::Offset3d const& src_offset,
    mnexus::Extent3d const& src_extent,
    mnexus::TextureHandle dst_texture_handle,
    mnexus::TextureSubresourceRange const& dst_subresource_range,
    mnexus::Offset3d const& dst_offset,
    mnexus::Extent3d const& dst_extent,
    mnexus::Filter filter
  ) {
    // Caller is required to have transitioned src into kTransferSrc and dst
    // into kTransferDst via TextureBarrier.
    this->FlushPipelineBarrier();

    auto const src_pool_handle = resource_pool::ResourceHandle::FromU64(src_texture_handle.Get());
    auto [src_hot, src_cold, src_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(src_pool_handle);
    VkImage const src_vk_image = src_hot.GetVkImage().handle();

    auto const dst_pool_handle = resource_pool::ResourceHandle::FromU64(dst_texture_handle.Get());
    auto [dst_hot, dst_cold, dst_lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(dst_pool_handle);
    VkImage const dst_vk_image = dst_hot.GetVkImage().handle();

    VkImageBlit const region {
      .srcSubresource = ToVkImageSubresourceLayers(src_subresource_range),
      .srcOffsets = {
        VkOffset3D {
          static_cast<int32_t>(src_offset.x),
          static_cast<int32_t>(src_offset.y),
          static_cast<int32_t>(src_offset.z),
        },
        VkOffset3D {
          static_cast<int32_t>(src_offset.x + src_extent.width),
          static_cast<int32_t>(src_offset.y + src_extent.height),
          static_cast<int32_t>(src_offset.z + src_extent.depth),
        },
      },
      .dstSubresource = ToVkImageSubresourceLayers(dst_subresource_range),
      .dstOffsets = {
        VkOffset3D {
          static_cast<int32_t>(dst_offset.x),
          static_cast<int32_t>(dst_offset.y),
          static_cast<int32_t>(dst_offset.z),
        },
        VkOffset3D {
          static_cast<int32_t>(dst_offset.x + dst_extent.width),
          static_cast<int32_t>(dst_offset.y + dst_extent.height),
          static_cast<int32_t>(dst_offset.z + dst_extent.depth),
        },
      },
    };

    vkCmdBlitImage(
      encoder_->vk_cb_handle(),
      src_vk_image,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      dst_vk_image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region,
      ToVkFilter(filter)
    );

    referenced_resources_.push_back(src_pool_handle);
    referenced_resources_.push_back(dst_pool_handle);
  }

  //
  // Compute
  //

  IMPL_VAPI(void, BindExplicitComputePipeline,
    mnexus::ComputePipelineHandle compute_pipeline_handle
  ) {
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

  IMPL_VAPI(void, DispatchCompute,
    uint32_t workgroup_count_x,
    uint32_t workgroup_count_y,
    uint32_t workgroup_count_z
  ) {
    this->FlushPipelineBarrier();
    encoder_->CmdDispatchCompute(workgroup_count_x, workgroup_count_y, workgroup_count_z);
  }

  //
  // Resource Binding
  //

  IMPL_VAPI(void, BindUniformBuffer,
    mnexus::BindingId const& id,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);
    encoder_->BindBuffer(
      id.group, id.binding, id.array_element,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      buffer_handle.Get(), hot.vk_buffer.handle(), offset, size
    );
    referenced_resources_.push_back(pool_handle);
  }

  IMPL_VAPI(void, BindStorageBuffer,
    mnexus::BindingId const& id,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);
    encoder_->BindBuffer(
      id.group, id.binding, id.array_element,
      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      buffer_handle.Get(), hot.vk_buffer.handle(), offset, size
    );
    referenced_resources_.push_back(pool_handle);
  }

  IMPL_VAPI(void, BindSampledTexture,
    mnexus::BindingId const& id,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range
  ) {
    // Pre-condition: the texture's subresource range MUST already be in
    // VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR. machina/folgos-style: this
    // call only writes the descriptor and does not queue a layout
    // transition (pipeline barriers cannot be issued inside a dynamic
    // rendering instance). The default layout for SAMPLED textures is
    // READ_ONLY_OPTIMAL_KHR, so freshly-created textures are fine; for
    // textures that may be in another layout (e.g. just after a copy)
    // the caller is responsible via a future explicit barrier API.
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(texture_handle.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    VulkanImage const& vk_image = hot.GetVkImage();
    VkImage const vk_image_handle = vk_image.handle();
    mnexus::TextureDesc const& desc = cold.GetTextureDesc();
    VkFormat const vk_format = ToVkFormat(desc.format);
    VkImageSubresourceRange const vk_subresource_range = ToVkImageSubresourceRange(subresource_range);

    constexpr VkImageLayout kShaderReadLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;

    // Get/create a VkImageView via the cache.
    auto make_vk_image_view = [this](ImageViewCacheKey const& key) {
      VkImageViewCreateInfo const create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = key.vk_image,
        .viewType = key.view_type,
        .format = key.format,
        .components = VkComponentMapping {
          .r = VK_COMPONENT_SWIZZLE_IDENTITY,
          .g = VK_COMPONENT_SWIZZLE_IDENTITY,
          .b = VK_COMPONENT_SWIZZLE_IDENTITY,
          .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = key.subresource_range,
      };

      VkImageView vk_image_view_handle = VK_NULL_HANDLE;
      VkResult const result = vkCreateImageView(this->vk_device_->handle(), &create_info, nullptr, &vk_image_view_handle);
      MBASE_ASSERT(result == VK_SUCCESS);

      return std::make_shared<VulkanImageView>(
        vk_image_view_handle,
        [vk_device = this->vk_device_, vk_image_view_handle]() {
          vkDestroyImageView(vk_device->handle(), vk_image_view_handle, nullptr);
        },
        this->vk_device_->GetDeferredDestroyer()
      );
    };

    // TODO: Image view type should be derived from the texture's TextureDimension.
    constexpr VkImageViewType kImageViewType = VK_IMAGE_VIEW_TYPE_2D;

    VulkanImageViewPtr vk_image_view = resource_storage_->image_view_cache.FindOrInsert(
      ImageViewCacheKey {
        .vk_image = vk_image_handle,
        .view_type = kImageViewType,
        .format = vk_format,
        .subresource_range = vk_subresource_range,
      },
      // SAFETY: Executed immediately.
      make_vk_image_view
    );

    encoder_->BindSampledImage(
      id.group, id.binding, id.array_element,
      reinterpret_cast<uint64_t>(vk_image_view->handle()),
      vk_image_view->handle(),
      kShaderReadLayout
    );

    referenced_resources_.push_back(pool_handle);
  }

  IMPL_VAPI(void, BindSampler,
    mnexus::BindingId const& id,
    mnexus::SamplerHandle sampler_handle
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(sampler_handle.Get());
    auto [hot, lock] = resource_storage_->samplers.GetHotConstRefWithSharedLockGuard(pool_handle);

    encoder_->BindSampler(
      id.group, id.binding, id.array_element,
      sampler_handle.Get(),
      hot.vk_sampler.handle()
    );

    referenced_resources_.push_back(pool_handle);
  }

  //
  // Explicit Pipeline Binding
  //

  IMPL_VAPI(void, BindExplicitRenderPipeline,
    mnexus::RenderPipelineHandle /*render_pipeline_handle*/
  ) {
    STUB_NOT_IMPLEMENTED();
  }

  //
  // Render Pass
  //

  IMPL_VAPI(void, BeginRenderPass,
    mnexus::RenderPassDesc const& desc
  ) {
    // Flush any pending pipeline barriers before entering the render pass:
    // vkCmdPipelineBarrier2 is not allowed inside a dynamic rendering instance.
    this->FlushPipelineBarrier();

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

    if (render_state_event_log_.IsEnabled()) {
      render_state_event_log_.Record(
        mnexus::RenderStateEventTag::kBeginRenderPass,
        render_pipeline_state_tracker_.BuildSnapshot());
    }
  }

  IMPL_VAPI(void, EndRenderPass) {
    if (render_state_event_log_.IsEnabled()) {
      render_state_event_log_.Record(
        mnexus::RenderStateEventTag::kEndRenderPass,
        render_pipeline_state_tracker_.BuildSnapshot());
    }
    encoder_->CmdEndRendering();
  }

  //
  // Render State (auto-generation path)
  //

  IMPL_VAPI(void, BindRenderProgram,
    mnexus::ProgramHandle program_handle
  ) {
    auto const pool_handle = resource_pool::ResourceHandle::FromU64(program_handle.Get());
    referenced_resources_.push_back(pool_handle);
    render_pipeline_state_tracker_.SetProgram(program_handle);

    // Inform the descriptor-set binder of the program's pipeline layout so
    // that subsequent Bind*Buffer / BindSampledTexture / BindSampler calls
    // can target the correct layout. The actual VkPipeline is not bound
    // until Draw flush time (cache lookup).
    auto [program_hot, program_cold, program_lock] =
      resource_storage_->programs.GetConstRefWithSharedLockGuard(pool_handle);

    VulkanPipelineLayoutPtr const& pipeline_layout_ref = program_hot.pipeline_layout_ref;
    encoder_->AssumeRenderPipelineLayout(
      pipeline_layout_ref->handle(),
      pipeline_layout_ref->descriptor_set_layouts.data(),
      static_cast<uint32_t>(pipeline_layout_ref->descriptor_set_layouts.size())
    );
  }

  IMPL_VAPI(void, SetVertexInputLayout,
    mnexus::container::ArrayProxy<mnexus::VertexInputBindingDesc const> bindings,
    mnexus::container::ArrayProxy<mnexus::VertexInputAttributeDesc const> attributes
  ) {
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

  IMPL_VAPI(void, BindVertexBuffer,
    uint32_t binding,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset
  ) {
    if (binding >= bound_vertex_buffers_.size()) {
      bound_vertex_buffers_.resize(binding + 1);
    }
    bound_vertex_buffers_[binding] = BoundVertexBuffer {
      .handle = buffer_handle,
      .offset = offset,
    };
    referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(buffer_handle.Get()));
  }

  IMPL_VAPI(void, BindIndexBuffer,
    mnexus::BufferHandle buffer_handle,
    uint64_t offset,
    mnexus::IndexType index_type
  ) {
    bound_index_buffer_ = BoundIndexBuffer {
      .handle = buffer_handle,
      .offset = offset,
      .index_type = index_type,
    };
    referenced_resources_.push_back(resource_pool::ResourceHandle::FromU64(buffer_handle.Get()));
  }

  IMPL_VAPI(void, SetPrimitiveTopology,
    mnexus::PrimitiveTopology topology
  ) {
    render_pipeline_state_tracker_.SetPrimitiveTopology(topology);
  }

  IMPL_VAPI(void, SetPolygonMode,
    mnexus::PolygonMode mode
  ) {
    render_pipeline_state_tracker_.SetPolygonMode(mode);
  }

  IMPL_VAPI(void, SetCullMode,
    mnexus::CullMode cull_mode
  ) {
    render_pipeline_state_tracker_.SetCullMode(cull_mode);
  }

  IMPL_VAPI(void, SetFrontFace,
    mnexus::FrontFace front_face
  ) {
    render_pipeline_state_tracker_.SetFrontFace(front_face);
  }

  // Depth

  IMPL_VAPI(void, SetDepthTestEnabled,bool enabled) {
    render_pipeline_state_tracker_.SetDepthTestEnabled(enabled);
  }

  IMPL_VAPI(void, SetDepthWriteEnabled,bool enabled) {
    render_pipeline_state_tracker_.SetDepthWriteEnabled(enabled);
  }

  IMPL_VAPI(void, SetDepthCompareOp,
    mnexus::CompareOp op
  ) {
    render_pipeline_state_tracker_.SetDepthCompareOp(op);
  }

  // Stencil

  IMPL_VAPI(void, SetStencilTestEnabled,bool enabled) {
    render_pipeline_state_tracker_.SetStencilTestEnabled(enabled);
  }

  IMPL_VAPI(void, SetStencilFrontOps,
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  ) {
    render_pipeline_state_tracker_.SetStencilFrontOps(fail, pass, depth_fail, compare);
  }

  IMPL_VAPI(void, SetStencilBackOps,
    mnexus::StencilOp fail, mnexus::StencilOp pass,
    mnexus::StencilOp depth_fail, mnexus::CompareOp compare
  ) {
    render_pipeline_state_tracker_.SetStencilBackOps(fail, pass, depth_fail, compare);
  }

  // Per-attachment blend

  IMPL_VAPI(void, SetBlendEnabled,
    uint32_t attachment, bool enabled
  ) {
    render_pipeline_state_tracker_.SetBlendEnabled(attachment, enabled);
  }

  IMPL_VAPI(void, SetBlendFactors,
    uint32_t attachment,
    mnexus::BlendFactor src_color, mnexus::BlendFactor dst_color, mnexus::BlendOp color_op,
    mnexus::BlendFactor src_alpha, mnexus::BlendFactor dst_alpha, mnexus::BlendOp alpha_op
  ) {
    render_pipeline_state_tracker_.SetBlendFactors(
      attachment, src_color, dst_color, color_op,
      src_alpha, dst_alpha, alpha_op
    );
  }

  IMPL_VAPI(void, SetColorWriteMask,
    uint32_t attachment, mnexus::ColorWriteMask mask
  ) {
    render_pipeline_state_tracker_.SetColorWriteMask(attachment, mask);
  }

  //
  // Draw
  //

  IMPL_VAPI(void, Draw,
    uint32_t vertex_count, uint32_t instance_count,
    uint32_t first_vertex, uint32_t first_instance
  ) {
    this->FlushRenderPipeline();
    this->FlushVertexBuffers();

    if (render_state_event_log_.IsEnabled()) {
      render_state_event_log_.Record(
        mnexus::RenderStateEventTag::kDraw,
        render_pipeline_state_tracker_.BuildSnapshot());
    }

    encoder_->CmdDraw(vertex_count, instance_count, first_vertex, first_instance);
  }

  IMPL_VAPI(void, DrawIndexed,
    uint32_t index_count, uint32_t instance_count,
    uint32_t first_index, int32_t vertex_offset, uint32_t first_instance
  ) {
    this->FlushRenderPipeline();
    this->FlushVertexBuffers();
    this->FlushIndexBuffer();

    if (render_state_event_log_.IsEnabled()) {
      render_state_event_log_.Record(
        mnexus::RenderStateEventTag::kDrawIndexed,
        render_pipeline_state_tracker_.BuildSnapshot());
    }

    encoder_->CmdDrawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
  }

  //
  // Viewport / Scissor
  //

  IMPL_VAPI(void, SetViewport,
    float x, float y, float width, float height,
    float min_depth, float max_depth
  ) {
    encoder_->CmdSetViewport(VkViewport {
      .x = x,
      .y = y,
      .width = width,
      .height = height,
      .minDepth = min_depth,
      .maxDepth = max_depth,
    });
  }

  IMPL_VAPI(void, SetScissor,
    int32_t x, int32_t y, uint32_t width, uint32_t height
  ) {
    encoder_->CmdSetScissor(VkRect2D {
      .offset = VkOffset2D { .x = x, .y = y },
      .extent = VkExtent2D { .width = width, .height = height },
    });
  }

private:
  struct BoundVertexBuffer {
    mnexus::BufferHandle handle = mnexus::BufferHandle::Invalid();
    uint64_t offset = 0;
  };

  struct BoundIndexBuffer {
    mnexus::BufferHandle handle = mnexus::BufferHandle::Invalid();
    uint64_t offset = 0;
    mnexus::IndexType index_type = mnexus::IndexType::kUint16;
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

    if (render_state_event_log_.IsEnabled()) {
      render_state_event_log_.RecordPso(
        render_pipeline_state_tracker_.BuildSnapshot(),
        key.ComputeHash(),
        cache_hit);
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

  /// Emits queued image-layout transitions (TextureBarrier) to the GPU.
  /// Must be called at the start of each op that requires the GPU state to
  /// match what the user has queued via TextureBarrier (i.e. before transfer
  /// ops, dispatch, and BeginRenderPass; never inside a render pass).
  void FlushPipelineBarrier() {
    image_layout_tracker_.FlushPendingTransitions(pending_pipeline_barrier_);
    pending_pipeline_barrier_.FlushAndClear(encoder_->vk_cb_handle());
  }

  /// Pushes the currently-bound index buffer to the encoder, if any.
  void FlushIndexBuffer() {
    if (!bound_index_buffer_.has_value() || !bound_index_buffer_->handle.IsValid()) {
      return;
    }
    auto const buffer_pool_handle = resource_pool::ResourceHandle::FromU64(bound_index_buffer_->handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(buffer_pool_handle);
    encoder_->CmdBindIndexBuffer(
      hot.vk_buffer.handle(),
      bound_index_buffer_->offset,
      ToVkIndexType(bound_index_buffer_->index_type)
    );
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
  std::optional<BoundIndexBuffer> bound_index_buffer_;
};

} // namespace

// ----------------------------------------------------------------------------------------------------
// IMnexusCommandListVulkan::Create
//

IMnexusCommandListVulkan* IMnexusCommandListVulkan::Create(
  IVulkanDevice* vk_device,
  IDescriptorSetAllocator* ds_allocator,
  ResourceStorage* resource_storage,
  uint32_t queue_family_index
) {
  return new MnexusCommandListVulkan(vk_device, ds_allocator, resource_storage, queue_family_index);
}

} // namespace mnexus_backend::vulkan
