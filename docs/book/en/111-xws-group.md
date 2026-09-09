---
num: 111
slug: xws-group
title: xws (Part 3): Connection Groups and Broadcast
volume: 卷十一 其他扩展库
type: practice
lead: Unique membership and the reference lifecycle, hard capacity and sealing, stable snapshots allocated outside the lock, and waitable batch broadcast — from "send to one" to "send to all".
api: xws-websocket_group, xws-websocket_runtime
---

## Orientation

The basic operation of a push system is "send one message to every online connection" — `xwsgroup` makes "a set of connections" into a correct concurrent container: **unique membership** (the same connection joining twice is still one member), **reference lifecycle** (Add takes a reference, Remove/Clear/Destroy give it back — connections are neither closed nor aborted), **hard capacity** (when Limit is full, new members get a retryable capacity error), **sealing** (Seal permanently stops admissions — the standard move when a server stops taking sessions), **stable snapshots** (Snapshot copies members in join order and increments each reference — allocation outside the lock, so big snapshots don't block Add/Remove), and **batch broadcast** (synchronous and asynchronous forms; the async form returns a **waitable operation object** — cancellation of the wait does not break the ownership of data already accepted by each connection). Broadcast does not bypass per-connection limits: slow connections are skipped, waited on, or closed per your policy — never dragged into an unbounded shared queue.

## Introduction

"Maintaining the list of online connections" looks like an `数组+锁` (array + lock) beginner exercise — until the concurrency details queue up: the same connection's Open and Close callbacks run concurrently on different Workers, Add and Remove race; members join and leave during a broadcast iteration (iterator invalidation — Chapter 18's old problem); a slow connection holds up the whole batch during broadcast (one drags N); at server shutdown, "stop taking new ones first, close the old ones gracefully" (a half-closed state). Each is daily life for a real push service.

xwsgroup answers them one by one: unique-membership semantics absorb duplicate Adds; Snapshot's "linearize inside the lock, allocate outside it" gives the iteration a stable set (the snapshot holds its own references — even destroying the original group changes nothing; iterator invalidation becomes impossible); broadcast is **per-connection independent acceptance** (a slow connection doesn't drag others) + aggregate waiting (the async operation object); Seal provides the half-closed state. This chapter's value is not the API count (a dozen entrances) but that **every concurrency semantic is thought through** — read it as a "concurrent-container design lesson" with a WebSocket example.

## Concepts

### Lifecycle and unique membership

```diagram flow
- Create: GroupCreate(Limit) - 0 = no explicit cap; Ref/Destroy shared
- Join: a successful Add holds one connection reference; adding the same pointer repeatedly still succeeds but forms no second member
- Remove: Remove/Clear/Destroy give references back - no closing, no aborting (the group's responsibility boundary)
- Query: Has/Count/Limit take a locked instantaneous snapshot; Sealed queries the permanent sealed state
```

The meaning of **unique membership**: Open-callback replays (an occasional network-layer event) cannot leak double references; Remove returns exactly the one share Add took. **Capacity semantics**: when Limit is full, Add fails with `XERR_AGAIN / XWS_GROUP_ERROR_CAPACITY` — **retryable** (after removing a member you can add again), not a protocol error; this error-class design separates "overloaded" from "illegal" (Chapter 4's category model applied at the container layer).

### Sealing: the half-closed state

`xrtWsGroupSeal` is permanent and idempotent: after sealing no new members join, but **re-adding an existing member still succeeds** (idempotent semantics continue), and remove/clear/snapshot work as usual. The standard shutdown order: stop accepting new connections (the listener layer) → Seal the group (in-flight Opens no longer join) → close or abort each existing session gracefully — "close the entrance first, then clear the hall".

### Snapshot: the correct way to iterate stably

`xrtWsGroupSnapshotCreate`: copies the current members in join order and **increments a reference** for each; the key engineering detail — **the contiguous snapshot storage is allocated outside the group lock**: first attempt the allocation lock-free; after retaking the lock, if membership grew past capacity, free and retry; only when capacity suffices does it increment references in order **at the linearization point**. The effect: a big snapshot's allocation work (which may trip the allocator lock) does not block the Add/Remove/Clear hot path. The snapshot **does not hold the group** — after the original group is destroyed the snapshot's members stay valid (their references live on the snapshot); release with `SnapshotDestroy` when done. This is exactly Chapter 18's "container invalidation rules" in solved form: **when you need a stable iteration, copy a reference-holding snapshot — don't gamble your way across the original container**.

### Batch broadcast: independent acceptance and aggregate waiting

The synchronous form: iterate the snapshot and send per connection (each connection's AGAIN/failure handled independently — skip, note as slow, close per policy). The asynchronous form: entrances like `xrtWsGroupTextAsync` return an `xwsgroupop` **operation object** — `GroupOpWait` waits for the whole batch to finish, destroy releases; **cancellation of the aggregate wait does not break the data ownership already accepted by each connection** (the batch edition of Chapter 109's "cancel ≠ destroy"). Broadcast does not bypass per-connection send limits — a slow connection's queue stays bounded as always, and your policy decides its fate.

### Pairing with the three send states

A broadcast's message payload is a natural fit for the **ref form** (Chapter 110): one static payload, references handed to N connections' queues — the release callback counts down (the true release only after the last write). The group's async broadcast entrances are exactly the batch application of this form; a hand-written iteration assembles the same with `xrtWsConnBinaryRef` (Chapter 110's exercise did this).

