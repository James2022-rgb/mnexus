#pragma once

// c++ headers ------------------------------------------
#if defined(__cplusplus)
# include <memory>
#endif

// public project headers -------------------------------
#include "mbase/public/call.h"

#include "mnexus/public/types.h"

#define MNEXUS_NO_THROW MBASE_NO_THROW
#define MNEXUS_CALL     MBASE_STDCALL

#if defined(__cplusplus)

namespace mnexus {

// ====================================================================================================
// API Conventions
//
// This header uses RFC 2119 keywords to state normative requirements:
//
//   **MUST** / **MUST NOT**  - Absolute requirement or prohibition.
//                              Violation results in undefined behavior.
//   **SHOULD** / **SHOULD NOT** - Recommended practice. Violation is valid
//                                 but may result in suboptimal behavior or
//                                 implementation-defined diagnostics.
//   **MAY** - Behavior or usage that is explicitly permitted.
//
// "Undefined behavior" means the implementation makes no guarantees about
// the outcome. Debug layers or validation backends MAY detect and report
// such violations, but are not required to.
//
// Handle types (e.g. `BufferHandle`, `TextureHandle`) use a sentinel value
// to represent an invalid/null handle. Passing an invalid handle where a
// valid handle is required results in undefined behavior unless the method
// documentation explicitly states otherwise.
//
// ## Informative Text
//
// Paragraphs prefixed with **Note:** are informative and do not impose
// normative requirements. They provide implementation guidance, rationale,
// or backend-specific details.
//
// ## Parameter Lifetime
//
// Unless the documentation for a specific parameter explicitly states
// otherwise, all pointers and references passed to a function need only
// remain valid until the function returns.
//
// ## Object Lifetime
//
// Every resource created via a `Create` method has a corresponding `Destroy`
// method. Once `Destroy` is called, the handle is invalidated and **MUST
// NOT** be used for any further API calls.
//
// > **Note:** The underlying GPU resource may be retained by the
// > implementation until in-flight operations that reference it have
// > completed (deferred deletion).
// ====================================================================================================

class IDevice;
class ICommandList;

#define _MNEXUS_VAPI(ret, name, ...) \
  virtual MNEXUS_NO_THROW ret MNEXUS_CALL name(__VA_ARGS__) = 0

struct NexusDesc final {
  bool headless = false;
};

class INexus {
public:
  /// Creates a new mnexus instance and initializes the GPU backend.
  ///
  /// If initialization fails (e.g. no suitable GPU adapter), the process is
  /// terminated via `mbase::Trap()`. This function never returns null.
  ///
  /// - `desc`: Instance configuration. If `desc.headless` is `true`, no
  ///   surface or swapchain is created; the device is available immediately.
  /// - Returns: A non-null `INexus` pointer. The caller **MUST** eventually
  ///   call `Destroy` to release it.
  static INexus* Create(NexusDesc const& desc = {});

  virtual ~INexus() = default;

  // ----------------------------------------------------------------------------------------------
  // Lifecycle.

  /// Destroys this `INexus` instance and all associated GPU resources.
  ///
  /// Any resources that have not been explicitly destroyed are released by the
  /// implementation. Any in-flight queue operations are completed or aborted
  /// as needed.
  ///
  /// After this call, the `INexus` pointer and any `IDevice` pointer obtained
  /// from `GetDevice` are invalid and **MUST NOT** be dereferenced.
  _MNEXUS_VAPI(void, Destroy);

  // ----------------------------------------------------------------------------------------------
  // Surface lifecycle.

  /// Notifies the instance that display properties have changed (e.g. DPI,
  /// color space, HDR capability).
  ///
  /// This is a silent no-op in headless mode.
  _MNEXUS_VAPI(void, OnDisplayChanged);

  /// Notifies the instance that the rendering surface has been destroyed or
  /// is about to be resized.
  ///
  /// ## Pre-conditions
  /// - The instance **MUST NOT** be headless.
  /// - A surface **MUST** have been previously configured via
  ///   `OnSurfaceRecreated`.
  ///
  /// ## Post-conditions
  /// - The swapchain is unconfigured. `IDevice::GetSwapchainTexture`
  ///   returns an invalid handle until `OnSurfaceRecreated` is called.
  _MNEXUS_VAPI(void, OnSurfaceDestroyed);

