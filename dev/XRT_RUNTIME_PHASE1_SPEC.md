# XRT Runtime Phase-1 Refactor Specification

## 1. Document Control

- Document: `dev/XRT_RUNTIME_PHASE1_SPEC.md`
- Status: Active draft
- Version: `0.1.0`
- Last updated: `2026-03-13`
- Scope: Phase-1 runtime and threading refactor for XRT
- Source of truth: This file is the primary development spec for phase-1 runtime work

### 1.1 Purpose

This document exists to prevent design drift during the runtime refactor.
It records the target state model, thread lifecycle, runtime modes, cleanup
rules, compatibility policy, milestones, and work tracking model.

If future work happens in a new chat or after long pauses, development must
resume from this file first.

### 1.2 Update Rules

The following rules are mandatory:

1. Every material runtime design change must update this file before or with code.
2. Every completed task must update the progress checklist in Section 14.
3. Every decision that changes scope, compatibility, or tradeoffs must add an item to Section 17.
4. Every work session should append a short entry to Section 18.
5. Phase-1 code is not considered stable until the acceptance criteria in Section 15 are met.

### 1.3 Status Legend

- `todo`: Not started
- `in_progress`: Active work
- `blocked`: Waiting on a decision, dependency, or bug fix
- `done`: Implemented and accepted against milestone criteria
- `deferred`: Explicitly pushed out of phase-1


## 2. Refactor Summary

`xrt` started as a convenience utility library, later expanded toward network
infrastructure, and is now being repositioned as a C infrastructure runtime for
the Internet + AI era.

The current runtime core no longer matches that target:

- process-global state and thread-bound state are mixed inside `xCore`
- runtime-bound APIs depend on process-global mutable storage
- thread lifecycle semantics are weak and platform behavior is inconsistent
- temporary memory and error storage are not safe for concurrent use
- the network stack is already evolving toward stronger ownership and threading guarantees
- memory-system phase-2 work needs a stable runtime/thread foundation first

Phase-1 is not the memory pool rewrite. Phase-1 exists to establish the runtime
execution model that phase-2 will build upon.


## 3. Goals

### 3.1 Product Goals

Phase-1 must make XRT easier to use in threaded code without making the API feel
heavy or framework-like.

The desired user experience is:

- single-threaded code remains simple
- `xrt`-created threads can call XRT APIs directly without extra boilerplate
- runtime-bound APIs still feel script-like and convenient
- internal runtime rules become deterministic enough for later pool and container work

### 3.2 Technical Goals

- Split runtime state into one process-global state and one thread-local state
- Remove thread-bound mutable data from the global runtime object
- Define a strict runtime execution model based on thread attachment
- Refactor the thread library so thread lifecycle and cleanup semantics are explicit
- Guarantee automatic cleanup of XRT-managed thread resources
- Keep convenience APIs such as `xvoGetValueText` working during phase-1
- Ensure future thread-local memory pools can bind to thread state without redesigning the runtime again

### 3.3 Phase-1 Success Criteria

Phase-1 is successful only if all of the following are true:

- global runtime data and thread runtime data are structurally separated
- `LastError`, temporary memory, and default RNG live in thread state
- `xrtInit()` attaches the current thread to the runtime
- `xrtThreadCreate()` creates threads that automatically attach and clean up
- thread cleanup is deterministic and does not rely on user discipline for XRT-managed threads
- no hot-path design depends on repeated FLS/TLS lookup per allocation or per object operation
- no phase-2 memory pool design assumption contradicts the phase-1 thread model


## 4. Non-Goals

The following are explicitly out of scope for phase-1:

- making legacy containers internally thread-safe
- making legacy memory pools internally thread-safe
- redesigning the `xvalue` public convenience API family
- removing `xvoGetValueText` or similar convenience APIs
- finishing full embed-mode support for every platform shape in the first iteration
- rewriting all network subsystems to a single threading abstraction in phase-1


## 5. Scope Boundaries

### 5.1 Included in Phase-1

- runtime state split
- thread state attach/detach model
- thread creation/wait/destroy lifecycle redesign
- thread cleanup stack
- migration of `LastError`
- migration of temporary memory
- migration of default RNG state
- compatibility behavior for convenience APIs that depend on temporary memory

### 5.2 Deferred to Phase-2

Deferred phase-2 work is now tracked in `dev/XRT_RUNTIME_PHASE2_SPEC.md`.

- thread-bound memory pools for general-purpose runtime allocations
- thread-safe legacy mempool/memunit/bsmm/fsmempool
- thread-safe `xvalue` refcounting
- thread-safe dict/list/array/buffer internals
- deep cross-thread ownership rules for legacy high-level structures


## 6. Core Design Principles

