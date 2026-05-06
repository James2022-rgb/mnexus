cmake_minimum_required(VERSION 3.22)

# --------------------------------------------------------------------------------
# Project
#

set(TARGET_NAME "mnexus")
set(SRC_DIR "src")

project(${TARGET_NAME}
  VERSION 0.1.0.0
  LANGUAGES CXX
)

#
# Public variables
#

option(MNEXUS_ENABLE_BACKEND_WGPU "Enable WebGPU backend support" ON)
option(MNEXUS_ENABLE_BACKEND_VULKAN "Enable Vulkan backend support" ON)
option(MNEXUS_ENABLE_DAWN "Enable Dawn backend support for the WebGPU backend" ON)
option(MNEXUS_ENABLE_TINT_ON_WEB "Enable Tint SPIR-V to WGSL conversion for Emscripten" ON)
# Vulkan Video decode support. Pulls in vidsynt (Rust H.265 NAL parser) as a
# PUBLIC dependency. Disabled by default since Vulkan Video is desktop-only
# (no Web / Android / Apple) and requires cargo on the build host.
option(MNEXUS_ENABLE_VIDEO_CODING "Enable Vulkan Video coding (decode); requires vidsynt + cargo" OFF)

set(CMAKE_CXX_STANDARD 23 CACHE STRING "C++ standard" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(${TARGET_NAME} STATIC)
target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)

if(MSVC)
  target_compile_options(${TARGET_NAME} PRIVATE /W4)
else()
  target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Werror -Wno-nonportable-include-path)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(MSVC) # clang-cl
    target_compile_options(${TARGET_NAME} PRIVATE /clang:-Werror=thread-safety)
  else()
    target_compile_options(${TARGET_NAME} PRIVATE -Werror=thread-safety)
  endif()
endif()

# --------------------------------------------------------------------------------
# Internal variables
#

set(_enable_backend_webgpu OFF)
if(${MNEXUS_ENABLE_BACKEND_WGPU} AND NOT ANDROID)
  # The WebGPU backend on native targets requires Dawn (see `include_dawn.h`),
  # and Dawn's Android build pulls in ~78 submodules (including ANGLE / Chromium
  # infrastructure). Android targets use the Vulkan backend instead.
  set(_enable_backend_webgpu ON)
endif()

set(_enable_backend_vulkan OFF)
if(${MNEXUS_ENABLE_BACKEND_VULKAN} AND NOT EMSCRIPTEN)
  set(_enable_backend_vulkan ON)
endif()

# Video coding requires the Vulkan backend and a desktop platform on which
# vidsynt (Rust crate) can be built. Currently restricted to Windows; extend
# the platform check when adding Linux / macOS support.
set(_enable_video_coding OFF)
if(${MNEXUS_ENABLE_VIDEO_CODING} AND ${_enable_backend_vulkan} AND WIN32)
  set(_enable_video_coding ON)
endif()

set(_enable_dawn OFF)
if(${_enable_backend_webgpu} AND ${MNEXUS_ENABLE_DAWN} AND NOT EMSCRIPTEN)
  set(_enable_dawn ON)
endif()

# Tint is enabled when Dawn is enabled (native) or when explicitly enabled on Emscripten.
set(_enable_tint OFF)
if(${_enable_dawn})
  set(_enable_tint ON)
elseif(EMSCRIPTEN AND ${MNEXUS_ENABLE_TINT_ON_WEB})
  set(_enable_tint ON)
endif()

# --------------------------------------------------------------------------------
# External libraries
#

set(_thirdparty_dir "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")

#
# Emscripten WebGPU
#
if(EMSCRIPTEN)
  target_compile_options(${TARGET_NAME} PUBLIC "--use-port=emdawnwebgpu")
  target_link_options(${TARGET_NAME} PUBLIC "--use-port=emdawnwebgpu")
endif()

