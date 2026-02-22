#pragma once

// c++ headers ------------------------------------------
#include <memory>

// public project headers -------------------------------
#include "mbase/public/call.h"

#include "mnexus/public/types.h"

#define MNEXUS_NO_THROW MBASE_NO_THROW
#define MNEXUS_CALL     MBASE_STDCALL

#if defined(__cplusplus)

namespace mnexus {

class IDevice;
class ICommandList;

#define _MNEXUS_VAPI(ret, name, ...) \
  virtual MNEXUS_NO_THROW ret MNEXUS_CALL name(__VA_ARGS__) = 0

struct NexusDesc final {
  bool headless = false;
};

class INexus {
public:
  static INexus* Create(NexusDesc const& desc = {});

  virtual ~INexus() = default;

  // ----------------------------------------------------------------------------------------------
  // Lifecycle.

  _MNEXUS_VAPI(void, Destroy);

  // ----------------------------------------------------------------------------------------------
  // Surface lifecycle.

  _MNEXUS_VAPI(void, OnDisplayChanged);

  _MNEXUS_VAPI(void, OnSurfaceDestroyed);
  _MNEXUS_VAPI(void, OnSurfaceRecreated, SurfaceSourceDesc const& surface_source_desc);

  // ----------------------------------------------------------------------------------------------
  // Presentation.

  /// ## Post-conditions
  /// - The swapchain's current backbuffer is acquired and ready for rendering.
  _MNEXUS_VAPI(void, OnPresentPrologue);

  /// ## Post-conditions
  /// - The swapchain's current backbuffer is queued for presentation.
  _MNEXUS_VAPI(void, OnPresentEpilogue);

  // ----------------------------------------------------------------------------------------------
  // Device

  _MNEXUS_VAPI(IDevice*, GetDevice);

protected:
  INexus() = default;
};

class IDevice {
public:
  virtual ~IDevice() = default;

  // ----------------------------------------------------------------------------------------------
  // Queue
  //

  _MNEXUS_VAPI(uint32_t, QueueGetFamilyCount);

  _MNEXUS_VAPI(MnBool32, QueueGetFamilyDesc,
    uint32_t queue_family_index,
    QueueFamilyDesc& out_desc
  );

  _MNEXUS_VAPI(IntraQueueSubmissionId, QueueSubmitCommandList,
    QueueId const& queue_id,
    ICommandList* command_list
  );

  /// Writes data from CPU memory into a GPU buffer.
  /// - `data_size_in_bytes`: **MUST** be a multiple of 4 bytes.
  _MNEXUS_VAPI(IntraQueueSubmissionId, QueueWriteBuffer,
    QueueId const& queue_id,
    BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void const* data,
    uint32_t data_size_in_bytes
  );

  /// Reads data from a GPU buffer into a CPU-accessible destination.
  ///
  /// - `buffer_handle`: Source buffer. Must have `kTransferSrc` or `kStorage` usage.
  /// - `buffer_offset`: Byte offset into the source buffer. Must be 4-byte aligned.
  /// - `dst`: Destination pointer. Must remain valid until
  ///   `QueueGetCompletedValue() >= returned value`.
  /// - `size_in_bytes`: Number of bytes to read. Must be 4-byte aligned.
  /// - Returns: Timeline value. Data at `dst` is valid once
  ///   `QueueGetCompletedValue() >= returned value`.
  _MNEXUS_VAPI(IntraQueueSubmissionId, QueueReadBuffer,
    QueueId const& queue_id,
    BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void* dst,
    uint32_t size_in_bytes
  );

  /// Returns the highest completed timeline value on the given queue.
  /// All operations that returned an `IntraQueueSubmissionId` <= this value
  /// have completed and their side-effects are visible.
  _MNEXUS_VAPI(IntraQueueSubmissionId, QueueGetCompletedValue,
    QueueId const& queue_id
  );

  /// Blocks until the given timeline value has completed on the queue.
  /// Guarantees all operations with `IntraQueueSubmissionId` <= `value`
  /// are complete when this returns.
  _MNEXUS_VAPI(void, QueueWait,
    QueueId const& queue_id,
    IntraQueueSubmissionId value
  );

  // ----------------------------------------------------------------------------------------------
  // Resource creation/acquisition.
  //

  //
  // Command List
  //

  _MNEXUS_VAPI(ICommandList*, CreateCommandList, CommandListDesc const& desc);

  /// Discards an `ICommandList` that was created but not submitted.
  /// **MUST NOT** be called on a command list that has already been submitted.
  _MNEXUS_VAPI(void, DiscardCommandList, ICommandList* command_list);

  //
  // Buffer
  //

