# XRT Runtime Phase-2 Refactor Specification

## 1. Document Control

- Document: `dev/XRT_RUNTIME_PHASE2_SPEC.md`
- Status: Active draft
- Version: `0.1.2`
- Last updated: `2026-03-13`
- Scope: Phase-2 memory-system and legacy-container refactor for XRT
- Source of truth: This file is the primary development spec for phase-2 runtime work

### 1.1 Purpose

This document exists to prevent design drift during the memory-system refactor
that follows phase-1.

It records the ownership model, thread-bound memory-context model, allocator
mode split, container synchronization boundaries, compatibility policy,
milestones, and work tracking model.

If future work happens in a new chat or after long pauses, development must
resume from this file first.

### 1.2 Update Rules

The following rules are mandatory:

1. Every material phase-2 design change must update this file before or with code.
2. Every completed task must update the progress checklist in Section 14.
3. Every decision that changes scope, compatibility, or performance tradeoffs must add an item to Section 17.
4. Every work session should append a short entry to Section 18.
5. Phase-2 code is not considered stable until the acceptance criteria in Section 15 are met.

### 1.3 Status Legend

- `todo`: Not started
- `in_progress`: Active work
- `blocked`: Waiting on a decision, dependency, or bug fix
- `done`: Implemented and accepted against milestone criteria
- `deferred`: Explicitly pushed out of phase-2


## 2. Refactor Summary

Phase-1 established the runtime execution model:

- one process-global runtime state
- one thread-local runtime state
- explicit thread attachment
- automatic cleanup for XRT-managed threads
- thread-local `LastError`, temporary memory, and default RNG

Phase-2 now needs to build the memory system and legacy containers on top of
that model.

The current memory/container layer no longer matches the target runtime:

- `xbsmm`, `xmemunit`, `xfsmempool`, and `xmempool` mutate shared counters and linked lists without any synchronization
- `xavltree` embeds `xfsmempool` and a mutable `NodeCache`, so dict/list inherit the same unsynchronized allocator state
- `xarray` / `xparray` / `xdynstack` are raw resizable buffers with no owner-thread metadata
- `xvalue` refcounting is plain non-atomic mutation and cannot support safe cross-thread sharing
- current APIs do not distinguish "thread-private", "shared", and "misused across threads"

Phase-2 is not a garbage collector rewrite and is not a license to put locks on
everything by default.

Phase-2 exists to establish:

- thread-bound fast-path allocation
- explicit ownership rules for legacy objects
- explicit shared-mode behavior where cross-thread mutation is actually needed


## 3. Goals

### 3.1 Product Goals

Phase-2 must keep XRT easy to use in the common case.

The desired user experience is:

- same-thread code still feels script-like and lightweight
- XRT-managed worker threads can allocate and use containers directly
- default usage stays simple for owner-thread objects
- cross-thread sharing is supported through explicit, understandable object modes
- wrong-thread misuse becomes diagnosable instead of silently corrupting memory

### 3.2 Technical Goals

- Add a thread-bound memory context to `xrtThreadData`
- Route future small-allocation hot paths through thread memory state without repeated TLS/FLS lookup
- Refactor low-level allocators to support explicit object modes
- Refactor core legacy containers to record owner/shared semantics at the root object level
- Define safe `xvalue` sharing rules, including refcount semantics
- Preserve phase-1 runtime invariants while preparing for later higher-level thread-safe subsystems

### 3.3 Phase-2 Success Criteria

Phase-2 is successful only if all of the following are true:

- default owner-thread objects do not pay shared-mode synchronization cost
- thread-bound allocation can be reached from the current thread state without redesigning phase-1 runtime structures
- low-level allocator mode and container mode use one consistent ownership vocabulary
- cross-thread misuse is detectable
- explicitly shared objects have clear synchronization semantics
- `xvalue` no longer relies on unsafe cross-thread refcount mutation


## 4. Non-Goals

The following are explicitly out of scope for phase-2:

- adding a tracing GC or generational GC
- redesigning public convenience APIs such as `xvoGetValueText`
- making every legacy object transparently and silently shared-safe
- lock-free container implementations
- cross-process shared-memory allocators
- rewriting all higher-level network/application modules in the same phase


## 5. Scope Boundaries

