---
num: 49
slug: signal
title: Cross-Platform Signals
volume: 卷五 系统服务
type: practice
lead: Signal observers, once-semantics, and automatic cleanup — graceful cross-platform handling from Ctrl+C to SIGTERM.
api: signal
---

## Orientation

The signal module unifies POSIX signals and Windows console events into the **signal observer** model: an `xsignalwatch` handle plus the `xrtSignalOn/Once/OnOwned/OnceOwned` registration family subscribes to signals, and events arrive as **asynchronous notifications** (not the restricted environment of signal handler functions); **once semantics** fits signals like SIGINT that should be "handled once"; automatic cleanup prevents dangling triggers on exit paths. The three high-frequency signals — SIGINT (Ctrl+C), SIGTERM (the orchestrator's stop command), and SIGHUP (the traditional reload signal) — are the three main entrances of service lifecycle management.

## Introduction

Hand-writing signal handlers is a famous minefield of C: **only a small handful of functions are async-signal-safe inside a handler** (printf/malloc are not among them — logging or allocating in a handler is undefined behavior); cross-platform differences (Windows has no real POSIX signals; Ctrl+C is a console event); handler-to-main-loop communication only through restricted mechanisms like `volatile sig_atomic_t`. Every one of these is a pit already stepped in.

XRT's solution is **observers + asynchronous delivery**: register an observer declaring the signals of interest; when a signal arrives, the module delivers the event to your callback in a safe context (not the raw handler) — inside the callback, the whole library API is usable normally; `xsignalevent` carries the signal name and count (multiple arrivals of the same signal merge into a count). The restricted-environment problem is structurally bypassed, not left to programmers memorizing signal-safety lists.

## Concepts

### The observer model

```diagram flow
- Register: xrtSignalOn(signal, callback, data) persistent / Once one-shot / OnOwned-OnceOwned owner-bound
- Arrival: signal fires -> module captures -> delivers xsignalevent (name+count) in a safe context
- Callback: executes in a normal environment - printf/logging/containers all usable
- Cleanup: xrtSignalOff/Free unsubscribes, or automatic cleanup at process exit - dangling triggers prevented
```

The count semantics deserve note: a callback may see merged arrivals (the event carries `count`) — "Ctrl+C pressed three times" is one count=3 event, not three callbacks, so the rapid-press user intent (force quit) isn't swallowed by per-arrival processing latency.

### Once semantics and owned-on

The signal_tour sample unfolds two advanced semantics: **once** — the observer auto-unregisters after one trigger (SIGINT's "first Ctrl+C exits gracefully" use case: a second Ctrl+C meaning "just kill it" should no longer be intercepted); **owned-on** — the observer's lifecycle binds to an owner (the auto-destroy count verifies `destroyed=1`), so exit paths never dangle. The two together cover the traditional hard part, "signal-handling cleanup".

### The three entrance signals

| Signal | Trigger | Standard response |
| --- | --- | --- |
| SIGINT | Ctrl+C (interactive) | graceful exit: stop accepting, drain in flight, timed force-quit |
| SIGTERM | orchestrator stop (k8s/docker stop) | as above - the primary exit channel in container environments |
| SIGHUP | terminal disconnect / traditional reload | config reload: reread config, atomic switch (Chapter 46) |

The three entrances converge on the **graceful exit/reload** pattern: receive signal → set the stop/reload flag → main loop checks the flag → walk the drain path (Flush logs, close handles, wait for in-flight requests) — the signal callback only sets a flag; heavy work waits for the main loop (the same root as Chapter 48's light-callback discipline).

### The division with Chapter 53's processes

This chapter's signal module governs **receiving** (how my process responds to signals); Chapter 53's process module governs **sending** (how I signal a child — the SIGTERM→SIGKILL ladder for timeout kills). Both ends share signal names (`xrtSignalName`), but the APIs stay separate — the same chapter-division logic as the file family.

## Examples

### Complete program: signal observation and counting

From the repository sample `examples/process/signal/main.c`:

```embed path="examples/process/signal/main.c" title="examples/process/signal/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/process/signal/main.c -lws2_32 -liphlpapi
$ ./a.exe
signal=INT count=1 total=1
```

**What just happened.** (1) The observer registers its signal set and callback — the `xsignalwatch` handle holds this subscription. (2) After the signal arrives (Ctrl+C), the event is delivered to the callback — `xsignalevent` carries `signal=INT` (the name) and `count=1` (arrivals merged this time); `total` is the sample's own running count. (3) The callback executes in a normal environment — printf used directly (in a raw signal handler this would be undefined behavior; the observer model makes it legal). (4) The event also carries `Total` (cumulative) and `Name` (string name) — the source of the three fields in `signal=INT count=1 total=1`. The program exits orderly after the signal, and observer cleanup happens automatically.

### Complete program: the full-interface tour

From `examples/process/signal_tour/main.c` — support queries, owned-on, once, and count management:

```embed path="examples/process/signal_tour/main.c" title="examples/process/signal_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/process/signal_tour/main.c -lws2_32 -liphlpapi
signal: supported=1/1 name=INT healthy=1
signal: owned-on fired=1 destroyed=1
signal: once fired=2 destroyed=1
signal: count/received/clear ok
signal: ignore/restore/all ok
```

**What just happened.** (1) Support queries (`supported=1/1`) and name conversion (`name=INT`) — signal capabilities are per-platform probeable (ask before registering when writing portable code). (2) owned-on: fires within the owner's scope (`fired=1`), auto-cleans when the owner is destroyed (`destroyed=1`) — lifecycle binding prevents dangling. (3) once: two firings (`fired=2`), then the one-shot observer self-destructs (`destroyed=1`) — the mechanism behind "intercept the first, let the rest through". (4) Count, fetch, and clear (`count/received/clear ok`) — the entrance for polling-style consumption. (5) Ignore, restore, and batch cleanup (`ignore/restore/all ok`) — signal mask management: temporarily ignore a signal then restore, batch unregister to finish.

## Contracts

- **Safe environment**: callbacks execute in a normal context — the whole library API available; the raw handler's restrictions are absorbed inside the module.
- **Merged counting**: multiple arrivals of one signal merge into one event (the count field); rapid repeated presses never lose user intent.
- **once/owned**: once semantics (self-destruct after firing) and owner binding (cleaned with the owner) — drain paths never dangle.
- **Light callbacks**: callbacks only set flags/enqueue; heavy work is the main loop's (Chapter 48's discipline, signal edition).
- **Send/receive division**: this chapter receives; sending signals to children is Chapter 53's process module.
- **Capability probing**: signal support is queryable per platform — portable code probes, then registers.

### From examples to engineering: three hosts of signal handling

**Service lifecycle** (the three entrances' home turf): at startup register once observers for SIGINT/SIGTERM → first signal sets the stop flag → the main loop drains (stop accepting, wait in-flight with a time limit, Flush logs, close handles) → exit; after once unregisters, a second signal takes the default termination — the force-quit exit. **Hot config reload**: SIGHUP triggers the reload pipeline (Chapter 43 config → diff → atomic switch) — the signal is the trigger, the heavy work all on the main loop's established paths. **Child supervision**: in concert with Chapter 53 — SIGCHLD-class events enter the reaper flow through an observer (reaping child resources); the sending side lives in the process module. The signal callbacks of all three hosts share one shape: **set a flag or enqueue, done in a few lines** — Chapter 48's light-work discipline in full agreement.

### Dissolving a historical burden: why the signal API looks like this

Understanding this chapter's API design requires knowing what it dissolves. A POSIX signal handler runs in "an arbitrary preempted context" — your main thread may be interrupted by a signal while holding malloc's internal lock, and calling malloc again inside the handler re-enters the same lock — deadlock. So traditional signal handling permits only async-signal-safe functions (a very short list), printf excluded. XRT's observer model does minimal capture in the low-level handler and moves delivery to a safe context — your callback therefore "looks like an ordinary function". This isn't hiding complexity; it's moving complexity to the layer where it belongs: **the restricted-environment discipline is borne by the module, and business callbacks keep the directness of normal code**. You will meet the same thinking again in Chapter 48 (system callbacks → task pool) and the Chapter 63 preview (IO completion notifications) — "minimize at the boundary, normalize at the business" is the universal philosophy of XRT's asynchronous design.

### The complete checklist: signals and graceful exit

Graceful exit is signal handling's summit scenario; a complete checklist closes the chapter: **register** — SIGINT/SIGTERM once observers (first interception); **flag** — an atomic stop bit, the callback's only action; **drain order** — stop accepting (close listeners) → wait in-flight (bounded deadline) → Flush (logs/files) → close handles (reverse dependency order) → exit code (deliberately distinguish graceful from timed out); **timeout exit** — when the deadline hits or a second signal arrives, force-quit; never wait forever; **test** — rehearse the signal path (CI sends itself a signal and verifies the drain). The six-item checklist is exercise two's grading rubric — and the acceptance sheet of every production service's "stop action".

## Pitfalls

### Pitfall 1: heavy work inside a signal callback

Symptom: occasional deadlock or crash during signal handling — the logging system's Flush in the callback collides with the main thread's same Sink; the allocator's lock in the callback collides with the same lock the interrupted thread held.

Cause: signals arrive anytime — the callback runs concurrently with any main-thread code; touching non-reentrant resources the main thread is using (logging, allocator, containers) inside the callback is a race.

```c bad
static void onSignal(const xsignalevent* pEvent, ptr pData)
{
	StopAllWorkers();     /* heavy work: stopping thread pools, flushing logs - a concurrency race collection */
	ExitProcess(0);
}
```

```c good
static volatile bool gStop = false;   /* flag bit (or atomic) */
static void onSignal(const xsignalevent* pEvent, ptr pData)
{
	gStop = true;        /* light work: set the flag only */
}
/* main loop checks gStop -> walks the drain path (stop pools/flush/close handles - safe context) */
```

### Pitfall 2: ignoring the second signal's user intent

Symptom: the service sticks in its drain path — rapid Ctrl+C does nothing, and the user is left with kill -9; some wait in the drain code never times out.

Cause: the first signal triggered graceful exit, but the drain path has no deadline; subsequent Ctrl+C presses keep being "gracefully swallowed" by the same observer — the user's force-quit intent is ignored.

```c bad
xrtSignalOn(SIGINT, gracefulSignal, NULL);   /* persistent: every signal is graceful - no exit */
static void gracefulSignal(...) { gStop = true; }
/* main loop: while(!gStop) ... drain WaitAll() may wait forever */
```

```c good
xrtSignalOnce(SIGINT, firstSignal, NULL);    /* once semantics: intercept only the first */
static void firstSignal(...) { gStop = true; }
/* drain bounded by time (Chapter 42 deadline); timeout or second signal -> exit immediately */
/* after once unregisters, the second Ctrl+C takes the default behavior (terminate) - the force-quit exit */
```

## Exercises

### Basic: register the three signals

Register observers for SIGINT/SIGTERM/SIGHUP setting stop and reload flags respectively; the main loop checks every second and prints state changes (simulating both paths: graceful exit and hot reload).

### Advanced: a graceful-exit timer

Implement the full exit flow: first signal → stop accepting → wait in-flight (5-second limit) → Flush → exit; within the limit print "graceful", on timeout force-quit and print "forced". Use two threads to simulate in-flight work and verify the wait semantics.

### Challenge: a hot config reloader

SIGHUP triggers config reload: reread Chapter 43's three config layers → diff the old and new value trees (Chapter 31 traversal) → print changed fields and apply (log level takes effect immediately; listen address is rejected and rolled back). Acceptance criteria: no service interruption during reload; illegal config rejected with the old config still serving; the change list complete and auditable.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Registration family | `xrtSignalOn/Once/OnOwned/OnceOwned`; unregister with `xrtSignalOff`/`xrtSignalFree` |
| Safe environment | callbacks run in a normal context - printf/logging/containers usable (restrictions absorbed by the module) |
| Event fields | Name / Count (merged this time) / Total (cumulative); polling consumption has count/received/clear |
| once/owned | self-destruct after one trigger / cleaned with the owner - drains never dangle |
| Three entrances | SIGINT interactive exit / SIGTERM orchestrator stop / SIGHUP config reload |
| Light-work discipline | callback sets the flag, main loop drains - the signal edition of the async boundary law |
| Mask management | ignore/restore/batch unregister - temporarily mute a signal then restore; send/receive division: sending is Chapter 53 |
