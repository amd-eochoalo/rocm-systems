// Copyright (c) 2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#ifndef ROCJITSU_CODE_AMDGPU_ELF_READER_H_
#define ROCJITSU_CODE_AMDGPU_ELF_READER_H_

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rocjitsu {

struct AmdGpuKernelSite {
  std::string kernel_name;
  uint64_t descriptor_file_offset = 0;
  uint64_t entry_file_offset = 0;
};

bool is_supported_amdgpu_elf(std::span<const uint8_t> image);

std::vector<AmdGpuKernelSite> discover_amdgpu_kernel_sites(std::span<const uint8_t> image);

} // namespace rocjitsu

#endif // ROCJITSU_CODE_AMDGPU_ELF_READER_H_
