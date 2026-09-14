---
num: 133
slug: testing
title: Testing, Fault Injection, and Fuzzing
volume: 卷十二 工程实践
type: practice
lead: The testRequire assertion family, the test-shape matrix across threads/negative/mutation/per-allocation-point OOM, and LibFuzzer-style structured fuzzing — the gate's four weapons.
api: core, memory_debug
---

## Orientation

Every API of the preceding chapters is guarded by a crowd of tests — this chapter covers those tests' **morphology**. XRT's test system (hung on the manifest's tests field, run by build.py on both tracks) has four weapons: **positive tests** (the `testRequire(条件, "消息")` (condition, "message") assertion family — acceptance terse enough for one line); **thread tests** (the `_threads` suffix — concurrency semantics measured for real); **negative and mutation tests** (the `_negative`/`_mutation` suffixes — robustness on malformed input: full-byte mutation round trips, boundary seeds); **per-allocation-point OOM tests** (the `_oom` suffix — 185 files! A failing allocator injected point by point, verifying "failure breaks no state and the error reports correctly"). **Fuzzing** (fuzz/): LibFuzzer-style entry points plus in-repo **structured fuzzing** (deterministic shift-register noise with syntax seeds first — the shape of `test_http_auth_fuzz`). The four weapons cover the four quadrants "right behavior, behavior under concurrency, behavior on wrong input, behavior without resources".

## Introduction

Why do OOM tests number 185 files? Because **memory exhaustion is the most easily skipped path**: developers write code assuming allocation succeeds (in normal environments it does), and tests run on memory-rich machines — so "cleanup after allocation failure" becomes a purely untested zone. Yet that path hides the most leaks and state corruption (half-initialized structures, partially built containers). XRT's discipline is **per-allocation-point verification**: a failing allocator (like `testMapOomAlloc` — "forward to the underlying allocator while allowed; return NULL once the quota runs out") makes each allocation of the code under test fail in turn — one round per the Nth failure, looping until every point has taken its turn — "allocation failure breaks no state" becomes an exhaustively enumerated fact, not an assumption.

Mutation testing shares the motive but a different object: **parser robustness**. HTTP headers, percent encodings, TLS messages — the real network delivers every byte sequence; dozens of hand-made cases are never enough. Mutation testing flips **every byte** of legal inputs one by one, then runs whole rounds of random noise — "arbitrary input never crashes; failure goes through structured errors" is machine-proven. Fuzzing adds one more layer: syntax seeds (real protocol shapes) + deterministic random variation — running combinations in CI that humans wouldn't think of.

## Concepts

### The assertion family and the test-shape matrix

```diagram flow
- Positive (plain test names): testRequire(condition, "note") - failure prints the message and exits
- Threads (_threads): concurrency semantics for real - SPSC stress / cancellation race conditions / error-slot isolation
- Negative (_negative): explicit rejection of illegal input - error class and code asserted
- Mutation (_mutation): full-byte mutation + case mixing - round-trip stability
- OOM (_oom): failing allocator injected point by point - the 185-file matrix
- Fuzz (_fuzz): syntax seeds + deterministic noise - structured exploration
```

`testRequire` is the library-wide unified assertion (the test framework is minimal: on failure print message + line, exit non-zero — the quality gate trusts only exit codes). Shape suffixes are a **naming convention** — the manifest shows each module's covered quadrants at a glance (queue's card: test_queue/test_queue_spsc_threads/test_queue_spsc_oom — three quadrants present).

### Per-allocation-point OOM: the failing-allocator pattern

```diagram flow
- Setup: a custom allocator (Fail flag / Limited quota / Remaining count)
- Loop: fail at point N -> run the tested flow -> assert (failure return / state unchanged / error = XERR_MEMORY + domain)
  -> N+1 -> until the flow no longer allocates
- Extra assertion: error reporting itself allocates no memory (errors must be reportable even in OOM - the core _oom assertion)
```

Chapter 6 (memory debugging) covered the failing allocator's **tool face** (`xrtMemDebugFailAfter`); the test system uses it plus hand-written quota allocators to cover every module. **"Error reporting allocates no memory"** is a recurring assertion (`xrtErrorCreate` on failure keeps the OOM category in the error slot — the error system stays alive under resource exhaustion): the test face of Chapter 4's "minimal dependencies" promise.

