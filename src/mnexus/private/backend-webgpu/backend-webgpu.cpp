// TU header --------------------------------------------
#include "backend-webgpu/backend-webgpu.h"

// c++ headers ------------------------------------------
#include <cstring>
#include <mutex>
#include <vector>

// platform detection header ----------------------------
#include "mbase/public/platform.h"

// conditional platform headers -------------------------
#if MBASE_PLATFORM_WINDOWS
# if !defined(NOMINMAX)
#   define NOMINMAX
# endif
# if !defined(WIN32_LEAN_AND_MEAN)
#   define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# elif MBASE_PLATFORM_WEB
# include <emscripten/html5.h>
#endif

// public project headers -------------------------------
#include "mbase/public/log.h"
#include "mbase/public/assert.h"
#include "mbase/public/tsa.h"

// project headers --------------------------------------
#include "container/generational_pool.h"

#include "backend-webgpu/include_dawn.h"
#include "backend-webgpu/types_bridge.h"
#include "backend-webgpu/backend-webgpu-compute_pipeline.h"
#include "backend-webgpu/backend-webgpu-buffer.h"
#include "backend-webgpu/backend-webgpu-layout.h"
#include "backend-webgpu/backend-webgpu-shader.h"
#include "backend-webgpu/backend-webgpu-texture.h"

namespace mnexus_backend::webgpu {

struct ResourceStorage final {
  ShaderModuleResourcePool shader_modules;
  ProgramResourcePool programs;
  ComputePipelineResourcePool compute_pipelines;

  BufferResourcePool buffers;
  TextureResourcePool textures;

  std::mutex swapchain_texture_mutex; // Protects `TextureHot` and `TextureCold`.
  container::ResourceHandle swapchain_texture_handle = container::ResourceHandle::Null(); // Not protected; set only during initialization.
};

#define IMPL_VAPI(ret, func, ...) \
  MNEXUS_NO_THROW ret MNEXUS_CALL func(__VA_ARGS__) override

class MnexusCommandListWebGpu : public mnexus::ICommandList {
public:
  explicit MnexusCommandListWebGpu(
    ResourceStorage* resource_storage,
    wgpu::CommandEncoder wgpu_command_encoder
  ) :
    resource_storage_(resource_storage),
    wgpu_command_encoder_(std::move(wgpu_command_encoder))
  {}
  ~MnexusCommandListWebGpu() override = default;
  MBASE_DISALLOW_COPY_MOVE(MnexusCommandListWebGpu);

  wgpu::CommandEncoder const& GetWgpuCommandEncoder() const {
    return wgpu_command_encoder_;
  }

  // --------------------------------------------------------------------------------------------------
  // mnexus::ICommandList implementation
  //

  IMPL_VAPI(void, End) {
    if (current_compute_pass_.has_value()) {
      current_compute_pass_->End();
      current_compute_pass_ = std::nullopt;
    }
  }

  //
  // Transfer
  //

  IMPL_VAPI(void, ClearTexture,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureSubresourceRange const& subresource_range,
    mnexus::ClearValue const& clear_value
  ) {
    // TODO: We're assuming only color aspect for now.
    constexpr wgpu::TextureAspect kSupportedAspects = wgpu::TextureAspect::All;

    auto pool_handle = container::ResourceHandle::FromU64(texture_handle.Get());
    auto [hot, cold, lock] = resource_storage_->textures.GetConstRefWithSharedLockGuard(pool_handle);

    // Swapchain texture hot handle can be null if not acquired this frame.
    if (!hot.wgpu_texture) {
      return;
    }

    wgpu::TextureFormat const wgpu_texture_format = ToWgpuTextureFormat(cold.desc.format);

    wgpu::TextureViewDescriptor view_desc = MakeWgpuTextureViewDesc(
      wgpu_texture_format,
      wgpu::TextureViewDimension::e2D,
      subresource_range,
      kSupportedAspects
    );

    wgpu::TextureView view = hot.wgpu_texture.CreateView(&view_desc);

    wgpu::RenderPassColorAttachment attachment {
      .view = view,
      .resolveTarget = nullptr,
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = {
        clear_value.color.r,
        clear_value.color.g,
        clear_value.color.b,
        clear_value.color.a,
      },
    };

    wgpu::RenderPassDescriptor pass_desc {
      .colorAttachmentCount = 1,
      .colorAttachments = &attachment,
      .depthStencilAttachment = nullptr,
    };

    wgpu::RenderPassEncoder pass = wgpu_command_encoder_.BeginRenderPass(&pass_desc);
    pass.End();
  }

  //
  // Compute
  //