1. One process-global runtime state, one thread-local runtime state.
2. Runtime-bound APIs require an attached XRT thread context.
3. `xrt`-managed threads must feel as simple as single-threaded use.
4. Thread cleanup must be automatic for XRT-managed threads.
5. Hot paths must not depend on repeated platform TLS/FLS queries.
6. Strict mode is the primary phase-1 target.
7. Embed mode must not distort strict-mode hot-path design.
8. Convenience is preserved, but hidden lifetime tricks must stay inside thread state, not process-global state.


## 7. Runtime Modes

### 7.1 Strict Mode

Strict mode is the primary phase-1 target.

Rules:

- runtime-bound APIs require the calling thread to already be attached
- `xrtInit()` attaches the current thread as the bootstrap runtime thread
- `xrtThreadCreate()` automatically attaches the created thread inside the wrapper
- no implicit attach on random API entry
- unsupported build/runtime combinations should fail early instead of silently degrading

Strict mode is designed for:

- normal executable builds
- single-header integration where the application owns startup
- `xserver` and other XRT-centered programs
- future high-performance thread-bound allocators

### 7.2 Embed Mode

Embed mode exists for secondary integration shapes such as:

- host-managed threads
- plugin scenarios
- shared library integration

Rules:

- external threads may explicitly call `xrtThreadAttachCurrent()` and `xrtThreadDetachCurrent()`
- platform TLS/FLS fallback is acceptable only at the thread-state boundary
- embed-mode support must never force strict-mode hot paths to perform repeated TLS/FLS lookup

Phase-1 target:

- define embed-mode API shape and invariants
- allow partial implementation later if needed


## 8. State Model

### 8.1 Global Runtime State

The global runtime state stores only process-wide data.

Target shape:

```c
typedef struct xrt_global_state {
    bool bReady;
    uint32 iInitRef;

    const unsigned char* sNull;

    xvalue vNull;
    xvalue vTrue;
    xvalue vFalse;

    char* AppFile;
    char* AppPath;

#if defined(_WIN32) || defined(_WIN64)
    uint64 Frequency;
#endif

    ptr (*malloc_fn)(size_t iSize);
    ptr (*calloc_fn)(size_t iNum, size_t iSize);
    ptr (*realloc_fn)(ptr pMem, size_t iSize);
    void (*free_fn)(ptr pMem);

    void (*OnError)(str sError);

    int iApproxIntMode;
    double fApproxIntTol;
    int iApproxNumMode;
    double fApproxNumTol;
    int iApproxTimeTol;
    int iApproxStrMode;
    double fApproxStrTol;
    bool bApproxStrCase;

    xrt_tls_backend tTlsBackend;
    xrt_tls_slot tThreadSlot;
} xrt_global_state;
```

### 8.2 Thread Runtime State

The thread runtime state stores only thread-bound mutable data.

Target shape:

```c
typedef struct xrt_thread_state {
    xrt_global_state* pGlobal;

    uint64 iThreadId;
    xthread pSelf;
    uint32 iAttachDepth;

    str LastError;
    bool bFreeLastError;

    xrt_scratch_arena tScratch;

    xrand rand32;
    xrand rand64_low;
    xrand rand64_high;

    xrt_thread_cleanup* pCleanupTop;

    xrt_thread_mem_state tMem;
} xrt_thread_state;
```

### 8.3 State Placement Rules

Mandatory placement rules:

- `AppFile`, `AppPath`, static singleton values, allocator hooks, and global config belong to global state
- `LastError`, temporary memory, default RNG, and future thread-bound allocator state belong to thread state
- global state must not contain thread-bound mutable storage
- thread state must not store process-wide configuration copies unless explicitly cached for hot-path reasons


## 9. Null String and Static Singleton Values

### 9.1 `sNull`

The null-string sentinel will no longer rely on an integer object trick.

Target representation:

```c
static const unsigned char XRT_NULL_BYTES[4] = "\0\0\0";
```

Semantics:

- this object is process-global and immutable
- it is valid as a UTF-8/UTF-16/UTF-32 zero start sentinel
- global state stores `sNull = (str)XRT_NULL_BYTES`

### 9.2 Static `xvalue` Singletons

`null`, `true`, and `false` remain process-global singletons.

Rules:

- they are not copied into thread state
- they are initialized once during global runtime initialization
- all thread states reference the same singleton objects


## 10. Thread Lifecycle Model

### 10.1 Thread Categories

Phase-1 recognizes two categories:

- bootstrap thread: attached by `xrtInit()`
- XRT-managed thread: created by `xrtThreadCreate()`

Embed-mode may later add:

- external attached thread: attached manually by host code

### 10.2 `xrtInit()` / `xrtUnit()`

Target semantics:

