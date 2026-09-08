---
num: 8
slug: core-trim
title: Versions, ABI, and Module Trimming
volume: 卷一 起步与核心
type: practice
lead: Forward module selection and dependency closure — from one macro line to an exact binary; version duals and the engineering conventions of delivery shapes.
api: core, memory_stats
---

## Orientation

In Chapter 2 you compiled XRT in with two macro lines; this chapter explains them thoroughly: how the **forward selection** of `XRT_MODULE_*` expands through the dependency closure into an exact feature set, how `XRT_EXCLUDE_*` removes intrusive diagnostics while keeping full functionality, and how compile-time `XRT_FEATURE_*` gets probed. Then versions: what the compile-time macros and the runtime `xrtVersion()` each govern, and how headers and libraries verify they match. These things are invisible in normal times, but when something goes wrong (link failures, odd sizes, header/library mismatch) it is always them — this chapter closes Volume 1 here, and completes the full answer to Chapter 2's "three-tier size comparison" exercise.

## Introduction

The same library must enter two completely different hosts: a few-hundred-KB command-line tool that only reads a JSON configuration; and a resident backend service wanting the full networking, TLS, and HTTP family. In the scattered-dependency era these were two dependency lists and two build configurations — and the "small tool" half was usually forced to link the entire transport stack, because the dependency tree's granularity stopped at "library".

XRT's delivery granularity is the **module**: behind 91 public headers stand hundreds of trimmable modules, each declaring its own source files and dependencies. You declare the root modules you need in a translation unit, and the build system expands the closure — the small tool links the few thousand lines it uses; the service links everything; same version, same headers. The mechanism's name is **forward module selection**: declare only "what I want", never "what I don't".

Compared to "reverse exclusion" (compile everything, then carve out), forward selection has one decisive advantage: **new dependencies never silently change your build**. In a reverse system, some module today adds a transport-layer dependency; your "exclusion list" is unchanged yet the artifact got fatter. In a forward system, your declared roots are unchanged, and however the closure changes, it affects only that root's own subtree. Delivery predictability follows — which is why this book has taught only the forward form since Chapter 2.

## Concepts

### The dependency closure: from root modules to feature sets

```diagram flow
- Declare root modules: define XRT_MODULE_REGEX (write only what you need)
- Expand the closure: features.h recursively brings in every dependency per modules.json
- Generate feature macros: every affected module gets an XRT_FEATURE_* definition
- Exact implementation: only selected modules' sources compile and link
```

Four key points. **Declare roots only**: the dependency table is maintained in `config/modules.json`; users need not — and should not — copy it; that is what "forward selection" means. **Macros before includes**: module macros must be defined before that translation unit's **first** inclusion of `xrt.h` or `xrt/features.h`; too late means no effect. **Implement once**: `XRT_IMPLEMENTATION` is defined in one translation unit of the whole project (the root of Chapter 2's Pitfall 1). **Consistent sets**: with modular linking, all translation units participating in the same XRT instance should use a consistent module set — two units each picking their own yields undefined behavior in the linked instance.

`modules.json` as the single source of truth deserves another look: every module's sources, internal headers, dependencies, tests, and examples live in the manifest, and both `features.h` (the basis of closure expansion) and the website's reference pages are generated from it. The "module domains" you saw in Chapter 2, Chapter 6's material lists, and this chapter's trimming behavior all point at the same data — documentation, code, and build never talk past each other; that is the engineering payoff of "single source of truth".

### Exclusion macros: full functionality minus diagnostics

When `XRT_MODULE_ALL` enables everything and you only need to exclude **intrusive diagnostics** like memory debugging (it changes the global allocation path — Chapter 6), use an exclusion macro rather than hand-listing hundreds of forward macros: `XRT_EXCLUDE_MEMORY_DEBUG` excludes both `memory_debug` and its reporting modules while keeping `memory_stats`. Exclusion macros act only on `XRT_MODULE_ALL`'s implicit selection — under forward selection you never selected them to begin with.

The design motive deserves unpacking: diagnostic modules come in exactly two states — "optional add-on" or "must be explicitly off" — with no middle. `XRT_MODULE_ALL` means "give me complete production functionality" — and complete production functionality precisely does **not** need the memory debug heap resident. So the exclusion macros became the standard way to express "full functionality minus diagnostics": CI release builds use them to drop the debug modules; local development builds keep everything. Understanding this layer, you can read why the module macros in the repository's example headers look the way they do — they are all instances of these semantics.