  IMPL_VAPI(void, BindComputePipeline, mnexus::ComputePipelineHandle compute_pipeline_handle) {
    auto pool_handle = container::ResourceHandle::FromU64(compute_pipeline_handle.Get());
    auto [hot, lock] = resource_storage_->compute_pipelines.GetHotConstRefWithSharedLockGuard(pool_handle);

    if (!current_compute_pass_.has_value()) {
      wgpu::ComputePassEncoder pass = wgpu_command_encoder_.BeginComputePass();
      current_compute_pass_ = std::move(pass);
    }

    current_compute_pass_->SetPipeline(hot.wgpu_compute_pipeline);
  }

  IMPL_VAPI(void, DispatchCompute,
    uint32_t workgroup_count_x,
    uint32_t workgroup_count_y,
    uint32_t workgroup_count_z
  ) {
    MBASE_ASSERT(current_compute_pass_.has_value());
    
    current_compute_pass_->DispatchWorkgroups(workgroup_count_x, workgroup_count_y, workgroup_count_z);
  }

private:
  ResourceStorage* resource_storage_ = nullptr;
  wgpu::CommandEncoder wgpu_command_encoder_;

  std::optional<wgpu::ComputePassEncoder> current_compute_pass_;
};

class MnexusDeviceWebGpu : public mnexus::IDevice {
public:
  // ----------------------------------------------------------------------------------------------------
  // mnexus::IDevice implementation
  //

  // -----------------------------------------------------------------------------------------------
  // Queue
  //

  IMPL_VAPI(uint32_t, QueueGetFamilyCount) {
    // Only one queue family is supported.
    return 1;
  }

  IMPL_VAPI(MnBool32, QueueGetFamilyDesc,
    uint32_t queue_family_index,
    mnexus::QueueFamilyDesc& out_desc
  ) {
    if (queue_family_index != 0) {
      // Only one queue family is supported.
      return MnBoolFalse;
    }

    out_desc = mnexus::QueueFamilyDesc {
      .queue_count = 1,
      .capabilities = mnexus::QueueFamilyCapabilityFlagBits::kGraphics |
                      mnexus::QueueFamilyCapabilityFlagBits::kCompute |
                      mnexus::QueueFamilyCapabilityFlagBits::kTransfer,
    };
    return MnBoolTrue;
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueSubmitCommandList,
    mnexus::QueueId const& queue_id,
    mnexus::ICommandList* command_list
  ) {
    MBASE_ASSERT_MSG(queue_id.queue_family_index == 0 && queue_id.queue_index == 0, "WebGPU backend only supports a single queue");

    mbase::LockGuard queue_lock(queue_mutex_);

    this->PollPendingOps();

    // Downcast to our command list implementation.
    MnexusCommandListWebGpu* webgpu_command_list = dynamic_cast<MnexusCommandListWebGpu*>(command_list);

    wgpu::CommandBuffer wgpu_command_buffer = webgpu_command_list->GetWgpuCommandEncoder().Finish();

    wgpu::Queue wgpu_queue = wgpu_device_.GetQueue();
    wgpu_queue.Submit(1, &wgpu_command_buffer);

    this->DiscardCommandList(command_list);

    // Track GPU-side completion via OnSubmittedWorkDone.
    wgpu::Future work_done_future = wgpu_queue.OnSubmittedWorkDone(
      wgpu::CallbackMode::WaitAnyOnly,
      [](wgpu::QueueWorkDoneStatus status, wgpu::StringView) {
        if (status != wgpu::QueueWorkDoneStatus::Success) {
          MBASE_LOG_ERROR("OnSubmittedWorkDone failed: {}", static_cast<uint32_t>(status));
        }
      }
    );

    mnexus::IntraQueueSubmissionId const id = this->AdvanceTimeline();

    pending_ops_.emplace_back(
      PendingOp {
        .timeline_value = id.Get(),
        .future = work_done_future,
      }
    );

    this->UpdateCompletedValue();
    return id;
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueWriteBuffer,
    mnexus::QueueId const& queue_id,
    mnexus::BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void const* data,
    uint32_t data_size_in_bytes
  ) {
    MBASE_ASSERT_MSG(queue_id.queue_family_index == 0 && queue_id.queue_index == 0, "WebGPU backend only supports a single queue");

    mbase::LockGuard queue_lock(queue_mutex_);

    auto pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());

    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);

    wgpu::Queue wgpu_queue = wgpu_device_.GetQueue();

    wgpu_queue.WriteBuffer(
      hot.wgpu_buffer,
      buffer_offset,
      data,
      data_size_in_bytes
    );