- `xrtInit()` performs process-global one-time initialization and attaches the current thread
- repeated `xrtInit()` calls increase a global init refcount
- repeated calls on the same thread increase attach depth instead of reallocating thread state
- `xrtUnit()` detaches the current thread
- when global init refcount drops to zero, process-global runtime resources are released

### 10.3 `xrtThreadCreate()`

Target semantics:

- creates a managed thread object
- wrapper attaches thread state before entering the user function
- wrapper records the current `xthread` pointer in thread state
- user code inside the thread may call runtime-bound APIs directly
- wrapper runs cleanup before the native thread exits

### 10.4 `xrtThreadWait()`

Target semantics:

- wait for managed thread completion
- preserve exit code
- never use "state query" behavior that consumes join state

### 10.5 `xrtThreadDestroy()`

Target semantics:

- destroy only the management object and native handle storage
- it must not silently detach or silently abandon still-running threads
- destroying a running joinable thread is an error


## 11. Thread Cleanup Model

### 11.1 Cleanup Requirements

When an attached thread exits, XRT must reclaim thread-bound runtime resources
without requiring user discipline.

### 11.2 Cleanup Stack

Thread state includes a cleanup stack:

```c
typedef void (*xrt_thread_cleanup_fn)(xrt_thread_state* ts, ptr pArg);

typedef struct xrt_thread_cleanup {
    xrt_thread_cleanup_fn pfnCleanup;
    ptr pArg;
    struct xrt_thread_cleanup* pPrev;
} xrt_thread_cleanup;
```

Target API shape:

```c
bool xrtThreadPushCleanup(xrt_thread_cleanup_fn fn, ptr pArg);
bool xrtThreadPopCleanup(xrt_thread_cleanup_fn fn, ptr pArg);
```

Rules:

- cleanup entries execute in LIFO order
- cleanup stack belongs to thread state
- cleanup callbacks must tolerate partially torn-down thread state

### 11.3 Automatic Cleanup for XRT-Managed Threads

The thread wrapper must always perform, in order:

1. run registered cleanup callbacks in reverse order
2. free thread-local error storage if owned
3. release thread scratch temporary memory
4. release future thread-local memory state placeholders
5. clear the current thread-state slot
6. store thread exit code into the management object

This cleanup is mandatory even if the user thread function returns early.

### 11.4 Bootstrap Thread Cleanup

For the bootstrap thread:

- `xrtUnit()` performs thread detach
- detach must release the same thread-local resources as wrapper-driven exit
- process-global shutdown happens only after detach and only when the global init refcount reaches zero


## 12. Temporary Memory Policy

### 12.1 Phase-1 Policy

Public convenience behavior is preserved in phase-1.

This means:

- APIs such as `xvoGetValueText` keep their current user-facing simplicity
- phase-1 does not force a public split into `Dup/Write/Temp` variants
- the implementation moves from process-global ring behavior to thread-local temporary storage

### 12.2 Storage Strategy

The old process-global 32-slot ring must be replaced by a thread-local scratch mechanism.

Requirements:

- storage belongs to thread state
- no cross-thread overwrite
- no process-global temporary-memory index
- implementation detail may begin as a slot-based scratch model if migration cost is lower
- design should remain compatible with later replacement by a bump arena

### 12.3 Compatibility Requirement

The following public behavior must remain true in phase-1:

- callers can continue to use temporary-memory-returning convenience APIs without explicit release in common cases
- code inside XRT-managed threads should feel as simple as single-threaded usage


## 13. Error and RNG Policy

### 13.1 Error State

`LastError` becomes thread-local.

Rules:

- each attached thread owns its own `LastError`
- global `OnError` callback remains process-global
- setting thread-local error state must not free another thread's error buffer

### 13.2 Default RNG

Default RNG state becomes thread-local.

Rules:

- `xrtRand32()`, `xrtRand64()`, and `xrtRandRange()` use thread state
- `Ex` APIs remain valid for caller-managed RNG state
- later thread-local allocator work may reuse thread-state seeding utilities


## 14. Work Breakdown and Progress

### 14.1 Milestones

| Milestone | Status | Exit Criteria |
| --- | --- | --- |
| M1 Spec freeze | done | phase-1 goals, scope, state split, thread model, and cleanup model documented |
| M2 Runtime state split | done | global/thread state structures introduced and old mixed fields removed |
| M3 Thread lifecycle rewrite | done | thread creation, wait, destroy, attach, detach, and cleanup semantics implemented |
| M4 Runtime-bound state migration | done | `LastError`, temp memory, and default RNG use thread state |
| M5 Validation and docs | done | tests updated, compatibility behavior verified, follow-up notes recorded |

### 14.2 Task Checklist

#### M1 Spec Freeze