### Compile-time probing: XRT_FEATURE_*

After closure expansion, every enabled module defines its corresponding `XRT_FEATURE_` macro. The public headers use `#if defined(...)` liberally for conditional declarations — which means your code can probe capabilities at compile time too: a header can offer different inline implementations depending on whether the TLS feature exists, without relying on the build system passing flags. Chapter 9's atomic-operations header is one of the first consumers.

### Version duals and delivery shapes

Version has two lines (their shared source was demonstrated in Chapter 3): **compile-time** `XRT_VERSION_MAJOR/MINOR/PATCH` and `XRT_VERSION_TEXT` (integer macros testable with `#if`; the string macro for display), and **runtime** `xrtVersion()` returning the version string linked into the program. In single-header integration the two necessarily agree; in **linked-library** integration they may not — when the header is new and the library old, `xrtVersion()` is the single truth, and checking it once at program startup before doing any work is the standard move.

Delivery takes two roads: the single header (used by all of this book's examples; `single/xrt.h` self-contains the implementation) and sources/libraries (`config/modules.json` declares ownership; Volume 12 covers packaging). The API is identical on both roads; switching changes only the build script. Three practical selection considerations: **start-up speed** — the single header is copy-and-go, unarguably fastest; **build incrementality** — in medium-to-large projects a single header means the implementation unit's recompile cost grows with library size, while the sources/library shape restores normal incremental builds; **symbol boundaries** — dynamic-library delivery controls the export table, and scenarios with multiple components in a host process each linking XRT also rely on the library shape for isolation. The common evolution path is exactly "prototype with the single header, switch to libraries at engineering scale" — not one line of code changes between the roads.

## Examples

### First program: minimal module set and size comparison

Chapter 2's three-tier comparison gets its full version here — one program, two module tiers:

```c
/* hello.c —— serves both tiers: module macros injected via -D on the command line */
#include <xrt.h>
#include <stdio.h>

int main(void)
{
	printf("XRT %s\n", xrtVersion());
	return 0;
}
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single hello.c -lws2_32 -liphlpapi
$ gcc -O1 -DXRT_IMPLEMENTATION -I single hello.c
$ ls -l a.exe a2.exe
-rwxr-xr-x 1 admin 106496 a.exe
-rwxr-xr-x 1 admin  12288 a2.exe
（体积因版本与工具链而异，比值才是重点）
```

**What just happened.** The module macros moved to `-D` on the command line — exactly equivalent to writing them in the source, with the benefit that one source compiles into several tiers (the standard posture for trim-matrix testing in CI). The `-DXRT_MODULE_ALL` tier pulls in all modules (including networking/TLS/HTTP, hence linking the system network libraries); the second tier **writes no module macro at all** — the core foundation is always present and untrimmable, `xrtVersion` belongs to core, so defining only `XRT_IMPLEMENTATION` compiles it, with no system network libraries needed. The order-of-magnitude difference shows "trimming" is not saving a few functions — it is whole dependency subtrees entering and leaving.

### Second program: probing capabilities at compile time

```c
/* probe.c —— probe which features the closure expanded */
#define XRT_MODULE_STRING
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

int main(void)
{
	printf("version=%s\n", xrtVersion());
#if defined(XRT_FEATURE_STRING)
	printf("feature: string = yes\n");
#else
	printf("feature: string = no\n");
#endif
#if defined(XRT_FEATURE_TLS)
	printf("feature: tls = yes\n");
#else
	printf("feature: tls = no\n");
#endif
	return 0;
}
```

```term
$ gcc -O1 -DXRT_MODULE_STRING -DXRT_IMPLEMENTATION -I single probe.c -o probe.exe
$ ./probe.exe
version=2.0.0-dev
feature: string = yes
feature: tls = no
```

**What just happened.** Only `XRT_MODULE_STRING` was declared; after closure expansion `XRT_FEATURE_STRING` is defined and `XRT_FEATURE_TLS` is not — probe results correspond one-to-one with the selection. The pattern's value lives in headers and template code: capability differences fork at **compile time**, with no runtime cost and no extra build-system flags. Note the probe macros must be tested in the same translation unit as `xrt.h` (include first, then test), and keep include order stable when crossing headers.

Connecting the two examples completes the module system's full picture: the command-line `-D` decides "what is selected" (delivery), `XRT_FEATURE_` probing reflects "what was selected" (capability), and `xrtVersion()` reports "who is providing the service" (version). The three close the loop in one translation unit — you never maintain any "what this project enabled" list outside the build script; the macros and the probes are the list, and the compiler checks consistency for you.

## Contracts

- **Macros before includes**: module macros and `XRT_IMPLEMENTATION` must all be defined before the first inclusion of `xrt.h`/`features.h`.
- **Implement exactly once**: one implementation translation unit per project; all other units include declarations only.
- **Consistent sets**: all translation units of the same XRT instance use a consistent module set.
- **Exclusion boundary**: `XRT_EXCLUDE_*` suppresses only `XRT_MODULE_ALL`'s implicit selection and never affects forward declarations.
- **Version check**: with linked-library integration, compare `xrtVersion()` against the compile-time macros at startup; a mismatch is a deployment error.

## Pitfalls

### Pitfall 1: defining module macros after the include

Symptoms: the macro is clearly defined yet behavior says otherwise — missing symbols at link time or features not enabled, with no explicit error.

Cause: macro expansion happens at the point of inclusion; definitions later than `#include` have no effect on the already-completed inclusion.

```c bad
#include <xrt.h>          /* include first: the closure already expanded with "no selection" */
#define XRT_MODULE_STRING /* too late — this translation unit will not enable string */
```

```c good
#define XRT_MODULE_STRING /* declare first */
#define XRT_IMPLEMENTATION
#include <xrt.h>
```

### Pitfall 2: two translation units each picking their own modules

Symptoms: bizarre runtime behavior — an object works on some call paths and crashes on others; or missing/duplicate symbols at link time.

Cause: unit A selected `XRT_MODULE_NET_TCP`, unit B only core — the structures B sees at compile time disagree with A's layout (conditional declaration differences); the same XRT instance now holds two world-views.

```c bad
/* a.c */                /* b.c */
#define XRT_MODULE_ALL   #define XRT_MODULE_STRING
#include <xrt.h>         #include <xrt.h>
/* two units with inconsistent module sets: linked into one instance, layouts diverge */
```

```c good
/* common.h —— the project's single module declaration point */
#define XRT_MODULE_ALL
/* common.c: #define XRT_IMPLEMENTATION then include (the only implementation unit) */
/* a.c / b.c: include common.h directly; the set is always consistent */
```

## Exercises

### Basic: the probe triple

Compile probe.c for the three tiers "no module macro", "XRT_MODULE_STRING", "XRT_MODULE_ALL"; record the `tls` probe result for each and explain the difference in one sentence.

### Advanced: the trimming matrix

Compile hello.c in five tiers (none / string / regex / string+regex / all); record for each whether `-lws2_32` is needed and the binary size, and draw a comparison table. Hint: inject module macros with `-D` and distinguish outputs with `-o`.

### Challenge: selection for a small tool

For a CLI tool that only needs "read a JSON config + write logs": pick the minimal module set, verify it compiles, links, and works, and record the final binary size and its ratio to the `XRT_MODULE_ALL` tier. Acceptance: every macro in the set has a one-sentence justification; the ratio is below one quarter; the functional verification output is correct.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Forward selection | Declare root modules only (the `XRT_MODULE_` family); the dependency closure expands automatically; delivery is predictable |
| Macro timing | All macros before the first inclusion of `xrt.h` / `features.h`; later definitions never take effect |
| Implementation | `XRT_IMPLEMENTATION` once per project, in the single implementation translation unit |
| Exclusion | `XRT_EXCLUDE_MEMORY_DEBUG` etc. suppress only `XRT_MODULE_ALL`'s implicit full selection |
| Probing | The closure defines the `XRT_FEATURE_` family; compile-time forking at zero runtime cost |
| Version | Compile-time macros test with `#if`; linked-library integration checks `xrtVersion()` at startup before working |
| Delivery | Single header (`single/`) and sources/libraries (modules.json) share one API; prototype with the header, switch to libraries at scale |
| The trio | `-D` picks delivery, `XRT_FEATURE_` probes capability, `xrtVersion()` verifies version — closed in one unit |
