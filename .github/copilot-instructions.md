# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`CLAUDE.md` is a symlink to `.github/copilot-instructions.md`.

## Commit messages

See @.github/git-commit-instructions.md

## Language

All code comments MUST be written in English.

## Symbols

It is forbidden to use the full width forms of symbols that have counterparts in ASCII.
e.g. `()`, `:`, `,`, `0-9`

## Build

mnexus is a CMake-based C++23 static library. It is typically built as part of a parent project (e.g., wentos) rather than standalone.

When adding source files to `CMakeLists.txt`, list them in alphabetical order within each `set(...)` block.

Key CMake options:
- `MNEXUS_ENABLE_BACKEND_WGPU` - Enable WebGPU backend (default: ON)
- `MNEXUS_ENABLE_BACKEND_VULKAN` - Enable Vulkan backend (default: ON, ignored on Emscripten). Requires `VULKAN_SDK` environment variable
- `MNEXUS_ENABLE_DAWN` - Enable Dawn for WebGPU on native platforms (default: ON, ignored on Emscripten)
- `MNEXUS_ENABLE_TINT_ON_WEB` - Enable Tint SPIR-V to WGSL conversion on Emscripten (default: ON)
- `MNEXUS_BUILD_TESTS` - Build test executables (default: OFF)

Compiler warnings: `-Wall -Wextra -Werror` (GCC/Clang), `/W4` (MSVC). Thread-safety analysis (`-Werror=thread-safety`) is enabled on Clang/GCC.

**Dear ImGui integration:** if the consuming project defines a `dear_imgui` CMake target, mnexus links it transitively (`PUBLIC`) and defines `MNEXUS_HAVE_DEAR_IMGUI=1` for downstream code; otherwise it's `0`. mnexus does not vendor Dear ImGui itself.

**Emscripten note:** mnexus propagates `--use-port=emdawnwebgpu` as PUBLIC but does NOT propagate `--closure=1`. Final binaries that want Closure optimization (e.g., wentos-desktop) must specify `--closure=1` on their own target. This is because Closure conflicts with ASYNCIFY + exception handling (`_setThrew` undeclared).

## Architecture

mnexus is a graphics abstraction layer providing a unified API over WebGPU (via Dawn on native platforms, or native WebGPU on Emscripten/Web) and Vulkan. The WebGPU backend is fully implemented. The Vulkan backend records render, compute, and transfer commands end-to-end: explicit `TextureBarrier`, `ClearTexture`, `CopyBufferToTexture`, `BlitTexture`, dynamic render passes (`VK_KHR_dynamic_rendering`) with the auto-generation render-pipeline path (cache + state-event log), `Draw` / `DrawIndexed`, `DispatchCompute`, and full descriptor binding (uniform buffer, storage buffer, sampled texture, sampler) via the shared `DescriptorSetAllocator` / `DescriptorSetBinder`. A few `IDevice` facade methods remain stubbed (`STUB_NOT_IMPLEMENTED`); see `STUB_NOT_IMPLEMENTED` references for the current set.

### Core Interfaces (`src/mnexus/public/mnexus.h`)

- `INexus` - Main entry point; static `Create()` / `Destroy()`, surface lifecycle, presentation, and device access. `EnumerateBackends()` returns available backends at runtime
- `IDevice` - Resource creation (buffers, textures, samplers, shaders, programs, pipelines) and command submission. Provides queue operations (`QueueSubmitCommandList`, `QueueWriteBuffer`, `QueueReadBuffer`, `QueueWaitIdle`), device capabilities (`GetAdapterCapability`, `GetClipSpaceConvention`, `GetAdapterInfo`), and PSO cache diagnostics (`GetRenderPipelineCacheSnapshot`)
- `ICommandList` - Command recording for render, compute, and transfer operations. Thread-affine (all recording must happen on the creating thread). Supports debug markers (`PushDebugGroup` / `PopDebugGroup`), explicit pipeline binding, auto-generation render state setters, explicit resource transitions via `TextureBarrier`, and `GetStateEventLog()` for per-command-list PSO diagnostics
- `Texture` - RAII wrapper around texture handles

### Resource Barriers (`ICommandList::TextureBarrier`)