- [x] Confirm phase-1 only covers runtime/thread foundation
- [x] Confirm `xvoGetValueText` family remains behavior-compatible in phase-1
- [x] Confirm `sNull` changes to explicit 4-byte immutable storage
- [x] Confirm `vNull/vTrue/vFalse` remain process-global singletons
- [x] Define global-state vs thread-state split
- [x] Define strict vs embed mode model
- [x] Define automatic thread cleanup model
- [x] Freeze exact internal naming for global/thread state structs and helpers

#### M2 Runtime State Split

- [x] Add new global runtime struct
- [x] Add new thread runtime struct
- [x] Remove thread-bound fields from the old global runtime object
- [x] Introduce internal helpers for fetching global/thread state
- [x] Migrate initialization code to the new split model

#### M3 Thread Lifecycle Rewrite

- [x] Add attach/detach APIs
- [x] Refactor thread wrapper for both Windows and POSIX
- [x] Remove implicit detach behavior from `xrtThreadDestroy`
- [x] Redefine wait and state-query behavior
- [x] Add cleanup stack support
- [x] Ensure wrapper cleanup runs on all normal thread exits

#### M4 Runtime-Bound State Migration

- [x] Migrate `LastError`
- [x] Migrate temporary memory
- [x] Migrate default RNG state
- [x] Keep convenience API behavior compatible
- [x] Audit remaining uses of process-global mutable runtime-bound state

#### M5 Validation and Docs

- [x] Update thread tests
- [x] Add runtime attach/detach tests
- [x] Validate `xrt`-managed thread convenience behavior
- [x] Update public/internal docs where phase-1 changes behavior
- [x] Record deferred phase-2 work


## 15. Acceptance Criteria

Phase-1 is accepted only if:

1. `xrtInit()` + `xrtUnit()` work correctly on the bootstrap thread.
2. `xrtThreadCreate()` threads can use XRT APIs without extra user boilerplate.
3. thread-local temporary memory and `LastError` do not cross-thread overwrite.
4. default RNG calls are no longer backed by process-global mutable state.
5. thread exit always reclaims attached runtime resources for XRT-managed threads.
6. hot-path design does not require repeated FLS lookup per operation.
7. deferred phase-2 items are explicitly listed and not quietly mixed into phase-1.


## 16. Risks

### 16.1 Primary Risks

- phase-1 may accidentally spill into phase-2 if legacy containers are "partially fixed"
- compatibility expectations around `xrtInit()` and `xrtUnit()` may hide attach-depth edge cases
- strict-mode assumptions may conflict with some existing DLL/shared build expectations
- temporary-memory migration may keep legacy semantics but still leak implementation complexity into internals
- thread cleanup order mistakes may create shutdown crashes or silent leaks

### 16.2 Risk Controls

- keep phase-1 scope narrow
- update this spec before broadening scope
- preserve old convenience API surface where possible
- separate structural cleanup from later allocator/container synchronization


## 17. Decision Log

1. `2026-03-13`: Phase-1 will focus on runtime/thread foundations only; container and mempool thread safety is deferred to phase-2.
2. `2026-03-13`: Runtime state will be split into one process-global state and one thread-local state.
3. `2026-03-13`: Public convenience APIs such as `xvoGetValueText` remain behavior-compatible in phase-1.
4. `2026-03-13`: `sNull` will move to an explicit immutable 4-byte zero buffer instead of an integer storage trick.
5. `2026-03-13`: `vNull`, `vTrue`, and `vFalse` remain process-global singleton values.
6. `2026-03-13`: Automatic cleanup of XRT-managed thread resources is a required part of phase-1.
7. `2026-03-13`: Phase-1 implementation naming is frozen around `xrtGlobalData`, `xrtThreadData`, `xrtThreadGetCurrent`, `xrtThreadAttachCurrent`, `xrtThreadDetachCurrent`, and `xrtGetError`.


## 18. Work Log

- `2026-03-13`: Created the phase-1 runtime/thread refactor spec. Captured goals, scope, state model, strict/embed execution model, cleanup design, and milestone checklist.
- `2026-03-13`: Implemented the source-tree phase-1 skeleton: split runtime/thread state, moved `LastError` / temp memory / default RNG into thread state, rewrote managed-thread wrapper lifecycle, and added a minimal runtime verification harness.
- `2026-03-13`: Updated thread tests for phase-1 semantics, including runtime auto-attach, nested attach/detach depth, thread-local error/temp state isolation, cleanup callback execution, exit-code retention, and destroy-while-running protection.
- `2026-03-13`: Updated public docs for phase-1 runtime semantics in both Chinese and English, regenerated `singlehead/xrt.h`, and verified the single-header build with a minimal managed-thread runtime smoke test.
- `2026-03-13`: Opened `dev/XRT_RUNTIME_PHASE2_SPEC.md` as the follow-up source of truth for memory-context, allocator, container, and `xvalue` sharing work deferred from phase-1.
