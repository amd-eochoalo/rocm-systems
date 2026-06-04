// Copyright (c) 2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocjitsu/code/amdgpu_elf_reader.h"

#include "rocjitsu/code/amdgpu_elf.h"

#include "hsa/AMDHSAKernelDescriptor.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <string_view>

namespace rocjitsu {

namespace {

using KernelDescriptor = rocr::llvm::amdhsa::kernel_descriptor_t;

bool image_contains_range(size_t image_size, uint64_t offset, uint64_t size) {
  return offset <= image_size && size <= image_size - offset;
}

template <typename T> std::optional<T> read_at(std::span<const uint8_t> image, uint64_t offset) {
  if (!image_contains_range(image.size(), offset, sizeof(T)))
    return std::nullopt;

  T value{};
  std::memcpy(&value, image.data() + offset, sizeof(T));
  return value;
}

bool is_elf_header(const Elf64_Ehdr &ehdr) {
  return std::memcmp(ehdr.e_ident, EI_MAGIC, EI_MAGIC_SIZE) == 0;
}

bool supported_machine_flags(uint32_t flags) {
  switch (flags & EF_AMDGPU_MACH) {
  case EF_AMDGPU_MACH_AMDGCN_GFX942:
  case EF_AMDGPU_MACH_AMDGCN_GFX950:
  case EF_AMDGPU_MACH_AMDGCN_GFX1200:
  case EF_AMDGPU_MACH_AMDGCN_GFX1201:
    return true;
  default:
    return false;
  }
}

std::optional<Elf64_Ehdr> parse_supported_header(std::span<const uint8_t> image) {
  auto ehdr = read_at<Elf64_Ehdr>(image, 0);
  if (!ehdr.has_value())
    return std::nullopt;
  if (!is_elf_header(*ehdr) || ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
      ehdr->e_ident[EI_OSABI] != ELFOSABI_AMDGPU_HSA || ehdr->e_machine != EM_AMDGPU)
    return std::nullopt;
  if (ehdr->e_type != ET_DYN && ehdr->e_type != ET_REL)
    return std::nullopt;
  if (ehdr->e_shentsize != sizeof(Elf64_Shdr) || ehdr->e_shnum == 0)
    return std::nullopt;
  if (!image_contains_range(image.size(), ehdr->e_shoff,
                            static_cast<uint64_t>(ehdr->e_shnum) * sizeof(Elf64_Shdr)))
    return std::nullopt;
  if (!supported_machine_flags(ehdr->e_flags))
    return std::nullopt;
  return ehdr;
}

std::optional<std::vector<Elf64_Shdr>> parse_section_headers(std::span<const uint8_t> image,
                                                             const Elf64_Ehdr &ehdr) {
  std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
  std::memcpy(shdrs.data(), image.data() + ehdr.e_shoff, shdrs.size() * sizeof(Elf64_Shdr));
  return shdrs;
}

bool kernel_descriptor_symbol(const Elf64_Sym &symbol, const char *strtab, size_t strtab_size) {
  if (symbol.st_size != sizeof(KernelDescriptor))
    return false;
  if (elf_symbol_type(symbol.st_info) != kElfSymbolTypeObject ||
      elf_symbol_bind(symbol.st_info) != kElfSymbolBindGlobal)
    return false;
  if (strtab == nullptr || strtab_size == 0 || symbol.st_name == 0 || symbol.st_name >= strtab_size)
    return false;

  const char *name = strtab + symbol.st_name;
  const size_t len = strnlen(name, strtab_size - symbol.st_name);
  return len > 3 && std::string_view(name, len).ends_with(".kd");
}

std::string kernel_name_from_symbol(const Elf64_Sym &symbol, const char *strtab,
                                    size_t strtab_size) {
  const char *name = strtab + symbol.st_name;
  const size_t len = strnlen(name, strtab_size - symbol.st_name);
  return std::string(name, len - 3);
}

std::optional<uint64_t> symbol_file_offset(const Elf64_Sym &symbol,
                                           std::span<const Elf64_Shdr> shdrs, size_t image_size) {
  if (symbol.st_shndx == SHN_UNDEF || symbol.st_shndx == SHN_ABS || symbol.st_shndx >= shdrs.size())
    return std::nullopt;

  const Elf64_Shdr &section = shdrs[symbol.st_shndx];
  if (symbol.st_value < section.sh_addr)
    return std::nullopt;

  const uint64_t section_offset = symbol.st_value - section.sh_addr;
  if (section_offset > section.sh_size)
    return std::nullopt;

  const uint64_t file_offset = section.sh_offset + section_offset;
  if (!image_contains_range(image_size, file_offset, sizeof(KernelDescriptor)))
    return std::nullopt;

  return file_offset;
}

std::optional<uint64_t> add_signed_offset(uint64_t base, int64_t offset) {
  if (offset >= 0) {
    const uint64_t unsigned_offset = static_cast<uint64_t>(offset);
    if (base > UINT64_MAX - unsigned_offset)
      return std::nullopt;
    return base + unsigned_offset;
  }

  const uint64_t magnitude = static_cast<uint64_t>(-(offset + 1)) + 1;
  if (base < magnitude)
    return std::nullopt;
  return base - magnitude;
}

std::optional<uint64_t> executable_vaddr_to_file_offset(uint64_t vaddr,
                                                        std::span<const Elf64_Shdr> shdrs,
                                                        size_t image_size) {
  for (const Elf64_Shdr &section : shdrs) {
    if ((section.sh_flags & SHF_EXECINSTR) == 0)
      continue;
    if (vaddr < section.sh_addr || vaddr >= section.sh_addr + section.sh_size)
      continue;

    const uint64_t file_offset = section.sh_offset + (vaddr - section.sh_addr);
    if (!image_contains_range(image_size, file_offset, 1))
      return std::nullopt;
    return file_offset;
  }
  return std::nullopt;
}

} // namespace

bool is_supported_amdgpu_elf(std::span<const uint8_t> image) {
  return parse_supported_header(image).has_value();
}

std::vector<AmdGpuKernelSite> discover_amdgpu_kernel_sites(std::span<const uint8_t> image) {
  const auto ehdr = parse_supported_header(image);
  if (!ehdr.has_value())
    return {};

  const auto shdrs = parse_section_headers(image, *ehdr);
  if (!shdrs.has_value())
    return {};

  std::vector<AmdGpuKernelSite> sites;
  for (const Elf64_Shdr &symtab : *shdrs) {
    if (symtab.sh_type != SHT_SYMTAB && symtab.sh_type != SHT_DYNSYM)
      continue;
    if (symtab.sh_entsize != sizeof(Elf64_Sym) || symtab.sh_link >= shdrs->size())
      continue;
    if (!image_contains_range(image.size(), symtab.sh_offset, symtab.sh_size))
      continue;

    const Elf64_Shdr &strtab_section = (*shdrs)[symtab.sh_link];
    if (!image_contains_range(image.size(), strtab_section.sh_offset, strtab_section.sh_size))
      continue;

    const char *strtab = reinterpret_cast<const char *>(image.data() + strtab_section.sh_offset);
    const size_t strtab_size = static_cast<size_t>(strtab_section.sh_size);
    const size_t symbol_count = symtab.sh_size / sizeof(Elf64_Sym);
    for (size_t i = 0; i < symbol_count; ++i) {
      const auto symbol = read_at<Elf64_Sym>(image, symtab.sh_offset + i * sizeof(Elf64_Sym));
      if (!symbol.has_value() || !kernel_descriptor_symbol(*symbol, strtab, strtab_size))
        continue;

      const auto descriptor_offset = symbol_file_offset(*symbol, *shdrs, image.size());
      if (!descriptor_offset.has_value())
        continue;

      const auto descriptor = read_at<KernelDescriptor>(image, *descriptor_offset);
      if (!descriptor.has_value())
        continue;

      const auto entry_vaddr =
          add_signed_offset(symbol->st_value, descriptor->kernel_code_entry_byte_offset);
      if (!entry_vaddr.has_value())
        continue;

      const auto entry_offset = executable_vaddr_to_file_offset(*entry_vaddr, *shdrs, image.size());
      if (!entry_offset.has_value())
        continue;

      sites.push_back(AmdGpuKernelSite{
          .kernel_name = kernel_name_from_symbol(*symbol, strtab, strtab_size),
          .descriptor_file_offset = *descriptor_offset,
          .entry_file_offset = *entry_offset,
      });
    }
  }

  std::sort(sites.begin(), sites.end(), [](const auto &lhs, const auto &rhs) {
    return lhs.descriptor_file_offset < rhs.descriptor_file_offset;
  });
  return sites;
}

} // namespace rocjitsu