### 5.1 Included in Phase-2

- `xrtThreadData` memory-context expansion
- internal allocation helpers for thread-bound fast paths
- ownership-mode metadata for low-level allocators
- ownership-mode metadata for core legacy containers
- shared-mode synchronization for low-level allocators where required
- shared-mode synchronization for core containers where required
- `xvalue` shared/refcount design and implementation
- tests, migration notes, and public/internal docs for affected semantics

### 5.2 Deferred to Later Work

- full immutable/frozen object graph support
- allocator telemetry, leak reports, and memory profiling UX
- lock-free queues/maps/containers
- automatic retrofitting of every advanced module in one pass
- deep networking ownership rewrite beyond objects directly touched by phase-2


## 6. Current State Findings

The following current-state findings define the phase-2 starting point:

1. `xbsmm` is page-backed raw struct storage plus a free-list of released slots; there is no owner metadata and no synchronization.
2. `xmemunit` mutates `Count`, `FreeCount`, `FreeOffset`, and `FreeList` directly; it assumes single-thread access.
3. `xfsmempool` and `xmempool` both maintain multiple mutable linked-list classes (`LL_Idle`, `LL_Full`, `LL_Null`, `LL_Free`) with no concurrency boundary.
4. `xavltree` embeds `xfsmempool` and a mutable `NodeCache`, so dict/list inherit allocator races even if tree logic were externally locked.
5. `xarray` and `xparray` perform raw `realloc`, `memmove`, and direct `Count` changes with no ownership checks.
6. `xvalue` uses a packed 26-bit `RefCount` field with plain increment/decrement operations.
7. Current APIs mostly expose root objects only; this makes root-level ownership metadata feasible without redesigning every node payload type first.


## 7. Core Design Principles

1. Default objects are owner-thread objects.
2. Shared safety is explicit, not implicit.
3. Same-thread hot paths must avoid lock acquisition and repeated TLS/FLS lookup.
4. Ownership must be represented at the root object level.
5. Low-level allocator mode and high-level container mode must agree.
6. Wrong-thread mutable access should fail fast in debug builds and fail clearly in normal builds.
7. Compatibility should preserve simple old call shapes where possible.
8. Phase-2 must not weaken phase-1 attach/detach or cleanup rules.


## 8. Ownership Model

### 8.1 Object Modes

Phase-2 will standardize mutable runtime objects around these modes:

- `XRT_OBJMODE_LOCAL`
  - default mode
  - object belongs to exactly one attached thread
  - no internal synchronization
  - hot-path optimized

- `XRT_OBJMODE_SHARED`
  - object may be mutated from multiple attached threads
  - internal synchronization is enabled at the root object and/or allocator boundary
  - higher overhead is accepted because sharing is explicit

### 8.2 Root-Object Ownership Metadata

Mutable root objects added or refactored in phase-2 must carry enough metadata
to answer:

- which thread owns the object in local mode
- whether the object is local or shared
- which memory context backs the object
- whether wrong-thread mutation should be rejected

Exact field layout may vary by type, but the conceptual contract is:

```c
typedef struct xrt_object_owner {
	xrtThreadData* pOwnerThread;
	uint32 iMode;
	void* pMemContext;
} xrt_object_owner;
```

This metadata lives on root objects such as:

- `xarray`
- `xparray`
- `xbsmm`
- `xfsmempool`
- `xmempool`
- `xavltree`
- `xdict`
- `xlist`
- later `xbuffer`, `xdynstack`, and other mutable runtime structures

### 8.3 Wrong-Thread Access Policy

For local-mode objects:

- owner-thread mutation is allowed
- wrong-thread mutation is invalid
- debug builds should assert
- normal builds should set current-thread `LastError` and fail safely if possible

Read-only access to local-mode objects from other threads is not guaranteed
safe unless explicitly documented later. Phase-2 should not silently imply
cross-thread read safety for mutable legacy structures.


## 9. Memory Context Model

### 9.1 Thread Memory State

`xrtThreadData` will grow a phase-2 memory substate:

```c
typedef struct xrt_thread_mem_state {
	uint64 iOwnerThreadId;
	uint32 iFlags;
	void* pSmallAlloc;
	void* pSmallFree;
	void* pSharedFallback;
} xrt_thread_mem_state;
```

