---
num: 2
slug: intro
title: Meet XRT and the Development Environment
volume: 卷一 起步与核心
type: practice
lead: The kernel-plus-extensions two-layer structure, a repository tour, single-header integration, and the first program — getting the experimental environment running.
api: core, string
---

## Orientation

XRT is not "yet another utility function library" — it is a systematic cross-platform C infrastructure: from memory, errors, containers, and coroutines to the five-backend network engine, an in-house TLS, and the HTTP/WebSocket protocol core. The first lesson of learning XRT is not memorizing APIs but understanding its "kernel + extensions" two-layer structure — this determines which layer every line you write stands on. This chapter first builds the global picture, then completes all preparation from zero to compilable: get the source, learn the directories, understand the two integration styles, compile the first program, and catch a glimpse of the power of feature trimming. After this chapter you own a development environment ready for free experimentation.

## Introduction

Suppose you want to write a small C service that fetches a configuration file and forwards data. Along the scattered-libraries route you would need: a JSON parsing library, an HTTP client, a TLS library, a thread pool, assorted containers… They come from different authors with different error-handling styles (return codes, errno, setjmp, abort), different memory conventions (whoever allocates frees, reference counting, GC macros), and different build systems (CMake, Meson, hand-rolled Makefiles). Time spent on "stitching" quickly exceeds the business logic itself, and any upstream security update forces you to re-evaluate the whole dependency chain.

XRT's answer is to make these capabilities a **single technology stack**: a unified error model (Chapter 4), unified memory and ownership conventions (Chapter 5), unified view data conventions (Chapter 3), running through from bottom to the networking and protocol layers. Understanding this unification matters more than remembering any single API — that is exactly the panorama this chapter builds.

One hidden cost deserves spelling out: the **upgrade paths** of scattered dependencies are every library for itself — a TLS vulnerability means a separate assessment, a JSON license change means a separate migration, and test coverage varies wildly. A single stack collapses these into one decision: bump one version number, and the whole stack's security patches and regression tests (the repository ships thousands of test source files) arrive together. Chapter 8 covers the version and compatibility conventions; Volume 12 covers managing this upgrade path in engineering.

## Concepts

### Kernel and extensions: a strict two-layer structure

- **Kernel** (`src/` + `include/xrt/`, 91 public headers, hundreds of trimmable modules): the foundation (core / error / atomic / memory), containers, text and data, logging and files/processes, coroutines and async, the five-backend network engine (IOCP / epoll / kqueue / io_uring / select), in-house TLS 1.2/1.3 with X.509, and the HTTP/1.x and WebSocket **protocol core**.
- **Extension libraries** (`extlibs/`, five independent products): `xhttp` (HTTP client/server runtimes), `xws` (WebSocket runtime), `xruntime` (dynamic types and object system), `xmail` (the full mail protocol suite), `xssh` (the SSH2 protocol stack).

Dependencies are **strictly one-way**: extensions call only the kernel; the kernel has zero references to extensions. Volumes 1 through 9 of this book cover the kernel; Volumes 10 and 11 cover the extensions. The direct benefit for users is that "kernel capabilities stand alone" — write a small configuration-parsing tool using only the kernel, completely unaware the extensions exist; and any extension feature, traced to the bottom, is kernel primitives you have already learned. The learning order follows naturally: after Volumes 1–9, you can read extension documentation directly.

One easily confused point: the kernel keeps the HTTP and WebSocket **protocol core** (parsing, encoding, semantic validation), but high-level runtimes — "client", "server", "routing", "connection pooling" — live in the xhttp / xws extensions. Seeing `xrtHttp1RequestParse` means kernel; seeing xhttp's client APIs means extension — different prefix styles, different products.

### Repository tour

After fetching the source, day-to-day development touches only five directories:

| Directory | Contents | Role in this book |
| --- | --- | --- |
| `include/xrt/` | The kernel's 91 public headers; exported functions carry `XRT_API` and contract comments | The "primary text" for every chapter |
| `single/` | Single-header artifacts `xrt.h` / `xrt_decl.h` (self-contained full kernel implementation) | The compilation basis of all examples in this book |
| `src/` | Kernel implementation and `src/internal/` private contract headers | The "answer key" when digging into details |
| `extlibs/` | The five extensions, each with src / include / tests / examples / single | The texts for Volumes 10 and 11 |
| `examples/` | Runnable examples organized by module | The "outside reading" accompanying each chapter |

