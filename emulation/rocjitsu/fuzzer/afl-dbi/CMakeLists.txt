# Copyright (c) 2026 Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

include(rj_configure_target)

set(ROCFUZZ_ROCM_PATH
    "/opt/rocm"
    CACHE PATH
    "ROCm installation used by RocFuzz"
)

find_library(
    ROCFUZZ_HIP_RUNTIME
    amdhip64
    HINTS "${ROCFUZZ_ROCM_PATH}"
    ENV ROCM_PATH
    PATH_SUFFIXES lib lib64
)
find_path(
    ROCFUZZ_HIP_INCLUDE_DIR
    hip/hip_runtime_api.h
    HINTS "${ROCFUZZ_ROCM_PATH}"
    ENV ROCM_PATH
    PATH_SUFFIXES include
)

if(ROCFUZZ_HIP_RUNTIME AND ROCFUZZ_HIP_INCLUDE_DIR)
    add_library(rocjitsu_afl_preload SHARED runtime/afl_preload.cpp)
    target_include_directories(
        rocjitsu_afl_preload
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${ROCFUZZ_HIP_INCLUDE_DIR}
    )
    target_link_libraries(
        rocjitsu_afl_preload
        PRIVATE ${ROCFUZZ_HIP_RUNTIME} ${CMAKE_DL_LIBS}
    )
    target_compile_definitions(
        rocjitsu_afl_preload
        PRIVATE __HIP_PLATFORM_AMD__
    )
    rj_configure_target(rocjitsu_afl_preload PRIVATE_INCLUDES)
    set_target_properties(
        rocjitsu_afl_preload
        PROPERTIES OUTPUT_NAME rocjitsu_afl_preload
    )
    add_executable(
        rocjitsu_afl_preload_symbols_test
        tests/afl_preload_symbols_test.cpp
    )
    target_compile_definitions(
        rocjitsu_afl_preload_symbols_test
        PRIVATE ROCJITSU_AFL_PRELOAD_PATH="$<TARGET_FILE:rocjitsu_afl_preload>"
    )
    target_link_libraries(
        rocjitsu_afl_preload_symbols_test
        PRIVATE GTest::gtest_main ${CMAKE_DL_LIBS}
    )
    rj_configure_target(rocjitsu_afl_preload_symbols_test TEST)
    add_dependencies(rocjitsu_afl_preload_symbols_test rocjitsu_afl_preload)
    add_test(
        NAME RocjitsuAflDbi.PreloadSymbols
        COMMAND rocjitsu_afl_preload_symbols_test
    )

    add_library(rocjitsu_afl_fake_hip_runtime SHARED tests/fake_hip_runtime.cpp)
    rj_configure_target(rocjitsu_afl_fake_hip_runtime PRIVATE_INCLUDES)

    add_executable(
        rocjitsu_afl_preload_runtime_test
        tests/afl_preload_runtime_test.cpp
    )
    target_include_directories(
        rocjitsu_afl_preload_runtime_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(
        rocjitsu_afl_preload_runtime_test
        PRIVATE
            ROCJITSU_AFL_PRELOAD_PATH="$<TARGET_FILE:rocjitsu_afl_preload>"
            ROCJITSU_AFL_FAKE_HIP_RUNTIME_PATH="$<TARGET_FILE:rocjitsu_afl_fake_hip_runtime>"
    )
    target_link_libraries(
        rocjitsu_afl_preload_runtime_test
        PRIVATE GTest::gtest_main ${CMAKE_DL_LIBS}
    )
    rj_configure_target(rocjitsu_afl_preload_runtime_test TEST)
    add_dependencies(
        rocjitsu_afl_preload_runtime_test
        rocjitsu_afl_preload
        rocjitsu_afl_fake_hip_runtime
    )
    add_test(
        NAME RocjitsuAflDbi.PreloadRuntime
        COMMAND rocjitsu_afl_preload_runtime_test
    )
else()
    message(
        STATUS
        "RocFuzz AFL preload disabled (amdhip64 or HIP headers not found)"
    )
endif()