The exact allocator internals can change during implementation, but the phase-2
contract is fixed:

- each attached thread has a default local memory context
- hot paths obtain that context once from thread state
- internal allocation paths pass context pointers directly
- large or unsupported allocations may fall back to the configured global allocator

### 9.2 Allocation Domains

Phase-2 will distinguish at least these domains:

- local-thread domain
  - for default owner-thread objects
  - optimized for the current attached thread

- shared-object domain
  - for explicitly shared allocators or containers
  - synchronization is allowed inside this domain

- global fallback domain
  - for large allocations or types not yet migrated
  - still uses the configured process-global allocator hooks

### 9.3 Hot-Path Rule

The design target is:

- public API boundary gets current `xrtThreadData`
- internal implementation resolves the active memory context once
- nested allocator/container calls pass the context directly

Phase-2 must not regress into repeated TLS/FLS lookups inside every allocator
or container operation.


## 10. Low-Level Allocator Strategy

### 10.1 Allocators in Scope

Phase-2 low-level allocator work covers:

- `xbsmm`
- `xmemunit`
- `xfsmempool`
- `xmempool`

### 10.2 Required Mode Behavior

For each allocator above:

- local mode
  - records owner thread
  - keeps current single-thread fast path
  - does not acquire shared synchronization primitives

- shared mode
  - provides internal synchronization around allocator state mutation
  - guarantees correctness for concurrent allocate/free operations within documented limits

### 10.3 Layering Rule

Allocator layering must remain coherent:

- if a root allocator is shared, its subordinate allocator state must also behave as shared
- if a root allocator is local, subordinate state must remain local-fast-path

No mixed "shared container over local allocator internals" design is allowed.


## 11. Legacy Container Strategy

### 11.1 Containers in Scope

Phase-2 container work covers:

- `xparray`
- `xarray`
- `xavltree`
- `xdict`
- `xlist`
- `xdynstack`
- `xbuffer` if touched by allocator refactor dependencies

### 11.2 Container Rules

- old constructors remain the simple default and create local-mode objects
- shared use must be explicit through new constructors or flags
- root objects own their synchronization policy
- local-mode containers reject wrong-thread mutation
- shared-mode containers synchronize internal mutation

### 11.3 Iterator Policy

Shared-mode iteration is a risk area.

Phase-2 baseline policy:

- local-mode iterators remain owner-thread only
- shared-mode iteration does not imply mutation-safe lock-free traversal
- generic `xavltree` root iterators now hold the root shared gate for the lifetime of one iterator session
- dict/list and other pointer-view style root APIs may still require explicit caller-side locking where the public surface exposes raw internal pointers across multiple calls

Phase-2 must document this clearly instead of implying stronger guarantees than the implementation can provide.


## 12. `xvalue` Strategy

### 12.1 Current Problem

`xvalue` currently combines:

- mutable reference count
- nested containers
- process-wide singleton values
- thread-local convenience behavior layered underneath some getters

The immediate unsafe part for phase-2 is refcount mutation and nested mutable
container sharing.

### 12.2 Phase-2 Direction

Phase-2 will apply these rules:

- static singleton values remain process-global
- default heap `xvalue` objects remain local-mode unless created or promoted explicitly for sharing
- shared `xvalue` objects require safe refcount semantics
- container-backed `xvalue` objects must align their mode with the underlying container mode
- real shared `xvalue` root support now covers array/list/table/coll-backed values when their underlying roots are real shared

### 12.3 Refcount Rule

Phase-2 should move away from the current packed non-atomic bitfield approach.

Target rule:

- local-mode `xvalue` keeps a cheap fast path
- shared `xvalue` uses atomic refcount mutation
- the implementation may use one common atomic field or split local/shared paths, but shared values must no longer tear under concurrent add-ref/unref


## 13. API Direction and Compatibility Policy

### 13.1 Compatibility Baseline

Existing simple constructors should remain valid and simple:

- `xrtArrayCreate(...)`
- `xrtDictCreate(...)`
- `xrtListCreate(...)`
- `xvoCreateArray()`
- `xvoCreateTable()`

In phase-2 they become the default local-mode constructors.

### 13.2 New Explicit Shared Entry Points

Phase-2 adds explicit mode-aware entry points instead of silently changing the
semantics of old constructors.

