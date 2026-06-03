# RocFuzz AFL++ Integration

RocFuzz is the planned AFL++ integration for rocjitsu device-code
instrumentation. It will build a separate `librocjitsu_afl_preload.so` that
interposes HIP and HSA loader APIs, patches supported AMDGPU code objects, and
merges device coverage into AFL's shared coverage map.

The fuzzer preload is separate from the KMD simulator preload. RocFuzz is meant
to instrument HIP/HSA loader and launch paths for AFL++ coverage feedback, while
the KMD preload emulates the kernel-driver interface for simulated execution.
They should remain independently usable and share common rocjitsu DBI/DBT
building blocks such as code-object parsing, decoding, CFG analysis, relocation,
and probe insertion. Future work may validate composing both preloads, but that
is not the default workflow described here.

This directory currently contains only build scaffolding. Runtime DBI behavior
is being ported in small, tested slices.