  _MNEXUS_VAPI(BufferHandle, CreateBuffer,
    BufferDesc const& desc
  );
  _MNEXUS_VAPI(void, DestroyBuffer,
    BufferHandle buffer_handle
  );

  _MNEXUS_VAPI(void, GetBufferDesc,
    BufferHandle buffer_handle,
    BufferDesc& out_desc
  );

  //
  // Texture
  //

  _MNEXUS_VAPI(TextureHandle, GetSwapchainTexture);

  _MNEXUS_VAPI(TextureHandle, CreateTexture,
    TextureDesc const& desc
  );
  _MNEXUS_VAPI(void, DestroyTexture,
    TextureHandle texture_handle
  );

  _MNEXUS_VAPI(void, GetTextureDesc,
    TextureHandle texture_handle,
    TextureDesc& out_desc
  );

  //
  // Sampler
  //

  _MNEXUS_VAPI(SamplerHandle, CreateSampler,
    SamplerDesc const& desc
  );
  _MNEXUS_VAPI(void, DestroySampler,
    SamplerHandle sampler_handle
  );

  //
  // ShaderModule
  //

  _MNEXUS_VAPI(ShaderModuleHandle, CreateShaderModule,
    ShaderModuleDesc const& desc
  );
  _MNEXUS_VAPI(void, DestroyShaderModule,
    ShaderModuleHandle shader_module_handle
  );

  //
  // Program
  //

  _MNEXUS_VAPI(ProgramHandle, CreateProgram,
    ProgramDesc const& desc
  );
  _MNEXUS_VAPI(void, DestroyProgram,
    ProgramHandle program_handle
  );

  //
  // ComputePipeline
  //

  _MNEXUS_VAPI(ComputePipelineHandle, CreateComputePipeline,
    ComputePipelineDesc const& desc
  );
  _MNEXUS_VAPI(void, DestroyComputePipeline,
    ComputePipelineHandle compute_pipeline_handle
  );

  //
  // RenderPipeline
  //

  _MNEXUS_VAPI(RenderPipelineHandle, CreateRenderPipeline,
    RenderPipelineDesc const& desc
  );

  //
  // Device Capability
  //

  _MNEXUS_VAPI(DeviceCapability, GetDeviceCapability);

protected:
  IDevice() = default;
};

class ICommandList {
public:
  virtual ~ICommandList() = default;

  _MNEXUS_VAPI(void, End);

  //
  // Explicit Pipeline Binding
  //

  _MNEXUS_VAPI(void, BindExplicitComputePipeline, ComputePipelineHandle compute_pipeline_handle);
  _MNEXUS_VAPI(void, BindExplicitRenderPipeline, RenderPipelineHandle render_pipeline_handle);

  //
  // Compute
  //

  _MNEXUS_VAPI(void, DispatchCompute,
    uint32_t workgroup_count_x,
    uint32_t workgroup_count_y,
    uint32_t workgroup_count_z
  );

  //
  // Resource Binding
  //

  _MNEXUS_VAPI(void, BindUniformBuffer,
    BindingId const& id,
    BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  );

  _MNEXUS_VAPI(void, BindStorageBuffer,
    BindingId const& id,
    BufferHandle buffer_handle,
    uint64_t offset,
    uint64_t size
  );

  _MNEXUS_VAPI(void, BindSampledTexture,
    BindingId const& id,
    TextureHandle texture_handle,
    TextureSubresourceRange const& subresource_range
  );

  _MNEXUS_VAPI(void, BindSampler,
    BindingId const& id,
    SamplerHandle sampler_handle
  );

  //
  // Render Pass
  //

  _MNEXUS_VAPI(void, BeginRenderPass, RenderPassDesc const& desc);
  _MNEXUS_VAPI(void, EndRenderPass);

  //
  // Render State (auto-generation path)
  //
  // These setters configure fixed-function pipeline state for the auto-generation path.
  // State is accumulated and resolved into a pipeline object at the next Draw/DrawIndexed call.
  // Default values are documented below; only call a setter when you need a non-default value.
  //

  _MNEXUS_VAPI(void, BindRenderProgram, ProgramHandle program_handle);

  _MNEXUS_VAPI(void, SetVertexInputLayout,
    container::ArrayProxy<VertexInputBindingDesc const> bindings,
    container::ArrayProxy<VertexInputAttributeDesc const> attributes
  );

  _MNEXUS_VAPI(void, BindVertexBuffer,
    uint32_t binding,
    BufferHandle buffer_handle,
    uint64_t offset
  );

  _MNEXUS_VAPI(void, BindIndexBuffer,
    BufferHandle buffer_handle,
    uint64_t offset,
    IndexType index_type
  );