Current mode-aware entry points:

```c
XXAPI xarray xrtArrayCreate(uint32 iItemLength, uint32 iMode);
XXAPI xdict xrtDictCreate(uint32 iItemLength, uint32 iMode);
XXAPI xlist xrtListCreate(uint32 iItemLength, uint32 iMode);
XXAPI xavltree xrtAVLTreeCreate(uint32 iItemLength, AVLTree_CompProc procComp, uint32 iMode);
XXAPI xvalue xvoCreateArrayEx(uint32 iMode);
XXAPI xvalue xvoCreateListEx(uint32 iMode);
XXAPI xvalue xvoCreateCollEx(uint32 iMode);
XXAPI xvalue xvoCreateTableEx(uint32 iMode);
```

Additional low-level `mode-aware constructor/init` helpers also exist for allocator roots so
mode can propagate through embedded sub-objects.

Current behavior:

- old APIs stay simple and local by default
- shared behavior must be requested explicitly
- low-level allocator roots (`xbsmm`, `xmemunit`, `xfsmempool`, `xmempool`) now use a real shared gate in `XRT_OBJMODE_SHARED`
- standalone shared container roots now use a real shared gate for their supported public root operations, including generic `xavltree`
- shared root containers now expose explicit `Lock/Unlock` entry points so pointer-view, walk, and iterator usage can be made stable under caller-controlled synchronization
- generic `xavltree` root wrappers now self-synchronize `Insert` / `Remove` / `Search` / `Walk` and one iterator session; the raw `xrtAVLTB_*` base layer remains internal/no-lock
- shared `xvalue` roots now use atomic top-level refcounting across array/list/table/coll-backed values when the underlying root is real shared
- direct write and merge paths on real shared `xvalue` array/list/table/coll roots now auto-promote primitive or already-share-compatible child values into shared refcount mode
- direct write and merge paths on real shared `xvalue` array/list/table/coll roots reject local nested container values that do not already have a real shared root
- staged shared objects may be constructed and mutated on the owner thread
- non-owner mutation of staged shared tree/value objects is rejected with a dedicated `"shared mode synchronization is not implemented yet."` runtime error
- pointer-view and callback-style traversal APIs on shared roots now require explicit `Lock/Unlock` pairing; using them without an external/shared root lock remains outside the validated contract

### 13.3 Diagnostics

Phase-2 should also add internal helpers/macros so ownership checks are easy to
apply consistently.

Examples of expected internal concepts:

- require current attached thread
- require object owner thread in local mode
- reject staged shared container/value cross-thread mutation until synchronization exists
- acquire/release object shared gate
- expose explicit root locking where the public API returns internal pointers or drives iterators/walk callbacks


## 14. Milestones

| Milestone | Status | Exit Criteria |
|-----------|--------|---------------|
| M1 Spec freeze | completed | phase-2 goals, scope, ownership model, and compatibility policy documented |
| M2 Thread memory context | in_progress | `xrtThreadData` memory substate and internal allocation routing introduced |
| M3 Low-level allocator refactor | completed | `xbsmm` / `xmemunit` / `xfsmempool` / `xmempool` support local/shared semantics |
| M4 Core container refactor | completed | `xarray` / `xparray` / `xavltree` / `xdict` / `xlist` ownership and shared behavior implemented |
| M5 `xvalue` sharing semantics | completed | safe refcount strategy and container-mode alignment implemented |
| M6 Validation and docs | completed | tests, migration notes, and public/internal docs updated |

### 14.1 M1 Spec Freeze

- [x] Capture current-state findings
- [x] Define phase-2 goals and non-goals
- [x] Define object ownership model
- [x] Define memory-context direction
- [x] Define compatibility baseline

### 14.2 M2 Thread Memory Context

- [x] Extend `xrtThreadData` memory substate
- [x] Add internal memory-context helpers
- [ ] Route migrated fast paths through thread memory context
- [ ] Preserve global allocator fallback behavior

### 14.3 M3 Low-Level Allocator Refactor

- [x] Add ownership/mode metadata to low-level allocator roots
- [x] Implement local-mode fast path
- [x] Implement shared-mode synchronization
- [x] Add wrong-thread diagnostics for local mode

### 14.4 M4 Core Container Refactor

