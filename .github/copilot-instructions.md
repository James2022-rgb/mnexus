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
- `MNEXUS_ENABLE_DAWN` - Enable Dawn for WebGPU on native platforms (default: ON, ignored on Emscripten)
- `MNEXUS_ENABLE_TINT_ON_WEB` - Enable Tint SPIR-V to WGSL conversion on Emscripten (default: ON)
- `MNEXUS_BUILD_TESTS` - Build test executables (default: OFF)

Compiler warnings: `-Wall -Wextra -Werror` (GCC/Clang), `/W4` (MSVC). Thread-safety analysis (`-Werror=thread-safety`) is enabled on Clang/GCC.

**Emscripten note:** mnexus propagates `--use-port=emdawnwebgpu` as PUBLIC but does NOT propagate `--closure=1`. Final binaries that want Closure optimization (e.g., wentos-desktop) must specify `--closure=1` on their own target. This is because Closure conflicts with ASYNCIFY + exception handling (`_setThrew` undeclared).

## Architecture

mnexus is a graphics abstraction layer providing a unified API over WebGPU (via Dawn on native platforms, or native WebGPU on Emscripten/Web). Additional backends under consideration: Vulkan (primary), DX12 (secondary).

### Core Interfaces (`src/mnexus/public/mnexus.h`)

- `INexus` - Main entry point for surface lifecycle, presentation, and device access
- `IDevice` - Resource creation (buffers, textures, shaders, programs, pipelines, layouts) and command submission
- `ICommandList` - Command recording for render, compute, and transfer operations
- `Texture` - RAII wrapper around texture handles

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

### Backend Structure (`src/mnexus/private/`)

- `binding/` - Backend-agnostic bind group state tracking
  - `BindGroupStateTracker` - Tracks current bindings per group with dirty flags
  - `BindGroupCacheKey` - Hashable descriptor for bind group deduplication
- `pipeline/` - Backend-agnostic pipeline scaffolding
  - `TPipelineLayoutCache<TLayout>` - Thread-safe hash-based pipeline layout cache; keyed by `PipelineLayoutCacheKey` (bind group layout structure from shader reflection). Programs with identical bind group configurations share a single backend layout object
  - `RenderPipelineStateTracker` - Tracks mutable render state (program, vertex input, fixed-function) on a command list; assembles `RenderPipelineCacheKey` at draw time
  - `TRenderPipelineCache<TPipeline>` - Thread-safe hash-based render pipeline cache with shared/exclusive locking
  - `RenderPipelineCacheKey` - Hashable key combining program, vertex layout, fixed-function state, and render target formats
  - `PerDrawFixedFunctionStaticState` / `PerAttachmentFixedFunctionStaticState` - Packed uint8 structs for fast memcmp/memhash
- `builtin_shader/` - Embedded SPIR-V for internal operations (blit, buffer row repack, full-screen quad)
- `backend-iface/` - Abstract backend interface (`IBackend`)
- `backend-webgpu/` - WebGPU implementation
  - `backend-webgpu.cpp/.h` - Main backend: `IBackendWebGpu`, `MnexusDeviceWebGpu`, `MnexusCommandListWebGpu`
  - `backend-webgpu-buffer.cpp/.h` - Buffer resource pool (`BufferHot`, `BufferCold`)
  - `backend-webgpu-texture.cpp/.h` - Texture resource pool (`TextureHot`, `TextureCold`)
  - `backend-webgpu-shader.cpp/.h` - Shader module and program resource pools; SPIR-V to WGSL conversion via Tint
  - `backend-webgpu-compute_pipeline.cpp/.h` - Compute pipeline resource pool
  - `backend-webgpu-layout.cpp/.h` - BindGroupLayout and PipelineLayout resource pools
  - `types_bridge.cpp/.h` - Conversion between mnexus types and wgpu types
  - `include_dawn.h` - Platform-appropriate WebGPU header inclusion
  - `webgpu_cpp_print.h` - `operator<<` for wgpu types (debug output)
  - `webgpu_format.h` - spdlog formatter specializations for wgpu types

### Shader Reflection (`src/mnexus/private/shader/`)

SPIR-V shader introspection via SPIRV-Reflect:

- `ShaderModuleReflection` - Reflects a single SPIR-V module into `BindGroupLayout` / `BindGroupLayoutEntry` structures (sorted by set/binding). Each entry records type, count, and `writable` flag (from the `NonWritable` SPIR-V decoration).
- `MergedPipelineLayout` - Incrementally merges bind group layouts from multiple shader modules (e.g., vertex + fragment). Detects conflicting bindings (same set/binding but different type/count) and OR-merges the `writable` flag.

### Resource Management (`src/mnexus/private/container/`)

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

### Dependencies

- `mbase` - Base utilities (logging, assertions, `SmallVector`, `ArrayProxy`, thread safety annotations, platform detection, `BitFlags`)
- `thirdparty/dawn` - Google Dawn WebGPU implementation (native platforms only). Tint (SPIR-V to WGSL compiler) from Dawn is also used on Emscripten builds, since browser WebGPU only accepts WGSL
  - **Warning:** Dawn pulls in ~78 recursive submodules (ANGLE, SwiftShader, Vulkan SDK, Chromium infra, etc.). ANGLE's sub-submodules reference `chrome-internal.googlesource.com` which requires Google-internal credentials and will hang or prompt for auth.
  - To skip ANGLE (not needed for WebGPU): set `submodule.third_party/angle.update none` in **Dawn's** local git config, then run `GIT_TERMINAL_PROMPT=0 GIT_ASKPASS= git submodule update --recursive`.
  - Fork (Git client): "Update submodules after checkout" should be OFF to avoid automatic submodule updates on branch switch.
- `thirdparty/SPIRV-Reflect` - SPIR-V reflection library for extracting bind group layouts and decorations from shader modules