### Mutation testing: round trips and stability

Two shapes: **full-byte mutation** (a legal vector → each byte individually ±1/flipped → re-parse/re-decode — rejection or success both fine, but no crashes or leaks, and successes semantically match the original); **noise round trips** (deterministic random input — e.g. percent's 6000 rounds of "any bytes + case-mixed escaping round-trip stably"). **Determinism** (fixed seeds) is the key: failures reproduce — a fuzz-found issue replays by reporting the seed.

### Fuzzing: seeds + noise + invariants

`test_http_auth_fuzz`'s structure is the template: (1) **syntax seeds first** (a string array of real protocol shapes — empty string / standard Basic/Bearer/Digest with complex parameters — known boundaries pass first); (2) **deterministic noise** (shift-register variable-length random input — exploring combinations humans wouldn't think of); (3) **invariant assertions** (parsing either succeeds or fails structurally — `FuzzAbort` reports contract violations with line numbers, "avoiding losing diagnostics after the optimizer merges cold branches" — even compiler optimizations are accounted for). **The fuzz/ directory** (xhttp's four LibFuzzer-style entries: http_auth/http_route/http_router/http_sse): persistent entries for external fuzzers — CI quickly runs the built-in fuzzing; offline deep runs use external fuzzers, same invariants.

## Examples

### First complete program: the everyday face of fault injection

The program below is from `examples/memory/fail_inject` — the application-level posture of a failing allocator:

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

**What just happened.** (1) `xrtMemDebugFailAfter(N)`'s **measured semantics**: the first N succeed, the N+1st fails — the output's 1st=ok 2nd=NULL is exactly FailAfter(1). (2) The **event stream** (event[0]: alloc) — ordered event access on the debug heap: the injected failure also enters the event stream (complete audit). (3) `FailClear` also resets the "already triggered" flag (after clearing, 3rd=ok and triggered-still=0) — **every _oom test variant is built on this set of entrances** (header comment verbatim: tests and applications share one implementation — another "no second implementation").

### Second complete program: locating a leak

The second program is from `examples/memory/debug` — the Reset/Snapshot/VisitLive trio:

```embed path="examples/memory/debug/main.c" title="examples/memory/debug/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/memory/debug/main.c -lws2_32 -liphlpapi
live_count=1 live_bytes=64 peak_bytes=64
live address=000002bd... size=64 site=examples/memory/debug/main.c:30
alloc_count=1 free_count=1 events=2
```

**What just happened.** (1) The **standard posture for locating a leak** (header comment): `Reset` before the suspect segment, `Snapshot` after — a `LiveCount` that won't return to zero plus the `site` printed by `VisitLive` (**precise to file:line**) is the leak point. (2) The output shows alloc_count=1 free_count=1 but live_count=1 — a live block after one alloc and one free? That's the example's deliberate leftover (to demonstrate that VisitLive sees it) — the count-reconciliation mindset: three counters (alloc/free/live) cross-check allocation discipline. (3) The stronger capability (quarantine catching double-free/UAF) comes in the JSON report form (`debug_report` sample) — tests and operations share the same data.

### The test pyramid's third layer: the mutation/fuzz divide

Picture XRT's tests as a pyramid: the base is a mass of positive assertions (dozens of testRequire per module — behavior nailed down item by item); the middle is quadrant expansion (threads/oom — one group for each of the two costly quadrants); the top is mutation and fuzzing — **mechanical exploration of the input space**. Remember the divide: mutation anchors on **known legal inputs** (flip every byte — guaranteeing "the illegal near the legal" all passes); fuzzing anchors on **syntax templates** (randomly assembled realistic input — exploring the "structurally plausible but never seen" region). The former guards against regressions (old boundaries broken); the latter against the unknown (unwritten boundaries hiding). Both live on determinism as their lifeline — also the precondition for entering CI (minutes to run, failures always reproducible); true-random deep exploration is left to offline external fuzzers.

## Contracts

- **Shape naming**: `_threads`/`_negative`/`_mutation`/`_oom`/`_fuzz` suffixes — covered quadrants visible in the manifest.
- **Minimal assertions**: the testRequire family — failure prints + non-zero exit; the quality gate trusts only exit codes.
- **OOM point by point**: the failing allocator rotates point by point; assert failure return + unchanged state + XERR_MEMORY + correct domain.
- **Error system alive**: error reporting in OOM allocates no memory — the error slot keeps the OOM category.
- **Mutation determinism**: fixed seeds — failures reproduce (the seed is the replay credential).
- **Invariants first**: fuzz asserts the either/or "succeed or fail structurally" — crashes/leaks are contract violations.
- **Seeds first**: syntax boundary seeds precede random noise — known boundaries must pass.
- **Dual-mode fuzzing**: built-in deterministic (CI) + external fuzzer entries (offline deep runs) — the same invariants.
- **Examples are testware**: the fail_inject/debug samples share entrances with _oom tests — no second implementation.

## Pitfalls

### Pitfall 1: writing only positive tests

Symptom: merged, then production crashes — the three paths of concurrency races/malformed input/memory exhaustion all untested.

Cause: positive tests cover only the "right behavior" quadrant; four quadrants (right/concurrent/wrong input/no resources) need four weapons — the last three are exactly where incidents come from.

```c bad
testRequire(parse(str) == OK, "parse");
/* concurrency? malformed? OOM? - all untested */
```

```c good
/* at least one group per quadrant: positive + threads + mutation/negative + oom */
testRequire(parse(str) == OK, "parse");
/* tests/xxx/test_xxx_threads.c / _mutation.c / _oom.c attached to the same module in the manifest */
```

### Pitfall 2: fuzzing with true randomness (irreproducible)

Symptom: CI fails intermittently — a local rerun passes (different seed), the report cannot localize.

Cause: fuzzing's value is **reproducibility** — fixed-seed deterministic noise replays every failure. True randomness is for offline exploration only (on discovery, convert to a fixed-seed case and check it in).

```c bad
srand(time(NULL));            /* different every time - a failure is lost */
run_fuzz(random_input());
```

```c good
uint32 State = UINT32_C(0x9E3779B9);  /* fixed seed */
run_fuzz(noise(&State));               /* a failing seed replays */
```

### Pitfall 3: OOM tests that stop at "returns failure"

Symptom: state corrupted after OOM — the next call crashes; or the wrong error category (OOM reported as a parameter error).

Cause: the full OOM assertion is threefold: failure return + **state unchanged** (the object remains usable or safely destructible) + correct error (XERR_MEMORY + domain). Checking only the return value lets half the bugs through.

```c bad
testRequire(map_insert(...) == false, "oom fails");
/* state? error? - unchecked */
```

```c good
testRequire(map_insert(...) == false, "oom fails");
testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "kind");
testRequire(map_still_consistent(&Map), "state intact");  /* state unchanged */
testRequire(map_destroy_path_clean(&Map), "可安全清理");   /* teardown does not crash twice */
```

## Exercises

### Basic: run one module's full shapes

`build.py --suite queue` — observe the threads/oom variants running. Acceptance criteria: you can name each test file's covered quadrant.

### Advanced: write the four-piece set for a custom module

Write a small allocating module (a simple cache, say) with positive/threads/mutation/oom test files attached to the manifest. Acceptance criteria: deliberately leave a leak on the OOM path — the oom test catches it (reconcile with the debug heap).

### Challenge: a structured fuzzer

Model on `test_http_auth_fuzz`: syntax seeds (≥8 real shapes) + deterministic noise 5000 rounds + invariants (parse succeeds or fails structurally). Acceptance criteria: inject a crashing bug (an out-of-bounds read, say) — the fuzzer catches it within ≤1000 rounds; the output carries a replayable seed.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Four weapons | positive (testRequire) / threads (_threads) / mutation-negative (_mutation/_negative) / OOM (_oom) |
| Fuzzing | _fuzz built-in + fuzz/ external entries — syntax seeds first + deterministic noise |
| OOM pattern | failing allocator rotating point by point; threefold assertion (return/state/error) |
| Error survival | error reporting in OOM allocates no memory — the slot keeps the category |
| Mutation shapes | every byte flipped + deterministic noise round trips |
| Determinism | fixed seeds — failures reproduce; true randomness only for offline exploration |
| Invariants | the either/or "succeed or fail structurally" — crash/leak is a violation |
| Application face | the fail_inject/debug samples share entrances with tests — no second implementation |
| Naming convention | shape suffixes — covered quadrants visible in the manifest |
| Scale fact | 185 _oom files — per-allocation-point verification is a discipline, not a slogan |