Resource state transitions are caller-driven. The backend never auto-transitions a texture inside a transfer / dispatch / render-pass op — callers must emit a `TextureBarrier` first.

- `mnexus::ResourceBarrierState` (in `types.h`): abstract destination state — `kIndirectArgument`, `kIndexBuffer`, `kVertexBuffer`, `kUniformBuffer`, `kReadOnly`, `kAttachment`, `kUnorderedAccess`, `kTransferSrc`, `kTransferDst`.
- `mnexus::ResourceBarrierStageFlags`: pipeline-stage mask — `kDrawIndirectInput`, `kVertexInput`, `kVertexShader`, `kEarlyFragmentTests`, `kFragmentShader`, `kLateFragmentTests`, `kColorAttachmentOutput`, `kComputeIndirectInput`, `kComputeShader`, `kTransfer`, plus presets `kFragmentTestsBits` / `kGraphicsBits` / `kComputeBits`.

State contracts the backend assumes at op entry (documented on each method in `mnexus.h`):
- `ClearTexture` / `CopyBufferToTexture` / `BlitTexture(dst)` — `kTransferDst` (stage `kTransfer`).
- `BlitTexture(src)` — `kTransferSrc` (stage `kTransfer`).
- `BeginRenderPass` color/depth attachments — `kAttachment` (stage `kColorAttachmentOutput` for color, `kEarlyFragmentTests` / `kLateFragmentTests` for depth).
- `BindSampledTexture` — `kReadOnly` (stage `kFragmentShader` / etc.).

The Vulkan backend queues transitions via `command/image_layout_tracker.h` and flushes them through `command/pending_pipeline_barrier.h` at the start of every transfer / dispatch op and at `BeginRenderPass` — never inside a render pass instance. The WebGPU backend implements `TextureBarrier` as a no-op (WebGPU manages resource state implicitly).

Stage-aware access derivation: in the Vulkan backend, `ToVkAccessFlags2(state, stage_flags)` masks attachment access bits by the stage mask so `kAttachment` + `kColorAttachmentOutput` emits only `COLOR_ATTACHMENT_R/W`, while `kAttachment` + fragment-tests emits only `DEPTH_STENCIL_ATTACHMENT_R/W` — required to satisfy `VUID-VkImageMemoryBarrier2-dstAccessMask-*`.

### Public Utility (`src/mnexus/public/container/array_proxy.h`)

`ArrayProxy<T>` - Non-owning span-like view used throughout the public API for passing arrays without requiring a specific container type. Constructible from raw pointer+count, `std::vector`, `std::array`, `std::span`, and `std::initializer_list`.

Public API methods should be documented with `///` doc comments. Use RFC 2119 keywords (MUST, MUST NOT, SHOULD, MAY, etc.) to state normative requirements on each parameter. These constraints define the API contract that all backend implementations must satisfy. See `CopyBufferToTexture` / `CopyTextureToBuffer` for reference.

### Headless Mode

`INexus` supports headless operation (no surface/swapchain) for compute, transfer, and offscreen rendering workloads.

```cpp
mnexus::INexus* nexus = mnexus::INexus::Create({.headless = true});
mnexus::IDevice* device = nexus->GetDevice();
// Use device for compute, buffer read/write, offscreen render-to-texture, etc.
nexus->Destroy();
```

- The GPU device is created immediately, independent of any surface.
- All `IDevice` operations (buffers, textures, samplers, shaders, pipelines, command lists, queue ops) work without a surface.
- Surface/presentation methods (`OnSurfaceRecreated`, `OnSurfaceDestroyed`, `OnPresentPrologue`, `OnPresentEpilogue`) must NOT be called on a headless instance (guarded by assertions).
- `OnDisplayChanged` is a silent no-op in headless mode.

### Surface Resize

On desktop platforms, `OnSurfaceDestroyed()` + `OnSurfaceRecreated()` is called on every window resize. The WebGPU backend keeps the `wgpu::Surface` alive across this sequence when the native window handle hasn't changed (i.e. the same HWND). Only the surface configuration (dimensions) is updated. This avoids destroying and recreating the underlying DXGI swapchain on every resize, which can fail on Dawn's D3D12 backend if the old swapchain hasn't been fully released yet.

When the native window handle differs (e.g. Android background/foreground cycle where the `ANativeWindow` is destroyed), the old surface is destroyed and a new one is created.

