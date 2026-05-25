// Copyright (c) 2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @file byte_memory.h
/// @brief ISA-facing abstract memory interface for RISC-V.
///
/// Provides sized read/write operations that RISC-V ISA instruction
/// implementations require. Free of simdojo dependencies so that ISA
/// targets can compile without the simulation framework.

#ifndef ROCJITSU_VM_RISC_V_BYTE_MEMORY_H_
#define ROCJITSU_VM_RISC_V_BYTE_MEMORY_H_

#include <cstdint>

namespace rocjitsu {
namespace risc_v {

/// @brief Abstract interface for byte-addressable memory used by RISC-V ISA.
///
/// Concrete implementation (risc_v::Memory) inherits this alongside
/// simdojo::SparseMemory. ISA code accesses memory exclusively through
/// this interface and never needs to see simdojo headers.
class ByteMemory {
public:
  virtual ~ByteMemory() = default;

  virtual uint8_t  read8(uint64_t addr) const = 0;
  virtual uint16_t read16(uint64_t addr) const = 0;
  virtual uint32_t read32(uint64_t addr) const = 0;
  virtual uint64_t read64(uint64_t addr) const = 0;

  virtual void write8(uint64_t addr, uint8_t val) = 0;
  virtual void write16(uint64_t addr, uint16_t val) = 0;
  virtual void write32(uint64_t addr, uint32_t val) = 0;
  virtual void write64(uint64_t addr, uint64_t val) = 0;
};

} // namespace risc_v
} // namespace rocjitsu

#endif // ROCJITSU_VM_RISC_V_BYTE_MEMORY_H_
