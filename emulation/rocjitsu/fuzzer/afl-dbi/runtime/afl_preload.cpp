#include "rocjitsu_fuzzer/afl_runtime.h"

#include <hip/hip_runtime_api.h>

#include <stdio.h>
#include <stdlib.h>

namespace {

bool env_flag(const char *name) {
  const char *value = getenv(name);
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

bool verbose_enabled() { return env_flag("ROCJITSU_AFL_VERBOSE"); }

} // namespace

extern "C" {

int rocjitsu_afl_persistent_begin() {
  if (verbose_enabled())
    fprintf(stderr, "rocjitsu-afl: persistent iteration begin\n");
  return 0;
}

int rocjitsu_afl_persistent_end() {
  const hipError_t status = hipDeviceSynchronize();
  if (status != hipSuccess) {
    fprintf(stderr, "rocjitsu-afl: hipDeviceSynchronize failed: %s\n", hipGetErrorString(status));
    return static_cast<int>(status);
  }

  if (verbose_enabled())
    fprintf(stderr, "rocjitsu-afl: persistent iteration end\n");
  return 0;
}

} // extern "C"