  /// Creates or reconfigures the rendering surface from a platform window.
  ///
  /// - `surface_source_desc`: Platform-specific surface source. The relevant
  ///   handle fields **MUST** be valid for the current platform.
  ///
  /// ## Pre-conditions
  /// - The instance **MUST NOT** be headless.
  ///
  /// ## Post-conditions
  /// - The swapchain is configured and ready for use.
  /// - Swapchain texture dimensions are clamped to a minimum of 1x1.
  ///
  /// > **Note:** When the native window handle matches the previous call
  /// > (e.g. a desktop window resize), the WebGPU backend reuses the
  /// > underlying `wgpu::Surface` and only reconfigures its dimensions,
  /// > avoiding a full swapchain teardown.
  _MNEXUS_VAPI(void, OnSurfaceRecreated, SurfaceSourceDesc const& surface_source_desc);

  // ----------------------------------------------------------------------------------------------
  // Presentation.

  /// Acquires the swapchain's current backbuffer for rendering.
  ///
  /// ## Pre-conditions
  /// - The instance **MUST NOT** be headless.
  /// - A surface **MUST** have been configured via `OnSurfaceRecreated`.
  ///
  /// ## Post-conditions
  /// - The swapchain's current backbuffer is acquired and ready for
  ///   rendering.
  /// - `IDevice::GetSwapchainTexture` returns a valid handle for the
  ///   acquired backbuffer until `OnPresentEpilogue` is called.
  _MNEXUS_VAPI(void, OnPresentPrologue);

  /// Queues the swapchain's current backbuffer for presentation.
  ///
  /// ## Pre-conditions
  /// - `OnPresentPrologue` **MUST** have been called this frame.
  /// - All command lists referencing the swapchain texture **MUST** have
  ///   been submitted before this call.
  ///
  /// ## Post-conditions
  /// - The backbuffer is queued for presentation.
  /// - The swapchain texture handle is no longer valid for rendering.
  _MNEXUS_VAPI(void, OnPresentEpilogue);

  // ----------------------------------------------------------------------------------------------
  // Device

  /// Returns the `IDevice` associated with this instance.
  ///
  /// - Returns: A non-null `IDevice` pointer valid for the lifetime of this
  ///   `INexus` instance. The caller **MUST NOT** delete the returned pointer.
  _MNEXUS_VAPI(IDevice*, GetDevice);

protected:
  INexus() = default;
};

class IDevice {
public:
  virtual ~IDevice() = default;

  // ==============================================================================================
  // Synchronization Model
  //
  // mnexus uses a timeline-based synchronization model. Each queue maintains a
  // monotonically increasing timeline counter (`IntraQueueSubmissionId`).
  //
  // **Submission ordering**: Operations submitted to the same queue via
  // `QueueSubmitCommandList`, `QueueWriteBuffer`, and `QueueReadBuffer`
  // execute and complete in submission order (FIFO). An operation with a
  // higher `IntraQueueSubmissionId` completes no earlier than one with a
  // lower value on the same queue.
  //
  // **Timeline values**: Every queue operation returns an
  // `IntraQueueSubmissionId` representing the point on the queue's timeline
  // at which the operation's side-effects become visible. A valid timeline
  // value is always non-zero and strictly increasing within a queue.
  //
  // **Completion visibility**: When `QueueGetCompletedValue(queue_id)` returns V:
  // - All operations with timeline value <= V have completed and their
  //   side-effects are visible.
  //
  // **Blocking**: `QueueWait(queue_id, V)` blocks the calling thread until
  // `QueueGetCompletedValue(queue_id) >= V`.
  // ==============================================================================================

  // ----------------------------------------------------------------------------------------------
  // Queue
  //

  /// Returns the number of queue families supported by this device.
  ///
  /// - Returns: Queue family count (always >= 1).
  _MNEXUS_VAPI(uint32_t, QueueGetFamilyCount);

