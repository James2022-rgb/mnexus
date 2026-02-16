# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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

Compiler warnings: `-Wall -Wextra -Werror` (GCC/Clang), `/W4` (MSVC). Thread-safety analysis (`-Werror=thread-safety`) is enabled on Clang/GCC.

## Architecture

mnexus is a graphics abstraction layer providing a unified API over WebGPU (via Dawn on native platforms, or native WebGPU on Emscripten/Web). Additional backends under consideration: Vulkan (primary), DX12 (secondary).

### Core Interfaces (`src/mnexus/public/mnexus.h`)

- `INexus` - Main entry point for surface lifecycle, presentation, and device access
- `IDevice` - Resource creation (buffers, textures, shaders, programs, pipelines, layouts) and command submission
- `ICommandList` - Command recording for compute and transfer operations
- `Texture` - RAII wrapper around texture handles

### C/C++ FFI Type Layer (`src/mnexus/public/types.h`)

Types are defined in two layers for FFI compatibility:
- **C types** (outside `#if defined(__cplusplus)`): `Mn`-prefixed types (`MnBool32`, `MnResourceHandle`, `MnBufferDesc`, `MnTextureDesc`, `MnShaderModuleDesc`, `MnProgramDesc`, `MnComputePipelineDesc`, `MnRenderPipelineDesc`, `MnBindGroupLayoutDesc`, `MnPipelineLayoutDesc`, etc.) usable from C and other languages
- **C++ types** (in `namespace mnexus`): Type-safe wrappers (`BufferHandle`, `TextureHandle`, `ShaderModuleHandle`, `ProgramHandle`, `ComputePipelineHandle`, etc.) with `static_assert` ensuring ABI compatibility with C counterparts

### Backend Structure (`src/mnexus/private/`)

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

### Dependencies

- `mbase` - Base utilities (logging, assertions, `SmallVector`, `ArrayProxy`, thread safety annotations, platform detection, `BitFlags`)
- `thirdparty/dawn` - Google Dawn WebGPU implementation (native platforms only). Tint (SPIR-V to WGSL compiler) from Dawn is also used on Emscripten builds, since browser WebGPU only accepts WGSL
  - **Warning:** Dawn pulls in ~78 recursive submodules (ANGLE, SwiftShader, Vulkan SDK, Chromium infra, etc.). ANGLE's sub-submodules reference `chrome-internal.googlesource.com` which requires Google-internal credentials and will hang or prompt for auth.
  - To skip ANGLE (not needed for WebGPU): set `submodule.third_party/angle.update none` in **Dawn's** local git config, then run `GIT_TERMINAL_PROMPT=0 GIT_ASKPASS= git submodule update --recursive`.
  - Fork (Git client): "Update submodules after checkout" should be OFF to avoid automatic submodule updates on branch switch.
- `thirdparty/SPIRV-Reflect` - SPIR-V reflection library for extracting bind group layouts and decorations from shader modules