Surface dimensions are clamped to a minimum of 1x1 to prevent invalid WebGPU surface configuration (e.g. if `GetClientRect` returns 0x0 during a minimize transition).

### C/C++ FFI Type Layer (`src/mnexus/public/types.h`)

Types are defined in two layers for FFI compatibility:
- **C types** (outside `#if defined(__cplusplus)`): `Mn`-prefixed types (`MnBool32`, `MnResourceHandle`, `MnBufferDesc`, `MnTextureDesc`, `MnShaderModuleDesc`, `MnProgramDesc`, `MnComputePipelineDesc`, `MnRenderPipelineDesc`, `MnBindGroupLayoutDesc`, `MnPipelineLayoutDesc`, etc.) usable from C and other languages
- **C++ types** (in `namespace mnexus`): Type-safe wrappers (`BufferHandle`, `TextureHandle`, `ShaderModuleHandle`, `ProgramHandle`, `ComputePipelineHandle`, etc.) with `static_assert` ensuring ABI compatibility with C counterparts

### PSO Observability (`src/mnexus/public/render_state_event_log.h`, `render_pipeline_state_snapshot.h`)

A per-command-list structured event log for debugging the auto-generated render pipeline (PSO) path. Each event carries a full `RenderPipelineStateSnapshot` so the complete pipeline state can be inspected at any recorded point without replay.

**Public types (all in `mnexus` namespace):**
- `RenderStateEventTag` - Enum of event kinds (BeginRenderPass, SetCullMode, PsoResolved, DrawIndexed, etc.)
- `RenderStateEvent` - Tag + full state snapshot + PSO hash/cache-hit (for `kPsoResolved`)
- `RenderStateEventLog` - Container with `SetEnabled` / `Record` / `RecordPso` / `GetEvent` / `Clear`. Recording is a no-op when disabled (null-pointer + bool branch per setter)
- `RenderPipelineStateSnapshot` - Strongly-typed representation of complete pipeline state (native enum types, not packed uint8_t). Mirrors the internal `RenderPipelineCacheKey` but is human-inspectable

**Usage pattern:**
```cpp
auto& log = command_list->GetStateEventLog();
log.SetEnabled(true);
// ... record rendering commands ...
for (uint32_t i = 0; i < log.GetCount(); ++i) {
  auto const& ev = log.GetEvent(i);
  // ev.tag, ev.state (full snapshot), ev.pso_hash, ev.cache_hit
}
log.Clear();
```

**Text formatting** is available via static methods on `RenderPipelineStateTracker` (private header): `FormatSnapshot()` and `FormatDiff()`. These take the public `RenderPipelineStateSnapshot` type.

### Backend Structure (`src/mnexus/private/`)

- `binding/` - Backend-agnostic bind group state tracking
  - `BindGroupStateTracker` - Tracks current bindings per group with dirty flags
  - `BindGroupCacheKey` - Hashable descriptor for bind group deduplication
- `pipeline/` - Backend-agnostic pipeline scaffolding
  - `TPipelineLayoutCache<TLayout>` - Thread-safe hash-based pipeline layout cache; keyed by `PipelineLayoutCacheKey` (bind group layout structure from shader reflection). Programs with identical bind group configurations share a single backend layout object
  - `RenderPipelineStateTracker` - Tracks mutable render state (program, vertex input, fixed-function) on a command list; assembles `RenderPipelineCacheKey` at draw time. Also provides `BuildSnapshot()` (converts packed internal state to public `RenderPipelineStateSnapshot`), `FormatSnapshot()` / `FormatDiff()` (text formatting), and `SetEventLog()` (wires structured event recording)
  - `TRenderPipelineCache<TPipeline>` - Thread-safe hash-based render pipeline cache with shared/exclusive locking. `FindOrInsert` requires a mandatory `bool* out_cache_hit` output parameter
  - `RenderPipelineCacheKey` - Hashable key combining program, vertex layout, fixed-function state, and render target formats
  - `PerDrawFixedFunctionStaticState` / `PerAttachmentFixedFunctionStaticState` - Packed uint8 structs for fast memcmp/memhash
  - `RenderStateEventLog` (impl) - Implementation of the public `RenderStateEventLog` class