    mnexus::IntraQueueSubmissionId const id = this->AdvanceTimeline();
    this->UpdateCompletedValue();
    return id;
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueReadBuffer,
    mnexus::QueueId const& queue_id,
    mnexus::BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void* dst,
    uint32_t size_in_bytes
  ) {
    MBASE_ASSERT_MSG(queue_id.queue_family_index == 0 && queue_id.queue_index == 0, "WebGPU backend only supports a single queue");
    MBASE_ASSERT_MSG((buffer_offset % 4) == 0, "buffer_offset must be 4-byte aligned");
    MBASE_ASSERT_MSG((size_in_bytes % 4) == 0, "size_in_bytes must be 4-byte aligned");

    mbase::LockGuard queue_lock(queue_mutex_);

    auto pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());
    auto [hot, lock] = resource_storage_->buffers.GetHotConstRefWithSharedLockGuard(pool_handle);

    // Create a staging buffer for the readback.
    wgpu::BufferDescriptor staging_desc {
      .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
      .size = size_in_bytes,
    };
    wgpu::Buffer staging_buffer = wgpu_device_.CreateBuffer(&staging_desc);

    // Encode copy from source buffer to staging buffer.
    wgpu::CommandEncoder encoder = wgpu_device_.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(hot.wgpu_buffer, buffer_offset, staging_buffer, 0, size_in_bytes);
    wgpu::CommandBuffer command_buffer = encoder.Finish();

    wgpu::Queue wgpu_queue = wgpu_device_.GetQueue();
    wgpu_queue.Submit(1, &command_buffer);

    // Initiate async map.
    wgpu::Future map_future = staging_buffer.MapAsync(
      wgpu::MapMode::Read, 0, size_in_bytes,
      wgpu::CallbackMode::WaitAnyOnly,
      [](wgpu::MapAsyncStatus status, wgpu::StringView message) {
        if (status != wgpu::MapAsyncStatus::Success) {
          MBASE_LOG_ERROR("MapAsync failed: {}", message);
        }
      }
    );

    mnexus::IntraQueueSubmissionId const id = this->AdvanceTimeline();

    pending_readbacks_.emplace_back(
      PendingReadback {
        .timeline_value = id.Get(),
        .staging_buffer = std::move(staging_buffer),
        .map_future = map_future,
        .dst = dst,
        .size_in_bytes = size_in_bytes,
      }
    );

    this->UpdateCompletedValue();
    return id;
  }

  IMPL_VAPI(mnexus::IntraQueueSubmissionId, QueueGetCompletedValue,
    mnexus::QueueId const& queue_id
  ) {
    MBASE_ASSERT_MSG(queue_id.queue_family_index == 0 && queue_id.queue_index == 0, "WebGPU backend only supports a single queue");

    mbase::LockGuard queue_lock(queue_mutex_);

    this->PollPendingOps();
    this->UpdateCompletedValue();
    return mnexus::IntraQueueSubmissionId { completed_value_ };
  }

  IMPL_VAPI(void, QueueWait,
    mnexus::QueueId const& queue_id,
    mnexus::IntraQueueSubmissionId value
  ) {
    MBASE_ASSERT_MSG(queue_id.queue_family_index == 0 && queue_id.queue_index == 0, "WebGPU backend only supports a single queue");

    mbase::LockGuard queue_lock(queue_mutex_);

    uint64_t const target = value.Get();

    while (completed_value_ < target) {
      bool found_pending = false;

      // Block on pending submit/write ops.
      for (size_t i = 0; i < pending_ops_.size(); ) {
        PendingOp& op = pending_ops_[i];
        if (op.timeline_value <= target) {
          found_pending = true;

          wgpu::WaitStatus wait_status = wgpu_instance_.WaitAny(op.future, UINT64_MAX);
          if (wait_status == wgpu::WaitStatus::Success) {
            pending_ops_.erase(pending_ops_.begin() + static_cast<ptrdiff_t>(i));
          } else {
            MBASE_LOG_ERROR("WaitAny failed during QueueWait (pending op)");
            ++i;
          }
        } else {
          ++i;
        }
      }

      // Block on pending readbacks.
      for (size_t i = 0; i < pending_readbacks_.size(); ) {
        PendingReadback& rb = pending_readbacks_[i];
        if (rb.timeline_value <= target) {
          found_pending = true;

          wgpu::WaitStatus wait_status = wgpu_instance_.WaitAny(rb.map_future, UINT64_MAX);
          if (wait_status == wgpu::WaitStatus::Success) {
            void const* mapped = rb.staging_buffer.GetConstMappedRange(0, rb.size_in_bytes);
            MBASE_ASSERT(mapped != nullptr);
            std::memcpy(rb.dst, mapped, rb.size_in_bytes);
            rb.staging_buffer.Unmap();

            pending_readbacks_.erase(pending_readbacks_.begin() + static_cast<ptrdiff_t>(i));
          } else {
            MBASE_LOG_ERROR("WaitAny failed during QueueWait (pending readback)");
            ++i;
          }
        } else {
          ++i;
        }
      }

      this->UpdateCompletedValue();

      if (!found_pending) {
        break;
      }
    }
  }

  // -----------------------------------------------------------------------------------------------
  // Resource creation/acquisition.
  //

  //
  // Command List
  //

  IMPL_VAPI(mnexus::ICommandList*, CreateCommandList, mnexus::CommandListDesc const& desc) {
    (void)desc; // Unused for now.

    wgpu::CommandEncoder wgpu_command_encoder = wgpu_device_.CreateCommandEncoder();

    return new MnexusCommandListWebGpu(resource_storage_, std::move(wgpu_command_encoder));
  }

  MNEXUS_NO_THROW void MNEXUS_CALL DiscardCommandList(mnexus::ICommandList* command_list) override {
    MBASE_ASSERT(command_list != nullptr);
    delete command_list;
  }

  //
  // Buffer
  //

  IMPL_VAPI(mnexus::BufferHandle, CreateBuffer,
    mnexus::BufferDesc const& desc
  ) {
    wgpu::Buffer wgpu_buffer = CreateWgpuBuffer(wgpu_device_, desc);

    container::ResourceHandle pool_handle = resource_storage_->buffers.Emplace(
      std::forward_as_tuple(BufferHot { std::move(wgpu_buffer) }),
      std::forward_as_tuple(BufferCold { desc })
    );

    return mnexus::BufferHandle { pool_handle.AsU64() };
  }
  IMPL_VAPI(void, DestroyBuffer,
    mnexus::BufferHandle buffer_handle
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());
    resource_storage_->buffers.Erase(pool_handle);
  }

  IMPL_VAPI(void, GetBufferDesc,
    mnexus::BufferHandle buffer_handle,
    mnexus::BufferDesc& out_desc
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(buffer_handle.Get());
    {
      auto [cold, lock] = resource_storage_->buffers.GetColdConstRefWithSharedLockGuard(pool_handle);
      out_desc = cold.desc;
    }
  }

  //
  // Texture
  //

  IMPL_VAPI(mnexus::TextureHandle, GetSwapchainTexture) {
    return mnexus::TextureHandle { resource_storage_->swapchain_texture_handle.AsU64() };
  }

  IMPL_VAPI(mnexus::TextureHandle, CreateTexture,
    mnexus::TextureDesc const& desc
  ) {
    wgpu::TextureUsage const usage = ToWgpuTextureUsage(desc.usage) | wgpu::TextureUsage::CopyDst;

    wgpu::TextureDescriptor wgpu_texture_desc {
      .usage = usage,
      .dimension = ToWgpuTextureDimension(desc.dimension),
      .size = {
        .width = desc.width,
        .height = desc.height,
        .depthOrArrayLayers = 1,
      },
      .format = ToWgpuTextureFormat(desc.format),
      .mipLevelCount = 1,
      .sampleCount = 1,
      .viewFormatCount = 0,
      .viewFormats = nullptr,
    };

    wgpu::Texture wgpu_texture = wgpu_device_.CreateTexture(&wgpu_texture_desc);

    container::ResourceHandle pool_handle = resource_storage_->textures.Emplace(
      std::forward_as_tuple(TextureHot { std::move(wgpu_texture) }),
      std::forward_as_tuple(TextureCold { desc })
    );

    return mnexus::TextureHandle { pool_handle.AsU64() };
  }

  IMPL_VAPI(void, DestroyTexture,
    mnexus::TextureHandle texture_handle
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(texture_handle.Get());

    MBASE_ASSERT(pool_handle != resource_storage_->swapchain_texture_handle);

    resource_storage_->textures.Erase(pool_handle);
  }

  IMPL_VAPI(void, GetTextureDesc,
    mnexus::TextureHandle texture_handle,
    mnexus::TextureDesc& out_desc
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(texture_handle.Get());

    {
      auto [cold, lock] = resource_storage_->textures.GetColdConstRefWithSharedLockGuard(pool_handle);

      out_desc = cold.desc;
    }
  }

  //
  // ShaderModule
  //

  IMPL_VAPI(mnexus::ShaderModuleHandle, CreateShaderModule,
    mnexus::ShaderModuleDesc const& desc
  ) {
    container::ResourceHandle pool_handle = EmplaceShaderModuleResourcePool(
      resource_storage_->shader_modules,
      wgpu_device_,
      desc
    );

    if (pool_handle.IsNull()) {
      return mnexus::ShaderModuleHandle::Invalid();
    }

    return mnexus::ShaderModuleHandle { pool_handle.AsU64() };
  }
  IMPL_VAPI(void, DestroyShaderModule,
    mnexus::ShaderModuleHandle shader_module_handle
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(shader_module_handle.Get());
    resource_storage_->shader_modules.Erase(pool_handle);
  }

  //
  // Program
  //

  IMPL_VAPI(mnexus::ProgramHandle, CreateProgram,
    mnexus::ProgramDesc const& desc
  ) {
    container::ResourceHandle pool_handle = EmplaceProgramResourcePool(
      resource_storage_->programs,
      wgpu_device_,
      desc,
      resource_storage_->shader_modules,
      [](mnexus::ShaderModuleHandle shader_module_handle) {
        return container::ResourceHandle::FromU64(shader_module_handle.Get());
      }
    );
    if (pool_handle.IsNull()) {
      return mnexus::ProgramHandle::Invalid();
    }

    return mnexus::ProgramHandle { pool_handle.AsU64() };
  }
  IMPL_VAPI(void, DestroyProgram,
    mnexus::ProgramHandle program_handle
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(program_handle.Get());
    resource_storage_->programs.Erase(pool_handle);
  }

  //
  // ComputePipeline
  //

  IMPL_VAPI(mnexus::ComputePipelineHandle, CreateComputePipeline,
    mnexus::ComputePipelineDesc const& desc
  ) {
    auto shader_module_pool_handle = container::ResourceHandle::FromU64(desc.shader_module.Get());

    auto [shader_module_hot, lock] = resource_storage_->shader_modules.GetHotConstRefWithSharedLockGuard(
      shader_module_pool_handle
    );

    wgpu::ComputePipeline wgpu_compute_pipeline = CreateWgpuComputePipeline(
      wgpu_device_,
      shader_module_hot.wgpu_shader_module
    );

    if (!wgpu_compute_pipeline) {
      return mnexus::ComputePipelineHandle::Invalid();
    }

    container::ResourceHandle pool_handle = resource_storage_->compute_pipelines.Emplace(
      std::forward_as_tuple(ComputePipelineHot { std::move(wgpu_compute_pipeline) }),
      std::forward_as_tuple(ComputePipelineCold { })
    );

    return mnexus::ComputePipelineHandle { pool_handle.AsU64() };
  }
  IMPL_VAPI(void, DestroyComputePipeline,
    mnexus::ComputePipelineHandle compute_pipeline_handle
  ) {
    auto pool_handle = container::ResourceHandle::FromU64(compute_pipeline_handle.Get());
    resource_storage_->compute_pipelines.Erase(pool_handle);
  }

  //
  // RenderPipeline
  //

  IMPL_VAPI(mnexus::RenderPipelineHandle, CreateRenderPipeline,
    mnexus::RenderPipelineDesc const& desc
  ) {
    (void)desc; // Unused for now.
    return mnexus::RenderPipelineHandle::Invalid();
  }

  //
  // Module local
  //

  void Initialize(
    wgpu::Instance wgpu_instance,
    wgpu::Device wgpu_device,
    ResourceStorage* resource_storage
  ) {
    MBASE_ASSERT(!wgpu_device_);

    wgpu_instance_ = std::move(wgpu_instance);
    wgpu_device_ = std::move(wgpu_device);
    resource_storage_ = resource_storage;

    resource_storage_->swapchain_texture_handle = resource_storage_->textures.Emplace(
      std::forward_as_tuple(TextureHot {}),
      std::forward_as_tuple(TextureCold {})
    );

    InitializeShaderSubsystem();
  }

  void Shutdown() {
    {
      mbase::LockGuard queue_lock(queue_mutex_);

      if (!pending_ops_.empty() || !pending_readbacks_.empty()) {
        MBASE_LOG_WARN(
          "Shutting down with {} pending op(s) and {} pending readback(s)",
          pending_ops_.size(), pending_readbacks_.size()
        );
      }

      pending_ops_.clear();
      pending_readbacks_.clear();
    }

    ShutdownShaderSubsystem();
    resource_storage_ = nullptr;
    wgpu_device_ = nullptr;
    wgpu_instance_ = nullptr;
  }

  void OnWgpuSurfaceConfigured(wgpu::SurfaceConfiguration const& surface_config) {
    mbase::LockGuard sw_lock(resource_storage_->swapchain_texture_mutex);

    auto [hot, cold, lock] = resource_storage_->textures.GetRefWithSharedLockGuard(
      resource_storage_->swapchain_texture_handle
    );

    hot = TextureHot {}; // No actual `wgpu::Texture` until acquired from the swapchain.

    mnexus::TextureDesc desc {
      .usage = FromWgpuTextureUsage(surface_config.usage),
      .format = FromWgpuTextureFormat(surface_config.format),
      .dimension = mnexus::TextureDimension::k2D,
      .width = surface_config.width,
      .height = surface_config.height,
      .depth = 1,
      .mip_level_count = 1,
      .array_layer_count = 1,
    };

    cold = TextureCold { .desc = desc };
  }
  void OnWgpuSurfaceUnconfigured() {
    mbase::LockGuard sw_lock(resource_storage_->swapchain_texture_mutex);

    auto [hot, cold, lock] = resource_storage_->textures.GetRefWithSharedLockGuard(
      resource_storage_->swapchain_texture_handle
    );

    hot = TextureHot {};
    cold = TextureCold {};
  }

  void OnWgpuSurfaceTextureAcquired(wgpu::Texture wgpu_texture) {
    auto [hot, cold, lock] = resource_storage_->textures.GetRefWithSharedLockGuard(
      resource_storage_->swapchain_texture_handle
    );

    hot.wgpu_texture = std::move(wgpu_texture);

  }
  void OnWgpuSurfaceTextureReleased() {
    auto [hot, cold, lock] = resource_storage_->textures.GetRefWithSharedLockGuard(
      resource_storage_->swapchain_texture_handle
    );

    hot.wgpu_texture = wgpu::Texture {};
  }

