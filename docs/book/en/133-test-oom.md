---
num: 133
slug: test-oom
title: OOM and Fault-Injection Testing
volume: 卷十二 工程实践
type: practice
lead: The panorama of per-allocation-point rotation, three failing-allocator forms, the error system's survival under exhaustion, and the weaponization of the quarantine and debug heap — turning "no resources" from an assumption into an exhaustively enumerated fact.
api: memory_debug, memory_stats, error
---

## Orientation

Chapter 132 presented OOM testing as one of the four weapons; this chapter **magnifies it into a full discipline** — behind 185 `_oom` files stands a rigorous methodology. Four blocks: **per-allocation-point rotation** (the looping enumeration of "the Nth allocation fails" — "every allocation failing" is verified individually, not "some allocation fails"); **the three failing-allocator forms** (`xrtMemDebugFailAfter` counting / hand-written quota (Limited+Remaining — the map OOM test's shape) / fail-always (test_oom's `FailAllocator`) — each with its use); **the error system's survival under exhaustion** (error reporting in OOM allocates no memory — when `xrtErrorCreate` fails, the slot keeps `XERR_MEMORY`: reporting "no memory" itself needs no memory); **the debug heap weaponized** (FailAfter combined with the event stream — injected failures are auditable; the quarantine catches double-free/UAF — fault shapes go beyond "allocation failure"). One goal: **"no resources" goes from a runtime assumption to an exhaustively enumerated fact**.

## Introduction

One derivation shows the necessity of per-allocation-point rotation: a function makes 5 allocations — the cleanup path for the 3rd failing differs from the 5th (the former rolls back two full side effects, the latter four and a half); testing only "the 1st fails" (or not at all) leaves 4 cleanup paths never executed — any half-initialized leak/state corruption among them goes straight to production. The rotation method: the failure point sweeps from 1 to 5 (the whole flow rerun per round) — **every cleanup path executed, every failure state asserted**. The 185 files are exactly this sweep unrolled across all modules — the number itself is proof of discipline.

The error system's survival deserves separate emphasis: if reporting an error required allocation, then in OOM you would "fail to report the failure" — the troubleshooter sees a silent crash instead of a structured error. XRT's error object (Chapter 4) avoided this by design (immutable objects with minimal allocation, degrading to the static `XERR_MEMORY` category on failure) — and test_oom is exactly the machine verification of that promise: "verify that OOM error reporting itself allocates no memory" is its first-line comment.

## Concepts

### Per-allocation-point rotation: the shape of enumeration

```diagram flow
- Setup: a quota allocator (Limited=true, Remaining=N)
- Loop: N decreases from the start -> reset the tested object each round -> run the full flow
  -> assert three things: failure return / state unchanged (still usable or safely destructible) / correct error
- Termination: the flow succeeds (Remaining still >0 - past the last allocation point)
- Reconciliation throughout: debug heap Reset/Snapshot - zero leaks per round
```

The strong "state unchanged" assertion has two tiers: **still usable** (OOM is temporary — after memory frees, the same object works as before, e.g. a map whose insert failed still queries); **safely destructible** (at least the Destroy path doesn't crash a second time — half-initialized objects clean up completely). Both tiers are contracts: the first is quality, the second the floor.

### The three failing-allocator forms

| Form | Mechanism | Use |
| --- | --- | --- |
| Counting (`xrtMemDebugFailAfter(N)`) | first N succeed, the N+1st fails; Clear also resets the trigger bit | single-point injection + event auditing (applications and tests share it) |
| Quota (hand-written: `Fail`/`Limited`/`Remaining`) | forwards to the underlying allocator until the quota runs out | rotation sweeps (Remaining decreasing is the sweep) |
| Fail-always (`FailAllocator`) | always NULL | extreme paths (quick verification that the very first allocation fails) |

The three forms amount to "treating the memory budget as a controllable knob" — testing moves from the binary world of "is there memory" to the precise world of "which allocation runs out".

### The error system's survival under exhaustion

`test_oom`'s assertion chain is the template: `xrtMalloc(32)==NULL` + error category `XERR_MEMORY` + domain `xrt.memory`; then `xrtErrorCreate(...)==NULL` (the error object's own allocation fails) + **the slot still keeps XERR_MEMORY** — "the failure to create an error does not overwrite the OOM fact". This verifies Chapter 4's deep "minimal dependencies" promise: the error channel stays usable under resource exhaustion. **The all-OOM degradation semantics** (recurring in contracts): "if OOM has made even error objects unallocatable, degrade to static XERR_MEMORY" — not every failure can carry rich context, but **every failure can be seen**.