- `builtin_shader/` - Slang source files and pre-compiled SPIR-V (+ C header embeddings) for internal operations: blit, buffer row repack, full-screen quad
- `backend-iface/` - Abstract backend interface (`IBackend`): surface lifecycle, presentation, and device access
- `backend-webgpu/` - WebGPU implementation
  - `backend-webgpu.cpp/.h` - Main backend: `IBackendWebGpu`, `MnexusDeviceWebGpu`
  - `backend-webgpu-binding.cpp/.h` - Bind group flush and invalidation
  - `backend-webgpu-buffer.cpp/.h` - Buffer resource pool (`BufferHot`, `BufferCold`)
  - `backend-webgpu-command_list.cpp/.h` - `MnexusCommandListWebGpu` recording implementation
  - `backend-webgpu-compute_pipeline.cpp/.h` - Compute pipeline resource pool
  - `backend-webgpu-layout.cpp/.h` - BindGroupLayout and PipelineLayout resource pools
  - `backend-webgpu-render_pipeline.cpp/.h` - Render pipeline creation and cache integration
  - `backend-webgpu-shader.cpp/.h` - Shader module and program resource pools; SPIR-V to WGSL conversion via Tint
  - `backend-webgpu-texture.cpp/.h` - Texture resource pool (`TextureHot`, `TextureCold`)
  - `blit_texture.cpp/.h` - GPU-based texture blit using built-in full-screen quad + sampling shaders
  - `buffer_row_repack.cpp/.h` - Compute-shader based buffer row repacking for WebGPU's 256-byte `bytesPerRow` alignment
  - `builtin_shader.cpp/.h` - Runtime loader for embedded SPIR-V from `builtin_shader/`
  - `shader_module.cpp/.h` - Shader module management (SPIR-V storage, WGSL conversion)
  - `types_bridge.cpp/.h` - Conversion between mnexus types and wgpu types
  - `include_dawn.h` - Platform-appropriate WebGPU header inclusion
  - `webgpu_cpp_print.h` - `operator<<` for wgpu types (debug output)
  - `webgpu_format.h` - spdlog formatter specializations for wgpu types
