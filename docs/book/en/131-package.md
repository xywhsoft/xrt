---
num: 131
slug: package
title: Packaging, Distribution, and the Single Header
volume: 卷十二 工程实践
type: practice
lead: The static/shared artifact naming matrix, real-consumer verification, amalgamate's topological synthesis and consistency check — from the source tree to deliverables.
api: core, error
---

## Orientation

Chapter 129 compiled, Chapter 130 trimmed; this chapter turns the result into **deliverables**. `tools/package.py` packs two artifact kinds from the manifest: **static** (`libxrt.a`/`xrt.lib` — archived per closure) and **shared** (`xrt.dll`+import library/`libxrt.so`/`libxrt.dylib` — tri-state naming per platform); `--verify` does release-grade verification — **really linking and running a minimal consumer of the release artifact** (not "links successfully" but "the consumer runs"). **The single header** (`single/xrt.h` + the declarations-only `xrt_decl.h`): `tools/amalgamate.py` synthesizes it in module topological order — local includes inlined, selection macros reordered, `--check` asserting **zero difference between regeneration and the committed file** (the machine guarantee that the single header never drifts). The release-maturity check (`check_release_maturity.py`) and per-extension-library packaging (libxruntime et al. from their own manifests) close the chapter.

## Introduction

"Packaging" sounds like mere archiving — the real pitfalls lie in **verification depth**. Shallow verification: the artifact exists, the size looks sane. Deep verification: **the consumer's viewpoint** — can the static library really be linked and run by a third-party project (missing symbols/duplicate symbols/ABI mismatches all blow up at link time or runtime)? Does the dynamic library's import library really match the exports? Is the single header really **behaviorally identical** to the modular sources (a hand-maintained single header necessarily drifts — the source changed, regeneration forgotten)? XRT's answers are all mechanical: `--verify` compiles and **runs** a minimal consumer (the end-to-end of release semantics); `amalgamate --check` regenerates and compares (zero tolerance for single-header drift — the same discipline SPEC applies to "manually editing wwwroot is forbidden", applied to code); the maturity check aggregates all red lines.