The remaining directories: `tools/` is the build toolchain; `tests/` is the test suite (Volume 12 is devoted to reading it); `config/modules.json` is the module manifest — the single source of truth for the whole library; `dev/` holds benchmarks and archives.

### Two integration styles

From source to executable, the single-header path takes four steps:

```diagram flow
- Pick modules: define XRT_MODULE_ALL or list the module macros you need
- Enable the implementation: define XRT_IMPLEMENTATION so the header grows function bodies
- Include the single header: include xrt.h (compile with -I pointing at the single/ directory)
- Compile and link: gcc main.c is enough — no XRT library files to link
```

At the engineering stage you can switch to the second path: use the build tools to produce a static or shared library, or bring the kernel sources into your build system with header paths uniformly `<xrt/xxx.h>`. That path unfolds in Volume 12. This book's examples uniformly take the single-header path — its virtue is "copy one file and start", and trimming behavior is plain to see.

## Examples

### The first complete program

Here is the book's first complete program. It uses `xrtFormat` (printf-style formatting returning an XRT-managed string) to assemble a sentence, then calls `xrtFree` when done — these two calls already exercise the kernel's unified memory chokepoint:

```c
/* hello.c —— the first program of this book */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

int main(void)
{
	str s = xrtFormat("Hello, XRT %s!", xrtVersion());
	if ( s == NULL ) {
		return 1;
	}
	printf("%s\n", s);
	xrtFree(s);
	return 0;
}
```

```term
$ gcc -O1 -I single hello.c -lws2_32 -liphlpapi
$ ./a.exe
Hello, XRT 2.0.0-dev!
```

**What just happened.** Two macro lines appear before the include: `XRT_MODULE_ALL` selects all modules (you can also list only what you need — see the next section); `XRT_IMPLEMENTATION` makes this translation unit "grow" the function bodies — in single-header mode, implementation and declaration live in one file, switched by this macro. `xrtFormat`'s return type is `str` (an XRT-managed string; Chapter 3 expands the type aliases), and **whoever receives the return value is responsible for `xrtFree`** — the library-wide ownership convention, fully developed in Chapter 5.

Note in passing how the failure path is written: on allocation failure `xrtFormat` returns `NULL`, the program checks and returns a non-zero exit code. In real engineering you would also read the thread error slot to print the reason — that is Chapter 4's content; this chapter keeps the program minimal. One detail worth turning into a habit now: `main` returning anything other than `0` means failure. All examples in this book follow "zero succeeds, non-zero fails", aligned with the direction of XRT API `bool` returns.

The `-lws2_32 -liphlpapi` in the build command are the Windows platform's network system libraries — even though this program writes no networking code, the XRT kernel's network modules reference them when all modules are enabled; Linux/macOS do not need these two flags. This command recurs throughout the book; Chapter 8 explains why link dependencies also shrink after trimming.

### The power of trimming: one program, two module sets

Change the first macro line to only the modules needed, everything else untouched:

```c
/* hello-min.c —— enable only the string module and its dependency closure */
#define XRT_MODULE_STRING
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

int main(void)
{
	str s = xrtFormat("Hello, XRT %s!", xrtVersion());
	if ( s == NULL ) {
		return 1;
	}
	printf("%s\n", s);
	xrtFree(s);
	return 0;
}
```

```term
$ gcc -O1 -I single hello.c -lws2_32 -liphlpapi
$ gcc -O1 -I single hello-min.c -lws2_32 -liphlpapi
$ ls -l a.exe a2.exe
-rwxr-xr-x 1 admin 106496 a.exe      ← XRT_MODULE_ALL
-rwxr-xr-x 1 admin  24064 a2.exe     ← XRT_MODULE_STRING（数值因工具链与版本而异）
```