private:
  // Tracks GPU-side completion of queue submissions (via OnSubmittedWorkDone).
  struct PendingOp {
    uint64_t timeline_value;
    wgpu::Future future;
  };

  // Tracks a GPU->CPU readback (staging buffer map + memcpy).
  struct PendingReadback {
    uint64_t timeline_value;
    wgpu::Buffer staging_buffer;
    wgpu::Future map_future;
    void* dst;
    uint32_t size_in_bytes;
  };

  mnexus::IntraQueueSubmissionId AdvanceTimeline() MBASE_REQUIRES(queue_mutex_) {
    return mnexus::IntraQueueSubmissionId { next_timeline_value_++ };
  }

  void UpdateCompletedValue() MBASE_REQUIRES(queue_mutex_) {
    uint64_t min_pending = next_timeline_value_;

    for (auto const& op : pending_ops_) {
      if (op.timeline_value < min_pending) {
        min_pending = op.timeline_value;
      }
    }
    for (auto const& rb : pending_readbacks_) {
      if (rb.timeline_value < min_pending) {
        min_pending = rb.timeline_value;
      }
    }

    completed_value_ = min_pending - 1;
  }

  void PollPendingOps() MBASE_REQUIRES(queue_mutex_) {
    for (size_t i = 0; i < pending_ops_.size(); ) {
      PendingOp& op = pending_ops_[i];

      wgpu::WaitStatus status = wgpu_instance_.WaitAny(op.future, 0);
      if (status == wgpu::WaitStatus::Success) {
        pending_ops_.erase(pending_ops_.begin() + static_cast<ptrdiff_t>(i));
      } else {
        ++i;
      }
    }

    for (size_t i = 0; i < pending_readbacks_.size(); ) {
      PendingReadback& rb = pending_readbacks_[i];

      wgpu::WaitStatus status = wgpu_instance_.WaitAny(rb.map_future, 0);
      if (status == wgpu::WaitStatus::Success) {
        void const* mapped = rb.staging_buffer.GetConstMappedRange(0, rb.size_in_bytes);
        MBASE_ASSERT(mapped != nullptr);
        std::memcpy(rb.dst, mapped, rb.size_in_bytes);
        rb.staging_buffer.Unmap();

        pending_readbacks_.erase(pending_readbacks_.begin() + static_cast<ptrdiff_t>(i));
      } else {
        ++i;
      }
    }
  }

  wgpu::Instance wgpu_instance_;
  wgpu::Device wgpu_device_;
  ResourceStorage* resource_storage_ = nullptr;

  mbase::Lockable<std::mutex> queue_mutex_;
  uint64_t next_timeline_value_ MBASE_GUARDED_BY(queue_mutex_) = 1;
  uint64_t completed_value_ MBASE_GUARDED_BY(queue_mutex_) = 0;
  std::vector<PendingOp> pending_ops_ MBASE_GUARDED_BY(queue_mutex_);
  std::vector<PendingReadback> pending_readbacks_ MBASE_GUARDED_BY(queue_mutex_);
};


