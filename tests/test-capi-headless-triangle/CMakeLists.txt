add_executable(test-capi-headless-triangle main.c)
target_link_libraries(test-capi-headless-triangle PRIVATE mnexus)
target_include_directories(test-capi-headless-triangle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty)

if(WIN32 AND TARGET copy_d3dcompiler_dll)
  add_dependencies(test-capi-headless-triangle copy_d3dcompiler_dll)
  add_custom_command(TARGET test-capi-headless-triangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/$<CONFIG>/d3dcompiler_47.dll"
      "$<TARGET_FILE_DIR:test-capi-headless-triangle>"
  )
endif()
