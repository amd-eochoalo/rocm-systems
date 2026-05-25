// Copyright (c) 2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @file compute_unit_iface.h
/// @brief ISA-facing abstract interface for AMDGPU compute units.
///
/// Provides the minimal API surface that ISA instruction implementations
/// need from a compute unit: register file access, L1 cache management,
/// and component identity. Intentionally free of simdojo dependencies so
/// that ISA targets can compile without the simulation framework.

#ifndef ROCJITSU_VM_AMDGPU_COMPUTE_UNIT_IFACE_H_
#define ROCJITSU_VM_AMDGPU_COMPUTE_UNIT_IFACE_H_

#include <cstdint>
#include <string>

namespace rocjitsu {
namespace amdgpu {

/// @brief Abstract interface exposing compute unit operations to the ISA layer.
///
/// Concrete implementations (ComputeUnitCore and its subclasses) inherit
/// this alongside the simdojo component hierarchy. ISA code receives a
/// reference to this interface via Wavefront::cu() and never needs to
/// see the full ComputeUnitCore declaration or any simdojo headers.
class ComputeUnitIface {
public:
  virtual ~ComputeUnitIface() = default;

  /// @brief Read a scalar register from the physical SGPR file.
  virtual uint32_t read_sgpr(uint32_t reg_idx) const = 0;

  /// @brief Write a scalar register in the physical SGPR file.
  virtual void write_sgpr(uint32_t reg_idx, uint32_t val) = 0;

  /// @brief Read a vector register lane from the physical VGPR file.
  virtual uint32_t read_vgpr(uint32_t reg_idx, uint32_t lane) const = 0;

  /// @brief Write a vector register lane in the physical VGPR file.
  virtual void write_vgpr(uint32_t reg_idx, uint32_t lane, uint32_t val) = 0;

  /// @brief Invalidate all entries in the L1 scalar cache.
  virtual void invalidate_l1_scalar() = 0;

  /// @brief Write back all dirty entries in the L1 scalar cache.
  virtual void writeback_l1_scalar() = 0;

  /// @brief Invalidate all entries in the L1 vector cache.
  virtual void invalidate_l1_vector() = 0;

  /// @brief Return the simulation component ID.
  virtual uint32_t id() const = 0;

  /// @brief Return the full hierarchical path name (e.g. "soc.xcd0.se0.cu0").
  virtual std::string full_path() const = 0;
};

} // namespace amdgpu
} // namespace rocjitsu

#endif // ROCJITSU_VM_AMDGPU_COMPUTE_UNIT_IFACE_H_