  // Rasterization
  // Default: `kTriangleList`.
  _MNEXUS_VAPI(void, SetPrimitiveTopology, PrimitiveTopology topology);
  // Default: `kFill`.
  _MNEXUS_VAPI(void, SetPolygonMode, PolygonMode mode);
  // Default: `kNone`.
  _MNEXUS_VAPI(void, SetCullMode, CullMode cull_mode);
  // Default: `kCounterClockwise`.
  _MNEXUS_VAPI(void, SetFrontFace, FrontFace front_face);

  // Depth
  // Default: `false`.
  _MNEXUS_VAPI(void, SetDepthTestEnabled, bool enabled);
  // Default: `false`.
  _MNEXUS_VAPI(void, SetDepthWriteEnabled, bool enabled);
  // Default: `kAlways`.
  _MNEXUS_VAPI(void, SetDepthCompareOp, CompareOp op);

  // Stencil
  // Default: `false`.
  _MNEXUS_VAPI(void, SetStencilTestEnabled, bool enabled);
  // Default: all ops = `kKeep`, compare = `kAlways`.
  _MNEXUS_VAPI(void, SetStencilFrontOps,
    StencilOp fail, StencilOp pass, StencilOp depth_fail, CompareOp compare);
  // Default: all ops = `kKeep`, compare = `kAlways`.
  _MNEXUS_VAPI(void, SetStencilBackOps,
    StencilOp fail, StencilOp pass, StencilOp depth_fail, CompareOp compare);

  // Per-attachment blend
  // Default: `false`.
  _MNEXUS_VAPI(void, SetBlendEnabled, uint32_t attachment, bool enabled);
  // Default: src = `kOne`, dst = `kZero`, op = `kAdd` (both color and alpha).
  _MNEXUS_VAPI(void, SetBlendFactors,
    uint32_t attachment,
    BlendFactor src_color, BlendFactor dst_color, BlendOp color_op,
    BlendFactor src_alpha, BlendFactor dst_alpha, BlendOp alpha_op);
  // Default: `kAll`.
  _MNEXUS_VAPI(void, SetColorWriteMask, uint32_t attachment, ColorWriteMask mask);

  //
  // Draw (triggers implicit PSO resolution in auto-generation mode)
  //

  _MNEXUS_VAPI(void, Draw,
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t first_vertex,
    uint32_t first_instance
  );

  _MNEXUS_VAPI(void, DrawIndexed,
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t vertex_offset,
    uint32_t first_instance
  );

  //
  // Viewport / Scissor
  //

  _MNEXUS_VAPI(void, SetViewport,
    float x, float y, float width, float height,
    float min_depth, float max_depth
  );

  _MNEXUS_VAPI(void, SetScissor,
    int32_t x, int32_t y, uint32_t width, uint32_t height
  );

  //
  // Transfer
  //

  _MNEXUS_VAPI(void, ClearTexture,
    TextureHandle texture_handle,
    TextureSubresourceRange const& subresource_range,
    ClearValue const& clear_value
  );

  _MNEXUS_VAPI(void, CopyBufferToTexture,
    BufferHandle src_buffer_handle,
    uint32_t src_buffer_offset,
    TextureHandle dst_texture_handle,
    TextureSubresourceRange const& dst_subresource_range,
    Extent3d const& copy_extent
  );

  _MNEXUS_VAPI(void, BlitTexture,
    TextureHandle src_texture_handle,
    TextureSubresourceRange const& src_subresource_range,
    Offset3d const& src_offset,
    Extent3d const& src_extent,
    TextureHandle dst_texture_handle,
    TextureSubresourceRange const& dst_subresource_range,
    Offset3d const& dst_offset,
    Extent3d const& dst_extent,
    Filter filter
  );

protected:
  ICommandList() = default;
};

//
// Wrappers around handles to provide easy access to resource descriptions etc.
//

class Texture final {
public:
  static Texture Create(IDevice* device, TextureDesc const& desc) {
    TextureHandle handle = device->CreateTexture(desc);
    return Texture(device, handle);
  }

  explicit Texture(IDevice* device, TextureHandle handle) : device_(device), handle_(handle) {
    device_->GetTextureDesc(handle_, desc_);
  }
  ~Texture() = default;

  bool IsValid() const { return handle_.IsValid(); }

  void Destroy() {
    if (this->IsValid()) {
      device_->DestroyTexture(handle_);
      handle_ = TextureHandle::Invalid();
    }
  }

  TextureHandle GetHandle() const { return handle_; }
  TextureDesc const& GetDesc() { return desc_; }

private:
  IDevice* device_ = nullptr;
  TextureHandle handle_;
  TextureDesc desc_;
};

} // namespace mnexus

#endif // defined(__cplusplus)