- [x] Add ownership/mode metadata to container roots
- [x] Add explicit shared lock APIs for stable pointer-view / walk / iterator access on supported roots
- [x] Complete standalone `xavltree` shared semantics
- [x] Refactor dict/list root operations on top of updated tree semantics
- [x] Promote supported `xarray` / `xparray` root operations into real shared mode
- [x] Document iterator limits in shared mode

### 14.5 M5 `xvalue` Sharing Semantics

- [x] Replace unsafe shared refcount behavior
- [x] Align container-backed values with container mode
- [x] Preserve process-global static singleton behavior
- [x] Add cross-thread `xvalue` tests

### 14.6 M6 Validation and Docs

- [x] Add allocator concurrency tests
- [x] Add container ownership/shared-mode tests
- [x] Add `xvalue` cross-thread tests
- [x] Update public/internal docs where phase-2 changes behavior
- [x] Record deferred later-phase work


## 15. Acceptance Criteria

Phase-2 is accepted only if:

1. attached threads expose a usable thread memory context for migrated fast paths
2. default local-mode allocations and container mutations do not require shared-mode locking
3. wrong-thread mutation of local-mode objects is detected
4. explicitly shared allocator/container smoke tests pass under concurrent use
5. shared `xvalue` refcounting is no longer vulnerable to plain data races
6. old constructor APIs remain simple for the common owner-thread case
7. phase-2 does not break phase-1 runtime attach/detach and cleanup invariants


## 16. Risks

### 16.1 Primary Risks

- accidental over-engineering of the allocator layer before the ownership model is stable
- silent performance regression if shared-mode machinery leaks into local-mode fast paths
- mixed local/shared sub-allocator state causing hard-to-debug corruption
- shared iterator semantics becoming underspecified or incorrect
- `xvalue` container nesting causing mode-propagation bugs
- compatibility drift if old constructors are quietly changed instead of explicitly versioned by mode

### 16.2 Risk Controls

- keep owner-thread mode as the default baseline
- introduce explicit shared mode instead of implicit global locking
- migrate low-level allocators before claiming shared safety for containers built on top of them
- document unsupported/shared-limited cases instead of overpromising
- keep this spec updated before expanding scope

### 16.3 Deferred Later-Phase Work

- route the thread memory context from `xrtThreadData.tMem` into more allocator hot paths instead of leaving it as a prepared slot only
- decide whether later phases will replace the current thread-local temp ring with a scratch arena without breaking script-style convenience APIs
- keep raw `xrtAVLTB_*` helpers internal/no-lock until there is a separate, explicitly documented low-level shared contract
- continue evaluating whether shared pointer-view APIs should stay lock-based or eventually gain a dedicated view/session abstraction


## 17. Decision Log

1. `2026-03-13`: Phase-2 will use owner-thread local objects as the default object model.
2. `2026-03-13`: Shared safety will be explicit and opt-in, not silently enabled on all legacy objects.
3. `2026-03-13`: Old constructor APIs remain simple and default to local mode.
4. `2026-03-13`: Low-level allocator mode and container mode must use one consistent ownership vocabulary.
5. `2026-03-13`: Shared `xvalue` refcounting must no longer rely on plain non-atomic mutation.
6. `2026-03-13`: Legacy root objects that are cast to base views must preserve prefix layout when owner metadata is added.
7. `2026-03-13`: Shared mode constructors using `..., XRT_OBJMODE_SHARED` are staged entry points; until shared synchronization exists they remain owner-thread mutable and reject non-owner mutation explicitly.
8. `2026-03-13`: Low-level allocators may complete real shared synchronization before containers and `xvalue`; staged shared semantics now apply only to higher-level container/value objects.
9. `2026-03-13`: Standalone `xparray`, `xarray`, `xdict`, and `xlist` may be promoted to real shared before generic `xavltree`; generic tree APIs stay staged until direct-pointer and traversal semantics are explicitly constrained or redesigned.
10. `2026-03-13`: Shared `xvalue` roots may promote to real shared once both the top-level value header and the underlying container root support it.
11. `2026-03-13`: Pointer-view and callback-style traversal APIs on real shared container roots are not yet a stable cross-thread contract; they currently require external synchronization and remain outside the validated shared surface.
12. `2026-03-13`: Supported shared root containers will expose explicit `Lock/Unlock` APIs so existing pointer-returning and iterator/walk entry points can remain usable without inventing a second view-only API family in phase-2.
13. `2026-03-13`: `xvalue` cannot get a reliable shared atomic refcount while its first word stays as direct `Type/Reserve/IsStatic/RefCount` bitfields without an explicit raw-header update path; M5 must first solve that header representation boundary.
14. `2026-03-13`: Shared `xvalue` roots now split by underlying container capability; once the underlying root is real shared, array/list/table/coll-backed values may enter real shared mode.
15. `2026-03-13`: Direct writes and merge paths into a real shared `xvalue` container now auto-promote primitive/share-compatible child values to shared refcount mode and explicitly reject local nested container values without a real shared root.
16. `2026-03-13`: Generic `xavltree` root wrappers are now allowed to enter real shared mode; root-level walk and one iterator session self-synchronize, while raw `xrtAVLTB_*` base helpers remain internal no-lock primitives.
17. `2026-03-13`: Standalone shared `coll` roots now route `remove` / `clear` / `itemcount` / `set parent` through the promoted tree contract, so `coll`-backed `xvalue` objects may participate in real shared store/refcount semantics like the other promoted container-backed values.


