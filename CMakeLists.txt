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
option(MNEXUS_ENABLE_DAWN "Enable Dawn backend support for the WebGPU backend" ON)
option(MNEXUS_ENABLE_TINT_ON_WEB "Enable Tint SPIR-V to WGSL conversion for Emscripten" ON)

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
if(${MNEXUS_ENABLE_BACKEND_WGPU})
  set(_enable_backend_webgpu ON)
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
  target_link_options(${TARGET_NAME} PUBLIC "--use-port=emdawnwebgpu" "--closure=1")
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
    ${_private_backend_webgpu_dir}/backend-webgpu-compute_pipeline.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-compute_pipeline.h
    ${_private_backend_webgpu_dir}/backend-webgpu-layout.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-layout.h
    ${_private_backend_webgpu_dir}/backend-webgpu-shader.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-shader.h
    ${_private_backend_webgpu_dir}/backend-webgpu-texture.cpp
    ${_private_backend_webgpu_dir}/backend-webgpu-texture.h
    ${_private_backend_webgpu_dir}/builtin_shader.cpp
    ${_private_backend_webgpu_dir}/builtin_shader.h
    ${_private_backend_webgpu_dir}/include_dawn.h
    ${_private_backend_webgpu_dir}/shader_module.cpp
    ${_private_backend_webgpu_dir}/shader_module.h
    ${_private_backend_webgpu_dir}/types_bridge.cpp
    ${_private_backend_webgpu_dir}/types_bridge.h
    ${_private_backend_webgpu_dir}/webgpu_cpp_print.h
    ${_private_backend_webgpu_dir}/webgpu_format.h
  )
endif()
source_group("Private/Backend-WebGPU" FILES ${_sources_private_backend_webgpu})



set(_private_container_dir "${_private_root_dir}/container")
set(_sources_private_container
  ${_private_container_dir}/generational_pool.h
  ${_private_container_dir}/resource_generational_pool.h
)
source_group("Private/Container" FILES ${_sources_private_container})

set(_private_binding_dir "${_private_root_dir}/binding")
set(_sources_private_binding
  ${_private_binding_dir}/state_tracker.cpp
  ${_private_binding_dir}/state_tracker.h
  ${_private_binding_dir}/cache_key.h
)
source_group("Private/Binding" FILES ${_sources_private_binding})

set(_private_shader_dir "${_private_root_dir}/shader")
set(_sources_private_shader
  ${_private_shader_dir}/reflection.cpp
  ${_private_shader_dir}/reflection.h
  ${_private_shader_dir}/wgsl.cpp
  ${_private_shader_dir}/wgsl.h
)
source_group("Private/Shader" FILES ${_sources_private_shader})

target_sources(${TARGET_NAME} PRIVATE
  ${_sources_public}
  ${_sources_public_container}
  ${_sources_private_root}
  ${_sources_private_backend_iface}
  ${_sources_private_backend_webgpu}
  ${_sources_private_binding}
  ${_sources_private_container}
  ${_sources_private_shader}
)

target_link_libraries(${TARGET_NAME} PUBLIC
  mbase
)

target_include_directories(${TARGET_NAME} PRIVATE
  ${_private_root_dir}
)
target_include_directories(${TARGET_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}
)

target_compile_definitions(${TARGET_NAME} PUBLIC
  MNEXUS_ENABLE_BACKEND_WGPU=$<BOOL:${MNEXUS_ENABLE_BACKEND_WGPU}>
)

target_compile_definitions(${TARGET_NAME} PRIVATE
  MNEXUS_INTERNAL_USE_DAWN=$<BOOL:${_enable_dawn}>
  MNEXUS_INTERNAL_USE_TINT=$<BOOL:${_enable_tint}>
)