#
# Dawn
#
if(${_enable_dawn})
  # Statically linking Dawn by Cheng:
  # https://groups.google.com/g/dawn-graphics/c/m9-XvaFA_aw
  # https://github.com/frost-beta/betann/blob/bd269590a219ca833786925402871680525f06b5/CMakeLists.txt#L81-L113

  # Until the Vulkan backend of Dawn stops giving us trouble, we disable it.
  set(_enable_dawn_vulkan OFF)

  set(DAWN_ENABLE_VULKAN ${_enable_dawn_vulkan} CACHE BOOL "" FORCE)
  set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
  set(DAWN_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
  set(DAWN_USE_GLFW OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_SPV_READER ON CACHE BOOL "" FORCE)
  set(DAWN_FETCH_DEPENDENCIES ON)
  set(DAWN_BUILD_MONOLITHIC_LIBRARY OFF)

  # Use existing SPIRV-Headers if available (e.g., from slang via mslang).
  # This avoids duplicate target errors and ensures version consistency.
  if(TARGET SPIRV-Headers AND DEFINED SPIRV-Headers_SOURCE_DIR)
    set(DAWN_SPIRV_HEADERS_DIR "${SPIRV-Headers_SOURCE_DIR}" CACHE PATH "" FORCE)
  endif()

  # Force static linking for SPIRV-Tools and abseil to avoid duplicate symbol errors.
  # Without these settings, both static and shared libraries would be built and linked,
  # causing LNK2005 errors on Windows.
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(SPIRV_TOOLS_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(ABSL_BUILD_DLL OFF CACHE BOOL "" FORCE)
  set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)

  # Inject C++17 flag into `CMAKE_REQUIRED_FLAGS` for Dawn build.
  set(_saved_required "${CMAKE_REQUIRED_FLAGS}")
  if(MSVC)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} /std:c++17")
  else()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++17")
  endif()

  # Set CMake folder for Dawn.
  set(_old_folder "${CMAKE_FOLDER}")
  set(CMAKE_FOLDER "external/dawn")

  # Save the original directory properties and clear them to avoid interference.
  get_property(_old_inc  DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
  get_property(_old_opts DIRECTORY PROPERTY COMPILE_OPTIONS)
  get_property(_old_defs DIRECTORY PROPERTY COMPILE_DEFINITIONS)
  set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "")
  set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "")
  set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS "")

  if(MSVC)
    # add_compile_options(/showIncludes)
  endif()

  add_subdirectory(${_thirdparty_dir}/dawn ${CMAKE_CURRENT_BINARY_DIR}/dawn)

  # Restore `CMAKE_REQUIRED_FLAGS`.
  set(CMAKE_REQUIRED_FLAGS "${_saved_required}")

  # Restore CMake folder.
  set(CMAKE_FOLDER "${_old_folder}")

  # Restore the original directory properties.
  set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "${_old_inc}")
  set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "${_old_opts}")
  set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS "${_old_defs}")

  target_link_libraries(${TARGET_NAME}
    PUBLIC $<BUILD_INTERFACE:dawncpp_headers>
    PRIVATE $<LINK_ONLY:dawn::dawn_common>
            $<LINK_ONLY:dawn::dawn_native>
            $<LINK_ONLY:tint_api>
  )

  # Build the procs that call dawn_native directly.
  find_package(Python3 REQUIRED)
  DawnJSONGenerator(TARGET "webgpu_dawn_native_proc"
                    PRINT_NAME "Dawn native WebGPU procs"
                    OUTPUT_SOURCES WEBGPU_DAWN_NATIVE_PROC_GEN_SOURCES)
  target_sources(${TARGET_NAME} PRIVATE ${WEBGPU_DAWN_NATIVE_PROC_GEN_SOURCES})
endif()