class BackendWebGpu final : public IBackendWebGpu {
public:
  explicit BackendWebGpu(
    wgpu::Instance instance,
    wgpu::Adapter adapter,
    wgpu::Device device
  ) :
    instance_(std::move(instance)),
    adapter_(std::move(adapter)),
    device_(std::move(device))
  {
    mnexus_device_.Initialize(instance_, device_, &resource_storage_);
  }
  ~BackendWebGpu() override {
    mnexus_device_.Shutdown();
  }

  // ----------------------------------------------------------------------------------------------
  // Surface lifecycle.

  void OnDisplayChanged() override {
    // No-op for now.
  }

  void OnSurfaceDestroyed() override {
    MBASE_ASSERT(bool(surface_));

    mnexus_device_.OnWgpuSurfaceUnconfigured();

    surface_.Unconfigure();
    surface_ = nullptr;
  }

  void OnSurfaceRecreated(mnexus::SurfaceSourceDesc const& surface_source_desc) override {
    constexpr wgpu::TextureFormat kPreferredFormat = wgpu::TextureFormat::RGBA8Unorm;
    constexpr wgpu::TextureUsage kTextureUsage = wgpu::TextureUsage::RenderAttachment;

    MBASE_ASSERT(!surface_);

    {
#if MBASE_PLATFORM_WINDOWS
      wgpu::SurfaceSourceWindowsHWND chained_desc{};
      chained_desc.hinstance = reinterpret_cast<void*>(surface_source_desc.instance_handle);
      chained_desc.hwnd = reinterpret_cast<void*>(surface_source_desc.window_handle);
#elif MBASE_PLATFORM_ANDROID
      wgpu::SurfaceSourceAndroidNativeWindow chained_desc{};
      chained_desc.window = reinterpret_cast<void*>(surface_source_desc.window_handle);
#elif MBASE_PLATFORM_WEB
      wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector chained_desc{};
      chained_desc.selector = wgpu::StringView(surface_source_desc.canvas_selector, surface_source_desc.canvas_selector_length);
#endif

      wgpu::SurfaceDescriptor desc{};
      desc.nextInChain = &chained_desc;

      surface_ = instance_.CreateSurface(&desc);
    }

    wgpu::SurfaceCapabilities surface_caps{};
    surface_.GetCapabilities(adapter_, &surface_caps);

    // Log surface capabilities.
    {
      MBASE_LOG_INFO("Surface Capabilities:");
      MBASE_LOG_INFO("Format count: {}", surface_caps.formatCount);
      for (uint32_t i = 0; i < surface_caps.formatCount; ++i) {
        MBASE_LOG_INFO("  Format[{}]: {}", i, surface_caps.formats[i]);
      }
      MBASE_LOG_INFO("Present mode count: {}", surface_caps.presentModeCount);
      for (uint32_t i = 0; i < surface_caps.presentModeCount; ++i) {
        MBASE_LOG_INFO("  PresentMode[{}]: {}", i, surface_caps.presentModes[i]);
      }
      MBASE_LOG_INFO("Alpha mode count: {}", surface_caps.alphaModeCount);
      for (uint32_t i = 0; i < surface_caps.alphaModeCount; ++i) {
        MBASE_LOG_INFO("  AlphaMode[{}]: {}", i, surface_caps.alphaModes[i]);
      }
    }

    // Assert that the surface supports the preferred format.
    {
      bool format_supported = false;
      for (uint32_t i = 0; i < surface_caps.formatCount; ++i) {
        if (surface_caps.formats[i] == kPreferredFormat) {
          format_supported = true;
          break;
        }
      }

      MBASE_ASSERT_MSG(
        format_supported,
        "Preferred surface format {} is not supported by the surface!",
        kPreferredFormat
      );
    }

    uint32_t width = 0;
    uint32_t height = 0;
    {
#if MBASE_PLATFORM_WINDOWS
      RECT rect;
      GetClientRect(reinterpret_cast<HWND>(surface_source_desc.window_handle), &rect);

      width = static_cast<uint32_t>(rect.right - rect.left);
      height = static_cast<uint32_t>(rect.bottom - rect.top);
#elif MBASE_PLATFORM_WEB
      std::string canvas_selector(
        surface_source_desc.canvas_selector,
        surface_source_desc.canvas_selector_length
      );

      int w = 0, h = 0;
      emscripten_get_canvas_element_size(canvas_selector.c_str(), &w, &h);

      width = static_cast<uint32_t>(w);
      height = static_cast<uint32_t>(h);
#else
# error Unsupported Platform
#endif
    }

    MBASE_LOG_INFO("Configuring surface with size {}x{}...", width, height);

    wgpu::SurfaceConfiguration surface_config {
      .device = device_,
      .format = kPreferredFormat,
      .usage = kTextureUsage,
      .width = width,
      .height = height,
      .viewFormatCount = 0,
      .viewFormats = nullptr,
      .alphaMode = wgpu::CompositeAlphaMode::Auto,
      .presentMode = wgpu::PresentMode::Fifo,
    };

    surface_.Configure(&surface_config);

    mnexus_device_.OnWgpuSurfaceConfigured(surface_config);
  }

