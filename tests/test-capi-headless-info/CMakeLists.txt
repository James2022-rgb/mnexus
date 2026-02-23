add_executable(test-capi-headless-info main.c)
target_link_libraries(test-capi-headless-info PRIVATE mnexus)

if(WIN32 AND TARGET copy_d3dcompiler_dll)
  add_dependencies(test-capi-headless-info copy_d3dcompiler_dll)
  add_custom_command(TARGET test-capi-headless-info POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/$<CONFIG>/d3dcompiler_47.dll"
      "$<TARGET_FILE_DIR:test-capi-headless-info>"
  )
endif()