## Examples

### First complete program: the group lifecycle skeleton

The program below is from `examples/websocket/group` — the standard integration shape of Add/Remove:

```embed path="extlibs/xws/examples/websocket/group/main.c" title="extlibs/xws/examples/websocket/group/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/group/main.c -lws2_32 -liphlpapi
WebSocket connection group is ready
```

**What just happened.** (1) `xrtWsGroupCreate(0)` builds an unlimited group (production passes a Limit — capacity is the overload line). (2) The two functions are the **integration template**: `connectionOpen` (in the upgrade-success callback, `GroupAdd` — the connection joins), `connectionClose` (in the sole Close callback, `GroupRemove` — the reference returns). A real server puts the group into the routing context (Chapter 112 — where the `Data` field earns its keep) — one-line integration for Open/Close, and the group's concurrency semantics all inherited. (3) This sample's main only verifies readiness (template shape); the `(void)` references in `connectionOpen/Close` are compile-time self-proof of "keeping the callback shape".

### Second complete program: a waitable broadcast on an empty group

The second program is from `examples/websocket/group_future` — the full lifecycle of the async broadcast operation object:

```embed path="extlibs/xws/examples/websocket/group_future/main.c" title="extlibs/xws/examples/websocket/group_future/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/group_future/main.c -lws2_32 -liphlpapi
（空组广播操作完成自检通过后正常退出）
```

**What just happened.** (1) `xrtWsGroupTextAsync(组, 消息)` (group, message) returns a complete operation object even for an **empty group** — the header comment spells out the design intent: "broadcast to an empty group also returns a full waitable, inspectable operation object" — the empty set is not an error; the aggregate semantics are self-consistent (zero connections, all succeed). (2) `GroupOpWait` waits for the whole batch (`XWAIT_OK`), `GroupOpDestroy` releases the operation object — the async broadcast's three-step lifecycle (initiate/wait/release). (3) Read the two samples together: after Open joins the group, `TextAsync` broadcasts — each member accepted independently; when the wait returns, the whole batch has its verdicts. The slow-connection policy (skip/close) lives in the configuration and your iteration code — the group does not decide for you.

## Contracts

- **Unique membership**: repeatedly Adding the same pointer succeeds without doubling; Remove/Clear/Destroy return exactly the references Add took; group operations **neither close nor abort** connections.
- **Capacity**: Limit zero = unlimited; a full Add returns `XERR_AGAIN/CAPACITY` (retryable, not a protocol error).
- **Sealing**: Seal is permanent and idempotent; after sealing no new members, re-adding existing ones still succeeds; remove/clear/snapshot as usual — at shutdown, Seal first, then clear.
- **Snapshot**: allocate outside the lock + linearized reference increments inside; order preserved by join order; does not hold the group (members stay valid after group destruction); Destroy when done.
- **Queries**: Has/Count/Limit/Sealed are locked instantaneous snapshots — no substitute for a stable iteration (use Snapshot).
- **Broadcast independence**: per-connection acceptance; per-connection limits not bypassed; the slow-connection policy (skip/wait/close) belongs to the caller.
- **Async operation objects**: the TextAsync family returns waitable, inspectable objects; cancelling a wait does not break accepted ownership.
- **Lock discipline**: Clear swaps in an empty set inside the lock and returns old references outside it — destruction and allocator work never hold the group lock.
- **Opaque validation**: group/snapshot/operation are all opaque; fixed headers and contiguous storage validated before dereference; wraparound rejected as a parameter error; `Destroy(NULL)` is a no-op.
- **Trimming**: `WEBSOCKET_GROUP`/`_GROUP_FUTURE` independent; the group depends on connections and set/mutex — the frame layer pulls in none of it.

## Pitfalls

### Pitfall 1: iterating and sending on the original group during broadcast

