---
num: 129
slug: build
title: The Build System, End to End
volume: 卷十二 工程实践
type: practice
lead: One manifest as the single source of truth, modular and single-header dual-track builds, fingerprint caching and parallel jobs, manifest stacking for extension libraries — the foundation of the toolchain.
api: core, error
---

## Orientation

Volume 12 starts with the toolchain. XRT's build is not a hand-written Makefile but a Python toolchain **with `config/modules.json` as its single source of truth**: the manifest declares every module's headers/sources/tests/examples/docs (including the dependency closure), and `tools/build.py` drives two verification tracks from it — **modular compilation** (selecting sources per module, caching objects by closure fingerprint) and the **single-header regression** (`tools/amalgamate.py` synthesizes `single/xrt.h`, then recompiles all tests). This chapter makes four things clear: **the manifest structure** (schema/dependencies/three test-asset classes: tests/single_tests/examples); **build.py's CLI face** (suite selection/test filtering/parallel jobs/cflag injection/manifest stacking — extension libraries share the same toolchain); **why two tracks** (modular proves "this trim compiles", single-header proves "the merge conflicts nowhere" — each track is a release gate); **fingerprint caching** (objects recompile only when the dependency-closure fingerprint changes — the correctness root of incremental builds).

## Introduction

C-library build systems suffer three chronic ills: **manifest drift** (CMakeLists and the real source files tell different stories — a file added, the list forgotten, or the reverse); **fake trimming** (claiming configurability but never verifying that trim combinations compile — feature-macro combinations never tested); **single-header desynchronization** (the single header maintained by hand, a module changed and the merge forgotten). XRT's answer: **one JSON manifest as the source of everything** — build (what to compile), amalgamate (what to merge), package (what to pack), measure (what to benchmark) all start from the same modules.json. "The manifest drives everything" eliminates drift by construction: a source not in the manifest doesn't compile; a test not in the manifest doesn't enter the regression.

The dual track, expanded: the **modular track** takes the dependency closure of your chosen suite (say `core,queue`) and compiles only those sources — verifying "this trim compiles and runs"; the **single-header track** first merges all selected modules into one `single/xrt.h` (amalgamate concatenates in topological order, deduplicates includes, inlines local headers), then compiles the same tests as one translation unit with `XRT_IMPLEMENTATION` — verifying "the merged single header has no symbol clashes or macro leaks". Every PR must pass both tracks before merging — this is the implementation face of the SPEC's "check + embedded examples compiled into CI".

## Concepts

### The manifest: each module's declaration card

```diagram flow
- modules.json (schema 1): one card per module
  - name / state / feature (feature macros)
  - depends (the module's dependency table - the edges for closure computation)
  - public_headers / internal_headers / sources
  - tests / single_tests / examples (three test-asset classes)
  - benchmarks / docs (benchmark and documentation ownership)
- Extension stacking: extlibs/*/config/modules.json stacks via the --manifest parameter
  - xhttp/xws/xssh/xmail/xruntime share the same toolchain
```

The manifest is simultaneously the **documentation index** (the docs field) and the **benchmark registry** (the benchmarks field — the queue module's card carries all of dev/bench/queue's scripts and reports) — the full meaning of "source of truth": compile, test, package, measure, document lists — five faces, one card.

### build.py's CLI face

| Parameter | Purpose |
| --- | --- |
| `--suite core,queue` | module names/comma combinations/all — dependency closure expanded automatically |
| `--test test_queue_oom` | run a single modular test |
| `--start-test`/`--start-single-test` | **resume** the regression from a named test (checkpoint restart — fix one, run the rest) |
| `--jobs 8` | parallel compile jobs for single-header tests (tests still run in manifest order — determinism) |
| `--no-single`/`--no-examples` | skip tracks (modular only / test gate only) |
| `--cflag`/`--ldflag` | inject compile/link flags (cross scenarios / sanitizers) |
| `--compiler tcc`/`--arch x86` | compiler and architecture choice (TinyCC 32-bit is also a gate combination) |
| `--manifest path` | stack an extension library's manifest |
| `--rebuild` | ignore fingerprints and force recompilation |

