add_library(mnexus_test_harness STATIC mnexus_test_harness.c)
target_include_directories(mnexus_test_harness PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(mnexus_test_harness PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty)
target_link_libraries(mnexus_test_harness PUBLIC mnexus)

if(EMSCRIPTEN)
    # ASYNCIFY: required for synchronous MnNexusCreate / MnDeviceQueueWait.
    # NOTE: ASYNCIFY and -fwasm-exceptions are incompatible. Using JS exceptions mode.
    target_compile_options(mnexus_test_harness PUBLIC -sNO_DISABLE_EXCEPTION_CATCHING)
    target_link_options(mnexus_test_harness PUBLIC
        -sASYNCIFY
        -sASYNCIFY_STACK_SIZE=65536
        -sSTACK_SIZE=1048576
        -sALLOW_MEMORY_GROWTH=1
        -sASSERTIONS=1
    )
endif()
