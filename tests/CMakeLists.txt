# Helper: create an mnexus test executable with common setup.
#   mnexus_add_test(<target_name> <source_file> ...)
function(mnexus_add_test TARGET_NAME)
  add_executable(${TARGET_NAME} ${ARGN})
  target_link_libraries(${TARGET_NAME} PRIVATE mnexus)
  target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)

  # Copy backend-dependent DLLs next to the executable.
  if(WIN32 AND TARGET copy_d3dcompiler_dll)
    add_dependencies(${TARGET_NAME} copy_d3dcompiler_dll)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/$<CONFIG>/d3dcompiler_47.dll"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>"
    )
  endif()
endfunction()

add_subdirectory(test-headless-info)
