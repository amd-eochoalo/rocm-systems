// Copyright (c) 2026 Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int kHipSuccess = 0;
constexpr int kHipMemcpyDeviceToHost = 2;

uint32_t *g_fake_device_counters = nullptr;
size_t g_fake_device_counter_bytes = 0;
int g_device_synchronize_calls = 0;

} // namespace

extern "C" {

int hipMalloc(void **ptr, size_t size) {
  g_fake_device_counter_bytes = size;
  g_fake_device_counters = static_cast<uint32_t *>(std::calloc(1, size));
  *ptr = g_fake_device_counters;
  return g_fake_device_counters == nullptr ? 999 : kHipSuccess;
}

int hipFree(void *ptr) {
  std::free(ptr);
  if (ptr == g_fake_device_counters) {
    g_fake_device_counters = nullptr;
    g_fake_device_counter_bytes = 0;
  }
  return kHipSuccess;
}

int hipMemset(void *dst, int value, size_t size) {
  std::memset(dst, value, size);
  return kHipSuccess;
}

int hipMemcpy(void *dst, const void *src, size_t size, int kind) {
  if (kind != kHipMemcpyDeviceToHost)
    return 999;
  std::memcpy(dst, src, size);
  return kHipSuccess;
}

int hipDeviceSynchronize() {
  ++g_device_synchronize_calls;
  return kHipSuccess;
}

const char *hipGetErrorString(int) { return "fake hip error"; }

uint32_t *rocfuzz_fake_device_counters() { return g_fake_device_counters; }

size_t rocfuzz_fake_device_counter_bytes() { return g_fake_device_counter_bytes; }

int rocfuzz_fake_device_synchronize_calls() { return g_device_synchronize_calls; }

} // extern "C"
