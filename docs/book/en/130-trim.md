---
num: 130
slug: trim
title: Feature Trimming and Size Control
volume: 卷十二 工程实践
type: practice
lead: Positive module selection with closure expansion, ALL+EXCLUDE exclusion semantics, granularity principles, size profiles and symbol prohibitions — trimming the library until the target can hold it.
api: core, error
---

## Orientation

Chapter 129 covered how the build compiles trim combinations; this chapter covers **how a trim is declared, and how the size is verified to actually shrink**. Three blocks: **the positive selection model** (`XRT_MODULE_*` declares only root modules; the dependency closure expands automatically — "users need not know which internal features a module currently depends on, nor copy the dependency table"); **ALL+EXCLUDE exclusion semantics** (`XRT_MODULE_ALL` + `XRT_EXCLUDE_MEMORY_DEBUG` — the precise expression of everything-minus-diagnostics; explicit root modules outrank exclusions); **the granularity principle** (one public module = one small group of capabilities sharing state/dependencies/scenarios — no per-function macros and no master switch; new capabilities first face the three questions "can it be tested, trimmed, and owned independently?"). **Size verification**: the profiles of `config/size_profiles.json` (each profile a suite + a `forbid_symbols` prohibition table) with `tools/measure_size.py` — the core profile forbids any symbol of the regex/compression/network/protocol families — **a trim is not a declaration but a verified fact**.

## Introduction

The embedded target can't hold the whole library? Cut what you don't need — sounds simple, but historical implementations suffer three ills: **hand-copied dependency tables** (users list every needed macro themselves — an internal dependency changes and every user's copy goes wrong); **implicit everything** (no selection gives you everything — the "minimal program" secretly carries TLS); **fake granularity** (macros per function — an explosion nobody uses; or one master macro — trimming becomes all-or-nothing). XRT's three remedies: closure expansion (declare the root, expand the dependencies — the table is maintained once, inside the library); **no selection = Core only** ("when no XRT_MODULE_* is defined, only the feature-free Core contract is provided" — the minimal integration is predictable, new modules never silently enlarge existing programs); **the three granularity questions** (independent testing / independent trimming / independently documentable ownership — all three before a new module exists).

The `forbid_symbols` design deserves emphasis: a size profile is not just "compile this suite" — it also **asserts the artifact contains no other family's symbols**: if a TLS-family symbol pops up in the core profile's link (a dependency pulled in by mistake), measure fails outright. Trim errors surface at the quality gate, not when the online binary has already bloated.

## Concepts

### Positive selection and the closure

```diagram flow
- Declare: #define XRT_MODULE_STRING_SPLIT (a root module) -> #include "xrt.h"
- Expand: features.h expands the closure from the module manifest - string base + split come along automatically
  - the user copies no dependency table; internal dependency evolution is invisible to users
- Implementation unit: XRT_IMPLEMENTATION defines in exactly one translation unit
- Consistency: at modular link time, all TUs use a consistent module set
```

**Timing discipline**: selection macros must be defined before the first include of `xrt.h`/`features.h` — a define after the include has no effect (the expansion already happened).

### ALL and EXCLUDE semantics

`XRT_MODULE_ALL`: for hosts/compilers/debug tools that need the complete runtime. `XRT_EXCLUDE_MEMORY_DEBUG`: ALL minus memory debugging (excluding both memory_debug and memory_debug_report, keeping memory_stats) — the precise expression of "full production functionality without invasive diagnostics". **Precedence rule**: EXCLUDE suppresses only ALL's implicit selection; **explicitly defined root modules still win** (ALL+EXCLUDE_X+explicit MODULE_X = X stays in). **The standing advice** (contract-literal): "ordinary applications should not select all modules by default".

Remember ALL+EXCLUDE's combination semantics with one derivation: `XRT_MODULE_ALL` implicitly selects every module in the manifest; `XRT_EXCLUDE_MEMORY_DEBUG` removes the two diagnostic modules from the implicit set — at this point the artifact = everything minus diagnostics; then an explicit `#define XRT_MODULE_MEMORY_DEBUG` — the explicit root module is **added back**, outranking the exclusion (three-layer evaluation order: explicit > ALL's implicit > EXCLUDE's removal). This precedence design lets "everything minus one" and "precisely named" coexist — a host tunes module by module atop ALL without losing default convenience.

### The granularity principle: the three module questions

One public module = one small group of capabilities sharing **state, dependencies, and scenario**. The counter-examples: macros per function (macro explosion) / one master switch for a whole family (all-or-nothing). The Queue family and the network family are positive samples: Queue's base/SPSC/MPSC/MPMC/waiting/cancel/Select/coroutine bridge select individually; the network's Engine/each backend/TCP/UDP/sync/Future/DNS/proxy/TLS each form their own closure. **New public capabilities pass the three questions first**: can it be tested independently? trimmed independently? owned and documented independently? — all yes, then a new module; never widen an existing master macro (the mechanism that keeps granularity from degrading).

### Size profiles and symbol prohibitions

```diagram flow
- Profile: each entry of size_profiles.json is {name, suite, forbid_symbols}
  - the core profile forbids the regex engine prefix, compression internals prefixes (private symbols of regex/compression)
  and the public symbol prefixes of the network/HTTP/WebSocket/TLS families
- Measure: measure_size.py compiles per profile -> records the environment baseline (platform/compiler/optimization/stripped)
  -> asserts zero prohibited symbols -> size report (baseline included, preventing cross-environment miscomparison)
- Gate: any prohibited symbol appearing fails - a mistaken dependency pulls in is exposed on the spot
```

**The environment baseline's** meaning: binary sizes are incomparable across compilers/optimization levels/platforms — the report records seven elements `platform/machine/compiler_family/compiler_version/arch/optimization/stripped`, and trend comparisons happen only within the same baseline (a prelude to Chapter 135's measurement method).

