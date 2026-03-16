# XRT Memory Telemetry Baseline 2026-03-16

## Scope

This report captures the first allocator telemetry baseline gathered without
changing allocation semantics.

Harness:

- `dev/test_memtelemetry_baseline.c`

Lanes:

- `runtime_phase2`
- `coroutine`
- `xnet_modern`

Telemetry mode:

- opt-in runtime counters only
- no allocator takeover yet
- size classes use the approved `16..1024` / `16B` step policy for reporting

## Summary

### `runtime_phase2`

- `malloc`: `512` calls / `406696` bytes
- `calloc`: `2` calls / `7168` bytes
- `realloc`: `32` calls / `33808` bytes
- `free`: `546` calls
- pooled-candidate: `486` calls / `26912` bytes
- fallback: `60` calls / `420760` bytes

Top pooled classes by call count:

1. `<=16B`: `415` calls / `4472` bytes
2. `<=512B`: `18` calls / `9216` bytes
3. `<=48B`: `12` calls / `576` bytes
4. `<=384B`: `12` calls / `4560` bytes
5. `<=80B`: `10` calls / `720` bytes

Interpretation:

- runtime/container churn is overwhelmingly tiny-object heavy
- a global `16..1024` small-object allocator should pay off quickly here
- fallback bytes are dominated by a small number of large runtime/container
  allocations rather than high call count

### `coroutine`

- `malloc`: `136` calls / `27752` bytes
- `calloc`: `6` calls / `480` bytes
- `realloc`: `0`
- `free`: `140` calls
- pooled-candidate: `142` calls / `28232` bytes
- fallback: `0`

Top pooled classes by call count:

1. `<=16B`: `41` calls / `360` bytes
2. `<=544B`: `39` calls / `21216` bytes
3. `<=128B`: `32` calls / `4096` bytes
4. `<=112B`: `16` calls / `1792` bytes
5. `<=32B`: `9` calls / `256` bytes

Interpretation:

- coroutine runtime is fully inside the planned pooled range in this baseline
- the heaviest single class is around the current coroutine object/scheduler
  working set (`<=544B`)
- this lane is a strong candidate for early benefit once the global small
  allocator is active

### `xnet_modern`

- `malloc`: `992` calls / `656906` bytes
- `calloc`: `93` calls / `88838` bytes
- `realloc`: `197` calls / `48200` bytes
- `free`: `1084` calls
- pooled-candidate: `1152` calls / `170184` bytes
- fallback: `130` calls / `623760` bytes

Top pooled classes by call count:

1. `<=32B`: `261` calls / `6680` bytes
2. `<=16B`: `168` calls / `1532` bytes
3. `<=48B`: `138` calls / `6624` bytes
4. `<=112B`: `111` calls / `11464` bytes
5. `<=96B`: `71` calls / `6666` bytes
6. `<=64B`: `69` calls / `3724` bytes
7. `<=256B`: `63` calls / `16128` bytes
8. `<=128B`: `60` calls / `7680` bytes

Interpretation:

- xnet churn is also strongly biased toward small-object classes
- fallback bytes remain large because transport/TLS/HTTP setup still performs a
  smaller number of much larger allocations
- phase-1 should focus on small-object takeover first and leave `>1024B` on the
  backing allocator exactly as planned

## Conclusions

1. The approved `16..1024` phase-1 pooling window is validated by real baseline
   evidence from runtime, coroutine, and modern network lanes.
2. The highest-frequency hotspots are concentrated in a narrow band below
   `128B`, especially `<=16B`, `<=32B`, and `<=48B`.
3. `>1024B` fallback still accounts for a large share of bytes in runtime and
   xnet, but not of call count; this supports deferring medium-allocation
   pooling to a later phase.
4. Coroutine and runtime lanes are the cleanest early beneficiaries of the
   upcoming global small-object allocator takeover.