### Dual-track builds and fingerprint caching

**Modular track**: suite→closure→compile objects per module (`-DXRT_FEATURE_*` generated from the manifest's feature fields)→link→run tests in manifest order; examples compile and run one by one (the tutorial's embedded examples are verified here — the SPEC gate's "embedded examples compile"). **Single-header track**: amalgamate produces `single/xrt.h` (plus the declarations-only `xrt_decl.h`)→single_tests compile with `#define XRT_IMPLEMENTATION` + include of the single header→run. **Fingerprint caching**: objects cache by "dependency-closure fingerprint" — any source/header inside the closure changes, the fingerprint changes, only then recompile; `--rebuild` invalidates explicitly. **Architecture matrix**: native/x86/x64 × gcc/tcc/cl — the release gates cover 32-bit TinyCC (the stand-in for resource-constrained targets).

### Extension libraries share the toolchain

The five extension libraries of Chapters 109–128 each carry a `config/modules.json` — with `--manifest extlibs/xhttp/config/modules.json` stacked, the same build.py compiles and tests the extension closure (including "the core trim closure the extension requires"). xruntime's README shows the full gate sequence: the build regression, the amalgamate consistency check (`--check`), the release-maturity check, package verification — **library and extensions, one toolchain, one discipline**.

## Examples

### First complete program: a manifest-declared example

The program below is from `examples/core/version_limits` — a member of the manifest's examples array (both build tracks compile it):

```embed path="examples/core/version_limits/main.c" title="examples/core/version_limits/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/version_limits/main.c -lws2_32 -liphlpapi
version=2.0.0-dev
limits: depth=128 entries=100000 input=268435456
```

**What just happened.** (1) This file appears in the core module's examples array — `python tools/build.py --suite core` compiles and runs it; every tutorial-embedded example since Chapter 3 works the same way (**tutorial code is gate code** — where the SPEC's red lines are machine-executed). (2) `xrtVersion()` and `xrtResourceLimitsInit` are APIs declared in core's public headers — the manifest's public_headers field maps one-to-one onto the docs/api contract cards. (3) The default resource bounds (depth 128 / 100k entries / 256 MiB input) are every parser's shared defense line — Chapter 132's testing system comes back to build DoS tests on these bounds.

### Second complete program: the allocator-tour example

The second program is from `examples/core/allocator_tour` — integration verification for a custom allocator:

```embed path="examples/core/allocator_tour/main.c" title="examples/core/allocator_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/allocator_tour/main.c -lws2_32 -liphlpapi
allocator: swap -> at-family alloc/calloc/dup/realloc ok
allocator: locked after first alloc -> swap rejected ok
```

