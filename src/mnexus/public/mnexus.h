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

class INexus {
public:
  static INexus* Create();

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

protected:
  IDevice() = default;
};

class ICommandList {
public:
  virtual ~ICommandList() = default;

  _MNEXUS_VAPI(void, End);

  //
  // Compute
  //

  _MNEXUS_VAPI(void, BindComputePipeline, ComputePipelineHandle compute_pipeline_handle);
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
