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
    target_link_libraries(rocjitsu_afl_preload PRIVATE ${ROCFUZZ_HIP_RUNTIME})
    target_compile_definitions(
        rocjitsu_afl_preload
        PRIVATE __HIP_PLATFORM_AMD__
    )
    rj_configure_target(rocjitsu_afl_preload PRIVATE_INCLUDES)
    set_target_properties(
        rocjitsu_afl_preload
        PROPERTIES OUTPUT_NAME rocjitsu_afl_preload
    )
else()
    message(
        STATUS
        "RocFuzz AFL preload disabled (amdhip64 or HIP headers not found)"
    )
endif()