  // ----------------------------------------------------------------------------------------------
  // Presentation.

  void OnPresentPrologue() override {
    MBASE_ASSERT(bool(surface_));

    wgpu::SurfaceTexture surface_texture;
    surface_.GetCurrentTexture(&surface_texture);

    MBASE_ASSERT(
      surface_texture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal ||
      surface_texture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal
    );

    mnexus_device_.OnWgpuSurfaceTextureAcquired(surface_texture.texture);

    return;

    wgpu::RenderPassColorAttachment attachment {
      .view = surface_texture.texture.CreateView(),
      .resolveTarget = nullptr,
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = { 1.0f, 0.0f, 0.0f, 1.0f },
    };

    wgpu::RenderPassDescriptor desc {
        .colorAttachmentCount = 1,
        .colorAttachments = &attachment,
        .depthStencilAttachment = nullptr,
    };

    wgpu::CommandEncoder encoder = device_.CreateCommandEncoder(nullptr);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&desc);
    pass.End();

    wgpu::CommandBuffer command_buffer = encoder.Finish();
    device_.GetQueue().Submit(1, &command_buffer);

    
  }

  void OnPresentEpilogue() override {
#if !MBASE_PLATFORM_WEB
    // On Emscripten, presentation happens automatically via requestAnimationFrame.
    // wgpuSurfacePresent is not supported.
    surface_.Present();
#endif

    mnexus_device_.OnWgpuSurfaceTextureReleased();
  }

  // ----------------------------------------------------------------------------------------------
  // Resource creation.

  mnexus::IDevice* GetDevice() override {
    return &mnexus_device_;
  }