  /// Retrieves the description of a queue family.
  ///
  /// - `queue_family_index`: **MUST** be less than `QueueGetFamilyCount()`.
  /// - `out_desc`: Populated with the queue family's properties on success.
  /// - Returns: `MnBoolTrue` on success, `MnBoolFalse` if the index is
  ///   out of range.
  _MNEXUS_VAPI(MnBool32, QueueGetFamilyDesc,
    uint32_t queue_family_index,
    QueueFamilyDesc& out_desc
  );

  /// Submits a recorded command list to the specified queue for execution.
  ///
  /// Ownership of `command_list` transfers to the queue. The caller
  /// **MUST NOT** use, discard, or reference `command_list` after this call.
  ///
  /// - `queue_id`: **MUST** identify a valid queue.
  /// - `command_list`: **MUST** be non-null. **MUST** have been finalized
  ///   via `ICommandList::End`. **MUST NOT** have been previously submitted
  ///   or discarded.
  /// - Returns: An `IntraQueueSubmissionId`. All commands in the list
  ///   complete no later than when
  ///   `QueueGetCompletedValue(queue_id) >= returned value`.
  _MNEXUS_VAPI(IntraQueueSubmissionId, QueueSubmitCommandList,
    QueueId const& queue_id,
    ICommandList* command_list
  );

  /// Writes data from CPU memory into a GPU buffer.
  ///
  /// - `queue_id`: **MUST** identify a valid queue.
  /// - `buffer_handle`: **MUST** be a valid handle.
  /// - `buffer_offset`: Byte offset into the destination buffer.
  /// - `data`: **MUST** be non-null. **MUST** point to at least
  ///   `data_size_in_bytes` readable bytes.
  /// - `data_size_in_bytes`: **MUST** be a multiple of 4 bytes.
  ///   `buffer_offset + data_size_in_bytes` **MUST NOT** exceed the
  ///   buffer's size.
  /// - Returns: An `IntraQueueSubmissionId`. The write is visible to
  ///   subsequent GPU operations on the same queue.
  ///
  /// > **Note:** The WebGPU backend implicitly adds `CopyDst` usage to
  /// > buffers created with common usage flags (Vertex, Index, Uniform,
  /// > Storage), so callers do not need to specify `kTransferDst`
  /// > explicitly for `QueueWriteBuffer` targets.
  _MNEXUS_VAPI(IntraQueueSubmissionId, QueueWriteBuffer,
    QueueId const& queue_id,
    BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void const* data,
    uint32_t data_size_in_bytes
  );

  /// Reads data from a GPU buffer into a CPU-accessible destination.
  ///
  /// The read is asynchronous. Data at `dst` is not valid until the
  /// returned timeline value completes.
  ///
  /// - `queue_id`: **MUST** identify a valid queue.
  /// - `buffer_handle`: **MUST** have `kTransferSrc` or `kStorage` usage.
  /// - `buffer_offset`: **MUST** be 4-byte aligned.
  /// - `dst`: **MUST** remain valid until
  ///   `QueueGetCompletedValue(queue_id) >= returned value`.
  /// - `size_in_bytes`: **MUST** be 4-byte aligned.
  ///   `buffer_offset + size_in_bytes` **MUST NOT** exceed the buffer's
  ///   size.
  /// - Returns: An `IntraQueueSubmissionId`. Data at `dst` is valid once
  ///   `QueueGetCompletedValue(queue_id) >= returned value`.
  _MNEXUS_VAPI(IntraQueueSubmissionId, QueueReadBuffer,
    QueueId const& queue_id,
    BufferHandle buffer_handle,
    uint32_t buffer_offset,
    void* dst,
    uint32_t size_in_bytes
  );

  /// Returns the highest completed timeline value on the given queue.
  ///
  /// All operations that returned an `IntraQueueSubmissionId` <= this value
  /// have completed and their side-effects are visible.
  ///
  /// - `queue_id`: **MUST** identify a valid queue.
  /// - Returns: The highest completed `IntraQueueSubmissionId`, or 0 if no
  ///   operations have been submitted.
  _MNEXUS_VAPI(IntraQueueSubmissionId, QueueGetCompletedValue,
    QueueId const& queue_id
  );

