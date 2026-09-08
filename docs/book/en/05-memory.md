---
num: 5
slug: memory
title: Memory Basics: The Global Heap, Alignment, and Allocators
volume: 卷一 起步与核心
type: practice
lead: Five entry points unify all dynamic memory in the library — allocation, growth, duplication, secure zeroing, and whole-allocator replacement.
api: memory, core
---

## Orientation

Chapter 3 established the language of "owning and borrowing"; this chapter provides the physical exit point of "owning": all dynamic memory in XRT goes through five entry points — `xrtMalloc` / `xrtCalloc` / `xrtRealloc` / `xrtMemDup` / `xrtFree` — plus `xrtSecureZero` for sensitive cases. The value of the chokepoint is not syntactic sugar: it makes three otherwise-impossible things possible — **replacing the underlying allocator wholesale** (to plug in a custom pool or host runtime), **per-allocation observation** (Chapter 6's statistics and debugging), and **precise fault injection** (to verify failure paths). After this chapter, every block you allocate stands on the unified exit.

## Introduction

A real dilemma: your program must embed into someone else's large system, and they require all memory to go through their allocator (they account, quota, and reclaim by module); meanwhile your code already has dozens of `malloc`/`free` calls. Replacing them one by one is a disaster — so you define macros, batch-replace, and pray nothing slips through. Another dilemma: in testing you want to know "if the 1000th allocation fails, does the program roll back correctly" — scattered `malloc` has no injection point at all.

Both dilemmas point to the same design answer: memory entry points must be **choked off**, and at the library level, not the coding-discipline level. XRT converges all dynamic memory into five functions, so replacement, observation, and injection each become a one-line, global-switch operation.

The chokepoint pays a third dividend: **uniform alignment and layout guarantees**. Pointers returned by the five entry points are allocated with maximal fundamental alignment — they can directly hold any scalar, struct, or SIMD vector, and you never need to debate "this buffer holds a float array — do I need aligned_alloc?"; all XRT containers (Volume 3) internally use the same entry points — element alignment inside containers is guaranteed by the library, independent of whatever your supplied allocator happens to do. The exit this chapter builds is the shared physical foundation of every later volume's containers, parsers, and network buffers.

## Concepts

### The semantics of the five entry points

| Entry point | Semantics | Key details |
| --- | --- | --- |
| `xrtMalloc` | Allocate (owning) | Same semantics as standard malloc |
| `xrtCalloc` | Allocate count × size and zero | Overflow checks match the standard |
| `xrtRealloc` | Grow/shrink | **On failure returns NULL and the original block stays valid** |
| `xrtMemDup` | Duplicate a memory range into a new owning allocation | Allocation + memcpy in one step |
| `xrtFree` | Free | **NULL is accepted** — no null checks needed |
| `xrtSecureZero` | Secure zeroing | Never elided by the compiler as a dead store |

The first five all follow the owning convention: **whoever receives the return value frees it, exactly once**. `xrtFree(NULL)` being legal looks small to write and is big to read — cleanup code never again wraps itself in layer upon layer of null checks.

### Allocator replacement: one decision, globally effective

```diagram flow
- Business code: the five entry points like xrtMalloc
- Global chokepoint: the single channel, carrying a replaceable allocator table
- Allocator table: xrtSetAllocator replaces it before the first allocation
- System heap: the default backend; swappable for a host allocator / custom pool
```

The `xallocator` struct carries allocate, grow, and free callbacks plus a context pointer; `xrtSetAllocator` swaps the whole table, and `xrtGetAllocator` copies the current one. **Replacement is allowed only before the first allocation** — afterwards the table is permanently locked and further replacement fails. This "lock early" convention buys a precious property: the allocator is constant during runtime, so no allocation path needs to consider the "allocator changed mid-flight" race condition, and no locks are needed on hot paths.

### The At family: allocations carrying their birthplace

`xrtMallocAt` / `xCallocAt` / `xReallocAt` / `xFreeAt` / `xMemDupAt` are debug entry points carrying `__FILE__` / `__LINE__` — the plain `xrtMalloc` macro expands to the At family when the debug module is enabled, giving every allocation a position. This is the cornerstone of Chapter 6's leak detection; here it is enough to know "position information comes free with the chokepoint".

### When you really need a different allocator

Three typical scenarios are worth meeting early. **Host embedding**: your module runs inside someone else's process (game engine, script runtime) and the host requires all memory through its accounting — wrap the host's allocation functions as the three callbacks and replace at the very start; this chapter's second example is the complete template. **Quotas and limits**: a multi-tenant service limits per module; a custom allocator accumulates bytes in its context and returns `NULL` past the limit — combined with Chapter 4's error model, over-limit behaves like ordinary OOM and business code needs no special case. **Measured comparison**: during performance tuning you want to A/B different backends (Chapter 22's memory pool is a ready candidate) — one replacement line switches. The common precondition is the same: because of "lock before first allocation", the replacement must happen at the very beginning of `main` — putting it after configuration loading may already be too late.

## Examples

### Complete program: walking the five entry points

From the repository example `examples/core/memory/main.c` — the full lifecycle of allocate, grow, duplicate, securely zero, and free:

```embed path="examples/core/memory/main.c" title="examples/core/memory/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/core/memory/main.c -lws2_32 -liphlpapi
value=42 copied=yes
```

**What just happened.** (1) `xrtCalloc(4, 1)` allocates and zeroes; the marker value 42 is written. (2) `xrtRealloc` grows to 16 bytes — the semantic crux is on the failure side: when it returns `NULL`, **the original block stays valid**; this example exits directly for brevity — the robust shape is Pitfall 1. (3) `xrtMemDup` copies the static source into an independent owning buffer, and `memcmp` verifies byte-for-byte equality; note the failure path frees the already-held `pValues` before exiting — "no half-built results on failure" has been practiced since Chapter 1, and this is the standard shape of Chapter 4's error model cooperating with this chapter's ownership convention. (4) `xrtSecureZero` clears both buffers: it targets the compiler's "dead-store elimination" — a plain `memset` right before `free` may be judged meaningless and deleted wholesale, and sensitive data (keys, tokens) then leaks into reused heap memory; `xrtSecureZero` uses volatile writes or barriers to guarantee the zeroing really happens. (5) The free order reverses the acquisition order, and `xrtFree` needs no null check.

### Complete program: replacing the allocator and observing the lock

From `examples/core/allocator_tour/main.c` — implement a counting allocator, replace before the first allocation, and verify the "lock early" convention:

```embed path="examples/core/allocator_tour/main.c" title="examples/core/allocator_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/allocator_tour/main.c -lws2_32 -liphlpapi
allocator: swap -> at-family alloc/calloc/dup/realloc ok
allocator: locked after first alloc -> swap rejected ok
```

**What just happened.** (1) The counting allocator still routes to the system heap, only accounting — the three callbacks `exampleAlloc` and friends fill the `xallocator` table, and `main` calls `xrtSetAllocator` as soon as it enters. (2) After replacement, a round of allocations through the At family all goes through the new table. (3) The second replacement is rejected: after the first allocation the table is locked, and the call returns failure rather than silently ignoring — the error signal is explicit and the "allocator is constant" invariant holds. This pattern is the standard hookup for embedding into a host runtime: wrap the host's allocation functions as three callbacks and complete the replacement at the very start.

Read through the example's implementation details too — three things are worth stealing. The callback signatures all carry a context pointer (`exampleAlloc(ptr pContext, size_t iSize)`) — the allocator's own state never lives in globals, so multiple allocator instances never interfere. The counting state lives in a struct carried by the context — fully isomorphic to Chapter 3's "callback user data" pattern, which you will see again and again. Finally, note the replacement happens before any XRT call: `main`'s first act is `xrtSetAllocator`, even before `printf` — "very start" is not rhetoric, it is a hard ordering.

## Contracts

- **Owning**: pointers returned by the five entry points are freed exactly once; `xrtFree(NULL)` is legal; cleanup code needs no layered null wrapping.
- **Realloc failure preservation**: failure returns `NULL` with the original block's contents and validity unchanged; after success the old pointer is dead and must not be referenced again.
- **SecureZero is not optional**: zeroing sensitive memory must use it; `memset` may be optimized away, and the risk only shows up in release builds.
- **Allocator lock**: `xrtSetAllocator` takes effect only before the first allocation; the allocator is constant at runtime and hot paths are lock-free; "very start" means `main`'s first act.
- **Alignment**: the entry points allocate with maximal fundamental alignment; returned pointers work for any scalar, struct, or vector type; container element alignment is guaranteed by the library.
- **Callback shape**: the allocator's three callbacks follow the library-wide callback convention — the leading context pointer carries state, never globals.

## Pitfalls

### Pitfall 1: overwriting the original pointer with Realloc's result

Symptoms: occasional memory leaks (failure branches), magnified under stress testing or OOM injection.

Cause: `xrtRealloc` returns `NULL` on failure and **the original block stays valid** — overwriting the original pointer with the return value loses it at the moment of failure, and that block is never freed.

```c bad
pValues = (unsigned char*)xrtRealloc(pValues, 4096);
if ( pValues == NULL ) {
	return 1;   /* the original pointer was overwritten — the pre-4096 block leaks forever */
}
```

```c good
unsigned char* pNew = (unsigned char*)xrtRealloc(pValues, 4096);
if ( pNew == NULL ) {
	xrtFree(pValues);   /* original block still valid: clean up along the failure path */
	return 1;
}
pValues = pNew;         /* take over the new pointer only on success */
```

### Pitfall 2: zeroing keys with memset

Symptoms: a security audit finds key fragments in heap dumps or subsequently reused memory; release builds show it more readily than debug builds.

Cause: the compiler sees `memset(key, 0, n)` immediately followed by `free(key)`, judges the zeroing a "dead store", and deletes it wholesale — an optimization the standard permits, which leaves sensitive data in the heap.

```c bad
memset(pSecretKey, 0, iKeySize);
xrtFree(pSecretKey);   /* the memset is very likely optimized away — key remains in heap memory */
```

```c good
xrtSecureZero(pSecretKey, iKeySize);
xrtFree(pSecretKey);   /* the zeroing is guaranteed to really happen */
```

## Exercises

### Basic: the five-entry checklist

Following `examples/core/memory/main.c`, write a program: Calloc a range, Realloc it larger and verify the old contents survive, MemDup and verify with memcmp, and finally free in reverse order. Print every verification result.

### Advanced: a counting allocator

Implement a "byte-counting" allocator (tracking current live bytes); after replacing it, run a stretch of mixed allocate/free code and print the live-byte count and a leak verdict at the end. Hint: the three callbacks carry the counting state in the context pointer; allocation adds, free subtracts; when the `realloc` callback receives an old pointer, subtract the old size first — how do you get the old size? The simplest way is to allocate one extra header storing the size and read it back in the callback. When you finish, you will better appreciate what "an allocator is a complete backend" means.

### Challenge: verify the locking timeline

Write a program verifying `xrtSetAllocator`'s locking boundary: the first replacement succeeds, one allocation happens, the second replacement must fail. Then answer: if XRT allowed runtime replacement, what protection would your counting allocator need? (Hint: think about whether reads and writes of the allocator table itself need a lock when two threads allocate simultaneously.) Acceptance: the two replacements' return values are one true and one false, and the output matches the comments.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Five entry points | Malloc / Calloc / Realloc / MemDup / Free (NULL legal; cleanup needs no null checks) |
| Owning | Whoever receives frees, exactly once; failure paths clean up too |
| Realloc | On failure `NULL` and the original block stays valid — take the new pointer first, overwrite later |
| Sensitive zeroing | `xrtSecureZero`; never rely on `memset` |
| Allocator | `xrtSetAllocator` replaces before the first allocation; permanently locked afterwards; replacement is `main`'s first act |
| At family | `xrtMallocAt` etc. carry `__FILE__`/`__LINE__` — the cornerstone of the debug module |
| Backend-swap scenarios | Host embedding (accounting), quota limits (over-limit behaves as OOM), backend A/B comparison |