#
# Build Tint + dependencies for Emscripten (when Dawn is not used but Tint is needed)
#
if(EMSCRIPTEN AND ${_enable_tint})
  set(_dawn_dir "${_thirdparty_dir}/dawn")
  set(_dawn_third_party_dir "${_dawn_dir}/third_party")
  set(_dawn_cmake_dir "${_dawn_dir}/src/cmake")

  # Include Dawn's cmake modules for helper functions
  list(INSERT CMAKE_MODULE_PATH 0 "${_dawn_cmake_dir}")
  include(DawnSetIfNotDefined)
  include(DawnCompilerChecks)
  include(DawnCompilerPlatformFlags)
  include(DawnCompilerExtraFlags)
  include(DawnCompilerWarningFlags)
  include(DawnLibrary)

  # Set Dawn/Tint variables
  # Note: TINT_ROOT_SOURCE_DIR must point to Dawn root since Tint uses "src/tint/..." include paths
  set(DAWN_THIRD_PARTY_DIR "${_dawn_third_party_dir}" CACHE PATH "" FORCE)
  set(DAWN_SRC_DIR "${_dawn_dir}/src" CACHE PATH "" FORCE)
  set(DAWN_INCLUDE_DIR "${_dawn_dir}/include" CACHE PATH "" FORCE)
  set(DAWN_BUILD_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/tint/gen" CACHE PATH "" FORCE)
  set(TINT_ROOT_SOURCE_DIR "${_dawn_dir}" CACHE PATH "" FORCE)

  # Create Dawn's internal configuration targets (required for dawn_shared_utils)
  if(NOT TARGET dawn_public_config)
    add_library(dawn_public_config INTERFACE)
    target_include_directories(dawn_public_config INTERFACE "${DAWN_INCLUDE_DIR}")
  endif()

  if(NOT TARGET dawn_internal_config)
    add_library(dawn_internal_config INTERFACE)
    target_include_directories(dawn_internal_config INTERFACE
      "${_dawn_dir}"
      "${DAWN_SRC_DIR}"
      "${DAWN_BUILD_GEN_DIR}/src"
    )
    target_link_libraries(dawn_internal_config INTERFACE dawn_public_config)
  endif()

  # Configure Tint options
  set(TINT_BUILD_SPV_READER ON CACHE BOOL "" FORCE)
  set(TINT_BUILD_WGSL_READER ON CACHE BOOL "" FORCE)
  set(TINT_BUILD_WGSL_WRITER ON CACHE BOOL "" FORCE)
  set(TINT_BUILD_SPV_WRITER OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_HLSL_WRITER OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_MSL_WRITER OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_GLSL_WRITER OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_GLSL_VALIDATOR OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_NULL_WRITER OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_IR_BINARY OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_TINTD OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
  set(DAWN_BUILD_PROTOBUF OFF CACHE BOOL "" FORCE)
  set(DAWN_WERROR OFF CACHE BOOL "" FORCE)

  # Configure static linking
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(SPIRV_TOOLS_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(ABSL_BUILD_DLL OFF CACHE BOOL "" FORCE)
  set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)

  # Set CMake folder for dependencies
  set(_old_folder "${CMAKE_FOLDER}")
  set(CMAKE_FOLDER "external/tint-deps")

  # Build Abseil
  set(DAWN_ABSEIL_DIR "${_dawn_third_party_dir}/abseil-cpp" CACHE PATH "" FORCE)
  if(NOT TARGET absl::strings)
    message(STATUS "mnexus: Building Abseil for Emscripten at ${DAWN_ABSEIL_DIR}")
    add_subdirectory(${DAWN_ABSEIL_DIR} "${CMAKE_CURRENT_BINARY_DIR}/abseil")
  endif()

  # Build SPIRV-Headers
  set(DAWN_SPIRV_HEADERS_DIR "${_dawn_third_party_dir}/spirv-headers/src" CACHE PATH "" FORCE)
  set(TINT_SPIRV_HEADERS_DIR "${DAWN_SPIRV_HEADERS_DIR}" CACHE PATH "" FORCE)
  if(NOT TARGET SPIRV-Headers)
    set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
    set(SPIRV_HEADERS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
    message(STATUS "mnexus: Building SPIRV-Headers for Emscripten at ${DAWN_SPIRV_HEADERS_DIR}")
    add_subdirectory(${DAWN_SPIRV_HEADERS_DIR} "${CMAKE_CURRENT_BINARY_DIR}/spirv-headers")
  endif()

  # Build SPIRV-Tools
  set(DAWN_SPIRV_TOOLS_DIR "${_dawn_third_party_dir}/spirv-tools/src" CACHE PATH "" FORCE)
  set(TINT_SPIRV_TOOLS_DIR "${DAWN_SPIRV_TOOLS_DIR}" CACHE PATH "" FORCE)
  if(NOT TARGET SPIRV-Tools)
    set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
    set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
    set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)
    set(SPIRV_WERROR OFF CACHE BOOL "" FORCE)
    message(STATUS "mnexus: Building SPIRV-Tools for Emscripten at ${DAWN_SPIRV_TOOLS_DIR}")
    add_subdirectory(${DAWN_SPIRV_TOOLS_DIR} "${CMAKE_CURRENT_BINARY_DIR}/spirv-tools")
  endif()

  # Build Dawn shared utils (required by Tint)
  if(NOT TARGET dawn_shared_utils)
    message(STATUS "mnexus: Building Dawn shared utils for Emscripten")
    add_subdirectory(${_dawn_dir}/src/utils "${CMAKE_CURRENT_BINARY_DIR}/dawn-utils")
  endif()

  # Restore CMake folder
  set(CMAKE_FOLDER "${_old_folder}")
  set(CMAKE_FOLDER "external/tint")

  # Build Tint
  message(STATUS "mnexus: Building Tint for Emscripten")
  add_subdirectory(${_dawn_dir}/src/tint "${CMAKE_CURRENT_BINARY_DIR}/tint")

  # Restore CMake folder
  set(CMAKE_FOLDER "${_old_folder}")

  # Link Tint (use LINK_ONLY to avoid inheriting Dawn's include directories which conflict with emdawnwebgpu)
  target_link_libraries(${TARGET_NAME} PRIVATE $<LINK_ONLY:tint_api>)

  # Add Tint header include path as SYSTEM to give emdawnwebgpu's webgpu headers priority
  # This is needed because dawn/include contains both tint/ and webgpu/ headers
  target_include_directories(${TARGET_NAME} SYSTEM PRIVATE "${_dawn_dir}/include")
endif()

#
# Vulkan SDK
#

if(${_enable_backend_vulkan})
  if(NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR "VULKAN_SDK environment variable is not set")
  endif()
  if(NOT EXISTS $ENV{VULKAN_SDK})
    message(FATAL_ERROR "VULKAN_SDK path '$ENV{VULKAN_SDK}' does not exist")
  endif()

  target_include_directories(${TARGET_NAME} PRIVATE "$ENV{VULKAN_SDK}/Include")
endif()

#
# VulkanMemoryAllocator (VMA)
#

if(${_enable_backend_vulkan})
  if(NOT TARGET VulkanMemoryAllocator)
    set(_old_folder "${CMAKE_FOLDER}")
    set(CMAKE_FOLDER "external/VulkanMemoryAllocator")
    add_subdirectory(${_thirdparty_dir}/VulkanMemoryAllocator ${CMAKE_CURRENT_BINARY_DIR}/VulkanMemoryAllocator)
    set(CMAKE_FOLDER "${_old_folder}")
  endif()

  target_link_libraries(${TARGET_NAME} PRIVATE VulkanMemoryAllocator)
endif()

if(${_enable_backend_vulkan})
  # Fetch volk from GitHub if not already provided by a parent project.
  # volk is a meta-loader for Vulkan that loads function pointers at runtime;
  # uses the Vulkan headers from VULKAN_SDK that mnexus already requires.
  if(NOT TARGET volk)
    include(FetchContent)

    set(_old_folder "${CMAKE_FOLDER}")
    set(CMAKE_FOLDER "external/volk")
    FetchContent_Declare(volk
      GIT_REPOSITORY https://github.com/zeux/volk.git
      GIT_TAG        vulkan-sdk-1.4.313.0
    )
    FetchContent_MakeAvailable(volk)
    set(CMAKE_FOLDER "${_old_folder}")

    # Upstream volk doesn't auto-define VK_USE_PLATFORM_*_KHR; without these
    # the platform-specific surface symbols (vkCreateWin32SurfaceKHR, etc.)
    # are stripped from volk.c and our WSI code fails to link.
    if(WIN32)
      target_compile_definitions(volk PRIVATE VK_USE_PLATFORM_WIN32_KHR=1)
    endif()
    if(UNIX AND NOT ANDROID)
      target_compile_definitions(volk PRIVATE
        VK_USE_PLATFORM_XLIB_KHR=1
        VK_USE_PLATFORM_WAYLAND_KHR=1
      )
    endif()
    if(ANDROID)
      target_compile_definitions(volk PRIVATE VK_USE_PLATFORM_ANDROID_KHR=1)
    endif()
  endif()

  target_link_libraries(${TARGET_NAME} PRIVATE volk)
endif()

#
# vidsynt (Rust H.265 / H.264 NAL parser, used by the Vulkan Video code path)
#
# Built via cargo with the `ffi` feature; consumed as an IMPORTED static library
# through vidsynt-sys/include/vidsynt.h. Linked PUBLICLY so that downstream
# users can call `vidsynt_*` directly when they want to (re-)parse NAL units
# they hand to mnexus.
#

if(${_enable_video_coding})
  if(NOT TARGET vidsynt)
    include(FetchContent)

    find_program(CARGO_EXECUTABLE cargo REQUIRED)

    FetchContent_Declare(vidsynt
      GIT_REPOSITORY https://github.com/James2022-rgb/vidsynt.git
      GIT_TAG        feature/capi
    )
    FetchContent_GetProperties(vidsynt)
    if(NOT vidsynt_POPULATED)
      FetchContent_Populate(vidsynt)
    endif()

    set(_vidsynt_dir "${vidsynt_SOURCE_DIR}")
    # Always build the Rust side as release; cargo's incremental cache makes
    # this cheap and avoids debug/release CRT-mixing concerns.
    set(_vidsynt_lib "${_vidsynt_dir}/target/release/vidsynt.lib")

    add_custom_command(
      OUTPUT  ${_vidsynt_lib}
      COMMAND ${CARGO_EXECUTABLE} build --release --features ffi
      WORKING_DIRECTORY ${_vidsynt_dir}
      COMMENT "Building vidsynt (cargo build --release --features ffi)"
      VERBATIM
    )
    add_custom_target(vidsynt_build DEPENDS ${_vidsynt_lib})
    set_target_properties(vidsynt_build PROPERTIES FOLDER "external/vidsynt")

    add_library(vidsynt STATIC IMPORTED GLOBAL)
    set_target_properties(vidsynt PROPERTIES
      IMPORTED_LOCATION             "${_vidsynt_lib}"
      INTERFACE_INCLUDE_DIRECTORIES "${_vidsynt_dir}/vidsynt-sys/include"
      # System libraries the Rust standard library / runtime depend on.
      INTERFACE_LINK_LIBRARIES      "ws2_32;userenv;bcrypt;ntdll"
    )
    add_dependencies(vidsynt vidsynt_build)
  endif()

  target_link_libraries(${TARGET_NAME} PUBLIC vidsynt)
endif()

#
# SPIRV-Reflect
#

if(NOT TARGET spirv-reflect-static)
  set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
  set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
  set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(SPIRV_REFLECT_INSTALL OFF CACHE BOOL "" FORCE)

  set(_old_folder "${CMAKE_FOLDER}")
  set(CMAKE_FOLDER "external/spirv-reflect")
  add_subdirectory(${_thirdparty_dir}/SPIRV-Reflect ${CMAKE_CURRENT_BINARY_DIR}/spirv-reflect)
  set(CMAKE_FOLDER "${_old_folder}")
endif()

target_link_libraries(${TARGET_NAME} PRIVATE spirv-reflect-static)

# --------------------------------------------------------------------------------
# Source files
#

set(_public_root_dir "${SRC_DIR}/${TARGET_NAME}/public")
set(_sources_public
  ${_public_root_dir}/mnexus.h
  ${_public_root_dir}/render_pipeline_state_snapshot.h
  ${_public_root_dir}/render_state_event_log.h
  ${_public_root_dir}/types.h
)
source_group("Public" FILES ${_sources_public})

set(_public_container_dir "${_public_root_dir}/container")
set(_sources_public_container
  ${_public_container_dir}/array_proxy.h
)
source_group("Public/Container" FILES ${_sources_public_container})

set(_private_root_dir "${SRC_DIR}/${TARGET_NAME}/private")
set(_sources_private_root
  ${_private_root_dir}/mnexus.cpp
  ${_private_root_dir}/mnexus_c.cpp
  ${_private_root_dir}/types.cpp
)
source_group("Private" FILES ${_sources_private_root})

set(_private_backend_iface_dir "${_private_root_dir}/backend-iface")
set(_sources_private_backend_iface
  ${_private_backend_iface_dir}/backend-iface.h
)
source_group("Private/Backend-Iface" FILES ${_sources_private_backend_iface})


set(_private_backend_webgpu_dir "${_private_root_dir}/backend-webgpu")
if(${_enable_backend_webgpu})
  set(_sources_private_backend_webgpu
    ${_private_backend_webgpu_dir}/backend-webgpu.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu.h
    ${_private_backend_webgpu_dir}/backend-webgpu-binding.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-binding.h
    ${_private_backend_webgpu_dir}/backend-webgpu-buffer.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-buffer.h
    ${_private_backend_webgpu_dir}/backend-webgpu-command_list.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-command_list.h
    ${_private_backend_webgpu_dir}/backend-webgpu-compute_pipeline.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-compute_pipeline.h
    ${_private_backend_webgpu_dir}/backend-webgpu-layout.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-layout.h
    ${_private_backend_webgpu_dir}/backend-webgpu-render_pipeline.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-render_pipeline.h
    ${_private_backend_webgpu_dir}/backend-webgpu-resource_storage.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-shader.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-shader.h
    ${_private_backend_webgpu_dir}/backend-webgpu-texture.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-texture.h
    ${_private_backend_webgpu_dir}/buffer_row_repack.cpp
    ${_private_backend_webgpu_dir}/buffer_row_repack.h
    ${_private_backend_webgpu_dir}/builtin_shader.cpp
    ${_private_backend_webgpu_dir}/builtin_shader.h
    ${_private_backend_webgpu_dir}/blit_texture.cpp
    ${_private_backend_webgpu_dir}/blit_texture.h
    ${_private_backend_webgpu_dir}/include_dawn.h
    ${_private_backend_webgpu_dir}/shader_module.cpp
    ${_private_backend_webgpu_dir}/shader_module.h
    ${_private_backend_webgpu_dir}/types_bridge.cpp
    ${_private_backend_webgpu_dir}/types_bridge.h
    ${_private_backend_webgpu_dir}/webgpu_cpp_print.h
    ${_private_backend_webgpu_dir}/webgpu_format.h
  )
endif()
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/${_private_backend_webgpu_dir}" PREFIX "Private/Backend-WebGPU" FILES ${_sources_private_backend_webgpu})


set(_private_backend_vulkan_dir "${_private_root_dir}/backend-vulkan")
if(${_enable_backend_vulkan})
  set(_sources_private_backend_vulkan
    # depend/
    ${_private_backend_vulkan_dir}/depend/vulkan.h
    ${_private_backend_vulkan_dir}/depend/vulkan_fwd.h
    ${_private_backend_vulkan_dir}/depend/vulkan_vma.cpp
    ${_private_backend_vulkan_dir}/depend/vulkan_vma.h
    # object/
    ${_private_backend_vulkan_dir}/object/fwd.h
    ${_private_backend_vulkan_dir}/object/vk-object.h
    ${_private_backend_vulkan_dir}/object/vk-object-buffer.h
    ${_private_backend_vulkan_dir}/object/vk-object-command_pool.h
    ${_private_backend_vulkan_dir}/object/vk-object-compute_pipeline.h
    ${_private_backend_vulkan_dir}/object/vk-object-descriptor_set.h
    ${_private_backend_vulkan_dir}/object/vk-object-descriptor_set_layout.h
    ${_private_backend_vulkan_dir}/object/vk-object-image.h
    ${_private_backend_vulkan_dir}/object/vk-object-pipeline_layout.h
    ${_private_backend_vulkan_dir}/object/vk-object-render_pipeline.h
    ${_private_backend_vulkan_dir}/object/vk-object-sampler.h
    ${_private_backend_vulkan_dir}/object/vk-object-shader_module.h
    # device/
    ${_private_backend_vulkan_dir}/device/fwd.h
    ${_private_backend_vulkan_dir}/device/vk-deferred_destroyer.h
    ${_private_backend_vulkan_dir}/device/vk-instance.cpp
    ${_private_backend_vulkan_dir}/device/vk-instance.h
    ${_private_backend_vulkan_dir}/device/vk-physical_device.cpp
    ${_private_backend_vulkan_dir}/device/vk-physical_device.h
    ${_private_backend_vulkan_dir}/device/vk-physical_device-debug_ui.cpp
    ${_private_backend_vulkan_dir}/device/vk-physical_device-debug_ui.h
    ${_private_backend_vulkan_dir}/device/vk-device.cpp
    ${_private_backend_vulkan_dir}/device/vk-device.h
    ${_private_backend_vulkan_dir}/device/vk-device_helper.cpp
    ${_private_backend_vulkan_dir}/device/vk-device_helper.h
    ${_private_backend_vulkan_dir}/device/vk-queue.cpp
    ${_private_backend_vulkan_dir}/device/vk-queue.h
    ${_private_backend_vulkan_dir}/device/vk-staging.cpp
    ${_private_backend_vulkan_dir}/device/vk-staging.h
    # resource/
    ${_private_backend_vulkan_dir}/resource/fwd.h
    ${_private_backend_vulkan_dir}/resource/image_view_cache.h
    ${_private_backend_vulkan_dir}/resource/resource_storage.cpp
    ${_private_backend_vulkan_dir}/resource/resource_storage.h
    ${_private_backend_vulkan_dir}/resource/shader_module.cpp
    ${_private_backend_vulkan_dir}/resource/shader_module.h
    ${_private_backend_vulkan_dir}/resource/types_bridge.cpp
    ${_private_backend_vulkan_dir}/resource/types_bridge.h
    # descriptor/
    ${_private_backend_vulkan_dir}/descriptor/fwd.h
    ${_private_backend_vulkan_dir}/descriptor/descriptor_set_allocator.cpp
    ${_private_backend_vulkan_dir}/descriptor/descriptor_set_allocator.h
    ${_private_backend_vulkan_dir}/descriptor/descriptor_set_binder.cpp
    ${_private_backend_vulkan_dir}/descriptor/descriptor_set_binder.h
    ${_private_backend_vulkan_dir}/descriptor/descriptor_set_write.cpp
    ${_private_backend_vulkan_dir}/descriptor/descriptor_set_write.h
    # command/
    ${_private_backend_vulkan_dir}/command/fwd.h
    ${_private_backend_vulkan_dir}/command/command_encoder.cpp
    ${_private_backend_vulkan_dir}/command/command_encoder.h
    ${_private_backend_vulkan_dir}/command/image_layout_tracker.cpp
    ${_private_backend_vulkan_dir}/command/image_layout_tracker.h
    ${_private_backend_vulkan_dir}/command/pending_pipeline_barrier.cpp
    ${_private_backend_vulkan_dir}/command/pending_pipeline_barrier.h
    # wsi/
    ${_private_backend_vulkan_dir}/wsi/fwd.h
    ${_private_backend_vulkan_dir}/wsi/vk-wsi_surface.cpp
    ${_private_backend_vulkan_dir}/wsi/vk-wsi_surface.h
    # root
    ${_private_backend_vulkan_dir}/backend-vulkan.cpp
    ${_private_backend_vulkan_dir}/backend-vulkan.h
    ${_private_backend_vulkan_dir}/backend-vulkan-command_list.cpp
    ${_private_backend_vulkan_dir}/backend-vulkan-command_list.h
    ${_private_backend_vulkan_dir}/backend-vulkan-buffer.cpp
    ${_private_backend_vulkan_dir}/backend-vulkan-buffer.h
    ${_private_backend_vulkan_dir}/backend-vulkan-compute_pipeline.cpp
    ${_private_backend_vulkan_dir}/backend-vulkan-compute_pipeline.h
    ${_private_backend_vulkan_dir}/backend-vulkan-render_pipeline.cpp
    ${_private_backend_vulkan_dir}/backend-vulkan-render_pipeline.h
    ${_private_backend_vulkan_dir}/backend-vulkan-shader.cpp
    ${_private_backend_vulkan_dir}/backend-vulkan-shader.h
    ${_private_backend_vulkan_dir}/backend-vulkan-texture.cpp
    ${_private_backend_vulkan_dir}/backend-vulkan-texture.h
  )
endif()
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/${_private_backend_vulkan_dir}" PREFIX "Private/Backend-Vulkan" FILES ${_sources_private_backend_vulkan})


set(_private_cache_dir "${_private_root_dir}/cache")
set(_sources_private_cache
  ${_private_cache_dir}/hash_cache.h
)
source_group("Private/Cache" FILES ${_sources_private_cache})

set(_private_impl_dir "${_private_root_dir}/impl")
set(_sources_private_impl
  ${_private_impl_dir}/impl_macros.h
)
source_group("Private/Impl" FILES ${_sources_private_impl})

set(_private_resource_pool_dir "${_private_root_dir}/resource_pool")
set(_sources_private_resource_pool
  ${_private_resource_pool_dir}/generational_pool.h
  ${_private_resource_pool_dir}/resource_generational_pool.h
)
source_group("Private/ResourcePool" FILES ${_sources_private_resource_pool})

set(_private_debug_ui_dir "${_private_root_dir}/debug_ui")
set(_sources_private_debug_ui
  ${_private_debug_ui_dir}/resource_pool_debug_ui.cpp
  ${_private_debug_ui_dir}/resource_pool_debug_ui.h
)
source_group("Private/DebugUi" FILES ${_sources_private_debug_ui})

set(_private_binding_dir "${_private_root_dir}/binding")
set(_sources_private_binding
  ${_private_binding_dir}/cache_key.h
  ${_private_binding_dir}/state_tracker.cpp
  ${_private_binding_dir}/state_tracker.h
)
source_group("Private/Binding" FILES ${_sources_private_binding})

set(_private_pipeline_dir "${_private_root_dir}/pipeline")
set(_sources_private_pipeline
  ${_private_pipeline_dir}/pipeline_layout_cache.h
  ${_private_pipeline_dir}/pipeline_layout_cache_key.cpp
  ${_private_pipeline_dir}/pipeline_layout_cache_key.h
  ${_private_pipeline_dir}/render_pipeline_cache.h
  ${_private_pipeline_dir}/render_pipeline_cache_key.cpp
  ${_private_pipeline_dir}/render_pipeline_cache_key.h
  ${_private_pipeline_dir}/render_pipeline_diagnostics.h
  ${_private_pipeline_dir}/render_pipeline_fixed_function.h
  ${_private_pipeline_dir}/render_pipeline_state_tracker.cpp
  ${_private_pipeline_dir}/render_pipeline_state_tracker.h
  ${_private_pipeline_dir}/render_state_event_log.cpp
)
source_group("Private/Pipeline" FILES ${_sources_private_pipeline})

set(_private_sync_dir "${_private_root_dir}/sync")
set(_sources_private_sync
  ${_private_sync_dir}/resource_sync.cpp
  ${_private_sync_dir}/resource_sync.h
)
source_group("Private/Sync" FILES ${_sources_private_sync})

set(_private_shader_dir "${_private_root_dir}/shader")
set(_sources_private_shader
  ${_private_shader_dir}/reflection.cpp
  ${_private_shader_dir}/reflection.h
  ${_private_shader_dir}/wgsl.cpp
  ${_private_shader_dir}/wgsl.h
)
source_group("Private/Shader" FILES ${_sources_private_shader})

set(_natvis_dir "natvis")
set(_natvis_files
  ${_natvis_dir}/mnexus.natvis
)
source_group("Natvis Files" FILES ${_natvis_files})

target_sources(${TARGET_NAME} PRIVATE
  ${_sources_public}
  ${_sources_public_container}
  ${_sources_private_root}
  ${_sources_private_backend_iface}
  ${_sources_private_backend_vulkan}
  ${_sources_private_backend_webgpu}
  ${_sources_private_binding}
  ${_sources_private_debug_ui}
  ${_sources_private_resource_pool}
  ${_sources_private_cache}
  ${_sources_private_impl}
  ${_sources_private_pipeline}
  ${_sources_private_shader}
  ${_sources_private_sync}
  ${_natvis_files}
)

# Fetch mbase from GitHub if not already provided by a parent project.
if(NOT TARGET mbase)
  include(FetchContent)
  FetchContent_Declare(mbase
    GIT_REPOSITORY https://github.com/James2022-rgb/mbase.git
    GIT_TAG        master
  )
  FetchContent_MakeAvailable(mbase)
endif()

target_link_libraries(${TARGET_NAME} PUBLIC
  mbase
)

# Dear ImGui (optional). If the consuming project provides a `dear_imgui`
# target, link it transitively and set MNEXUS_HAVE_DEAR_IMGUI=1.
if(TARGET dear_imgui)
  target_link_libraries(${TARGET_NAME} PUBLIC dear_imgui)
  set(_have_dear_imgui 1)
else()
  set(_have_dear_imgui 0)
endif()

target_include_directories(${TARGET_NAME} PRIVATE
  ${_private_root_dir}
)
target_include_directories(${TARGET_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}
)

target_compile_definitions(${TARGET_NAME} PUBLIC
  MNEXUS_ENABLE_BACKEND_WGPU=$<BOOL:${_enable_backend_webgpu}>
  MNEXUS_ENABLE_BACKEND_VULKAN=$<BOOL:${_enable_backend_vulkan}>
  MNEXUS_ENABLE_VIDEO_CODING=$<BOOL:${_enable_video_coding}>
  MNEXUS_HAVE_DEAR_IMGUI=${_have_dear_imgui}
)

target_compile_definitions(${TARGET_NAME} PRIVATE
  MNEXUS_INTERNAL_USE_DAWN=$<BOOL:${_enable_dawn}>
  MNEXUS_INTERNAL_USE_TINT=$<BOOL:${_enable_tint}>
)

# --------------------------------------------------------------------------------
# Solution folder
#

set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "mnexus")

# --------------------------------------------------------------------------------
# Tests
#

option(MNEXUS_BUILD_TESTS "Build mnexus tests" OFF)

if(MNEXUS_BUILD_TESTS)
  set(_old_folder "${CMAKE_FOLDER}")
  set(CMAKE_FOLDER "mnexus/tests")
  add_subdirectory(tests)
  set(CMAKE_FOLDER "${_old_folder}")
endif()