**What just happened.** (1) After `xrtSetAllocator` swaps in a custom allocator, the at-family four entrances run (Chapter 5's memory face, regressed at the example level). (2) The **locked-after-first-allocation** semantics verified: a swapped allocator cannot be swapped again — the stability guarantee for embedding hosts (Chapter 134). (3) The two output lines are themselves products of the dual-track build: manifest-driven, gate-executed — every tutorial example you have seen passed through this chain.

## Contracts

- **Single source of truth**: modules.json drives build/amalgamate/package/measure/doc — add the manifest before adding sources; keep both synchronous.
- **Three asset classes**: tests (modular) / single_tests (single header) / examples (tutorial + gate) — each with its track.
- **Closure semantics**: a suite expands to its dependency closure; compiling/trimming/packing all follow the closure.
- **Dual-track gate**: modular proves the trim compiles, single-header proves the merge conflicts nowhere — both green before merge.
- **Deterministic order**: tests run in manifest order (jobs parallelize compilation only, never tests).
- **Fingerprint caching**: objects cache by closure fingerprint; --rebuild invalidates explicitly.
- **Architecture matrix**: native/x86/x64 × gcc/tcc/cl — TinyCC 32-bit is the resource-constrained gate.
- **Checkpoint restart**: start-test/start-single-test resume from the breakpoint — fix one, run the rest.
- **Extension stacking**: --manifest stacks extension manifests — library and extensions, one toolchain.
- **Examples are gates**: examples compile and run in both tracks — tutorial-embedded code with zero extra maintenance.

## Pitfalls

### Pitfall 1: adding a source file and forgetting the manifest

Symptom: "compiles locally" (IDE full build), but CI's modular track compiles an incomplete closure — link failure, or worse (symbols leaking in from another module).

Cause: the IDE ignores the manifest; build.py compiles only what the manifest lists. The difference between "passes locally" and "passes the gate" is exactly the manifest.

```c bad
/* create src/foo/bar.c and commit directly - the manifest doesn't have it */
/* CI: the modular track's closure is incomplete - link failure */
```

```c good
/* add it to the foo module's sources array in modules.json in the same change */
/* the PR diff shows manifest entry and source file together - reviewable consistency */
```

### Pitfall 2: merging after running only the modular track

Symptom: the single-header track explodes — two modules' private symbols/macros clash after the merge; the user's single/xrt.h doesn't compile.

Cause: modular compilation isolates inter-module private-name clashes; only the single-header merge exposes them. Neither track can be skipped.

```c bad
python tools/build.py --suite my_module --no-single && merge
/* skipping the single-header track before merging - clashes blow up in the user's hands */
```

```c good
/* before merging a PR: both tracks green - --no-single is only for fast mid-development iteration */
python tools/build.py --suite my_module   /* includes the single-header track */
```

### Pitfall 3: treating --jobs as test parallelism

Symptom: assuming --jobs 8 runs tests in parallel — tests with order dependencies (shared ports/temp files) occasionally interfere.

Cause: --jobs parallelizes **compilation** only; tests run strictly in manifest order (determinism first — reproducible failures are the gate's value).

```c bad
/* assuming --jobs 8 runs tests in parallel - tests sharing resources occasionally interfere */
/* fact: --jobs parallelizes compilation only; tests strictly follow manifest order */
```

```c good
/* want speed: run a subset, not in parallel */
python tools/build.py --suite my_module --test test_queue_oom
/* checkpoint restart: --start-test continues from the failure */
```

## Exercises

### Basic: run the core suite on both tracks

`python tools/build.py --suite core` (no --no-single) — observe both tracks' output. Acceptance criteria: modular and single-header tests all green; examples compile once per track; you can point to fingerprint caching working (a second run skips compiled objects).

### Advanced: trim-combination verification

Compile with `--suite core,queue,queue_spsc` and run all tests; then compare timing with `--no-single`. Acceptance criteria: the closure contains no unselected module's sources; the trim combination's tests all green.

### Challenge: reproducing the extension gate sequence

Run the gate sequence from the xruntime README step by step (build regression / amalgamate --check / package --verify). Acceptance criteria: you understand what each command verifies; you can explain amalgamate --check's "generation consistency" in your own words (regenerating yields zero difference from the committed single header).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Source of truth | config/modules.json — compile/test/package/measure/docs, five faces one card |
| Three asset classes | tests (modular) / single_tests (single header) / examples (tutorial + gate) |
| Dual track | modular proves trimming, single-header proves merging — both green before merge |
| Fingerprint cache | objects keyed by closure fingerprint; --rebuild invalidates |
| Determinism | tests in manifest order; jobs parallelizes compilation only |
| Checkpoint restart | start-test/start-single-test |
| Architecture matrix | native/x86/x64 × gcc/tcc/cl — TinyCC 32-bit is a quality gate |
| Extension stacking | --manifest stacks — five extension libraries, one toolchain |
| Examples are gates | examples compile in both tracks — tutorial code with zero second implementations |
| cflag/ldflag | inject compile/link flags (sanitizers/cross) |