### Extending fault shapes: beyond allocation failure

The debug heap's (Chapter 6) quarantine extends fault injection to the **free side**: double free, use-after-free (UAF) — the quarantine's delayed reuse makes such errors **necessarily visible** in tests (instead of passing silently). Combining `_mutation` tests with OOM (mutation + point-by-point failure) is the extreme form of stress: malformed input × resource exhaustion — the parser's deepest corners are lit too. The same discipline in the extensions: xhttp's per-allocation-point rollback, the OOM gate of xws server routing — **every extension library carries its own _oom matrix** (the same gate after Chapter 129's manifest stacking).

## Examples

### First complete program: the error system surviving exhaustion

The program below is from `tests/core/test_oom.c` — this is the test itself (not an example), showing the core survival assertion:

```embed path="tests/core/test_oom.c" title="tests/core/test_oom.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c tests/core/test_oom.c -lws2_32 -liphlpapi
[PASS] oom
```

**What just happened.** (1) `testInstallFailAllocator()` — the fail-always form installed (an always-NULL allocator takes over library memory). (2) First assertion group: allocation failure + category + domain — **the error-reporting trio** holds in the minimal scenario. (3) The second group is this chapter's soul: **the very call creating the error object fails** (returns NULL) — and the error slot **still keeps XERR_MEMORY** — "the channel reporting failures doesn't go mute because of a failure". (4) Behind the single line `[PASS] oom` lies the floor guarantee for all resource-exhaustion paths. This file is barely a dozen lines — because it is the foundation (complex modules' _oom stack business assertions on top).

### Second complete program: the application face of injection

The second program is from `examples/memory/fail_inject` — the feel of the three-state knob:

```embed path="examples/memory/fail_inject/main.c" title="examples/memory/fail_inject/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/memory/fail_inject/main.c -lws2_32 -liphlpapi
enabled=1
fail-after-2: 1st=ok 2nd=NULL triggered=1
event[0]: alloc
cleared: 3rd=ok triggered-still=0
reset=1
```

**What just happened.** (1) Read alongside Chapter 132: there the point was the "four weapons" taxonomy; here the focus is **precise control of injection** — FailAfter(1)'s boundary semantics (1st=ok 2nd=NULL) visible field by field in the output. (2) The `triggered` flag's independence: queryable after triggering, reset only by Clear — the injector's own state is auditable (the alloc event appears in the event stream — **a failure is an event too**). (3) Real tests write rotation with these entrances: an outer for loop increments N, each round Resetting the injector state and the tested object — this sample is a single-step cross-section of that loop.

### Why C libraries' OOM discipline is stricter than managed languages'

The contrast shows why this investment is necessary. In managed languages (Java/Go/C#) allocation failure is a `Throwable` — the runtime unwinds the stack, the GC guarantees reclamation — the platform takes over most of the application's cleanup duty. C has no such backstop: allocation failure is just a `NULL` return, **cleanup is 100% the caller's responsibility** — every field of a half-initialized structure, every block of a partially built container must be returned by someone in the right order. XRT's 185 _oom files are the point-by-point acceptance ledger of that "return duty". Few libraries of this scale reach this density — most C libraries' OOM handling stops at "the critical path checks NULL" — exactly the root of "occasional memory leaks finally OOM-freezing" in embedded and long-running services: not some unchecked NULL, but a cleanup path no one ever walked. Rotational enumeration turns "every cleanup path walked" into a machine guarantee — that is its entire value.

## Contracts

- **Rotational enumeration**: the failure-point sweep covers every allocation — every cleanup path executed and asserted.
- **Threefold assertion**: failure return / state unchanged (still usable or safely destructible) / correct error (category + domain).
- **Reconciliation discipline**: zero leaks per round (debug heap Reset/Snapshot cross-checked).
- **Three allocator forms**: counting / quota / fail-always — the knob precise to "which one".
- **Error survival**: error reporting in OOM allocates no memory; on error-creation failure the slot keeps XERR_MEMORY.
- **Degradation semantics**: when error objects are unallocatable, degrade to static XERR_MEMORY — failures are always visible.
- **Free-side injection**: the quarantine catches double-free/UAF — fault shapes cover release paths.
- **Combined stress**: mutation × point-by-point failure — the product space of malformed input and resource exhaustion.
- **Extensions, same law**: every extension library carries its own _oom matrix — stacking manifests stacks the gate.

## Pitfalls

### Pitfall 1: OOM tests that don't reset the injector state

Symptom: every round from the second on "fails" — the first round's FailAfter trigger bit is still set; or the quota allocator's Remaining wasn't refilled — you tested "always fails", not "the Nth fails".

Cause: the injector is stateful — every loop round must reset completely (FailClear / Remaining refilled / tested object rebuilt). Forgetting the reset degrades "enumeration" into "re-testing the first state".

```c bad
for ( N = 1; N < 100; N++ ) {
	fail_after(N);
	run_flow();        /* previous round's trigger bit/quota residue - everything fails for N>1 */
}
```

```c good
for ( N = 1; N < 100; N++ ) {
	reset_injector();   /* Clear the trigger bit / refill the quota */
	rebuild_target();  /* rebuild the tested object to a consistent start */
	fail_after(N);
	run_flow();
	assert_three();    /* threefold assertion */
}
```

### Pitfall 2: asserting "returns failure" on an already-corrupted object

Symptom: the test passes but production crashes — the test discards the object right after asserting the failure return (Destroy never runs); production's caller walks the cleanup path and steps on half-initialized fields.

Cause: **"safely destructible" is a mandatory test item** — the failure path's Destroy must really execute in the test (that is the proof-of-execution for cleanup paths).

```c bad
testRequire(op(&Obj) == false, "oom");
/* return directly - Destroy has never executed in the failed state */
```

```c good
testRequire(op(&Obj) == false, "oom");
testRequire(destroy(&Obj), "失败态可安全销毁");  /* the cleanup path executes */
```

### Pitfall 3: error assertions checking only the category, not the domain

Symptom: OOM gets reported as `XERR_IO` or a parameter error — the test (checking only category) lets it through — upper-layer retry logic then takes the wrong branch (treating "no memory" as "transient IO error", a retry storm).

Cause: category and domain together form the error identity (Chapter 4); OOM tests assert both — a wrong domain is a wrong report.

```c bad
testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "kind");
/* domain unchecked: MEMORY of the xrt.io domain also passes */
```

```c good
testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "kind");
testRequire(strcmp(xrtErrorDomain(xrtGetError()),
	"xrt.memory") == 0, "domain");   /* category + domain double assertion */
```

## Exercises

### Basic: the feel of three forms

Inject all three forms into the same code with 3 allocations: FailAfter(2) (the 3rd fails) / quota 2 (equivalent) / fail-always (the 1st fails). Acceptance criteria: the three forms' failure-point assertions agree; you understand the equivalence and difference (counting carries event auditing).

### Advanced: a rotation harness

Write a generic `oom_scan(被测流程, 最大点数)` (tested flow, max points) rotator: internally loop inject-reset-assert-reconcile. Run it on Chapter 18's map insert. Acceptance criteria: the sweep terminates automatically (past the last allocation point); zero leaks per round; deliberately leave a leak on a cleanup path — the rotator names which round.

### Challenge: the mutation × OOM product

Stack point-by-point failure onto a parser's (say percent encoding) mutation testing: outer loop mutates input, inner loop sweeps failure points — sampling the product (the full product is too large). Acceptance criteria: the sampling strategy is justifiable (say 1 failure point per 10 mutation points); discover — or prove absent — defects visible only under "malformed input + exhaustion".

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Rotational enumeration | failure points swept 1..N — every cleanup path walked |
| Threefold assertion | failure return / state unchanged (two tiers) / error category + domain |
| Three forms | counting (auditing) / quota (sweeping) / fail-always (quick) |
| Error survival | reporting in OOM allocates no memory; error-creation failure keeps XERR_MEMORY |
| Degradation semantics | error objects unallocatable → static XERR_MEMORY — failures always visible |
| Free side | the quarantine catches double-free/UAF — injection beyond allocation failure |
| Combined stress | mutation × point-by-point failure — sampled exploration of the product space |
| Reset discipline | fully reset injector and tested object each round — enumeration doesn't degrade |
| Scale fact | 185 _oom files — discipline proven by numbers |
| Extensions, same law | extension libraries carry _oom matrices — stacking is the gate |
