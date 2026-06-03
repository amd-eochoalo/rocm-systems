// Copyright (c) 2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "runtime/raw_amdgpu_elf.h"

#include "minimal_amdgpu_elf.h"
#include "rocjitsu/code/amdgpu_elf.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

namespace {

template <typename T> void write_at(std::vector<uint8_t> &image, uint64_t offset, const T &value) {
  ASSERT_LE(offset + sizeof(T), image.size());
  std::memcpy(image.data() + offset, &value, sizeof(T));
}

} // namespace

TEST(RawAmdGpuElfTest, DetectsSupportedRawAmdGpuElf) {
  auto image = rocjitsu::fuzzer::afl::test::make_minimal_amdgpu_elf();
  EXPECT_TRUE(rocjitsu::fuzzer::afl::is_supported_raw_amdgpu_elf(image));

  auto wrong_machine = image;
  auto ehdr = *reinterpret_cast<const rocjitsu::Elf64_Ehdr *>(wrong_machine.data());
  ehdr.e_machine = rocjitsu::EM_X86_64;
  write_at(wrong_machine, 0, ehdr);
  EXPECT_FALSE(rocjitsu::fuzzer::afl::is_supported_raw_amdgpu_elf(wrong_machine));

  auto unsupported_target = image;
  ehdr = *reinterpret_cast<const rocjitsu::Elf64_Ehdr *>(unsupported_target.data());
  ehdr.e_flags = rocjitsu::EF_AMDGPU_MACH_AMDGCN_GFX90A;
  write_at(unsupported_target, 0, ehdr);
  EXPECT_FALSE(rocjitsu::fuzzer::afl::is_supported_raw_amdgpu_elf(unsupported_target));
}

TEST(RawAmdGpuElfTest, DiscoversKernelDescriptorSite) {
  const auto image = rocjitsu::fuzzer::afl::test::make_minimal_amdgpu_elf();

  const auto sites = rocjitsu::fuzzer::afl::discover_raw_amdgpu_kernel_sites(image);

  ASSERT_EQ(sites.size(), 1u);
  EXPECT_EQ(sites[0].kernel_name, "kernel");
  EXPECT_EQ(sites[0].descriptor_file_offset,
            rocjitsu::fuzzer::afl::test::kMinimalAmdGpuElfKernelDescriptorOffset);
  EXPECT_EQ(sites[0].entry_file_offset,
            rocjitsu::fuzzer::afl::test::kMinimalAmdGpuElfKernelEntryOffset);
}

TEST(RawAmdGpuElfTest, IgnoresMalformedInput) {
  constexpr uint8_t not_elf[] = {'n', 'o', 't', 'e', 'l', 'f'};
  EXPECT_FALSE(rocjitsu::fuzzer::afl::is_supported_raw_amdgpu_elf(not_elf));
  EXPECT_TRUE(rocjitsu::fuzzer::afl::discover_raw_amdgpu_kernel_sites(not_elf).empty());

  auto truncated = rocjitsu::fuzzer::afl::test::make_minimal_amdgpu_elf();
  truncated.resize(sizeof(rocjitsu::Elf64_Ehdr));
  EXPECT_FALSE(rocjitsu::fuzzer::afl::is_supported_raw_amdgpu_elf(truncated));
  EXPECT_TRUE(rocjitsu::fuzzer::afl::discover_raw_amdgpu_kernel_sites(truncated).empty());
}
