# RocFuzz AFL++ Integration

RocFuzz is the planned AFL++ integration for rocjitsu device-code
instrumentation. The first upstream slice is intentionally host-side only: it
provides a small `librocjitsu_afl_preload.so` with persistent-iteration hooks
that synchronize ROCm work before AFL++ advances to the next input.

The fuzzer preload is separate from the KMD simulator preload. RocFuzz is meant
to instrument HIP/HSA loader and launch paths for AFL++ coverage feedback, while
the KMD preload emulates the kernel-driver interface for simulated execution.
They should remain independently usable and share common rocjitsu DBI/DBT
building blocks such as code-object parsing, decoding, CFG analysis, relocation,
and probe insertion. Future work may validate composing both preloads, but that
is not the default workflow described here.

Targets can call `rocjitsu_afl_persistent_begin()` at the start of each
persistent iteration and `rocjitsu_afl_persistent_end()` before returning to
AFL++. The current implementation makes the begin hook a no-op boundary and uses
`hipDeviceSynchronize()` in the end hook.

The device DBI planner from commit
`6a09b5d2aaeebbfb90679e897c06ac1c02055e8a` is not part of this slice. That
commit adds instrumentation planning, relocation, patchability classification,
VOPD liveness, and planner tests, but it does not contain the host AFL
persistent runtime. Only the future-compatible AFL bitmap layout constants are
kept here.
