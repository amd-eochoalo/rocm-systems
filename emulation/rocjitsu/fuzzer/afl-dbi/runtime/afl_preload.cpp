#include "rocjitsu_fuzzer/afl_runtime.h"

#include <hip/hip_runtime_api.h>

#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <sys/shm.h>

#include <algorithm>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

namespace {

using rocjitsu::fuzzer::afl::kCoverageSlots;
using rocjitsu::fuzzer::afl::kDeviceStart;

constexpr hipError_t kHipErrorRuntimeUnavailable = static_cast<hipError_t>(999);
constexpr size_t kDeviceCounterBytes = sizeof(uint32_t) * kCoverageSlots;

using hipModuleLoadData_t = hipError_t (*)(hipModule_t *, const void *);
using hipModuleUnload_t = hipError_t (*)(hipModule_t);
using hipModuleGetFunction_t = hipError_t (*)(hipFunction_t *, hipModule_t, const char *);
using hipModuleLaunchKernel_t = hipError_t (*)(hipFunction_t, unsigned int, unsigned int,
                                               unsigned int, unsigned int, unsigned int,
                                               unsigned int, unsigned int, hipStream_t, void **,
                                               void **);
using hipDeviceSynchronize_t = hipError_t (*)();
using hipMemcpy_t = hipError_t (*)(void *, const void *, size_t, hipMemcpyKind);
using hipMalloc_t = hipError_t (*)(void **, size_t);
using hipMemset_t = hipError_t (*)(void *, int, size_t);
using hipFree_t = hipError_t (*)(void *);
using hipGetErrorString_t = const char *(*)(hipError_t);

hipModuleLoadData_t real_hipModuleLoadData = nullptr;
hipModuleUnload_t real_hipModuleUnload = nullptr;
hipModuleGetFunction_t real_hipModuleGetFunction = nullptr;
hipModuleLaunchKernel_t real_hipModuleLaunchKernel = nullptr;
hipDeviceSynchronize_t real_hipDeviceSynchronize = nullptr;
hipMemcpy_t real_hipMemcpy = nullptr;
hipMalloc_t real_hipMalloc = nullptr;
hipMemset_t real_hipMemset = nullptr;
hipFree_t real_hipFree = nullptr;
hipGetErrorString_t real_hipGetErrorString = nullptr;

std::once_flag g_symbol_once;
void *g_explicit_hip_runtime = nullptr;
thread_local uint32_t g_intercept_depth = 0;

std::mutex g_runtime_mutex;
uint8_t *g_afl_area = nullptr;
int g_afl_shm_id = -1;
void *g_device_counters = nullptr;
std::vector<uint32_t> g_host_counters;

bool env_flag(const char *name) {
  const char *value = getenv(name);
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

template <typename T> T load_symbol(const char *name) {
  return reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
}

bool verbose_enabled();

template <typename T> T load_hip_symbol(const char *name) {
  if (g_explicit_hip_runtime != nullptr)
    return reinterpret_cast<T>(dlsym(g_explicit_hip_runtime, name));
  return load_symbol<T>(name);
}

void resolve_symbols() {
  std::call_once(g_symbol_once, [] {
    const char *runtime_path = getenv("ROCJITSU_AFL_HIP_RUNTIME_PATH");
    if (runtime_path != nullptr && runtime_path[0] != '\0') {
      g_explicit_hip_runtime = dlopen(runtime_path, RTLD_LAZY | RTLD_LOCAL);
      if (g_explicit_hip_runtime == nullptr && verbose_enabled())
        fprintf(stderr, "rocjitsu-afl: failed to load HIP runtime '%s': %s\n", runtime_path,
                dlerror());
    }

    real_hipModuleLoadData = load_hip_symbol<hipModuleLoadData_t>("hipModuleLoadData");
    real_hipModuleUnload = load_hip_symbol<hipModuleUnload_t>("hipModuleUnload");
    real_hipModuleGetFunction = load_hip_symbol<hipModuleGetFunction_t>("hipModuleGetFunction");
    real_hipModuleLaunchKernel = load_hip_symbol<hipModuleLaunchKernel_t>("hipModuleLaunchKernel");
    real_hipDeviceSynchronize = load_hip_symbol<hipDeviceSynchronize_t>("hipDeviceSynchronize");
    real_hipMemcpy = load_hip_symbol<hipMemcpy_t>("hipMemcpy");
    real_hipMalloc = load_hip_symbol<hipMalloc_t>("hipMalloc");
    real_hipMemset = load_hip_symbol<hipMemset_t>("hipMemset");
    real_hipFree = load_hip_symbol<hipFree_t>("hipFree");
    real_hipGetErrorString = load_hip_symbol<hipGetErrorString_t>("hipGetErrorString");
  });
}

bool interception_bypassed() { return g_intercept_depth != 0; }

struct ScopedInterceptionBypass {
  ScopedInterceptionBypass() { ++g_intercept_depth; }
  ScopedInterceptionBypass(const ScopedInterceptionBypass &) = delete;
  ScopedInterceptionBypass &operator=(const ScopedInterceptionBypass &) = delete;
  ~ScopedInterceptionBypass() { --g_intercept_depth; }
};

bool verbose_enabled() { return env_flag("ROCJITSU_AFL_VERBOSE"); }

const char *hip_error_string(hipError_t status) {
  if (real_hipGetErrorString != nullptr)
    return real_hipGetErrorString(status);
  return "HIP runtime unavailable";
}

bool parse_shm_id(int *id) {
  const char *value = getenv("__AFL_SHM_ID");
  if (value == nullptr || value[0] == '\0')
    return false;

  errno = 0;
  char *end = nullptr;
  const long parsed = strtol(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0' || parsed < 0)
    return false;

  *id = static_cast<int>(parsed);
  return true;
}

bool ensure_afl_map_locked() {
  int shm_id = -1;
  if (!parse_shm_id(&shm_id))
    return false;

  if (g_afl_area != nullptr && g_afl_shm_id == shm_id)
    return true;

  if (g_afl_area != nullptr)
    shmdt(g_afl_area);

  g_afl_area = static_cast<uint8_t *>(shmat(shm_id, nullptr, 0));
  if (g_afl_area == reinterpret_cast<uint8_t *>(-1)) {
    g_afl_area = nullptr;
    g_afl_shm_id = -1;
    if (verbose_enabled())
      fprintf(stderr, "rocjitsu-afl: failed to attach AFL shared memory id %d\n", shm_id);
    return false;
  }

  g_afl_shm_id = shm_id;
  return true;
}

hipError_t ensure_device_counters_locked() {
  if (g_device_counters != nullptr)
    return hipSuccess;
  if (real_hipMalloc == nullptr || real_hipMemset == nullptr)
    return kHipErrorRuntimeUnavailable;

  hipError_t status = real_hipMalloc(&g_device_counters, kDeviceCounterBytes);
  if (status != hipSuccess)
    return status;

  status = real_hipMemset(g_device_counters, 0, kDeviceCounterBytes);
  if (status != hipSuccess) {
    if (real_hipFree != nullptr)
      static_cast<void>(real_hipFree(g_device_counters));
    g_device_counters = nullptr;
    return status;
  }

  g_host_counters.assign(kCoverageSlots, 0);
  return hipSuccess;
}

uint8_t saturating_add(uint8_t value, uint32_t delta) {
  const uint32_t sum = static_cast<uint32_t>(value) + delta;
  return static_cast<uint8_t>(std::min<uint32_t>(sum, 255));
}

} // namespace

extern "C" {

hipError_t hipModuleLoadData(hipModule_t *module, const void *image) {
  resolve_symbols();
  if (real_hipModuleLoadData == nullptr)
    return kHipErrorRuntimeUnavailable;
  if (interception_bypassed())
    return real_hipModuleLoadData(module, image);
  ScopedInterceptionBypass bypass;
  return real_hipModuleLoadData(module, image);
}

hipError_t hipModuleUnload(hipModule_t module) {
  resolve_symbols();
  if (real_hipModuleUnload == nullptr)
    return kHipErrorRuntimeUnavailable;
  if (interception_bypassed())
    return real_hipModuleUnload(module);
  ScopedInterceptionBypass bypass;
  return real_hipModuleUnload(module);
}

hipError_t hipModuleGetFunction(hipFunction_t *function, hipModule_t module, const char *kname) {
  resolve_symbols();
  if (real_hipModuleGetFunction == nullptr)
    return kHipErrorRuntimeUnavailable;
  if (interception_bypassed())
    return real_hipModuleGetFunction(function, module, kname);
  ScopedInterceptionBypass bypass;
  return real_hipModuleGetFunction(function, module, kname);
}

hipError_t hipModuleLaunchKernel(hipFunction_t f, unsigned int grid_dim_x, unsigned int grid_dim_y,
                                 unsigned int grid_dim_z, unsigned int block_dim_x,
                                 unsigned int block_dim_y, unsigned int block_dim_z,
                                 unsigned int shared_mem_bytes, hipStream_t stream,
                                 void **kernel_params, void **extra) {
  resolve_symbols();
  if (real_hipModuleLaunchKernel == nullptr)
    return kHipErrorRuntimeUnavailable;
  if (interception_bypassed()) {
    return real_hipModuleLaunchKernel(f, grid_dim_x, grid_dim_y, grid_dim_z, block_dim_x,
                                      block_dim_y, block_dim_z, shared_mem_bytes, stream,
                                      kernel_params, extra);
  }
  ScopedInterceptionBypass bypass;
  return real_hipModuleLaunchKernel(f, grid_dim_x, grid_dim_y, grid_dim_z, block_dim_x, block_dim_y,
                                    block_dim_z, shared_mem_bytes, stream, kernel_params, extra);
}

hipError_t hipDeviceSynchronize() {
  resolve_symbols();
  if (real_hipDeviceSynchronize == nullptr)
    return kHipErrorRuntimeUnavailable;
  if (interception_bypassed())
    return real_hipDeviceSynchronize();
  ScopedInterceptionBypass bypass;
  return real_hipDeviceSynchronize();
}

hipError_t hipMemcpy(void *dst, const void *src, size_t size_bytes, hipMemcpyKind kind) {
  resolve_symbols();
  if (real_hipMemcpy == nullptr)
    return kHipErrorRuntimeUnavailable;
  if (interception_bypassed())
    return real_hipMemcpy(dst, src, size_bytes, kind);
  ScopedInterceptionBypass bypass;
  return real_hipMemcpy(dst, src, size_bytes, kind);
}

int rocjitsu_afl_persistent_begin() {
  resolve_symbols();
  std::lock_guard<std::mutex> lock(g_runtime_mutex);

  if (!ensure_afl_map_locked())
    return 0;

  const hipError_t status = ensure_device_counters_locked();
  if (status != hipSuccess) {
    fprintf(stderr, "rocjitsu-afl: failed to allocate device counters: %s\n",
            hip_error_string(status));
    return static_cast<int>(status);
  }

  if (real_hipMemset == nullptr)
    return static_cast<int>(kHipErrorRuntimeUnavailable);

  const hipError_t memset_status = real_hipMemset(g_device_counters, 0, kDeviceCounterBytes);
  if (memset_status != hipSuccess) {
    fprintf(stderr, "rocjitsu-afl: failed to reset device counters: %s\n",
            hip_error_string(memset_status));
    return static_cast<int>(memset_status);
  }

  if (verbose_enabled())
    fprintf(stderr, "rocjitsu-afl: persistent iteration begin\n");
  return 0;
}

int rocjitsu_afl_persistent_end() {
  resolve_symbols();
  if (real_hipDeviceSynchronize == nullptr)
    return static_cast<int>(kHipErrorRuntimeUnavailable);

  ScopedInterceptionBypass bypass;
  const hipError_t status = real_hipDeviceSynchronize();
  if (status != hipSuccess) {
    fprintf(stderr, "rocjitsu-afl: hipDeviceSynchronize failed: %s\n", hip_error_string(status));
    return static_cast<int>(status);
  }

  std::lock_guard<std::mutex> lock(g_runtime_mutex);
  if (ensure_afl_map_locked() && g_device_counters != nullptr) {
    if (real_hipMemcpy == nullptr)
      return static_cast<int>(kHipErrorRuntimeUnavailable);

    if (g_host_counters.size() != kCoverageSlots)
      g_host_counters.assign(kCoverageSlots, 0);

    const hipError_t copy_status = real_hipMemcpy(g_host_counters.data(), g_device_counters,
                                                  kDeviceCounterBytes, hipMemcpyDeviceToHost);
    if (copy_status != hipSuccess) {
      fprintf(stderr, "rocjitsu-afl: failed to copy device counters: %s\n",
              hip_error_string(copy_status));
      return static_cast<int>(copy_status);
    }

    for (uint32_t slot = 0; slot < kCoverageSlots; ++slot) {
      const uint32_t delta = g_host_counters[slot];
      if (delta != 0)
        g_afl_area[kDeviceStart + slot] = saturating_add(g_afl_area[kDeviceStart + slot], delta);
    }
  }

  if (verbose_enabled())
    fprintf(stderr, "rocjitsu-afl: persistent iteration end\n");
  return 0;
}

} // extern "C"