  /// Blocks the calling thread until the given timeline value has completed.
  ///
  /// When this method returns, all operations with
  /// `IntraQueueSubmissionId` <= `value` are complete (e.g.
  /// `QueueReadBuffer` destinations are populated, GPU writes are committed).
  ///
  /// - `queue_id`: **MUST** identify a valid queue.
  /// - `value`: Timeline value to wait for. Passing 0 or an already-completed
  ///   value returns immediately.
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

  /// Creates a new command list in the recording state.
  ///
  /// Recording **MUST** be finalized by calling `ICommandList::End` before
  /// submission. The caller **MUST** eventually either:
  /// - Submit it via `QueueSubmitCommandList` (transfers ownership), or
  /// - Discard it via `DiscardCommandList`.
  ///
  /// - `desc`: `queue_family_index` **MUST** be less than
  ///   `QueueGetFamilyCount()`.
  /// - Returns: A non-null `ICommandList` pointer.
  _MNEXUS_VAPI(ICommandList*, CreateCommandList, CommandListDesc const& desc);

  /// Discards an `ICommandList` that was created but not submitted.
  /// **MUST NOT** be called on a command list that has already been submitted.
  _MNEXUS_VAPI(void, DiscardCommandList, ICommandList* command_list);

  //
  // Buffer
  //

  /// Creates a GPU buffer.
  ///
  /// - `desc.usage`: **MUST** have at least one usage flag set.
  /// - `desc.size_in_bytes`: **MUST** be greater than 0.
  /// - Returns: A valid `BufferHandle`.
  _MNEXUS_VAPI(BufferHandle, CreateBuffer,
    BufferDesc const& desc
  );

  /// Destroys a buffer.
  ///
  /// - `buffer_handle`: **MUST** be a valid handle returned by
  ///   `CreateBuffer`. After this call, the handle is invalid and
  ///   **MUST NOT** be used for any further API calls.
  _MNEXUS_VAPI(void, DestroyBuffer,
    BufferHandle buffer_handle
  );

  /// Retrieves the description of a buffer.
  ///
  /// - `buffer_handle`: **MUST** be a valid handle.
  /// - `out_desc`: Populated with the buffer's creation description.
  _MNEXUS_VAPI(void, GetBufferDesc,
    BufferHandle buffer_handle,
    BufferDesc& out_desc
  );

  //
  // Texture
  //

  /// Returns a handle to the current swapchain backbuffer texture.
  ///
  /// The returned handle is only valid between `OnPresentPrologue` and
  /// `OnPresentEpilogue`. It **MUST NOT** be passed to `DestroyTexture`.
  ///
  /// - Returns: A valid `TextureHandle` for the current backbuffer, or an
  ///   invalid handle if no surface is configured or the instance is
  ///   headless.
  _MNEXUS_VAPI(TextureHandle, GetSwapchainTexture);

  /// Creates a GPU texture.
  ///
  /// - `desc.usage`: **MUST** have at least one usage flag set.
  /// - `desc.format`: **MUST NOT** be `kUndefined`.
  /// - `desc.width`, `desc.height`: **MUST** be greater than 0.
  /// - `desc.mip_level_count`, `desc.array_layer_count`: **MUST** be >= 1.
  /// - Returns: A valid `TextureHandle`.
  _MNEXUS_VAPI(TextureHandle, CreateTexture,
    TextureDesc const& desc
  );

  /// Destroys a texture.
  ///
  /// - `texture_handle`: **MUST** be a valid handle returned by
  ///   `CreateTexture`. **MUST NOT** be the swapchain texture. After this
  ///   call, the handle is invalid and **MUST NOT** be used for any further
  ///   API calls.
  _MNEXUS_VAPI(void, DestroyTexture,
    TextureHandle texture_handle
  );

  /// Retrieves the description of a texture.
  ///
  /// - `texture_handle`: **MUST** be a valid handle.
  /// - `out_desc`: Populated with the texture's description.
  _MNEXUS_VAPI(void, GetTextureDesc,
    TextureHandle texture_handle,
    TextureDesc& out_desc
  );

