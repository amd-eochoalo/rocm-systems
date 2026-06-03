// Copyright (c) 2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rocjitsu::fuzzer::afl {

struct RawAmdGpuKernelSite {
  std::string kernel_name;
  uint64_t descriptor_file_offset = 0;
  uint64_t entry_file_offset = 0;
};

bool is_supported_raw_amdgpu_elf(std::span<const uint8_t> image);

std::vector<RawAmdGpuKernelSite> discover_raw_amdgpu_kernel_sites(std::span<const uint8_t> image);

} // namespace rocjitsu::fuzzer::afl
