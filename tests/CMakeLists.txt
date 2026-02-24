# Enable C language for the test harness (mnexus_test_harness.c).
enable_language(C)

# Test harness (provides main(), MnTestWritePng, Emscripten flags).
add_subdirectory(harness)

# Helper: create an mnexus test executable with common setup.
#   mnexus_add_test(<target_name> <source_file> ...)
function(mnexus_add_test TARGET_NAME)
  add_executable(${TARGET_NAME} ${ARGN})
  target_link_libraries(${TARGET_NAME} PRIVATE mnexus_test_harness)

  # Detect language: set C++ standard only when C++ sources are present.
  set(_has_cpp OFF)
  foreach(_src ${ARGN})
    if(_src MATCHES "\\.(cpp|cxx|cc)$")
      set(_has_cpp ON)
    endif()
  endforeach()
  if(_has_cpp)
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)
  endif()

  # Copy backend-dependent DLLs next to the executable.
  if(WIN32 AND TARGET copy_d3dcompiler_dll)
    add_dependencies(${TARGET_NAME} copy_d3dcompiler_dll)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/$<CONFIG>/d3dcompiler_47.dll"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>"
    )
  endif()

  if(EMSCRIPTEN)
    set_target_properties(${TARGET_NAME} PROPERTIES SUFFIX ".html")
  endif()
endfunction()

add_subdirectory(test-capi-headless-info)
add_subdirectory(test-capi-headless-triangle)
add_subdirectory(test-headless-info)
add_subdirectory(test-headless-triangle)