## Examples

### First complete program: minimal Core integration

The program below is from `examples/core/version_limits` — running under the feature-free Core contract:

```embed path="examples/core/version_limits/main.c" title="examples/core/version_limits/main.c"
```

```term
$ gcc -O1 -I single -include xrt.h impl.c examples/core/version_limits/main.c -lws2_32 -liphlpapi
version=2.0.0-dev
limits: depth=128 entries=100000 input=268435456
```

**What just happened.** (1) The compile line has **no XRT_MODULE_ALL** — no selection = Core only: version query and resource bounds are part of the Core contract (structured errors/error slot likewise), while printf-style error construction is a separate `XRT_MODULE_ERROR_FORMAT` choice — the minimal program carries no formatting runtime (the contract's own example). (2) Against Chapter 129's compile line (with ALL): **one example, two integration forms** — ALL is the tutorial's convenience form, no-selection is the factual declaration of minimal integration. (3) The size angle: this form's artifact is exactly the core size profile — the profile's symbol prohibitions assert it contains no regex/compression/network symbol whatsoever.

### Second complete program: minimal integration with allocator replacement

The second program is from `examples/core/allocator_tour` — host integration inside the Core closure:

```embed path="examples/core/allocator_tour/main.c" title="examples/core/allocator_tour/main.c"
```

```term
$ gcc -O1 -I single -include xrt.h impl.c examples/core/allocator_tour/main.c -lws2_32 -liphlpapi
allocator: swap -> at-family alloc/calloc/dup/realloc ok
allocator: locked after first alloc -> swap rejected ok
```

**What just happened.** (1) The same minimal form without ALL — `xrtSetAllocator` is part of Core's memory face (Chapter 5). (2) **First-allocation locking's trimming significance**: an embedding host (Chapter 134) swaps the allocator at the top of main — from then on every library allocation goes to the host; the lock guarantees it can't be swapped again in multi-module/multi-TU scenarios (integration stability). (3) Trimming + allocator replacement is the standard posture of embedded integration: small binary + memory under the host's governor — the two examples together form the full portrait of minimal integration.

### Versus host-language trimming views (a design observation)

The trimming problem exists in the host-language world too (C++ template instantiation, Rust feature gating); the contrast clarifies the trade. **Rust features**: Cargo.toml declares features + dependency closures — the model is nearly isomorphic (positive selection + closure expansion); the difference is that XRT's forbid_symbols assertion (artifact-level verification) is stricter than most ecosystems. **C++ templates**: "trimming" happens automatically by instantiation — zero declaration cost, but the trim result is unpredictable (instantiation creep) and unverifiable (no equivalent of "prohibited symbols"). XRT chooses explicit declaration + artifact assertion: one extra line of macro buys "what was trimmed" becoming a verifiable fact — in embedded delivery, that determinism is worth more than convenience.

## Contracts

- **Positive selection**: XRT_MODULE_* declares root modules; closures expand from the manifest; users copy no dependency table.
- **Timing**: selection macros before the first include, kept synchronous with it; XRT_IMPLEMENTATION in exactly one TU; a consistent TU set at link time.
- **No-selection semantics**: only the feature-free Core contract — new modules never silently enlarge existing programs.
- **EXCLUDE semantics**: suppresses ALL's implicit selection; explicit root modules win; MEMORY_DEBUG exclusion takes report, keeps stats.
- **The three granularity questions**: independent testing / independent trimming / independently documentable ownership — all three before a new module.
- **Granularity samples**: Queue's eight pieces and the network family select item by item — no per-function macros, no master switch.
- **Size profiles**: {name, suite, forbid_symbols}; any prohibited symbol appearing fails the quality gate.
- **Environment baseline**: seven elements (platform/machine/compiler family/version/architecture/optimization/strip) — trends compare only within the same baseline.
- **Standing advice**: ordinary applications should not default to ALL — ALL belongs to hosts and tools.

## Pitfalls

### Pitfall 1: defining module macros after the include

Symptom: the module's APIs are invisible (the compiler reports undeclared) — though you definitely defined them.

Cause: the expansion happens at the first include; later defines don't retroact. Macros must precede every xrt.h/features.h include (command-line -D or the very top of the file).

```c bad
#include "xrt.h"                 /* include first: the Core expansion is already fixed */
#define XRT_MODULE_QUEUE         /* a late selection: ineffective */
```

```c good
#define XRT_MODULE_QUEUE         /* declared at the top */
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

### Pitfall 2: copying the dependency table (manually listing every macro)

Symptom: after a library upgrade the build breaks — internal dependencies changed, your copied table is stale.

Cause: the dependency closure is the library's private business — the contract says plainly "the dependency table should not be copied". Declare only the root you want (STRING_SPLIT, not STRING + STRING_SPLIT's internal combination).

```c bad
/* a hand-copied "closure" */
#define XRT_MODULE_STRING
#define MY_SPLIT_LEGACY_PATCH /* a hand-patched macro copying an old dependency */
```

```c good
#define XRT_MODULE_STRING_SPLIT  /* declare the root only - the closure expands inside the library */
```

### Pitfall 3: comparing sizes across environments

Symptom: "this change made the binary 30% bigger" — checking baselines: last time gcc, this time tcc.

Cause: sizes are incomparable across compilers/optimization levels/strip; measure_size's seven-element baseline exists precisely for this. Trend comparisons must share a baseline (same profile, same environment), or use symbol prohibitions instead of absolute sizes.

```c good
/* verify the report's seven baseline elements match before comparing; gate size with forbid_symbols (environment-independent) */
```

## Exercises

### Basic: comparing three integration forms

Compile the same example with no selection / `MODULE_STRING_SPLIT` / `MODULE_ALL` — compare symbol tables (nm/findstr) and sizes. Acceptance criteria: the three forms' symbol sets grow strictly; the no-selection form has zero non-Core symbols.

### Advanced: EXCLUDE semantics verification

Compile with `MODULE_ALL` + `XRT_EXCLUDE_MEMORY_DEBUG` — confirm the memory-debug APIs are absent while stats remain; then add an explicit `XRT_MODULE_MEMORY_DEBUG` — confirm explicit precedence (the debug APIs return). Acceptance criteria: all three symbol checks match the contract.

### Challenge: a custom size profile

Write a new profile for a "containers + strings only" target: the suite fixes the root modules, forbid_symbols lists the network/protocol family symbols — run measure_size. Then deliberately pull a wrong dependency into the suite — watch the prohibition assertion fail. Acceptance criteria: the clean profile is all green; the mispulled profile fails on the prohibited symbol (the error names the symbol).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Selection model | positive root modules + closure expansion — no copying dependency tables |
| Timing | macros before the first include; IMPLEMENTATION in exactly one TU |
| No selection | Core contract only — minimal integration predictable |
| EXCLUDE | suppresses ALL's implicit; explicit roots win; MD exclusion takes report, keeps stats |
| Three granularity questions | independent testing/trimming/ownership — all three before a module |
| Granularity samples | Queue's eight pieces, the network family item by item — no per-function macros, no master switch |
| Size profiles | name+suite+forbid_symbols — zero prohibited occurrences (the quality gate) |
| Environment baseline | seven elements — trends compare only within one baseline |
| Advice | ordinary applications don't ALL — ALL belongs to hosts/tools |