private:
  wgpu::Instance instance_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;

  ResourceStorage resource_storage_;
  MnexusDeviceWebGpu mnexus_device_;

  wgpu::Surface surface_;
};

std::unique_ptr<IBackendWebGpu> IBackendWebGpu::Create() {
  std::vector<wgpu::InstanceFeatureName> required_features = {
    wgpu::InstanceFeatureName::TimedWaitAny
  };

#if MNEXUS_INTERNAL_USE_DAWN
  required_features.emplace_back(wgpu::InstanceFeatureName::ShaderSourceSPIRV);
#endif

  wgpu::InstanceDescriptor instance_desc {
    .requiredFeatureCount = static_cast<uint32_t>(required_features.size()),
    .requiredFeatures = required_features.data()
  };

  wgpu::Instance instance = wgpu::CreateInstance(&instance_desc);

  wgpu::Adapter adapter;
  {
    wgpu::Future f1 = instance.RequestAdapter(
      nullptr,
      wgpu::CallbackMode::WaitAnyOnly,
      [&adapter](wgpu::RequestAdapterStatus status, wgpu::Adapter a, wgpu::StringView message) {
        if (status != wgpu::RequestAdapterStatus::Success) {
          MBASE_LOG_ERROR("RequestAdapter failed: {}", message);
          mbase::Trap();
        }
        adapter = std::move(a);
      }
    );

    instance.WaitAny(f1, UINT64_MAX);
  }

  {
    wgpu::AdapterInfo info {};
    adapter.GetInfo(&info);

    MBASE_LOG_INFO("Adapter Info:");
    MBASE_LOG_INFO("  Name: {}", info.device);
    MBASE_LOG_INFO("  VendorID: {}", info.vendorID);
    MBASE_LOG_INFO("  Architecture: {}", info.architecture);
    MBASE_LOG_INFO("  Description: {}", info.description);
  }

  wgpu::Device device;
  {
    wgpu::DeviceDescriptor desc {};
    desc.SetUncapturedErrorCallback(
      [](const wgpu::Device&, wgpu::ErrorType errorType, wgpu::StringView message) {
        MBASE_LOG_ERROR("Uncaptured error ({}): {}", errorType, message);
        mbase::Trap();
      }
    );

    wgpu::Future f2 = adapter.RequestDevice(
      &desc,
      wgpu::CallbackMode::WaitAnyOnly,
      [&device](wgpu::RequestDeviceStatus status, wgpu::Device d, wgpu::StringView message) {
        if (status != wgpu::RequestDeviceStatus::Success) {
          MBASE_LOG_ERROR("RequestDevice: {}", message);
          mbase::Trap();
        }
        device = std::move(d);
      }
    );

    instance.WaitAny(f2, UINT64_MAX);
  }

  auto backend = std::make_unique<BackendWebGpu>(
    std::move(instance),
    std::move(adapter),
    std::move(device)
  );
  return backend;
}

} // namespace mnexus_backend::webgpu