- `backend-vulkan/` - Vulkan implementation (records render / compute / transfer end-to-end; a few `IDevice` facade methods still stubbed)
  - `backend-vulkan.cpp/.h` - Main backend: `IBackendVulkan`, `MnexusDeviceVulkan`
  - `backend-vulkan-buffer.cpp/.h` - Buffer resource pool (`BufferHot`, `BufferCold`); VMA-backed
  - `backend-vulkan-command_list.cpp/.h` - `IMnexusCommandListVulkan` (interface + `Create` factory) — top-level command recording. Owns an `ICommandEncoder`, an `ImageLayoutTracker`, a `PendingPipelineBarrier`, and a `RenderPipelineStateTracker`. `End()` transitions all touched textures back to their default state and flushes the final barrier. `BeginRenderPass` flushes pending barriers before recording the dynamic-rendering instance
  - `backend-vulkan-compute_pipeline.cpp/.h` - Compute pipeline resource pool
  - `backend-vulkan-render_pipeline.cpp/.h` - Render pipeline resource pool + `CreateVulkanRenderPipelineFromCacheKey` (used by `TRenderPipelineCache`)
  - `backend-vulkan-shader.cpp/.h` - Shader module / program resource pools
  - `backend-vulkan-texture.cpp/.h` - Texture resource pool (`TextureHot` / `TextureCold`)
  - `command/` - Per-command-list recording infrastructure
    - `command_encoder.cpp/.h` - `ICommandEncoder` interface + factory; concrete encoder lives in the `.cpp`'s anonymous namespace. Wraps a `VkCommandBuffer` + dirty tracking for descriptor-set resolution, dynamic rendering begin/end, and pipeline-binding operations
    - `image_layout_tracker.cpp/.h` - Per-(image, mip, layer) layout tracker. `Transition()` queues a transition; `FlushPendingTransitions()` emits matching `VkImageMemoryBarrier2KHR`s into a `PendingPipelineBarrier`. `TransitionAllToDefaults()` returns each touched texture to the format-derived default state (used by `End()`)
    - `pending_pipeline_barrier.cpp/.h` - Accumulator for `VkImageMemoryBarrier2KHR` (and global memory barriers); `FlushAndClear()` records `vkCmdPipelineBarrier2KHR`
    - `fwd.h` - Forward declarations for the `command/` package
  - `descriptor/` - Descriptor set lifetime + binding
    - `descriptor_set_allocator.cpp/.h` - `IDescriptorSetAllocator` interface + factory. Per-layout pool with deferred free; safe under shutdown
    - `descriptor_set_binder.cpp/.h` - Per-command-list descriptor-set state with dirty tracking. `AssumeRenderPipelineLayout` / `AssumeComputePipelineLayout` are called eagerly at `BindRenderProgram` / `BindExplicitComputePipeline` so descriptor writes route to the right set
    - `descriptor_set_write.cpp/.h` - Descriptor write helpers
    - `fwd.h` - Forward declarations for the `descriptor/` package
  - `device/` - Device / instance / queue / staging
    - `vk-device.cpp/.h` - `IVulkanDevice` interface + factory. Logical device, VMA allocator, timeline semaphores, queue selection
    - `vk-device_helper.cpp/.h` - Shared device-construction helpers
    - `vk-queue.cpp/.h` - `IVulkanQueue` interface + factory. Per-queue submit / present / wait
    - `vk-instance.cpp/.h` - Vulkan instance + debug messenger
    - `vk-physical_device.cpp/.h` - Physical-device selection and queue-family enumeration
    - `vk-staging.cpp/.h` - Staging buffer pool for CPU↔GPU transfers
    - `vk-deferred_destroyer.h` - Interface for deferred destruction of GPU resources after submission completes
    - `fwd.h` - Forward declarations for the `device/` package
  - `object/` - Refcounted wrappers around Vulkan handles (`vk-object-buffer.h`, `vk-object-image.h`, `vk-object-sampler.h`, `vk-object-shader_module.h`, `vk-object-render_pipeline.h`, `vk-object-compute_pipeline.h`, `vk-object-pipeline_layout.h`, `vk-object-descriptor_set.h`, `vk-object-descriptor_set_layout.h`, `vk-object-command_pool.h`, `vk-object.h`, `fwd.h`)
  - `resource/` - Resource storage / type bridging
    - `resource_storage.cpp/.h` - Centralized resource storage (textures / buffers / samplers / shader modules / programs / pipelines / pipeline-layout cache / render-pipeline cache)
    - `types_bridge.cpp/.h` - mnexus ↔ Vulkan conversions (formats, blend, stencil, vertex layout, image aspect / subresource, `ToVkPipelineStageFlags2`, stage-aware `ToVkAccessFlags2`, `ToVkImageLayout`, etc.)
    - `image_view_cache.h` - Per-texture image-view cache
    - `shader_module.cpp/.h` - SPIR-V shader module helper used by the program / pipeline construction paths
    - `fwd.h` - Forward declarations for the `resource/` package
  - `wsi/` - Window-system integration (`vk-wsi_surface.cpp/.h`, `fwd.h`)
  - `depend/` - Vulkan header includes (`vulkan.h`, `vulkan_fwd.h`) and VMA integration (`vulkan_vma.cpp/.h`)

### Shader Reflection and Conversion (`src/mnexus/private/shader/`)

SPIR-V shader introspection via SPIRV-Reflect:

- `ShaderModuleReflection` (`reflection.cpp/.h`) - Reflects a single SPIR-V module into `BindGroupLayout` / `BindGroupLayoutEntry` structures (sorted by set/binding). Each entry records type, count, and `writable` flag (from the `NonWritable` SPIR-V decoration).
- `MergedPipelineLayout` (`reflection.cpp/.h`) - Incrementally merges bind group layouts from multiple shader modules (e.g., vertex + fragment). Detects conflicting bindings (same set/binding but different type/count) and OR-merges the `writable` flag.

SPIR-V to WGSL conversion via Tint (required for WebGPU, which only accepts WGSL):

- `wgsl.cpp/.h` - `ConvertSpirvToWgsl()` wrapper around Tint's SPIR-V reader. Enabled by `MNEXUS_INTERNAL_USE_TINT` (set when Dawn or `MNEXUS_ENABLE_TINT_ON_WEB` is active). `InitializeWgslConverter()` / `ShutdownWgslConverter()` manage Tint's global state.

