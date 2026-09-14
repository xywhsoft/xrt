---
num: 3
slug: first
title: The First Program: Types, Views, and Versions
volume: 卷一 起步与核心
type: concept
lead: A close reading of core.h — type aliases, the view contract, resource limits, progress callbacks, and the reference-counting primitives.
api: core
---

## Orientation

Chapter 2 got the environment running; this chapter enters `include/xrt/core.h` for real — the lowest-level header of the entire kernel. Its content is small, but every item is used over and over by higher-level modules: any XRT program you write will sooner or later carry `xstrview` in a signature; any parser API you read will eventually run into `xrtresourcelimits`; any immutable shared object you touch owes its lifetime to `xrtRefRetain` / `xrtRefRelease`. This chapter walks the public content of core.h in the order "types → views → limits → progress → reference counting", and establishes two world-views that run through the whole book: **strings are not zero-terminated**, and **shared objects are not managed with the allocator**. As a learning strategy, this chapter is not worth memorizing item by item — every section will reappear at high frequency in later chapters. What is worth doing is running the two complete programs with your own hands, keeping the "borrowing versus owning table" and the "reference-counting state diagram" in your head, and leaving the remaining details to the end-of-chapter cheat sheet and the core reference page for lookup on demand.

## Introduction

Start with two pieces of code every C programmer has written. First: a function must return part of a text — do you return a `char*` or make a copy? Returning a pointer loses the length information and forces the caller to guess whether it is zero-terminated; copying adds an allocation, and with it another failure path. Second: a configuration object is shared by three places — who frees it? The manual convention "whoever finishes last frees it" has no language support in C and sooner or later degenerates into use-after-free or a leak. The third is more insidious: parsing an untrusted input — how deep does nesting have to go before it is malicious? How large before it is an attack? If every parser invents its own rules, the audit has to be done N times.

The industry's common answers to these three problems are: a string-slice type, reference counting, and unified resource limits. XRT gives them their plainest C form in core.h — the two-field struct `xstrview`, the two atomic primitives `xrtRefRetain`/`xrtRefRelease`, and the three-gate struct `xrtresourcelimits` — and makes every API across the library consume these three conventions uniformly. Once you understand this chapter, you can read "who owns what, and where the defenses are" out of any module's signature.

## Concepts

### Reading the library-wide style out of core.h

core.h also teaches you a method for "reading any XRT header". Exported functions carry the `XRT_API` marker and contract comments — the comments state **contracts** (who owns what, when it is valid, what state output parameters are left in on failure), not a restatement of parameter types; when behavior is in question, read the comment before the reference page. The sections of the header mirror the layering of the module: types first, constants in the middle, functions last — matching the layout of the official reference pages.

Note two further style details. First, **no side effects on failure**: every function with output pointers leaves the output objects untouched on the failure path, so caller cleanup can uniformly be written as "ignore the outputs on failure". Second, **no hidden allocation**: results that require allocation (such as the string returned by `xrtFormat`) are carried by the return value with explicit ownership — never "pass a double pointer and let the library quietly allocate for you". Both rules run through the whole library; you can verify them on any later module's signature.

### Type aliases: one library-wide spelling

XRT unifies type spelling with a set of short aliases, all defined in core.h:

| Alias | Equivalent to | Notes |
| --- | --- | --- |
| `int8` … `uint64` | `int8_t` … `uint64_t` | The full fixed-width integer family, used straight from `<stdint.h>` |
| `ptr` | `void*` | Generic pointer; the user-data parameter of callbacks is always this |
| `str` / `cstr` | `char*` / `const char*` | Zero-terminated strings; `str` usually means "you own it, `xrtFree` when done" |
| `bytes` / `cbytes` | `unsigned char*` and const version | Pointers to binary data |
| `xtime` | `int64` | Unix Epoch microseconds, absolute time (expanded in Chapter 41) |
| `xseek` | enum | `XSEEK_START` / `XSEEK_CURRENT` / `XSEEK_END`, the seek origin shared by files and generic IO |

Two details that experienced C hands tend to miss. First, `ptr` rather than a literal `void*` appears in every callback signature — the user-data parameter is uniformly this type, round-tripped with explicit casts. Second, `xtime` uses **microseconds** — not seconds or milliseconds — and is an absolute timestamp: this aligns with syscall precision and avoids the chaos of multiplying and dividing by 1000 everywhere. The time system (clocks, time zones, sleeping) is expanded in Chapter 41; for this chapter it is enough to remember "see `xtime`, think microseconds".