  //
  // Sampler
  //

  /// Creates a texture sampler.
  ///
  /// - Returns: A valid `SamplerHandle`.
  _MNEXUS_VAPI(SamplerHandle, CreateSampler,
    SamplerDesc const& desc
  );

  /// Destroys a sampler.
  ///
  /// - `sampler_handle`: **MUST** be a valid handle. After this call, the
  ///   handle is invalid and **MUST NOT** be used for any further API calls.
  _MNEXUS_VAPI(void, DestroySampler,
    SamplerHandle sampler_handle
  );

  //
  // ShaderModule
  //

  /// Creates a shader module from source code.
  ///
  /// - `desc.code_ptr`: **MUST** be a valid pointer (cast to `uint64_t`)
  ///   to shader bytecode. For SPIR-V, the data **MUST** be valid SPIR-V.
  /// - `desc.code_size_in_bytes`: **MUST** be > 0. For SPIR-V, **MUST**
  ///   be a multiple of 4.
  /// - Returns: A valid `ShaderModuleHandle`.
  _MNEXUS_VAPI(ShaderModuleHandle, CreateShaderModule,
    ShaderModuleDesc const& desc
  );

  /// Destroys a shader module.
  ///
  /// - `shader_module_handle`: **MUST** be a valid handle. After this call,
  ///   the handle is invalid and **MUST NOT** be used for any further API
  ///   calls.
  _MNEXUS_VAPI(void, DestroyShaderModule,
    ShaderModuleHandle shader_module_handle
  );

  //
  // Program
  //

  /// Creates a program by linking one or more shader modules.
  ///
  /// - `desc.shader_modules`: **MUST** contain at least one valid handle.
  /// - Returns: A valid `ProgramHandle`.
  ///
  /// > **Note:** Shader reflection extracts and merges bind group layouts
  /// > across all provided modules.
  _MNEXUS_VAPI(ProgramHandle, CreateProgram,
    ProgramDesc const& desc
  );

  /// Destroys a program.
  ///
  /// - `program_handle`: **MUST** be a valid handle. After this call, the
  ///   handle is invalid and **MUST NOT** be used for any further API calls.
  _MNEXUS_VAPI(void, DestroyProgram,
    ProgramHandle program_handle
  );

  //
  // ComputePipeline
  //

  /// Creates a compute pipeline from a compute shader module.
  ///
  /// - `desc.shader_module`: **MUST** be a valid `ShaderModuleHandle`
  ///   containing a compute shader entry point.
  /// - Returns: A valid `ComputePipelineHandle`.
  _MNEXUS_VAPI(ComputePipelineHandle, CreateComputePipeline,
    ComputePipelineDesc const& desc
  );

  /// Destroys a compute pipeline.
  ///
  /// - `compute_pipeline_handle`: **MUST** be a valid handle. After this
  ///   call, the handle is invalid and **MUST NOT** be used for any further
  ///   API calls.
  _MNEXUS_VAPI(void, DestroyComputePipeline,
    ComputePipelineHandle compute_pipeline_handle
  );

  //
  // RenderPipeline
  //

  /// Creates a pre-built render pipeline with explicit fixed-function state.
  ///
  /// Alternative to the auto-generation path where state is accumulated via
  /// `ICommandList` setters and resolved at draw time.
  ///
  /// - `desc.program`: **MUST** be a valid `ProgramHandle`.
  /// - Returns: A valid `RenderPipelineHandle`.
  _MNEXUS_VAPI(RenderPipelineHandle, CreateRenderPipeline,
    RenderPipelineDesc const& desc
  );

  //
  // Device Capability
  //

  /// Returns capability flags for the GPU adapter.
  _MNEXUS_VAPI(AdapterCapability, GetAdapterCapability);

  /// Retrieves human-readable information about the GPU adapter.
  ///
  /// - `out_info`: Populated with device name, vendor, architecture,
  ///   description, and numeric IDs.
  _MNEXUS_VAPI(void, GetAdapterInfo, AdapterInfo& out_info);

protected:
  IDevice() = default;
};

class ICommandList {
public:
  virtual ~ICommandList() = default;