### Resource Management (`src/mnexus/private/resource_pool/`)

Resources use a generational pool pattern with hot/cold data separation:
- `GenerationalPool` - Base generational index allocator with handle validation
- `TResourceGenerationalPool<THot, TCold>` - Thread-safe wrapper with shared mutex, separating frequently-accessed data (Hot) from metadata (Cold)

Each resource type defines `{Resource}Hot`, `{Resource}Cold` structs and a `{Resource}ResourcePool` typedef.

### Include Patterns

Files follow a consistent header ordering:
```cpp
// TU header --------------------------------------------
// c++ headers ------------------------------------------
// external headers -------------------------------------
// platform detection header ----------------------------
// conditional platform headers -------------------------
// public project headers -------------------------------
// project headers --------------------------------------
```

### Tests

Test executables live under `tests/`, each in its own subdirectory. Controlled by the `MNEXUS_BUILD_TESTS` CMake option (default: OFF).

#### Test harness (`tests/harness/`)

A shared test harness provides `main()`, Logger init/shutdown, and `MnTestWritePng()`. Each test implements `MnTestMain()` instead of `main()`:

```c
// C
#include "mnexus_test_harness.h"
int MnTestMain(int argc, char** argv) { /* ... */ return 0; }

// C++
#include "mnexus_test_harness.h"
extern "C" int MnTestMain(int, char**) { /* ... */ return 0; }
```

`MnTestWritePng()` abstracts PNG output: writes to file on native, triggers a browser download on Emscripten.

On Emscripten, the harness links ASYNCIFY flags and skips Logger shutdown (EXIT_RUNTIME=0 keeps the runtime alive). See `doc/memo_emscripten_asyncify.md` for constraints on main-loop tests.

#### Adding a test

`tests/CMakeLists.txt` provides a helper function:

```cmake
mnexus_add_test(<target_name> <source_file> ...)
```

This handles linking to `mnexus_test_harness` (which transitively links `mnexus`), auto-detecting C vs C++ sources for `cxx_std_23`, copying backend-dependent DLLs on Windows, and setting `.html` suffix on Emscripten. Individual test `CMakeLists.txt` files should be a single `mnexus_add_test()` call.

Test targets are grouped under the `mnexus/tests` solution folder in Visual Studio.

Commits to `master` must not break existing tests. If a change includes a breaking API change, the same commit must update all affected tests so that they build and run successfully.

### Synchronization (`src/mnexus/private/sync/`)

- `ResourceSync` (`resource_sync.cpp/.h`) - Queue timeline management. Tracks per-queue submission serials and provides helpers for cross-queue resource synchronization.

### Dependencies

- `mbase` - Base utilities (logging, assertions, `SmallVector`, `ArrayProxy`, thread safety annotations, platform detection, `BitFlags`)
- `thirdparty/dawn` - Google Dawn WebGPU implementation (native platforms only). Tint (SPIR-V to WGSL compiler) from Dawn is also used on Emscripten builds, since browser WebGPU only accepts WGSL
  - **Warning:** Dawn pulls in ~78 recursive submodules (ANGLE, SwiftShader, Vulkan SDK, Chromium infra, etc.). ANGLE's sub-submodules reference `chrome-internal.googlesource.com` which requires Google-internal credentials and will hang or prompt for auth.
  - To skip ANGLE (not needed for WebGPU): set `submodule.third_party/angle.update none` in **Dawn's** local git config, then run `GIT_TERMINAL_PROMPT=0 GIT_ASKPASS= git submodule update --recursive`.
  - Fork (Git client): "Update submodules after checkout" should be OFF to avoid automatic submodule updates on branch switch.
- `thirdparty/SPIRV-Reflect` - SPIR-V reflection library for extracting bind group layouts and decorations from shader modules
- `thirdparty/VulkanMemoryAllocator` - AMD VMA for Vulkan GPU memory allocation (Vulkan backend only)
- `volk` - Vulkan meta-loader for dynamic function dispatch (Vulkan backend only; linked but not vendored via `add_subdirectory` yet -- see TODO in `CMakeLists.txt`)
- `VULKAN_SDK` (environment variable) - Required for Vulkan backend builds; provides Vulkan headers