Two accompanying "non-existences": there is no "tri-state boolean" — truth is plain C `bool`; and there is no string type hierarchy — text is either a zero-terminated `str` or a length-carrying view (coming up next).

### Views: xstrview and xbytesview

This is XRT's most important data convention. The two structs have the same shape:

```c
typedef struct xstrview { cstr Data; size_t Size; } xstrview;
typedef struct xbytesview { cbytes Data; size_t Size; } xbytesview;
```

The header comment states the contract in one sentence: **it only borrows memory, owns no data, and does not require a trailing zero**. The corollary "can contain zero bytes" deserves its own expansion: in network protocols and binary formats, fields with a length prefix and arbitrary content are the norm — byte strings in HTTP headers and certificate payloads in TLS handshakes are not guaranteed free of zero bytes. Carrying them in zero-terminated strings means truncation or escaping, both of which are traps; carrying them in views means length is length and content is content. In Volumes 7 and 8 you will see every protocol API's inputs and outputs expressed in views — this is why. Three corollaries run through the whole book:

- **Zero termination is unusable** — you cannot hand `Data` directly to `printf` with `%s`; you must pair it with `Size` using `%.*s`;
- **Lifetime follows the source** — a view does not extend the life of the underlying memory; once the source is freed, the view dangles;
- **Can contain zero bytes** — binary-safe; the inputs and outputs of parser APIs are almost exclusively this.

Constructing a view from a C string literal uses the `XRT_STR_LITERAL("...")` macro (`sizeof(s)-1` automatically drops the zero terminator); at runtime, fill the two fields directly from a buffer, and use `XRT_STR_INIT` for static initialization. The "borrowing versus owning" contrast deserves to be nailed down with a table:

| Form | Owner | Free responsibility | Typical source |
| --- | --- | --- | --- |
| `xstrview` / `xbytesview` | No one | Nothing to free | Literals, slices of a buffer, lent out by an API |
| `str` / `bytes` | The caller | `xrtFree` exactly once | Creating APIs such as `xrtFormat` |
| Library-managed objects | Library and callers share by reference count | Whoever drops it to zero destroys | Chapter 4 `xerror`, Volume 8 certificates |

`XRT_NPOS` is the "no such position/length" sentinel (`(size_t)-1`); find-style APIs return it on a miss. The lifetime discipline of views:

```diagram flow
- Source alive: literals, buffers, and managed strings are all legal view sources
- While borrowing: the source must stay alive and its content unmodified
- Consume and discard: a view itself needs no freeing, and must never be stored anywhere that outlives its source
```

### Resource limits: the shared defense line for untrusted input

`xrtresourcelimits` is the anti-DoS language shared by every parser, with three gates: maximum nesting depth `iMaxDepth`, maximum entry count `iMaxEntries`, and maximum total input bytes `iMaxInputBytes`. `xrtResourceLimitsInit` fills in the defaults; tighten as needed and pass the struct to parser APIs such as JSON/XSON/HTTP. A default of "depth 128, one hundred thousand entries" means: a maliciously crafted ten-thousand-level nested text is rejected at level 128, instead of eating through the stack.

How do you pick your own numbers? Three questions suffice: **how deep can legitimate input nest** (configuration files are usually single-digit; protocol nesting follows the spec), **how many entries at most** (business peak times a margin), and **how large is a single request's input** (derive from the memory budget). Fill the three answers into the struct, and the limit becomes the business spec expressed in code — in review it serves as the documentation of your defense line, instead of `if` checks scattered everywhere. This is the other meaning of "shared language": the same struct moving from JSON to XSON, from file to network stream, keeps its defense semantics, and the audit is done once.

### Progress callbacks and reference counting

Long-running tasks may accept a progress callback. `xrtprogress` describes current progress (bytes processed, current phase, and other fields); `xrtprogressproc` is the callback signature — it receives the progress struct and user data, and returning `false` requests cancellation; `xrtProgressReport` is the library-side reporting entry: the implementer calls it at key points of the loop, doing the null-check and argument assembly boilerplate once and for all. Do no heavy work inside the callback — it runs on the task's processing path; the faster it returns, the more promptly cancellation is honored. Volume 4 uses it for parsing large files, and Volume 12 revisits it under performance.

Reference counting is the lifetime language of shared objects. XRT shapes it as two atomic primitives rather than a "smart pointer":

```diagram state
count=1 creator holds -> count>1 shared by many: xrtRefRetain (before a new holder takes over)
count>1 shared by many -> count=1 creator holds: xrtRefRelease (giving up one share)
count=1 creator holds -> destroyed and freed: the final xrtRefRelease drops it to zero
count>1 shared by many -> refuses to grow: Retain returns -1 (overflow protection)
```

