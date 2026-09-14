---
num: 60
slug: task-cancel
title: Cancellation and Structured Concurrency
volume: 卷六 进程与并发
type: practice
lead: Task groups: one wait point for a whole set of tasks, final-state statistics in one take, and a parent-child scoped cancellation tree — structured concurrency in full.
api: task, future, cancel
---

## Orientation

The task group (`xtaskgroup`) is structured concurrency in full: **one set of tasks, one wait point** (`xrtTaskGroupWait` — no more waiting Future by Future), **final-state statistics in one take** (`xtaskgroupstats` — the complete tally of succeeded/failed/cancelled/closed/rejected), **parent-child scopes** (`xrtTaskGroupChild` — the cancellation tree's structured form: cancelling a parent group requests the whole subtree, while leaf final states are still confirmed by the producing end). Chapter 54's tokens, Chapter 58's Futures, and Chapter 59's pools converge here — this chapter assembles the loose concurrency tools into program units with **lifecycle boundaries**.

## Introduction

The "wait for a batch" code without task groups: store 20 Futures in an array, Wait each or hand-write a counting loop, tally successes and failures by hand, build token forwarding yourself if one failure should cancel the rest — forty lines of boilerplate rewritten at every concurrent join, and every missed branch (does cancelled count as failed?) is a statistics bug.

The task group folds those forty lines into one concept. The greater value is **structure**: concurrent operations have scoped boundaries — the group is created at scope entry, all tasks inside belong to it, and at scope exit the group waits and drains ("either all complete or all cancel"). This is isomorphic to structured programming's block boundaries: **goto (bare threads/bare Futures) is replaced by blocks (scoped groups)** — leaked concurrent operations (a forgotten Wait on a Future) become structurally impossible, because the scope's exit is the join point. Parent-child groups nest the structure: the request-handling group is a child of the connection group, the connection group a child of the service group — service stop cancels the service group, the whole tree responds.

## Concepts

### The task group's core five

```diagram flow
- Create: xrtTaskGroupCreate(config) - the group is the scope
- Register: Add (an existing Future) / Start (launch a proc returning a Future) - reserve the slot first, then launch
- Close: Close (stop intake, natural finish) / Cancel (stop intake + cooperative cancel request)
- Join: Wait / For / Until / UntilCancel - close and wait
- Statistics: xtaskgroupstats - the final-state tally in one take
```

**Two registration ways**: `Add` tracks an existing Future (the group holds the reference to its final state — reference balancing is the group's); `Start` reserves a slot first, then invokes the launching procedure (which returns the Future — on failure no unregistered entry remains; registration and launch are atomic).**Two closing grades**: `Close` for a natural finish (the backlog runs out), `Cancel` for cooperative cancellation (Chapter 54's cancellation tree's big button — sends the request to current items and child groups).**Four join shapes**: Wait (unbounded), For/Until (timed), WaitCancel (cancellable by the caller — the transitive form of nested waiting, xrtTaskGroupWaitUntilCancel).

### Final-state statistics: the whole tally in one take

| Field | Semantics |
| --- | --- |
| `Succeeded` | tasks Resolved |
| `Failed` | tasks FAILED |
| `Cancelled` | tasks CANCELLED |
| `Closed` | tasks CLOSED |
| `Rejected` | submissions refused after the group stopped intake |
| `Active` / `Added` / `Completed` | live / cumulative / completed |

Chapter 58's "cancellation ≠ failure" landed in statistics: the three final states counted separately — publishing reports (Chapter 39's logs) needs no hand classification. `Rejected` deserves note: post-close submission attempts are counted too (overload evidence — a ready-made field for Chapter 50's observation).

### Parent-child scopes: the cancellation tree structured

`xrtTaskGroupChild(父, 配置)` (parent, config) spawns a child group — a parent Cancel propagates to child groups and leaf Futures (Chapter 54's tree, automatic form); **leaf final states are confirmed by the producing end** — after the cancellation request arrives, the producer (the Promise holder) chooses Reject/Resolve/Cancel (the scope sample verifies `parent completed = 1, cancelled = 1`: one leaf completed normally, one confirmed cancelled — two leaf outcomes under one parent cancel). A child group's Wait waits only the child (local join); the parent's Wait waits the whole tree (global join) — **the granularity of waiting aligns with the granularity of scopes**.

### The group's Done Future: asynchronous joining

`xrtTaskGroupFuture` returns the group's Done Future (succeeds when the group is closed and active reaches zero) — joining itself goes asynchronous: the group's completion can be a link in a continuation chain (Chapter 58), can be Watched (Chapter 58's callbacks), can enter another group (groups within groups — the Done Future is Add-ed into the upper group).**The wait point turns from a blocking call into a composable first-class value** — the confluence of structured concurrency and the Future system.

### Task groups with coroutines and pools

The task sample family unfolds three combinations.**Group + pool** (task_group_pool): tasks execute via the pool, Futures hang into the group — the pool manages execution, the group manages joining;**group + coroutine** (task_coroutine / task_group_coroutine): a coroutine proc returns a Future hung into the group — the coroutine's multi-step async whole counts as one group item;**group + Promise** (task_group main sample): hand-made Promises also hang in — the group doesn't care where Futures come from. Three sources, one join — **the group is the convergence layer of final states, orthogonal to the execution layer (pool/scheduler)**.

## Examples

### Complete program: one set, one wait point

From the repository sample `examples/concurrency/task_group/main.c` — two members, one join, final-state statistics:

```embed path="examples/concurrency/task_group/main.c" title="examples/concurrency/task_group/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/task_group/main.c -lws2_32 -liphlpapi
completed: 2, succeeded: 2
```

**What just happened.** (1) `xrtTaskGroupCreate(NULL)` builds the group (NULL takes defaults); two Promise/Future pairs are created (a hand-made producing end — mocking async operations). (2) The two Futures are `Add`-ed — the group's reference takes over (their final states are published by the Promise ends). (3) The Promise ends publish two values — both Futures end Resolved. (4) `xrtTaskGroupWait` **waits for two tasks in one wait** — replacing the per-Future loop; `xtaskgroupstats` takes all at once: `completed: 2, succeeded: 2`. This is the minimal proof of "one set, one wait point, one statistics take" — against the introduction's forty hand-written lines, all gone.

### Complete program: parent-child scoped cancellation

From `examples/concurrency/task_group_scope/main.c` — the cancellation tree, structured:

```embed path="examples/concurrency/task_group_scope/main.c" title="examples/concurrency/task_group_scope/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/concurrency/task_group_scope/main.c -lws2_32 -liphlpapi
parent completed = 1, cancelled = 1
```

**What just happened.** (1) A parent group is built, a child spawned via `xrtTaskGroupChild`, leaf Futures hung into the child — a **three-level scope tree**. (2) The parent `Cancel` — the request propagates down the tree to the child and every leaf (Chapter 54's tree, automatic: no hand-forwarding tokens to each leaf). (3) The leaf producers' **autonomous final-state confirmation**: one leaf had already completed before the cancel (completed=1), the other honors the cancel and confirms Cancelled (cancelled=1) — **the request goes out uniformly, final states are confirmed individually** (cooperative cancellation's full semantics: request ≠ result; the producing end owns the final-state decision). (4) After the parent's Wait, the statistics hold the mixed outcomes — `completed = 1, cancelled = 1` is this discipline's machine testimony. The task_tour sample is the full-interface tour (submit/group/pool/cancel/timed — one ok group each).

## Contracts

- **Atomic registration**: Start reserves the slot before launching — failure leaves no unregistered entry; after Add, references are the group's.
- **Two closing grades**: Close for a natural finish (backlog runs out) / Cancel for cooperative cancellation (tree propagation) — choose by shutdown semantics.
- **Autonomous final states**: cancellation requests go out uniformly; final states are confirmed by producers — the three-way tally of cancelled ≠ failed ≠ closed.
- **Waiting granularity**: a child waits the child, the parent waits the whole tree — waiting aligns with scopes; the Wait family's four shapes (unbounded/For-Until timed/UntilCancel cancellable).
- **Done Future**: joining goes asynchronous — the group's completion event can hang in upper groups, chain continuations, be Watched.
- **Orthogonal sources**: Futures from pools/coroutines/Promises all fit — the group is the convergence layer; the execution layer composes freely.

### From examples to engineering: three hosts of task groups

**Request handling** (most common): one child group per request — the request's concurrent operations (cache/DB/downstream) all hang in the child; on answer or timeout the child joins or cancels; the child's destruction is the request's end — request lifecycle and scope strictly aligned (the challenge exercise in full).**Service orchestration**: a service-level parent group holds all connection/request groups — Chapter 49's stop signal fires the parent's Cancel, the whole tree responds, the parent's Wait then exits safely — graceful shutdown, structurally implemented (against Chapter 54's hand-made "shutdown path" — the task group regularizes it).**Batch jobs**: one group per batch — the group's Wait is the batch's completion, stats the batch's report (succeeded/failed/cancelled itemized); partial-failure retry policy driven by the Failed list. The three hosts share one shape: **scope boundary = lifecycle boundary = join point** — that is the engineering meaning of "structured".

### Structured concurrency, past and present

Structured concurrency is no new invention — it is the formalization of old lessons.**The unstructured pain**: bare threads/bare Futures are like goto — control flow "jumps out and never returns" (forgotten waits, no cancellation, unmanaged leaks); the "callback hell" debates around 2016 were at heart the readability disaster of unstructured concurrency.**The formal lineage**: the nurseries concept (Nathaniel Smith, Trio's author, 2017) first systematized "must join within the scope"; Kotlin structured concurrency (2019) brought it mainstream; Swift's TaskGroup (2022) offered an API-shape reference. XRT's task group is this lineage's C implementation — the four design points **Child spawning, Close/Cancel's two grades, three-way final states, the group Future** all find their correspondents in the lineage. The value of knowing this history: structured is not "yet another API style" — it is a formal guarantee of concurrent correctness (the join guarantee at scope exit turns "leaked concurrent operations" from "avoided by discipline" into "impossible by structure").

### Confluence with Chapter 50's observation: final-state statistics as observation

The task group's stats structure is Chapter 50's observation pipeline's sentry at concurrency's endpoint.**Reporting cadence**: batch groups report once after Wait (Chapter 38's structured log fields — Succeeded/Failed/Cancelled as three JSON columns); service-level parent groups sample Active periodically (the live-count curve — Chapter 56's scheduler health metrics, task edition).**Alert triage**: Failed>0 alarms; a Cancelled surge traces upstream (who is cancelling); Rejected>0 checks overload (is the group's intake stop backpressure or shutdown?).**Reconciliation**: request child groups' stats sum-consistent with the connection parent's (nested statistics self-consistency — the challenge exercise's acceptance item). Final-state statistics turn from "counting by hand" into "structured observation fields" — Volume 6's cancellation main line finds its final landing in Chapter 50's pipeline.

## Pitfalls

### Pitfall 1: destroying without Wait/final states

Symptom: after the group's destruction tasks still run — dangling execution, lost statistics, occasional crashes (the group's resources freed while task final states still want to write stats).

Cause: the scope's exit is the join point — Wait (or WaitCancel) before Destroy; this is structured concurrency's "closing brace".

```c bad
xtaskgroup* Group = xrtTaskGroupCreate(NULL);
xrtTaskGroupStart(Group, launchQuery, NULL);   /* async task running */
xrtTaskGroupDestroy(Group);                     /* destroyed outright - the task dangles */
```

```c good
xtaskgroup* Group = xrtTaskGroupCreate(NULL);
xrtTaskGroupStart(Group, launchQuery, NULL);
if ( xrtTaskGroupWait(Group) != XWAIT_OK ) {   /* exit joins (structure's closing brace) */
	xrtTaskGroupCancel(Group);                   /* or Cancel then bounded wait */
}
xrtTaskGroupDestroy(Group);                     /* destroy after joining */
```

### Pitfall 2: treating Cancelled as Failed

Symptom: a normal shutdown (actively cancelling in-flight tasks) is reported as an incident — monitoring false alarms, alert noise; rollback and failure handling confused.

Cause: the three-way final tally (Succeeded/Failed/Cancelled) never entered the decision — Cancelled after a Cancel is an **expected outcome**, not an error.

```c bad
if ( Stats.Failed + Stats.Cancelled > 0 ) {
	Alert("tasks failed");   /* cancelled alarms too - the illusion that shutdown = incident */
}
```

```c good
if ( Stats.Failed > 0 ) { Alert("tasks failed"); }          /* real failures alarm */
if ( Stats.Cancelled > 0 ) { LogInfo("tasks cancelled"); }  /* cancellations logged */
/* release-shutdown vs fault-shutdown reported separately (Chapter 54's discipline, statistical face) */
```

## Exercises

### Basic: a three-final-state machine

One group, three tasks: one Resolve, one Reject, one Cancelled after a Cancel — after Wait print the stats' complete final-state line, itemized and checked.

### Advanced: nested scopes

Parent group → two child groups (three tasks each) — Cancel the parent and watch propagation; then Cancel only one child (parent and sibling unaffected) — both directions verified once, each level's statistics printed.

### Challenge: structured request handling

A full request group: request arrives → child group created → three concurrent paths (cache/DB/timeout race) hung in the child → child joins (First semantics hand-implemented) → answer → child destroyed. The parent (connection level) holds all request children; on disconnect the parent Cancels. Acceptance criteria: request child lifecycle strictly aligned with the request (zero leaks — Chapter 6's statistics); on disconnect every in-flight request drains (final states classified correctly); three-level statistics reconcile.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Core five | Create builds / Add-Start register / Close-Cancel two closing grades / the Wait family joins / stats tallies |
| Final-state tally | Succeeded/Failed/Cancelled/Closed/Rejected counted separately - cancellation ≠ failure |
| Parent-child scopes | Child spawns subgroups; parent Cancel propagates tree-wide; leaf final states confirmed autonomously by producers |
| Done Future | GroupFuture returns the completion Future - joining goes asynchronous, can hang in upper groups |
| Atomic registration | Start reserves the slot before launching - failure leaves no unregistered entry |
| Exit joins | Wait/Cancel before destroy - structure's "closing brace" |
| Orthogonal sources | Futures from pools/coroutines/Promises all fit - convergence layer decoupled from execution layer |
