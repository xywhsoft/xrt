---
num: 115
slug: xruntime-object
title: xruntime (Part 2): Objects and Object Graphs
volume: 卷十一 其他扩展库
type: practice
lead: Reference-counted heap objects and weak references, a trimmable cycle collector (two-pass marking, O(N+E)), safepoint discipline and failure atomicity — the complete lifecycle of host objects.
api: xruntime-runtime_object, xruntime-runtime_object_graph
---

## Orientation

The type system (Chapter 114) describes the facts; the object layer makes the facts **live**: `runtime_object` provides reference-counted heap objects (`xrtobject`, an opaque control block + one allocation carrying header/alignment/payload) and **weak references** (`xrtweak` — a promotable observer reference); `runtime_object_graph` adds **trimmable strong-reference cycle collection** on top — reference counting cannot handle circular references (A holds B, B holds A; the count never reaches zero), so the object graph collects unreachable cycles with two-pass marking (O(N+E)) at **safepoints chosen by the caller**. The design stance: "keep reference counting's advantages in deterministic destruction, C-extension ownership, and weak references, and only handle the cycles that plain reference counting cannot reclaim, at caller-chosen safepoints" — not a generational GC, a **precise patch**. The `Trace` contract (type-metadata-driven strong-reference enumeration) unifies tracing and destruction on the same facts (Chapter 114's InstanceOps honored).

## Introduction

Both ends of a host language's value lifetime are outside C's control: when a script variable disappears is decided by the interpreter, and cycles between objects are permitted by language semantics (`a.next = b; b.prev = a`). Reference counting handles the former (variable gone → Unref — deterministic destruction) but not the latter (counts inside a cycle stay positive). Three historical answers: a full generational GC (stop-the-world, deferred destruction — an ownership nightmare for C extensions); a hand-written `weak_table` (every language runtime rewrites one); **precise cycle collection** (only at safepoints, only for cycles, everything else left to reference counting) — xruntime takes the third path: the graph only borrows objects without holding resident references, and with the feature off the object layout has **zero graph fields** (trimming made real — pay nothing if you use no graph).

Safepoint discipline is the core: collection must be scheduled by the upper runtime when the **object graph is at rest** (tasks/Futures/generators in a stable state) — XRT does not pretend arbitrary workloads can be safely read concurrently under one global lock; `Trace`/root enumeration/`Drop` must not start new collections. This is Chapter 60's structured concurrency in object-graph form: **make a consistent observation inside a paused world**.

## Concepts

### Object layout and strong references

```diagram flow
- Create: xrtObjectCreate(&Type) - one allocation (header + alignment padding + payload)
  - the payload address satisfies InstanceAlign (including 32/64B alignment above the heap default)
  - the allocation is zeroed first, then TypeInitInstance; a failed Init reclaims immediately without calling Drop
- Strong references: returned by Create/Lock; Ref increments (a valid strong reference must already be held for the call - no reviving the dangling)
  - Unref decrements (the last reference Drops on the owning thread, and is no longer promotable afterwards)
- Payload access: ObjectData/ConstData/Size borrow - valid only while a strong reference is held
```

`CreateSized` supports real payloads at least InstanceSize in size (a variable trailing portion is interpreted/destroyed by the type itself). **Init/Drop responsibility**: a failed Init frees its partial resources itself and sets the error (the system does not call Drop — Drop runs only for fully initialized objects).

### Object values and weak references

**Object values** (strong-reference slots stored as ordinary values): declare COPYABLE+RELOCATABLE and `Ops = xrtObjectValueOps()` — the standard object-value operations support null initialization/atomic-on-failure copy/move/release/address-compare hash/strong-reference tracing — **the `xrtobject*` slots in fields, parameters, and container elements** are all handled uniformly by it (the basis for Chapter 117's typed containers storing objects). **Weak references** (`xrtweak`): small values living on the stack/in structures/in containers — zeroed before first use; Init (from an optional live object)/Copy (replaces the target, keeps the original on failure, self-copy is a no-op)/Expired (has the target ended)/Lock (**the only promotion** — success returns a new strong reference, an ended target returns null — the "check then use" race is annihilated by Lock's atomicity).

### The object graph: the trace contract

Collectible types enumerate the strong references their payload directly owns via `InstanceOps.Trace`:

```c
static bool nodeTrace(const void* Value, const xrttype* Type,
		xrtobjectvisitor Visit, ptr Context) {
	const node* Node = (const node*)Value;
	return (Node->Next == NULL) || Visit(Node->Next, Context);
}
```

Three iron rules: **each actual strong-reference slot is visited exactly once** (two fields holding the same object are visited twice — counting semantics); **weak references/borrowed pointers/null slots are never visited**; **Drop must release every strong reference Trace reported** — violating this means the collection rejects or leaks. Language fields, container elements, and closure captures should all **reuse the same type metadata** to generate tracing and destruction (a single source of facts — Chapter 114's philosophy honored by the GC).

### Collection: two-pass marking and roots

```diagram flow
- Snapshot: take the set of objects in the graph (TrackedCount)
- First pass: Trace everyone - count in-graph in-edges (EdgeCount)
- Root identification: strong references > in-graph in-edges = external root; the Roots callback adds borrowed roots (language stacks/generators/host state)
- Second pass: propagate reachability from the roots - O(N+E), temporary space O(N)
- Finalization: all unreachable candidates acquire finalization rights together -> Drop one by one -> unhook - weak references are no longer promotable from finalization on
```

**Special handling of the Value shell**: xvalue references and container COW backing do not add per-item internal strong references — automatic root inference is not enough; `runtime_value_roots` provides `CollectValueRoot`/`CollectValueRoots` (single value / batch stack-slot, global-slot, and suspended frames) — reusing `xrtValueTraceRuntimeObjects`, with failures wrapped in a cause chain. **Dynamic field nodes** (`xrtdynamicfields`): standalone dictionary nodes in the object graph — the host traces only the field objects, and the fields' typed dict payloads trace value references via `xrtTypeValue()` — layering that keeps the collector from recursively guessing into xvalue dictionaries.

### Failure atomicity and error domains

**All up-front checks complete before any destruction** (allocation/tracing/root enumeration/in-edge consistency/snapshot count checks) — on any failure, graph members/strong-weak references/payloads/outputs stay unchanged; **only when all candidates acquire finalization rights together are they unhooked** — a "collected half-way" state does not exist. The error domain `xrt.object-graph` has five codes (ARGUMENT/TRACK/TRACE/STATE/ROOTS); thread-error isolation and recovery have precise rules (successful recovery restores the caller's original error; failure keeps only the current error).

## Examples

### First complete program: collecting a self-cycle

The program below is from `examples/runtime/object_graph` — building and reclaiming a cyclic reference:

```embed path="extlibs/xruntime/examples/runtime/object_graph/main.c" title="extlibs/xruntime/examples/runtime/object_graph/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/object_graph/main.c -lws2_32 -liphlpapi
（输出 tracked/edges/collected 的收集结果自检行）
```

**What just happened.** (1) `nodeDrop` releases the `Next` reference, `nodeTrace` enumerates `Next` — **the symmetry of Drop and Trace** (iron rule three: Drop releases everything Trace reports). (2) Build the self-cycle: `Node->Next = xrtObjectRef(Node)` then Unref the external reference — at this point only the cycle's self-hold remains (reference count permanently positive, no external references). (3) `xrtObjectGraphCollect` in one call: snapshot → two-pass marking → identify "no external roots" → finalize — `CollectedCount` reports the reclaim count. **This is what reference counting cannot do and the object graph finishes in one sentence**; while everything outside collection (creation/access/ordinary destruction) remains the deterministic world of pure reference counting.

### Second complete program: object and weak-reference lifecycle

The second program is from `examples/runtime/object` — reference counting's everyday face:

```embed path="extlibs/xruntime/examples/runtime/object/main.c" title="extlibs/xruntime/examples/runtime/object/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xruntime/single -include xruntime.h impl.c extlibs/xruntime/examples/runtime/object/main.c -lws2_32 -liphlpapi
（输出 counter 值与弱引用 expired 状态的自检行）
```

**What just happened.** (1) `xrtObjectCreate` + `xrtObjectData` write the payload — one allocation, borrowed access (while a strong reference is held). (2) The counter decrement logic goes through `InstanceOps` (Init/Drop are provided by the type — the payload's scattered resources are cleaned in Drop). (3) The observer shape of `xrtWeakInit`/Expired: after the object ends, `expired=true` — a weak reference is "observation that doesn't extend life" (the standard part for caches/observer patterns); reviving requires `WeakLock` promotion to a strong reference (a new strong reference exists only while the object lives). Companion samples: `value_weak`/`value_roots` (the Value side's weak references and roots), `value_trace` (value tracing) — the Value face of the graph ecosystem.

## Contracts

- **Object layout**: one allocation carries header/alignment/payload; the payload satisfies InstanceAlign (including the unusual 32/64B); a failed Init reclaims immediately without Drop.
- **Strong-reference discipline**: Ref presupposes holding a valid reference (no reviving the dangling); the last Unref Drops on the owning thread; payload access only while holding strong references.
- **Object values**: COPYABLE+RELOCATABLE+ObjectValueOps — unified operations for field/element/parameter slots; address-compare hashing is process-local only.
- **Weak references**: zeroed on first use; Lock is the only atomic promotion; not promotable from finalization on.
- **Graph trimming**: feature off = zero graph fields, zero collector code; on = an object belongs to at most one graph, and the graph only borrows without adding references.
- **Trace iron rules**: each strong-reference slot exactly once; weak/borrowed/null never visited; Drop releases Trace's full set — violation = rejection or leak.
- **Member operations**: Track is idempotent (foreign-graph ownership → EXISTS); ordinary finalization unhooks automatically; same-graph member operations may run concurrently.
- **Safepoints**: collect at rest; Trace/roots/Drop must not re-enter collection; never alongside concurrent modification of the same batch of objects.
- **Failure atomicity**: all checks precede destruction; candidates finalize together; no half-collected state.
- **Complexity**: two-pass marking O(N+E), temporary space O(N); the result's four fields (Tracked/Edge/Root/Collected).
- **Value roots**: shell/COW references need explicit CollectValueRoot(s); dynamic fields trace in layers.

## Pitfalls

### Pitfall 1: Trace under-reporting strong references

Symptom: collection rejected (in-edge inconsistency) or an object leaks (should be collected but wasn't) — Drop released a reference Trace never reported.

Cause: Trace's "each slot exactly once" is the foundation of collection correctness — under-reporting skews in-edge statistics (what should be a root isn't), over-reporting gets reachable objects falsely judged. Trace generation for language fields must share the source with the field layout.

```c bad
static bool trace(const void* V, ..., xrtobjectvisitor Visit, ptr Ctx) {
	const node* N = V;
	return Visit(N->Next, Ctx);
	/* N->Prev strong reference still unreported - one in-edge missing: corrupted statistics */
}
```

```c good
static bool trace(const void* V, ..., xrtobjectvisitor Visit, ptr Ctx) {
	const node* N = V;
	if ( (N->Next != NULL) && !Visit(N->Next, Ctx) ) { return false; }
	return (N->Prev == NULL) || Visit(N->Prev, Ctx);  /* all strong references */
}
```

### Pitfall 2: using a raw pointer right after an Expired check

Symptom: alive at check time, ended by use time — a TOCTOU race crash.

Cause: Expired is an instantaneous observation; the only safe "check + use" is `WeakLock`'s atomic promotion — success means holding a new strong reference (the object necessarily lives for its duration).

```c bad
if ( !xrtWeakExpired(&Weak) ) {
	use(xrtObjectData(Weak...));   /* it may end between check and use */
}
```

```c good
xrtobject* Strong = xrtWeakLock(&Weak);
if ( Strong != NULL ) {
	use(xrtObjectData(Strong));   /* holding a strong reference: it must live for the duration */
	xrtObjectUnref(Strong);
}
```

### Pitfall 3: triggering collection outside a safepoint

Symptom: the collection reads reference fields being concurrently modified — a STATE error or, worse, a wrong collection.

Cause: the contract says plainly "collect at rest" — Trace/field writes/reference-count changes are all modifications. The host scheduler must enter the safepoint after tasks/Futures/generators stabilize (Chapter 60's pause semantics).

```c bad
/* while any thread is still mutating object fields */
xrtObjectGraphCollect(Graph, &Result);   /* STATE error / wrong-collection risk */
```

```c good
/* host enters the safepoint: pause tasks/generators, state stable, then */
host_suspend_all();
xrtObjectGraphCollect(Graph, &Result);
host_resume_all();
```

## Exercises

### Basic: strong-weak reference counting experiments

An object + two strong references + one weak reference: Unref step by step, observe where Expired flips; compare before and after Lock promotion. Acceptance criteria: the end point falls exactly at the last strong reference; the weak reference extends no life; after Lock fails, the Weak is reusable (Init to a new target).

### Advanced: two-way cycle collection

An A↔B two-way cycle + an external reference C→A: verify no collection while an external root exists (collection succeeds after C is released). Acceptance criteria: the two runs' Root/Collected counts match the hand derivation.

### Challenge: host safepoint integration

Simulate a host scheduler: N "tasks" hold object references — pause all (safepoint) → collect → resume; a suspended "generator" among the tasks holds references added via the Roots callback. Acceptance criteria: objects referenced by tasks are not collected; collected after release; the callback's borrowed roots stay alive for the collection's duration; STATE errors zero throughout.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Positioning | a cycle-collection patch above reference counting — not a generational GC; trimming is real (off = zero cost) |
| Object layout | one allocation; payload satisfies InstanceAlign; a failed Init skips Drop |
| Strong references | Ref requires already holding a valid one; the last Unref Drops on the owning thread; access only while holding |
| Object values | ObjectValueOps — unified for field/element slots; address comparison is process-local only |
| Weak references | Lock is the only atomic promotion; Expired is observation only; not promotable from finalization |
| Trace iron rules | each strong-reference slot exactly once; weak/borrowed/null never visited; Drop = Trace's full set |
| Collection algorithm | snapshot → in-edge counting → root identification (automatic + callback) → reachability propagation, O(N+E) |
| Safepoints | collect at rest; callbacks never re-enter; the host scheduler arranges them |
| Failure atomicity | checks precede destruction; candidates finalize together; the xrt.object-graph domain has five codes |
| Value roots | shell/COW need explicit ValueRoot(s); dynamic fields trace in layers |
