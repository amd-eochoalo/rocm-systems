# Copyright (c) 2025-2026 Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

rj_add_object_library(rocjitsu_vm
    soc.cpp
    virtual_machine.cpp
    rj_vm.cpp
)

# rj_vm.cpp uses config and checkpoint APIs which depend on FlatBuffers.
target_link_libraries(rocjitsu_vm PRIVATE flatbuffers)
target_include_directories(rocjitsu_vm PRIVATE ${GENERATED_DIR})
add_dependencies(rocjitsu_vm flatbuffers_schemas)

add_subdirectory(amdgpu)
add_subdirectory(risc_v)

# ---------------------------------------------------------------------------
# Aggregation targets for the VM layer split.
# ---------------------------------------------------------------------------

# INTERFACE target: simdojo-free headers for ISA consumers.
# ISA targets link this for compilation; it provides only the abstract
# interfaces (ComputeUnitIface, ByteMemory, Wavefront, HartState, etc.).
add_library(rocjitsu_vm_iface INTERFACE)
target_include_directories(rocjitsu_vm_iface INTERFACE
    ${ROCJITSU_INCLUDE_DIR} ${ROCJITSU_SRC_DIR})

# INTERFACE target: groups VM OBJECT libraries + simdojo.
# Only final binaries (rocjitsu_shared, tests) should link this.
# Uses $<TARGET_OBJECTS:> to ensure OBJECT library files are propagated
# through the INTERFACE target to consuming executables and shared libraries.
add_library(rocjitsu_vm_impl INTERFACE)
target_link_libraries(rocjitsu_vm_impl INTERFACE
    $<TARGET_OBJECTS:rocjitsu_vm>
    $<TARGET_OBJECTS:rocjitsu_vm_amdgpu>
    $<TARGET_OBJECTS:rocjitsu_vm_risc_v>
    simdojo rocjitsu_vm_iface)
target_include_directories(rocjitsu_vm_impl INTERFACE
    ${ROCJITSU_INCLUDE_DIR} ${ROCJITSU_SRC_DIR})