  /// Finalizes command recording.
  ///
  /// Any active render pass or compute pass is implicitly ended. The caller
  /// **MUST NOT** record further commands after this call. The command list
  /// is ready for submission via `IDevice::QueueSubmitCommandList` or
  /// disposal via `IDevice::DiscardCommandList`.
  ///
  /// ## Pre-conditions
  /// - The command list **MUST** be in the recording state (`End` has not
  ///   already been called).
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

  /// Copies pixel data from a buffer into a texture.
  ///
  /// The copy always starts from texture origin (0, 0, base_array_layer).
  ///
  /// - `src_buffer_handle`: **MUST** have been created with `kTransferSrc`
  ///   usage.
  /// - `src_buffer_offset`: **MUST** be a multiple of the texel block byte
  ///   size of the destination texture format (e.g. 4 for
  ///   `kR8G8B8A8_UNORM`). For depth-stencil formats, **MUST** be a
  ///   multiple of 4. The buffer **MUST** be large enough to hold the
  ///   tightly packed pixel data starting at this offset.
  /// - `dst_texture_handle`: **MUST** have been created with `kTransferDst`
  ///   usage. **MUST** have sample count 1 (multisampled textures are not
  ///   supported).
  /// - `dst_subresource_range`: `base_mip_level` **MUST** be less than the
  ///   texture's mip level count.
  /// - `copy_extent`: **MUST** fit within the destination texture's mip
  ///   level dimensions. For compressed formats, width and height **MUST**
  ///   be multiples of the texel block dimensions.
  ///
  /// > **Note:** The source buffer is assumed to contain tightly packed
  /// > pixel data (no row padding). The WebGPU backend handles 256-byte
  /// > `bytesPerRow` alignment internally via compute-shader repacking or
  /// > row-by-row fallback.
  _MNEXUS_VAPI(void, CopyBufferToTexture,
    BufferHandle src_buffer_handle,
    uint32_t src_buffer_offset,
    TextureHandle dst_texture_handle,
    TextureSubresourceRange const& dst_subresource_range,
    Extent3d const& copy_extent
  );

