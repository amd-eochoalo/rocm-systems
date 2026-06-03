#include "rocjitsu_fuzzer/afl_runtime.h"

#include <hip/hip_runtime_api.h>

#include <dlfcn.h>

#include <mutex>
#include <stdio.h>
#include <stdlib.h>

namespace {

constexpr hipError_t kHipErrorRuntimeUnavailable = static_cast<hipError_t>(999);

using hipModuleLoadData_t = hipError_t (*)(hipModule_t *, const void *);
using hipModuleUnload_t = hipError_t (*)(hipModule_t);
using hipModuleGetFunction_t = hipError_t (*)(hipFunction_t *, hipModule_t, const char *);
using hipModuleLaunchKernel_t = hipError_t (*)(hipFunction_t, unsigned int, unsigned int,
                                               unsigned int, unsigned int, unsigned int,
                                               unsigned int, unsigned int, hipStream_t, void **,
                                               void **);
using hipDeviceSynchronize_t = hipError_t (*)();
using hipMemcpy_t = hipError_t (*)(void *, const void *, size_t, hipMemcpyKind);

hipModuleLoadData_t real_hipModuleLoadData = nullptr;
hipModuleUnload_t real_hipModuleUnload = nullptr;
hipModuleGetFunction_t real_hipModuleGetFunction = nullptr;
hipModuleLaunchKernel_t real_hipModuleLaunchKernel = nullptr;
hipDeviceSynchronize_t real_hipDeviceSynchronize = nullptr;
hipMemcpy_t real_hipMemcpy = nullptr;

std::once_flag g_symbol_once;
thread_local uint32_t g_intercept_depth = 0;

bool env_flag(const char *name) {
  const char *value = getenv(name);
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

template <typename T> T load_symbol(const char *name) {
  return reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
}

void resolve_symbols() {
  std::call_once(g_symbol_once, [] {
    real_hipModuleLoadData = load_symbol<hipModuleLoadData_t>("hipModuleLoadData");
    real_hipModuleUnload = load_symbol<hipModuleUnload_t>("hipModuleUnload");
    real_hipModuleGetFunction = load_symbol<hipModuleGetFunction_t>("hipModuleGetFunction");
    real_hipModuleLaunchKernel = load_symbol<hipModuleLaunchKernel_t>("hipModuleLaunchKernel");
    real_hipDeviceSynchronize = load_symbol<hipDeviceSynchronize_t>("hipDeviceSynchronize");
    real_hipMemcpy = load_symbol<hipMemcpy_t>("hipMemcpy");
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
    fprintf(stderr, "rocjitsu-afl: hipDeviceSynchronize failed: %s\n", hipGetErrorString(status));
    return static_cast<int>(status);
  }

  if (verbose_enabled())
    fprintf(stderr, "rocjitsu-afl: persistent iteration end\n");
  return 0;
}

} // extern "C"