The rules: the count field must be a `volatile int32` embedded as the object's first field; a creation function returns with the count at 1 (the creator's vote); the caller whose `xrtRefRelease` returns 0 is responsible for destruction. Every immutable shared object in the library (regexes, templates, X.509 certificates, TLS sessions, …) is built on this pair of primitives.

## Examples

### Complete program: versions and resource limits

The following program comes from the repository example `examples/core/version_limits/main.c`; it demonstrates the compile-time and runtime version story in one go, along with the default resource limits:

```embed path="examples/core/version_limits/main.c" title="examples/core/version_limits/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/version_limits/main.c -lws2_32 -liphlpapi
version=2.0.0-dev
limits: depth=128 entries=100000 input=268435456
```

**What just happened.** `xrtVersion()` returns the runtime version string, which matches the result assembled from the compile-time macros `XRT_VERSION_MAJOR/MINOR/PATCH` — in the single-header scenario both share one source; with a linked-library integration, a mismatch means the header and the library come from different versions. The default resource limits (depth 128, one hundred thousand entries, 256 MB of input) are the concrete numbers of the three gates from the previous section. The semantics of the three gates are identical across all parsers: exceeding a limit fails and leaves a structured error (Chapter 4), and the remainder of the input is not consumed — "either parse completely, or fail with an explicit reason"; there is no middle state. This also means defensive code can be written very simply: pass in the limits, check the return value, read the error slot — three steps and done.

### Complete program: managing a shared object with reference counting

The second program comes from `examples/core/reference/main.c`; it creates an object with an embedded counter via `xrtMalloc` and walks the full lifecycle of "create → share → drop to zero and destroy":

```embed path="examples/core/reference/main.c" title="examples/core/reference/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/core/reference/main.c -lws2_32 -liphlpapi
value: 42
```

**What just happened.** The object embeds `volatile int32 RefCount` as its first field — a hard layout requirement of `xrtRefRetain`/`xrtRefRelease`, which operate on this field through the pointer. Creation is ownership (count 1); a second holder calls `xrtRefRetain` before taking over; each holder calls `xrtRefRelease` when giving up, and **the call that returns 0 performs the destruction** (here, `xrtFree`). `volatile` is not decoration — the count is incremented and decremented concurrently by multiple threads; remove it and the compiler may cache reads and writes in registers, and the count is no longer reliable.

Two design choices are worth chewing on. First, the primitives return **the new count value** rather than a boolean: the caller can directly check overflow protection (Retain returning -1) and the moment of reaching zero (Release returning 0), without a separate "query the count" API. Second, destruction belongs to "the call that drops the count to zero" rather than to the object itself — which means the same counting skeleton can carry any destruction behavior: freeing memory, closing handles, recursively freeing child objects. Chapter 4's `xerror` and Volume 8's certificate objects are both concretizations of this pattern; when you get there, come back and compare, and you will see the same skeleton growing different flesh.

## Contracts

- **The three view don'ts**: don't own, don't zero-terminate, don't extend lifetime; print with `%.*s` paired with `Size`; a view must never be stored into a structure that outlives its source.
- **Resource limits**: any parsing task over untrusted input must be given explicit limits; defaults are only a starting point, tighten per business; exceeding a limit fails the whole parse and consumes no further input.
- **Reference counting**: the count is embedded as the first `volatile int32` field; creation returns count 1; Retain returning -1 is overflow protection; whoever's Release reaches zero destroys; destruction mirrors creation (`xrtMalloc` against `xrtFree`).
- **Sentinel**: find misses return `XRT_NPOS`; test with `== XRT_NPOS`, never by sign or magnitude comparison.
- **No side effects on failure**: failure paths do not write output parameters and there are no hidden allocations; results requiring allocation are carried by the return value with explicit ownership.
- **Time convention**: `xtime` is an absolute microsecond timestamp; seeking uses the three `xseek` origins uniformly.

## Pitfalls

### Pitfall 1: treating a view as a zero-terminated string

Symptoms: garbage at the end of output, or truncation at bizarre positions; occasional out-of-bounds reads leading to crashes.

Cause: `xstrview`'s `Data` guarantees only `Size` valid bytes; there may be no zero terminator after them — or there may happen to be one; behavior depends on the source and must not be relied upon.

```c bad
xstrview View = XRT_STR_LITERAL("hello");
printf("%s\n", View.Data);   /* out-of-bounds read: no zero terminator is promised past Size */
```