  /// Copies pixel data from a texture into a buffer.
  ///
  /// The copy always starts from texture origin (0, 0, base_array_layer).
  ///
  /// - `src_texture_handle`: **MUST** have been created with `kTransferSrc`
  ///   usage. **MUST** have sample count 1 (multisampled textures are not
  ///   supported).
  /// - `src_subresource_range`: `base_mip_level` **MUST** be less than the
  ///   texture's mip level count.
  /// - `dst_buffer_handle`: **MUST** have been created with `kTransferDst`
  ///   usage. The buffer **MUST** be large enough to hold the copy data
  ///   with 256-byte aligned row stride.
  /// - `dst_buffer_offset`: **MUST** be a multiple of the texel block byte
  ///   size of the source texture format (e.g. 4 for `kR8G8B8A8_UNORM`).
  ///   For depth-stencil formats, **MUST** be a multiple of 4.
  /// - `copy_extent`: **MUST** fit within the source texture's mip level
  ///   dimensions. For compressed formats, width and height **MUST** be
  ///   multiples of the texel block dimensions.
  ///
  /// > **Note:** The data written to the destination buffer uses a row
  /// > stride of `Align(width_in_blocks * texel_block_bytes, 256)`. Rows
  /// > may be padded to a 256-byte boundary. Callers that read the buffer
  /// > back (e.g. via `QueueReadBuffer`) must account for this padding
  /// > when the image width is not 256-byte aligned.
  _MNEXUS_VAPI(void, CopyTextureToBuffer,
    TextureHandle src_texture_handle,
    TextureSubresourceRange const& src_subresource_range,
    BufferHandle dst_buffer_handle,
    uint32_t dst_buffer_offset,
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

// ----------------------------------------------------------------------------------------------------
// C API
//

typedef struct MnNexus_T*       MnNexus;
typedef struct MnDevice_T*      MnDevice;
typedef struct MnCommandList_T* MnCommandList;

#if defined(__cplusplus)
extern "C" {
#endif

MNEXUS_NO_THROW MnNexus  MNEXUS_CALL MnNexusCreate(MnNexusDesc const* desc);
MNEXUS_NO_THROW void     MNEXUS_CALL MnNexusDestroy(MnNexus nexus);
MNEXUS_NO_THROW MnDevice MNEXUS_CALL MnNexusGetDevice(MnNexus nexus);

MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceGetAdapterInfo(
  MnDevice device, MnAdapterInfo* out_info);

// ----------------------------------------------------------------------------------------------------
// IDevice: Resource creation / destruction
//

MNEXUS_NO_THROW MnResourceHandle MNEXUS_CALL MnDeviceCreateTexture(
  MnDevice device, MnTextureDesc const* desc);
MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceDestroyTexture(
  MnDevice device, MnResourceHandle handle);

MNEXUS_NO_THROW MnResourceHandle MNEXUS_CALL MnDeviceCreateBuffer(
  MnDevice device, MnBufferDesc const* desc);
MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceDestroyBuffer(
  MnDevice device, MnResourceHandle handle);

MNEXUS_NO_THROW MnResourceHandle MNEXUS_CALL MnDeviceCreateShaderModule(
  MnDevice device, MnShaderModuleDesc const* desc);
MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceDestroyShaderModule(
  MnDevice device, MnResourceHandle handle);

MNEXUS_NO_THROW MnResourceHandle MNEXUS_CALL MnDeviceCreateProgram(
  MnDevice device, MnProgramDesc const* desc);
MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceDestroyProgram(
  MnDevice device, MnResourceHandle handle);

MNEXUS_NO_THROW MnCommandList MNEXUS_CALL MnDeviceCreateCommandList(
  MnDevice device, MnCommandListDesc const* desc);

// ----------------------------------------------------------------------------------------------------
// IDevice: Queue operations
//

MNEXUS_NO_THROW MnIntraQueueSubmissionId MNEXUS_CALL MnDeviceQueueWriteBuffer(
  MnDevice device, MnQueueId const* queue_id,
  MnResourceHandle buffer, uint32_t offset,
  void const* data, uint32_t size);

MNEXUS_NO_THROW MnIntraQueueSubmissionId MNEXUS_CALL MnDeviceQueueSubmitCommandList(
  MnDevice device, MnQueueId const* queue_id, MnCommandList command_list);

MNEXUS_NO_THROW MnIntraQueueSubmissionId MNEXUS_CALL MnDeviceQueueReadBuffer(
  MnDevice device, MnQueueId const* queue_id,
  MnResourceHandle buffer, uint32_t offset,
  void* dst, uint32_t size);

MNEXUS_NO_THROW void MNEXUS_CALL MnDeviceQueueWait(
  MnDevice device, MnQueueId const* queue_id, MnIntraQueueSubmissionId value);

// ----------------------------------------------------------------------------------------------------
// ICommandList
//

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListBeginRenderPass(
  MnCommandList cl, MnRenderPassDesc const* desc);
MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListEndRenderPass(MnCommandList cl);

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListBindRenderProgram(
  MnCommandList cl, MnResourceHandle program);

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListSetVertexInputLayout(
  MnCommandList cl,
  MnVertexInputBindingDesc const* bindings, uint32_t binding_count,
  MnVertexInputAttributeDesc const* attributes, uint32_t attribute_count);

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListBindVertexBuffer(
  MnCommandList cl, uint32_t binding, MnResourceHandle buffer, uint64_t offset);

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListDraw(
  MnCommandList cl, uint32_t vertex_count, uint32_t instance_count,
  uint32_t first_vertex, uint32_t first_instance);

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListCopyTextureToBuffer(
  MnCommandList cl,
  MnResourceHandle src_texture, MnTextureSubresourceRange const* src_range,
  MnResourceHandle dst_buffer, uint32_t dst_offset,
  MnExtent3d const* extent);

MNEXUS_NO_THROW void MNEXUS_CALL MnCommandListEnd(MnCommandList cl);

#if defined(__cplusplus)
} // extern "C"
#endif
