// Copyright (c) 2025-2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#ifndef ROCJITSU_VM_RISC_V_MEMORY_H_
#define ROCJITSU_VM_RISC_V_MEMORY_H_

#include "rocjitsu/vm/risc_v/byte_memory.h"
#include "simdojo/components/sparse_memory.h"

#include <string>
#include <utility>

namespace rocjitsu {
namespace risc_v {

/// @brief RISC-V address space backed by simdojo::SparseMemory.
///
/// Per-hart memory component. Inherits SparseMemory for the actual storage
/// and ByteMemory for the ISA-facing virtual interface.
class Memory final : public simdojo::SparseMemory, public ByteMemory {
public:
  explicit Memory(std::string name) : simdojo::SparseMemory(std::move(name)) {}

  uint8_t  read8(uint64_t addr) const override { return SparseMemory::read8(addr); }
  uint16_t read16(uint64_t addr) const override { return SparseMemory::read16(addr); }
  uint32_t read32(uint64_t addr) const override { return SparseMemory::read32(addr); }
  uint64_t read64(uint64_t addr) const override { return SparseMemory::read64(addr); }
  void write8(uint64_t addr, uint8_t val) override { SparseMemory::write8(addr, val); }
  void write16(uint64_t addr, uint16_t val) override { SparseMemory::write16(addr, val); }
  void write32(uint64_t addr, uint32_t val) override { SparseMemory::write32(addr, val); }
  void write64(uint64_t addr, uint64_t val) override { SparseMemory::write64(addr, val); }
};

} // namespace risc_v
} // namespace rocjitsu

#endif // ROCJITSU_VM_RISC_V_MEMORY_H_