Symptom: occasional iteration scrambling or missed sends — members join and leave during the iteration (new connections join, old ones close and leave), invalidating the iteration state.

Cause: the group's Count/Has are instantaneous snapshots — the set changes while you iterate. "Walking and gambling" is Chapter 18's old container-invalidation pit.

```c bad
size_t n = xrtWsGroupCount(pGroup);
for ( size_t i = 0; i < n; i++ ) {
	/* take member i - Remove may have changed the set meanwhile */
}
```

```c good
xwsgroupsnapshot* pSnap = xrtWsGroupSnapshotCreate(pGroup);
for ( size_t i = 0; i < xrtWsGroupSnapshotCount(pSnap); i++ ) {
	xwsconn* pConn = xrtWsGroupSnapshotGet(pSnap, i);
	send_or_skip(pConn);   /* stable set: joins/leaves during iteration don't affect this snapshot */
}
xrtWsGroupSnapshotDestroy(pSnap);
```

### Pitfall 2: assuming Remove closed the connection

Symptom: the client "still receives messages" — you thought leaving the group disconnected it; in fact the connection is alive and well.

Cause: the group's responsibility is **membership**, not connection lifecycle — Remove only returns a reference; closing is `xrtWsConnClose/Abort`'s business (Chapter 109). The separation of responsibilities makes combinations like "out of the broadcast group but still managed" possible.

```c bad
xrtWsGroupRemove(pGroup, pConn);
/* assumed the connection is closed - the peer stays online */
```

```c good
xrtWsGroupRemove(pGroup, pConn);       /* leave the group first */
if ( !xrtWsConnClose(pConn, XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("kicked")) ) {
	xrtWsConnAbort(pConn);              /* then close per policy */
}
```

### Pitfall 3: destroying the group at shutdown without Sealing

Symptom: at the instant of shutdown new Opens are still joining — after the group is destroyed these connections' references are left dangling; or a shutdown-order race condition crashes.

Cause: destroying the group only returns the references the group held; in-flight Adds (callbacks still running) need sealing to block. "Seal first, then clear" is the only safe order.

```c bad
stop_listener();
xrtWsGroupDestroy(pGroup);   /* in-flight Opens' Adds haven't finished */
```

```c good
stop_listener();             /* 1. stop taking new connections */
xrtWsGroupSeal(pGroup);      /* 2. seal: in-flight Opens' Adds rejected */
drain_workers();             /* 3. wait for callbacks to finish (Open/Close go quiet) */
close_all_members(pGroup);   /* 4. gracefully close existing sessions */
xrtWsGroupDestroy(pGroup);   /* 5. destroy the group last */
```

## Exercises

### Basic: lifecycle and capacity experiments

A group with Limit=3: Add four connections — the fourth fails (capacity error); Remove one then Add succeeds; after Seal, Adding an existing member succeeds, a new one fails. Acceptance criteria: the six steps match the contract; the error codes distinguish capacity from sealing.

### Advanced: a snapshot broadcaster

Iterate a snapshot broadcasting a ref payload: one static message, `ConnBinaryRef` to the whole snapshot, the release callback counting to verify exactly-once. Loop 1000 broadcast rounds while connections toggle concurrently (random Add/Remove). Acceptance criteria: zero crashes, zero leaks (Chapter 6 stats); each round's delivery count matches that moment's snapshot count.

### Challenge: a channel system

Multiple channels (an array/map of groups): subscribe/unsubscribe (Add/Remove), per-channel broadcast, global announcement (all channels). Implement kick (leave + close) and the shutdown flow (Seal → drain → close per channel → destroy). Acceptance criteria: three channels under concurrent load testing with zero scrambling; a kick stops reception immediately; shutdown exits cleanly across the chain (final state count reconciliation).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Positioning | a concurrent container of connections: membership without lifecycle — no closing, no aborting |
| Unique membership | repeated Add succeeds without doubling; Remove returns exactly one share |
| Capacity | Limit zero = unlimited; full AGAIN/CAPACITY — retryable, not an error |
| Sealing | Seal permanent and idempotent; blocks new, not old; the first shutdown step |
| Snapshot | allocate outside the lock + increment inside; join order; does not hold the group; the only correct stable iteration |
| Queries | Has/Count/Limit/Sealed instantaneous — not for iteration |
| Broadcast | per-connection independent acceptance; per-connection limits not bypassed; the slow-connection policy is yours |
| Async operations | the TextAsync family returns waitable objects; cancelling a wait does not break ownership |
| Lock discipline | Clear swaps inside the lock, releases outside — the hot path is never blocked by allocation |
| Integration template | Add in Open, Remove in Close; the group goes into routing Data (next chapter) |
