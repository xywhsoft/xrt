# XRT Memory Debug Guide and Migration Notes

## Scope

This guide describes the new global memory pool, the debug-only diagnostics
layer, and the migration expectations for mainline XRT code.

The key rule is simple:

- release builds keep the production allocator hot path lean
- debug diagnostics are opt-in through compile-time macros

## 1. Release vs Debug Behavior

Release build:

- `xrtMalloc/xrtCalloc/xrtRealloc/xrtFree` use the new global allocator
- allocations in `16..1024B` go through the pooled small-object path
- allocations above `1024B` go through the configured backing allocator
- only the minimal allocation header is retained
- no file/line registry or heavy leak-tracking state is carried on the hot path

Debug build:

- compile with `XRT_MEM_DEBUG`
- `xrtMalloc/xrtCalloc/xrtRealloc/xrtFree` are routed through `Dbg` entry points
- allocation events can be recorded with file/line metadata
- live-allocation tracking, canary checks, foreign-allocation tracking, object
  lifetime diagnostics, and temp-arena reporting are enabled

## 2. Public Debug Control APIs

The main debug APIs are:

- `xrtMemDebugEnable(bool)`
- `xrtMemDebugIsEnabled()`
- `xrtMemDebugReset()`
- `xrtMemDebugDumpText(str sPath)`
- `xrtMemDebugDumpJson(str sPath)`

Recommended debug sequence:

```c
xrtInit();
xrtMemDebugReset();
xrtMemDebugEnable(TRUE);

/* run workload */

xrtMemDebugDumpText("dev/memdebug_report.txt");
xrtMemDebugDumpJson("dev/memdebug_report.json");
xrtUnit();
```

## 3. What the Debug Layer Detects

The current debug layer covers:

- heap leaks on the global allocator path
- pooled/explicit-pool leaks through the foreign-allocation registry
- object leaks for supported explicit-lifetime roots
- double free
- invalid free
- wrong-allocator free
- buffer overflow / underflow suspicion through canary validation
- temp arena allocation/reset activity

## 4. Report Types

The debug system emits two report formats:

- text report
- JSON report

Both contain:

- live allocation summary
- foreign/pool allocation summary
- leaked explicit roots
- site statistics grouped by file/line
- recent event stream
- temp arena counters

The current report also includes:

- `temp_current_bytes`
- `temp_peak_bytes`
- `temp_reset_count`

## 5. Supported Object Lifetime Diagnostics

The first phase of object diagnostics covers roots with stable
`Create/Destroy` or `Init/Unit` contracts:

- `array`
- `dict`
- `list`
- `avltree`
- `dynstack`
- `mempool`
- `fsmempool`

In debug mode, these roots are automatically registered through the public API
macros. Release builds do not carry the registry.

## 6. Temp Memory Migration

Old behavior:

- `xrtTempMemory()` used a small thread-local pointer ring
- each call still performed heap allocation
- cleanup meant freeing the pointer ring contents

New behavior:

- `xrtTempMemory()` uses a thread-local scratch arena
- small temporary allocations are served from reusable arena blocks
- oversized temporary allocations use spill blocks
- `xrtFreeTempMemory()` resets reusable arena blocks and releases spill blocks

Practical implications:

- small repeated temp allocations are cheaper than before
- temp memory now has explicit debug counters and events
- code that already treated temp memory as short-lived scratch storage does not
  need source changes

## 7. Explicit Pool Migration

`xmempool` changes:

- bucket lookup is now LUT-based
- tree-style bucket selection is no longer the mainline path
- constructor parameter `0` means official default `16B` step up to `1024B`
- constructor parameter `N > 0` means a per-pool fallback cutoff of `N`

`xfsmempool` changes:

- explicit fixed-size pool behavior is preserved
- debug mode now tracks foreign allocations and allocator misuse more clearly

## 8. Wrong Allocator Usage

The debug layer now distinguishes:

- correct `xrtFree` on global allocations
- correct explicit-pool free on pool allocations
- wrong-allocator free when these paths are mixed

Example of misuse now diagnosed in debug mode:

- memory allocated from `xrtFSMemPoolAlloc()` and released through `xrtFree()`

## 9. Current Limitations

This first debug phase still has boundaries:

- only the supported explicit-lifetime roots are covered by object tracking
- heavy diagnostics are compile-time opt-in, not runtime hot-swappable
- backing-allocation quarantine is debug-only and intentionally bounded
- the current benchmark pass did not accept the `xnet` pressure lane as a final
  regression metric

## 10. Recommended Team Workflow

For production:

- build without `XRT_MEM_DEBUG`
- use the mainline allocator normally

For memory diagnostics:

- rebuild with `XRT_MEM_DEBUG`
- call `xrtMemDebugReset()` before the target workload
- keep `xrtMemDebugEnable(TRUE)` active during the run
- dump both text and JSON reports after the workload
- investigate:
  - `LEAK`
  - `POOL_LEAK`
  - `OBJECT_LEAK`
  - `DOUBLE_FREE`
  - `INVALID_FREE`
  - `WRONG_ALLOCATOR_FREE`
  - `BUFFER_OVERFLOW_SUSPECT`
  - `OBJECT_DOUBLE_DESTROY`
  - `TEMP_ALLOC`
  - `TEMP_RESET`