**What just happened.** `XRT_MODULE_STRING` enables only the string module; the foundation it depends on (core, error, etc.) is automatically expanded by the **dependency closure** in features.h — no hand-listing needed. "Pick one module, bring everything it needs" is what closure means. Size differences vary by version and toolchain; the numbers above are illustrative only. Run `ls -l` yourself once and you will feel "on-demand delivery" directly. Note the second benefit in passing: trimming is not just about size — linked system libraries, the initialized module table, and even the attack surface all shrink together; all three matter equally when embedding into other small tools. The complete module-macro mechanism unfolds in Chapter 8.

## Contracts

- **One-way dependency**: extensions call only the kernel; kernel code never references xhttp/xws.
- **Single-header macro conventions**: `XRT_IMPLEMENTATION` may appear in exactly **one** translation unit of the whole project; module macros must be defined before including `<xrt.h>`.
- **Dependency closure**: enabling one `XRT_MODULE_` macro automatically brings all its dependencies; hand-listing them invites omissions — leave it to the closure.
- **Version line**: compile-time macros and the runtime `xrtVersion()` share one source (demonstrated in Chapter 3); linked-library integrations use them to verify header and library match.
- **Example baseline**: this book's examples use the repository root as working directory and Windows + GCC output as baseline; Linux/macOS simply drop the two link flags, and matching behavior is not annotated again.

## Pitfalls

### Pitfall 1: putting XRT_IMPLEMENTATION in a header or defining it twice

Symptoms: the linker reports a flood of "multiple definition" errors (the duplicated symbols being XRT implementation symbols).

Cause: `XRT_IMPLEMENTATION` makes the including translation unit generate function bodies; two units defining it = two implementations.

```c bad
/* xrt_util.h —— your own common header */
#define XRT_IMPLEMENTATION     /* included by several .c files → multiple implementations */
#include <xrt.h>
```

```c good
/* xrt_util.c —— the only implementation unit */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
/* xrt_util.h only carries #include <xrt.h> (without the two macros) */
```

### Pitfall 2: writing module macros from memory

Symptoms: the compiler reports "implicit declaration", or compilation passes but linking reports undefined symbols — you included the header yet functions are missing.

Cause: the wrong set of module macros — the enabled modules don't cover the APIs in use. Writing macro names from memory invites exactly this error; in particular, the kernel's foundation (core/error) is always present and has no separate module macro — there is no "add a foundation macro" move.

```c bad
#define XRT_MODULE_JSON        /* only JSON enabled — it does not depend on string formatting */
#define XRT_IMPLEMENTATION
#include <xrt.h>
/* calling xrtFormat later → string not enabled, linking fails */
```

```c good
#define XRT_MODULE_STRING      /* check include/xrt/features.h or the Chapter 8 cheat sheet */
#define XRT_IMPLEMENTATION
#include <xrt.h>
```

## Exercises

### Basic: run it and modify it

Compile and run `hello.c`; change the output to two greetings (call `xrtFormat` twice — mind the two `xrtFree` calls).

### Advanced: minimal module set

`hello.c` uses only formatting and the version query. Trim it to the minimal module set that still compiles, and record the macros you enabled and the binary size. Hint: start from `XRT_MODULE_STRING` and see what the linker still misses.

### Challenge: three-tier size comparison

Compile the same program with `XRT_MODULE_STRING`, `XRT_MODULE_REGEX`, and `XRT_MODULE_ALL`; record the three binary sizes and explain where the differences come from (what does regex carry beyond string? How many times larger is all-modules than one module?). Acceptance: a three-row comparison table plus two sentences of analysis; the `XRT_MODULE_ALL` tier must not be smaller than the single-module tier.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Two-layer structure | Kernel (`include/xrt/`) + five extensions (`extlibs/`); dependencies strictly one-way |
| Single-header three steps | Module macros → `XRT_IMPLEMENTATION` → `#include <xrt.h>` (`-I single`) |
| Implementation macro | Appears exactly once per project, in the single `.c` |
| Trimming | `XRT_MODULE_*` picks modules, dependency closure expands automatically; macro names per features.h |
| Managed string | `xrtFormat` returns `str`; `xrtFree` when done |
| Protocol boundary | The HTTP/WS protocol core is kernel (http, http1, websocket modules); runtimes are xhttp/xws |