The single header's value bears repeating: **one file + one include** is complete integration — no build-system hookup, no link configuration, no header search paths — the first choice for scripts, quick prototypes, and teaching (every term block's compile line since Chapter 3 of this book is the single-header form). The price is compile time (the whole library enters one TU) and no selective compilation (the trimming macros still work — `XRT_MODULE_*` functions identically inside the single header; Chapter 130's trim semantics carry over losslessly).

## Concepts

### The artifact matrix and naming

| kind | Windows/MSVC | Windows/GNU | macOS | Linux |
| --- | --- | --- | --- | --- |
| static | `xrt.lib` | `libxrt.a` | `libxrt.a` | `libxrt.a` |
| shared | `xrt.dll`+`xrt.lib` (import lib) | `xrt.dll`+`libxrt.dll.a` | `libxrt.dylib` | `libxrt.so` |

**Closures trim into artifacts**: `--suite` decides which modules are packed (closure expanded) — "a dynamic library with just networking" and "a full static library" are each one command away. Artifact names carry the product prefix (extension libraries pack as `libxruntime.a` etc. — the product field of their own manifests).

### verify: the consumer, end to end

```diagram flow
- Pack: closure sources -> compile objects -> archive (static) / link (shared)
- Verify: generate a minimal consumer program (include the public headers + call basic APIs)
  -> truly link against the artifact -> run -> assert the output
- Semantics: the release is "alive" from the consumer's viewpoint - not existing on disk but usable
```

`--verify`'s depth lies in **running**: a successful link only proves the symbols are all there; a successful run proves initialization/ABI/runtime behavior are correct. This is the same philosophy as the tutorial gate's "examples compile and run" (Chapter 129) — **only executable facts count**.

### amalgamate: synthesizing the single header

```diagram flow
- Input: the manifest (module topological order) + module sources
- Synthesis: local includes inlined (in-repo #includes expand into the single file)
  - system includes kept - the selection-macro zone reordered by dependency order
  - produces the implementation single/xrt.h and the declarations single/xrt_decl.h
- Consistency: amalgamate --check regenerates and compares byte for byte with the committed file
  - zero difference passes - single-header drift surfaces at the quality gate
```

**The declarations version** (`xrt_decl.h`): a declarations-only single header — the "just want the prototypes" form for multi-TU projects (declarations when linking a library artifact, the full version for the single implementation TU). **Topological order** is the root of synthesis correctness: inter-module dependencies fix the concatenation order — the depended-upon come first (amalgamate reuses the manifest's topological sort — yet another consumption of the same source of truth).

### The full release-gate chain

The xruntime README's sequence is the template: `build.py` (regression) → `amalgamate --check` (single-header consistency) → `check_release_maturity.py --release` (maturity red-line summary) → `package.py --kind static --verify` / `--kind shared --verify` (both artifacts + consumer verification). **Each command verifies one orthogonal dimension** — a failure points straight at its dimension.

## Examples

### First complete program: minimal integration as the consumer

The program below is from `examples/core/version_limits` — consumable in single-header, static-library, and dynamic-library forms:

```embed path="examples/core/version_limits/main.c" title="examples/core/version_limits/main.c"
```

```term
$ gcc -O1 -I single -include xrt.h impl.c examples/core/version_limits/main.c -lws2_32 -liphlpapi
version=2.0.0-dev
limits: depth=128 entries=100000 input=268435456
```

**What just happened.** (1) This compile line is the **single-header consumer form**: `-I single -include xrt.h` + `impl.c` (the implementation TU defining `XRT_IMPLEMENTATION` — the tutorial's fixed pairing); the minimal consumer generated by `package --verify` is isomorphic to this. (2) Switching to the library form: after building `libxrt.a`, the consumer's compile becomes `-I include` + `main.c -lxrt` (declarations from the public headers, implementation from the library) — **one main, three consumption forms**, whose behavior must agree (verify's implicit assertion). (3) No-selection=Core (Chapter 130) pays off here: this consumer also links and runs against the minimal artifact (the core closure).

### Second complete program: a customizing consumer

The second program is from `examples/core/allocator_tour` — memory takeover on the consumer side:

```embed path="examples/core/allocator_tour/main.c" title="examples/core/allocator_tour/main.c"
```

```term
$ gcc -O1 -I single -include xrt.h impl.c examples/core/allocator_tour/main.c -lws2_32 -liphlpapi
allocator: swap -> at-family alloc/calloc/dup/realloc ok
allocator: locked after first alloc -> swap rejected ok
```

**What just happened.** (1) The first-line swap-and-lock semantics hold equally for **library artifacts**: every allocation inside the static/dynamic library goes through the host allocator (the release library's "memory belongs to the host" integration contract). (2) This example's role in the gate is exactly "consumer diversity" — beyond verify's minimal consumer, the whole examples set are real consumers (Chapter 129: examples are gates); allocator integration is the deepest customization path among them. (3) The packaging view of trimming: the core-closure artifact already covers every symbol this example needs — the minimal release face suffices.

### Choosing among three consumer kinds

The three artifact forms map to three consumer kinds — the choice on one page. **Single header** (`single/xrt.h`): scripts, quick prototypes, teaching, small tools — zero build-system cost, one include is complete; the price is whole-single-header compile time in one TU. **Static library** (`libxrt.a`): the default for real projects — linking is freezing (no runtime dependency), closure trimming precise to modules, natural integration with the host build system; the price is one artifact per platform per configuration. **Dynamic library** (`so/dll/dylib`): many programs share one implementation, independently upgradable (with stable ABI) — rarer for C libraries but common for plugin hosts; the price is the platform matrix of import libraries/symbol exports and deployment distribution. This book's tutorial uses the single header throughout (the term compile lines prove it) because readers copy-and-run; real projects choose by deployment form — the three forms behave identically (verify's implicit assertion), so switching costs nothing.

## Contracts

- **Artifact matrix**: static/shared × four-platform naming rules; shared carries an import library on Windows; product names follow the manifest.
- **Closure trimming**: --suite decides artifact content; closure expansion as in build.
- **verify depth**: generate a minimal consumer → really link → run → assert — executable facts.
- **Single-header synthesis**: topological-order concatenation; local includes inlined, system includes kept; selection macros reordered by dependency.
- **Dual-version single header**: implementation xrt.h / declarations xrt_decl.h — multi-TU uses the declarations.
- **Consistency check**: --check regenerates and compares with zero difference — zero tolerance for single-header drift.
- **Maturity summary**: check_release_maturity aggregates the red lines — release is a conclusion, not a guess.
- **Extension packaging**: each extension library packs its own artifact from its own manifest (libxruntime etc.) — the closure includes the required core trim.
- **Mutually exclusive linking**: never link two libraries containing the same core closure simultaneously (stated plainly in the xruntime README).

## Pitfalls

### Pitfall 1: changing sources and forgetting to regenerate the single header

Symptom: modular tests all green, single-header users fail to compile or behave differently — single/xrt.h is stale.

Cause: the single header is a **generated artifact** — hand-editing it or forgetting amalgamate leaves the committed file inconsistent with the sources. `--check` intercepts in CI; the local habit is regenerate immediately after changing sources.

```c bad
/* hand-fix a typo in single/xrt.h - the next generation overwrites it, and CI --check fails */
```

```c good
/* change module source -> python tools/amalgamate.py -> commit source and generated header in one PR */
```

### Pitfall 2: linking duplicate closures of library and extension

Symptom: duplicate-symbol link errors, or worse — two copies of core state (an allocator each) scrambling at runtime.

Cause: the extension artifact (say libxruntime) already contains the required core closure — linking a full libxrt on top adds a second implementation. The contract states plainly "another library containing the same XRT closure must not be linked at the same time".

```c bad
gcc app.c -lxruntime -lxrt   /* two copies of core: duplicate symbols / double state */
```

```c good
/* use libxruntime (which embeds its core closure) - do not also link libxrt;
   or integrate the extension in source form (its single header embeds the required core) */
```

### Pitfall 3: taking artifact size as the release-quality metric

Symptom: "the new version is 5% bigger, roll back" — without checking baselines (different compiler/optimization/strip).

Cause: size comparison requires the same environment baseline (Chapter 130's seven elements); release quality's red lines are verify + the maturity check (function and consistency) — size is only a trend reference.

```c bad
if ( size(new) > size(old) * 1.05 ) { rollback(); }  /* meaningless across environments */
```

```c good
/* same-baseline trend monitoring + forbid_symbols structural assertion + verify functional red line */
```

## Exercises

### Basic: dual-artifact packaging verification

`package.py --suite core --kind static --verify` and `--kind shared --verify` — inspect artifact names and the consumer-verification output. Acceptance criteria: both artifacts generated, verify passes for each; the import library (Windows) present.

### Advanced: a trimmed artifact

Build a static library with `--suite core,queue,queue_spsc`; use Chapter 21's SPSC example as the consumer, link and run. Acceptance criteria: the consumer passes; the artifact contains no unselected module symbols (compare with nm).

### Challenge: a single-header consistency drill

Change one module source's comment → `amalgamate --check` (should fail — the header is stale) → regenerate → `--check` passes → both-track regression. Acceptance criteria: the loop closes end to end; you understand what --check compares (regeneration vs the committed file).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Artifact matrix | static (libxrt.a/xrt.lib) / shared (dll+import lib/so/dylib) |
| Closure into artifacts | --suite decides content — minimal release face in one command |
| verify depth | consumer really links + runs — executable facts |
| Single-header synthesis | topological concatenation, local includes inlined, macros reordered |
| Dual versions | xrt.h implementation / xrt_decl.h declarations — multi-TU uses declarations |
| Consistency | --check zero difference — zero tolerance for single-header drift |
| Maturity | check_release_maturity aggregates the red lines |
| Extension packaging | own manifest, own artifact — closure embeds the core |
| Mutual exclusion | never double-link two libraries with the same closure |
| Single-header value | one file, one include, full integration — scripts/prototypes/teaching |