## 18. Work Log

- `2026-03-13`: Created the phase-2 memory/container refactor spec. Audited current allocator/container structure across `xbsmm`, `xmemunit`, `xfsmempool`, `xmempool`, `xavltree`, `xdict`, `xlist`, `xarray`, and `xvalue`, then captured the ownership model, memory-context direction, compatibility baseline, and milestone checklist.
- `2026-03-13`: Started M2 skeleton work by extending `xrtThreadData` with `xrtThreadMemState` and wiring basic init/unit lifecycle hooks, without changing allocator behavior yet.
- `2026-03-13`: Added phase-2 internal helpers for thread memory state and owner-thread checks, then applied initial local-mode ownership diagnostics to `xbsmm`, `xmemunit`, and `xfsmempool`. Verified same-thread allocation and wrong-thread rejection with a minimal runtime harness.
- `2026-03-13`: Extended ownership metadata and local-mode wrong-thread diagnostics into `xmempool`, `xavltree`, `xdict`, and `xlist`. While validating, found that `xavltree` and embedded dict/list roots still depend on legacy prefix-cast layout (`xavltbase` and `FreeProc` paths), so owner metadata was moved out of the leading prefix to preserve old object layout. Recompiled `xrt.c` and `test.c`, then verified same-thread success plus wrong-thread rejection for mempool/tree/dict/list with a dedicated runtime harness.
- `2026-03-13`: Added owner-thread metadata and wrong-thread mutation diagnostics to `xparray` and `xarray`, including the inline pointer-array setter path. Recompiled `xrt.c` and `test.c`, then verified same-thread mutation plus wrong-thread rejection for pointer-array and struct-array operations with a dedicated runtime harness.
- `2026-03-13`: Promoted the owner-thread regression checks into the formal test tree by adding `test/test_runtime_phase2.h` and wiring it into `test.c`. The new coverage locks in same-thread success plus wrong-thread rejection for low-level allocators, mempool/tree/dict/list, and array/parray write paths. Verified by recompiling `xrt.c` and `test.c`, then running a dedicated phase-2 runtime test harness with exit code `0`.
- `2026-03-13`: Implemented the first staged shared-mode entry layer by adding mode-aware `mode-aware constructor/init` constructors across allocator/container roots plus `xvoCreate*Ex` container constructors. Shared mode now propagates into embedded sub-objects at construction time, but remains explicitly staged: owner-thread mutation works, while non-owner mutation fails with `"shared mode synchronization is not implemented yet."` until real shared synchronization is implemented. Recompiled `xrt.c` and `test.c`, then verified the staged shared entry semantics with a dedicated runtime harness (`exit=0`).
- `2026-03-13`: Completed real shared-mode synchronization for low-level allocator roots by adding the object shared gate to `xbsmm`, `xmemunit`, `xfsmempool`, and `xmempool`, then fixed three allocator correctness issues exposed by concurrent shared-mode testing: phase-2 tests were updated to treat `xrtGetError()==xCore.sNull` as the no-error state, `xrtMemUnitAlloc_Inline()` now writes `MMU_FLAG_USE` and enforces the 256-item bound, and the `xmempool` big-allocation path now stores a stable `BigMM` slot index and avoids use-after-free/null-dereference in free/GC flows. Recompiled `xrt.c` and `test.c`, then reran the dedicated phase-2 runtime harness with exit code `0`.
- `2026-03-13`: Promoted standalone shared container roots by wiring real shared gates into `xparray`, `xarray`, `xdict`, and `xlist`, while intentionally leaving generic `xavltree` staged. Added a dedicated shared container concurrency smoke test plus a staged shared-tree boundary test to `test/test_runtime_phase2.h`. Recompiled `xrt.c` and `test.c`, then reran a dedicated phase-2 runtime harness covering owner protection, shared entry behavior, staged shared tree behavior, shared container roots, and shared allocator roots with exit code `0`.
- `2026-03-13`: Made the shared root gate reentrant for the owning thread and added explicit `Lock/Unlock` APIs for `xparray`, `xarray`, `xavltree`, `xdict`, and `xlist`. Added formal tests that hold a shared root lock while calling existing `Get` / `Walk` / iterator entry points and while a second thread blocks on the same root, then reran the dedicated phase-2 runtime harness with exit code `0`.
- `2026-03-13`: Started M5 audit of `xvalue` sharing semantics. Current conclusion: the existing `Type/Reserve/IsStatic/RefCount` bitfield header is the immediate blocker for a reliable shared atomic refcount path, because all refcount operations currently mutate those bits directly with plain writes. M5 is now marked `in_progress`, but header representation must be solved before shared container-backed `xvalue` promotion can be implemented cleanly.
- `2026-03-13`: Completed the first real shared `xvalue` pass by introducing a raw header/CAS refcount path for shared values, marking shared array/list/table roots at construction time, and auto-promoting share-compatible child values during direct writes and merge paths into real shared value containers. Local nested container values are now rejected on those direct shared store paths unless they already own a real shared root. Recompiled `xrt.c` and `test.c`, then reran a dedicated phase-2 runtime harness covering shared entry points, shared container roots, shared allocator roots, and the new shared `xvalue` semantics with exit code `0`.
- `2026-03-13`: Promoted generic `xavltree` root wrappers from staged to real shared mode by activating the root shared gate, replacing the old macro-only `Walk/Iter` surface with owner-aware wrapper functions, and teaching one iterator session to hold/release the root shared gate automatically. Updated the phase-2 tree test from staged rejection to real shared success, then recompiled `xrt.c` and `test.c` and reran a dedicated phase-2 runtime harness covering shared tree, explicit lock API, shared entry points, shared container roots, shared allocator roots, and shared `xvalue` semantics with exit code `0`.
- `2026-03-13`: Promoted `coll`-backed `xvalue` objects into the real shared set by routing standalone shared coll root operations through the promoted tree contract, allowing `xvoMakeShared_Inline()` to accept real shared coll roots, and extending the phase-2 `xvalue` test to cover shared coll store/refcount plus local-coll rejection. Recompiled `xrt.c` and `test.c`, then reran the dedicated phase-2 runtime harness with exit code `0`.
- `2026-03-13`: Completed the phase-2 documentation pass for the current shared/runtime contract by updating the main README, docs index, migration guides, and value API docs in both Chinese and English. The public docs now describe explicit shared roots (`xvoCreate*Ex(..., XRT_OBJMODE_SHARED)`), atomic top-level refcount behavior for shared container-backed values, and the rule that nested containers written into shared values must already own a real shared root.
- `2026-03-13`: Synced the single-header distribution with the phase-2 runtime/shared contract. Reworked `singlehead/test_singlehead.c` into a focused runtime smoke test for bootstrap-thread attach, auto-attached worker threads, thread-local temp/error state, and shared `table/coll` store rules; narrowed Windows socket startup/shutdown to builds that still enable network-related modules; added the missing TinyCC Windows `SRWLOCK_INIT` compatibility define and switched the TinyCC Windows runtime TLS storage path to `__declspec(thread)`; regenerated `singlehead/xrt.h`; then verified the single-header smoke test by compiling and running it successfully with both GCC and TinyCC.