```c good
xstrview View = XRT_STR_LITERAL("hello");
printf("%.*s\n", (int)View.Size, View.Data);   /* length paired with data, binary-safe */
```

### Pitfall 2: forgetting the final Release (or releasing once too many)

Symptoms: the former shows memory that only grows (leak detection in Chapter 6 will catch it); the latter drops the count to zero too early, and the object is destroyed while still in use (crashes or corruption, and hard to reproduce).

Cause: the reference-counting convention is "exactly one Release per holder", and the creator's vote is also a holding.

```c bad
example_object* pKeep = exampleCreate(42);
example_object* pShare = pKeep;
xrtRefRetain(&pShare->RefCount);     /* sharing established */
/* only one share released — the count stays at 1 and the object is never destroyed */
xrtRefRelease(&pKeep->RefCount);
```

```c good
example_object* pObject = exampleCreate(42);
xrtRefRetain(&pObject->RefCount);     /* second vote: sharing established (two holders) */
xrtRefRelease(&pObject->RefCount);    /* first holder gives up */
if ( xrtRefRelease(&pObject->RefCount) == 0 ) {
	/* second release reaches zero: this call performs destruction — Retain and Release strictly paired */
}
```

Note: the correct mental model is "**on every execution path, Retain and Release are strictly paired**" — including error paths. One Release too few on an error path leaks just the same; one too many dangles just the same.

### Pitfall 3: storing a view into a structure that outlives its source

Symptoms: works fine for a while, then occasional garbage or crashes with stack traces pointing at completely unrelated modules; reproduction conditions drift (depending on when the source buffer is reused or freed).

Cause: a view is only a borrowing — once the source's lifetime ends (buffer reuse, managed string freed, stack frame returned), the view dangles. Storing it into a global table, a long-lived object, or another thread's queue plants a time bomb.

```c bad
xstrview gCached;              /* global cache */
void load(const char* sJson)
{
	str s = xrtFormat("%s", sJson);
	gCached = (xstrview){ s, strlen(s) };   /* view points into the managed string */
	xrtFree(s);                             /* source freed; gCached dangles from here on */
}
```

```c good
str gOwned = NULL;             /* to live long, own a copy yourself */
void load(const char* sJson)
{
	xrtFree(gOwned);
	gOwned = xrtFormat("%s", sJson);   /* copy into owning storage */
}
/* xrtFree(gOwned) exactly once before program exit */
```

## Exercises

### Basic: type self-check

Write a program that prints `sizeof(int8/int32/int64/ptr)` and the value of `XRT_NPOS` (following the shape of the types program in Chapter 2), confirming they match your platform's expectations. One step further: construct a view with `XRT_STR_LITERAL` and print it using this chapter's printing posture, getting a first-hand feel for "length separated from data".

### Advanced: walking a view by hand

Construct an `xstrview` over `"XRT-Program-Design"`, and — without any string functions — loop by hand to find the position of the first `-`, printing the two substrings around it (with `%.*s`). Hint: the position type is `size_t`, agree to return `XRT_NPOS` when not found; when checking the result, "not found" must be written `== XRT_NPOS` rather than `< 0` — `size_t` has no negative numbers.

### Challenge: concurrent reference counting

Add multi-threaded stress to the object in `examples/core/reference/main.c`: 4 threads each Retain/Release one million times, then the main thread performs the final Release. Acceptance: the program runs stably and prints "value" successfully (the object was not destroyed prematurely); remove `volatile` and rerun to compare (record whether abnormal exits appear, to appreciate the effect of compiler optimization — observation only, stable reproduction not required). Afterwards, answer one question: why is the object destroyed exactly once in the stress test, never zero or two times? Reason it through with the "Retain and Release strictly paired" rule.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Type aliases | `int8..uint64` / `ptr` / `str,cstr` / `bytes,cbytes` / `xtime` (microseconds) / `xseek` |
| View contract | Borrow only, no zero terminator, may contain zero bytes; print with `%.*s`; construct with `XRT_STR_LITERAL`; never store beyond the source's lifetime |
| Sentinel | Find misses return `XRT_NPOS` (`(size_t)-1`); test with `== XRT_NPOS`; sign comparison forbidden |
| Resource limits | `xrtResourceLimitsInit` three gates: depth/entries/bytes; tighten to business specs; exceeding a limit fails the whole parse |
| Reference counting | First field `volatile int32`; creation = 1; Retain/Release paired; whoever reaches zero destroys |
| Progress callback | `xrtprogressproc` returning `false` requests cancellation; keep the callback light and fast |
| Version | Compile-time macros and `xrtVersion()` share one source; a mismatch means header and library differ |
