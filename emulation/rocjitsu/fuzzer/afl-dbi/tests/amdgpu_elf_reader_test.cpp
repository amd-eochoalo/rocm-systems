// Copyright (c) 2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocjitsu/code/amdgpu_elf_reader.h"

#include "minimal_amdgpu_elf.h"
#include "rocjitsu/code/amdgpu_elf.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

template <typename T> void write_at(std::vector<uint8_t> &image, uint64_t offset, const T &value) {
  ASSERT_LE(offset + sizeof(T), image.size());
  std::memcpy(image.data() + offset, &value, sizeof(T));
}

} // namespace

TEST(AmdGpuElfReaderTest, DetectsSupportedAmdGpuElf) {
  auto image = rocjitsu::fuzzer::afl::test::make_minimal_amdgpu_elf();
  EXPECT_TRUE(rocjitsu::is_supported_amdgpu_elf(image));

  auto wrong_machine = image;
  auto ehdr = *reinterpret_cast<const rocjitsu::Elf64_Ehdr *>(wrong_machine.data());
  ehdr.e_machine = rocjitsu::EM_X86_64;
  write_at(wrong_machine, 0, ehdr);
  EXPECT_FALSE(rocjitsu::is_supported_amdgpu_elf(wrong_machine));

  auto unsupported_target = image;
  ehdr = *reinterpret_cast<const rocjitsu::Elf64_Ehdr *>(unsupported_target.data());
  ehdr.e_flags = rocjitsu::EF_AMDGPU_MACH_AMDGCN_GFX90A;
  write_at(unsupported_target, 0, ehdr);
  EXPECT_FALSE(rocjitsu::is_supported_amdgpu_elf(unsupported_target));
}

TEST(AmdGpuElfReaderTest, DiscoversKernelDescriptorSite) {
  const auto image = rocjitsu::fuzzer::afl::test::make_minimal_amdgpu_elf();

  const auto sites = rocjitsu::discover_amdgpu_kernel_sites(image);

  ASSERT_EQ(sites.size(), 1u);
  EXPECT_EQ(sites[0].kernel_name, "kernel");
  EXPECT_EQ(sites[0].descriptor_file_offset,
            rocjitsu::fuzzer::afl::test::kMinimalAmdGpuElfKernelDescriptorOffset);
  EXPECT_EQ(sites[0].entry_file_offset,
            rocjitsu::fuzzer::afl::test::kMinimalAmdGpuElfKernelEntryOffset);
}

TEST(AmdGpuElfReaderTest, IgnoresMalformedInput) {
  constexpr uint8_t not_elf[] = {'n', 'o', 't', 'e', 'l', 'f'};
  EXPECT_FALSE(rocjitsu::is_supported_amdgpu_elf(not_elf));
  EXPECT_TRUE(rocjitsu::discover_amdgpu_kernel_sites(not_elf).empty());

  auto truncated = rocjitsu::fuzzer::afl::test::make_minimal_amdgpu_elf();
  truncated.resize(sizeof(rocjitsu::Elf64_Ehdr));
  EXPECT_FALSE(rocjitsu::is_supported_amdgpu_elf(truncated));
  EXPECT_TRUE(rocjitsu::discover_amdgpu_kernel_sites(truncated).empty());
}

TEST(AmdGpuElfReaderTest, BuildsEntryCounterProbeFromStatePointer) {
  constexpr uint64_t state_pointer = 0x0123456789abcdefULL;

  const auto words =
      rocjitsu::build_amdgpu_entry_counter_probe_words(state_pointer, ROCJITSU_CODE_ARCH_RDNA4);

  ASSERT_FALSE(words.empty());
  // The runtime patches raw ELF bytes after allocating the device counters, so
  // the probe must carry that device pointer as immediate words. This is the
  // GPU-visible staging buffer, not AFL's host shared-memory bitmap.
  const auto low = std::find(words.begin(), words.end(), static_cast<uint32_t>(state_pointer));
  ASSERT_NE(low, words.end());
  const auto high = std::find(low + 1, words.end(), static_cast<uint32_t>(state_pointer >> 32));
  EXPECT_NE(high, words.end());
}
