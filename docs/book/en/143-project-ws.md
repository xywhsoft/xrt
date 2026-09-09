---
num: 143
slug: project-ws
title: Project 5: A WebSocket Real-Time Push Service
volume: 卷十三 实战项目
type: project
lead: Connection-group broadcast, reference-send multicast primitives, slow-client policy and backpressure, the graceful-shutdown flow end to end — the four xws chapters applied together under long-connection concurrency.
api: xws-websocket_server_router, xws-websocket_group, xhttp-http_server
---

## Orientation

Volume 13's Project 5: a **real-time push service** (pushd) — a WebSocket long-connection gateway: after connecting, clients subscribe to channels (`{"op":"sub","ch":"metrics"}`), and the server pushes backend-produced messages to every channel member (broadcast); it supports kicks (kick), channel-member queries, and graceful shutdown (the full Seal-then-clear flow). It applies the four xws chapters (109–112) together: the **routing endpoint** (Chapter 112's `xrtWsServerRoute` — the secure entrance with Origin policy and the authorize callback), **connection groups** (Chapter 111's `xwsgroup` — membership / snapshot broadcast / Seal half-close), **the send path** (Chapter 110's writer/references/compression — broadcast's multicast primitives), **the connection lifecycle** (Chapter 109's pause/resume/close protocol). The new system constraint is **long-connection concurrency**: the lifecycle management of hundreds of simultaneously live connections, the fan-out amplification of broadcast, and the impact of slow clients on the whole — Projects 1/2/4's lessons plus this one converge into the full stack in the static server (Chapter 144).

## Introduction

The requirement scene: a monitoring dashboard must show service metrics live — polling (one pull per second) is laggy and wasteful; SSE (Chapter 106) suffices for one-way, but the client needs **subscription semantics** across channels and bidirectional control (sub/unsub/kick) — WebSocket's bidirectional long connection is the natural form. pushd's interface design: **connection protocol** (upgrade at `/ws`, subprotocol `push.v1`); **subscription messages** (JSON: sub/unsub with channel names); **push messages** (JSON: channel + payload + sequence — clients check the sequence for losses); **management actions** (kick a connection, list channel members — the operations interface). **Acceptance criteria**: (1) 100 concurrent connections, three channels, broadcasts fully delivered (no misses — sequence reconciliation); (2) a slow client (receiving but not reading) does not drag fast clients (isolation verified); (3) kick takes effect immediately (later messages never reach the kicked); (4) graceful shutdown: after Seal new connections refused, in-flight messages delivered, exit only after all connections close (final-state count reconciliation); (5) broadcast memory decoupled from connection-count × message-size (reference send — verified with stats); (6) the service stays stable under backpressure (broadcast never blocks on a single full connection).

## Concepts

### Architecture: three-layer assembly

```diagram flow
- Entrance layer: the HTTP Router + xrtWsServerRoute("/ws") - Origin three-tier default (SAME_HOST_OR_ABSENT)
  + Authorize (optional token check); the Open callback Refs + GroupAdds
- Session layer: per-connection state (subscribed-channel set / last sequence / identity) hangs in a Map outside the groups
  - the groups manage membership, the Map manages business state (separation of duties)
- Broadcast layer: snapshot (GroupSnapshotCreate) -> per-member GroupTextAsync
  - reference payloads (one memory shared by N connections); slow connections per policy (skip/mark slow)
```

**The subscription-set placement decision**: the connection group manages only "the set of connections" (Chapter 111's boundary — no message encoding, no business); the **channel→connection-set** mapping is business state — an independent multi-group structure (one `xwsgroup` per channel) or Map<channel, group>. Decision: **one group per channel** — the group's concurrency semantics (unique membership/references/snapshots) are exactly what channel membership needs, obtained free; the alternative Map<channel, connection array> would hand-roll all the concurrency semantics — rejected.

### Module selection and rationale

| Step | Choice | Rationale |
| --- | --- | --- |
| WS entrance | the `xrtWsServerRoute` fixed endpoint | the five automatic responses (405/403/400/426/500) + Origin defaults — secure defaults free |
| Membership | one `xwsgroup` per channel | unique-member/capacity/snapshot concurrency semantics ready-made |
| Broadcast | snapshot + per-member reference send | one payload for N connections (memory decoupled — acceptance 5); snapshots give stable iteration (the positive form of Chapter 111 pitfall 1) |
| Payload construction | JSON (Chapter 32) | serialization of subscribe/push messages; the sequence field supports reconciliation |
| Slow clients | skip + mark slow (policy swappable) | a single connection's AGAIN never drags the whole (acceptance 2/6) — the group-broadcast "per-connection independent acceptance" semantics |
| Shutdown | Seal→drain→close per channel→destroy | Chapter 111's shutdown template applied directly |
| Business state | Map<connection, subscription set> | the connection's business context (an application-layer structure outside the groups) |

### The broadcast pipeline: from a backend message to N clients

```diagram flow
- A backend message arrives (producer: timer / upstream subscription)
- Build the payload: JSON encoding (channel + payload + a globally increasing sequence)
- Look up the channel group: GroupSnapshotCreate (a stable member snapshot - references added)
- Per member: GroupTextAsync (reference payload)
  - AGAIN/failure: the slow-connection policy (skip + count; kick optional)
- Snapshot destroyed (member references returned)
```

**The sequence number's role**: monotonically increasing per channel — clients detect losses (receiving 3 then 5 — 4 was lost: an active re-sync). **Reference send's multicast upgrade** (Chapter 110): one payload, each of the N connections' queues holding a reference — the release callback counts down (the true release only after the last write); `GroupTextAsync` is exactly this shape inside. **The three slow-connection grades**: skip + mark slow (default — a lost frame is acceptable, the client re-syncs); kick (a configured threshold — cleaning chronically degraded connections); unbounded waiting (**not provided** — a single connection dragging everyone violates acceptance 2/6).

### The subscription protocol and message handling

A message arrives (Chapter 109's event) → JSON parse (Chapter 32) → dispatch by the `op` field: **sub** (validate the channel name → `GroupAdd` into the group — create on first); **unsub** (`GroupRemove` + reference return); **heartbeat** (client ping — the connection layer's automatic Pong already handles it (Chapter 109); the application layer may optionally echo online status). **Illegal messages**: a parse failure returns an error frame (a JSON error code) without disconnecting (protocol leniency — one corrupt frame does not mean a bad connection); an illegal channel name returns an error — **every error locatable** (Chapter 4's error model, application-message edition).

### The graceful-shutdown flow

Chapter 111's shutdown template fully unfolded in pushd: **step one** stop accepting (listener-layer Drain — new HTTP upgrades no longer admitted); **step two** Seal every channel group (`GroupSeal` — idempotent; new subscriptions blocked, existing members send and receive as usual); **step three** the farewell broadcast (a "server shutting down" frame — a clear signal to clients, never a silent drop); **step four** close connection by connection (`ConnClose` with NORMAL code — the protocol-driven goodbye); **step five** wait for the Close-callback count to reach zero (all connections final); **step six** destroy groups and routes (`Release`'s Data lifecycle concludes here — Chapter 112's "exactly once after the Router and all connections exit"); **step seven** stop the Engine. **Acceptance 4's reconciliation points**: closing frames delivered (clients received shutting down) + zero leaks (the groups' member counts at zero) + exit code 0.

### Against the earlier projects' lessons

The twelve lessons (inventoried in Chapter 142) replay in long-connection form plus additions: **acceptance first** (six items — concurrency and timing entries rising); **shape decides the container** (channel = group, subscription set = Map, message = JSON — three shapes, three containers); **views are use-and-discard** (message-event views parsed and dispatched on the spot — no buffering); **layered swappability** (swapping the transport for TLS or the serialization for XSON is single-layer); **platform facts enter design** (Origin policy and subprotocol negotiation are WS's "platform facts"); **budget first** (broadcast fan-out = message rate × connection count — the budget basis for backpressure and slow policy); **four-quadrant testing** (concurrent tests become the main battlefield: 100 connections × three channels × sequence reconciliation). **Two additions**: **fan-out is an amplifier** (1 backend message × 1000 connections = 1000 sends — every nanosecond of the broadcast path is amplified; reference send is not an optimization but a necessity); **long-connection state cleanup is the final test** (a short connection's cleanup happens naturally after each request; a long connection's cleanup happens only at shutdown and kick — leaks are stealthier, and final-state count reconciliation is the only reliable acceptance).

## Examples

### First complete program: channel-group membership management

The program below is from `examples/websocket/group` — the integration template of channel membership:

```embed path="extlibs/xws/examples/websocket/group/main.c" title="extlibs/xws/examples/websocket/group/main.c"

```
```
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/group/main.c -lws2_32 -liphlpapi
WebSocket connection group is ready
```

**What just happened.** (1) The `connectionOpen/Close` pair is pushd's **subscribe/unsubscribe kernel**: on upgrade success `GroupAdd` (join the channel), in the sole Close callback `GroupRemove` (return the reference) — **a connection's group lifecycle synchronizes strictly with the connection lifecycle** (no unsubscribe leak possible — Close is the only final state, guaranteed exactly once). (2) pushd adds channel routing (multiple groups) and the message protocol on top — membership's concurrency semantics (idempotent repeated Add / capacity refusal / stable snapshots) all inherited from the group. (3) A real server puts the group into the route's `Data` (Chapter 112) — this sample locks in "the shape of integration".

### Second complete program: the async broadcast operation object

The second program is from `examples/websocket/group_future` — the waitable broadcast:

```embed path="extlibs/xws/examples/websocket/group_future/main.c" title="extlibs/xws/examples/websocket/group_future/main.c"

```
```
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/group_future/main.c -lws2_32 -liphlpapi
（空组广播操作完成自检通过后正常退出）
```

**What just happened.** (1) `xrtWsGroupTextAsync` returns an **operation object** — the three steps initiate/wait/release: pushd's broadcast call shape (the producer fires and doesn't wait — the timer rhythm drives; the management interface waits — confirm delivery after a kick). (2) An empty group is also a complete operation (zero connections = zero successes — aggregate semantics self-consistent). (3) The slow-connection policy point sits here: the operation object does not wait for any single slow connection (complete once all accepted — a slow one's AGAIN is handled by its own connection's backpressure) — **"per-connection independent acceptance"** is precisely the mechanism behind acceptance 2/6.

### Implementation walkthrough: the broadcast and subscription core source

pushd's two core loops — subscription handling and channel broadcast — in source form (key segments walked over the teaching samples):

```c
/* message event: JSON protocol dispatch (sub/unsub) */
static void on_message(xwsconn* Conn, const uint8_t* Data,
	size_t Size, ptr Ctx)
{
	pushd* S = Ctx;
	xvalue Msg;
	xstrview Op, Ch;

	/* parse failure: reply with an error frame, keep the connection (the lenient contract) */
	xvalue* V;
	if ( !xrtJsonParse(Data, Size, &Msg) ||
		((V = xrtValueObjectGet(Msg,
			XRT_STR_LITERAL("op"))) == NULL) ||
		!xrtValueGetString(V, &Op) ) {
		send_error(Conn, "bad message");
		return;
	}
	if ( ((V = xrtValueObjectGet(Msg,
			XRT_STR_LITERAL("ch"))) == NULL) ||
		!xrtValueGetString(V, &Ch) ||
		!channel_name_valid(Ch) ) {
		send_error(Conn, "bad channel");
		return;
	}

	if ( xrtStrEqual(Op, XRT_STR_LITERAL("sub")) ) {
		xwsgroup* Gr = pushd_channel(S, Ch);  /* fetch or create */
		xrtWsGroupAdd(Gr, Conn);               /* idempotent */
		send_ok(Conn, "subscribed");
	} else if ( xrtStrEqual(Op, XRT_STR_LITERAL("unsub")) ) {
		xwsgroup* Gr = pushd_find(S, Ch);
		if ( Gr != NULL ) { xrtWsGroupRemove(Gr, Conn); }
		send_ok(Conn, "unsubscribed");
	}
}

/* channel broadcast: snapshot + reference send + slow-connection skip */
static void pushd_broadcast(pushd* S, xstrview Ch, str Payload)
{
	xwsgroupsnapshot* Snap;
	size_t i;

	xwsgroup* Gr = pushd_find(S, Ch);
	if ( (Gr == NULL) || xrtWsGroupSealed(Gr) ) { return; }
	Snap = xrtWsGroupSnapshotCreate(Gr);   /* stable snapshot */
	for ( i = 0; i < xrtWsGroupSnapshotCount(Snap); i++ ) {
		xwsconn* C = xrtWsGroupSnapshotGet(Snap, i);
		xwsgroupop* Op = xrtWsGroupTextAsync(Gr, view(Payload));
		if ( Op == NULL ) { S->SlowCount++; }  /* skip + mark slow */
		else { xrtWsGroupOpDestroy(Op); }
	}
	xrtWsGroupSnapshotDestroy(Snap);       /* return the member references */
}
```

**Walkthrough points.** (1) `on_message`'s dispatch order: parse the whole frame first (a bad frame gets an error, no disconnect), then field validation, then action — **each level's error independently locatable** (bad message / bad channel / success receipt). (2) `pushd_channel` fetches or creates the group — the first subscription creates it, `GroupAdd` is idempotent (repeated subs to one channel safe — Chapter 111's unique-membership semantics cashed). (3) `pushd_broadcast`'s snapshot loop is the positive form of pitfalls 1/2: snapshot-stable iteration, group-level async operations, failure-skip counting — three contract pieces in place at once. (4) The shutdown path's `GroupSealed` check: after Seal, broadcasts fall silent naturally (the farewell broadcast goes out **before** the Seal — order is semantics).

### The acceptance matrix and concurrent-test design

The six criteria's test matrix (Chapter 132's four quadrants, long-connection edition): **positive** — 100 connections, three channels × 1000-frame broadcast with sequence reconciliation (acceptance 1); **concurrent races** — broadcast concurrent with subscribe/unsubscribe (snapshot semantics under a race condition, measured), broadcast concurrent with kick (no reach after Remove — acceptance 3); **slow client** — 1 slow (receiving, not reading) + 99 fast (acceptance 2: the fast ones' sequences unaffected); **backpressure** — inject production rate > consumption rate (acceptance 6: service stable, the slow count rising); **shutdown** — shutdown with 50 live connections (acceptance 4: shutting-down frames all delivered, counts at zero, exit code 0); **resources** — OOM point by point (an allocation failure on the broadcast path — skip that frame, don't crash). **The tests' client side** also needs a concurrency framework — Chapter 58's executor drives 100 virtual clients (connect/subscribe/receive/reconcile) — **the test code itself is asynchronous and concurrent**, and its correctness rests on sequence reconciliation (never on timing assumptions).

### Mapping onto the book's knowledge map

pushd's bricks: Chapter 4 (error model — message-level error frames), Chapter 18 (Map — the channel-name-to-group routing), Chapter 25 (strings — channel-name validation and op comparison), Chapter 32 (JSON — the subscription protocol's codecs), Chapters 54–58 (concurrency/executor — the test-client framework), Chapter 63 (event ports — what the Engine drives underneath), Chapter 66 (TCP — the connection's transport base), Chapter 93 (HTTP upgrade — WS's handshake layer), Chapters 94–96 (the WebSocket protocol core — frames/streams/close), Chapters 103–104 (the HTTP server — the Router and middleware entrance), Chapters 109–112 (the four xws chapters — this project's direct toolbox). **Sixteen chapters each in place inside pushd** — Project 5 is currently the widest-coverage project (the static-server finale will be wider). The knowledge map's verification meaning: **no chapter's knowledge is isolated** — each has been combined in some real shape.

## Contracts

- **Endpoint security**: the `xrtWsServerRoute` fixed endpoint — Origin three-tier default (SAME_HOST_OR_ABSENT) + optional Authorize token check; Open borrows the connection, long-term holding requires Ref before storing (Chapter 112's contract).
- **Channel as group**: one `xwsgroup` per channel — membership semantics (idempotent Add / capacity refusal / stable snapshot) from the group; subscription set and membership management separate (business to the application layer).
- **Broadcast pipeline**: snapshot (stable iteration + references) → per-member reference send → snapshot destroy returning references; one payload shared by N connections (memory decoupled from fan-out).
- **Slow clients, three grades**: skip + mark slow (default) / kick (threshold) / no unbounded waiting — a single connection never drags the whole.
- **Sequence protocol**: monotone per channel — clients check the sequence and actively re-sync (the operations dividend of restart-without-loss semantics).
- **Message leniency**: a single-frame parse failure returns an error frame without disconnect; every error locatable.
- **Shutdown in seven steps**: stop accepting → Seal (idempotent) → farewell broadcast → close per connection → counts to zero → destroy groups and routes → stop the Engine — final-state reconciliation.
- **Data lifecycle**: the route Release runs exactly once after the Router and all connections exit (Chapter 112's contract).
- **Kick semantics**: GroupRemove first (leave the group), then ConnClose — after leaving, broadcasts never reach the kicked (the mechanism behind acceptance 3).

### Delivery form and the operations interface

pushd's delivery and operations design: **deployment form** — a standalone process (single-header or static-library link — Chapter 131's choice); listen address / channel capacity / slow threshold read from the command line or the environment (Chapter 42) — **the tool's configuration face** stays minimal (three parameters suffice for deployment; complex configuration is configd's domain). **Operations interface** — a management endpoint (HTTP: list channels / member counts / slow counts / kicks) hung on another path of the same Router (Chapter 104's HTTP routes coexisting with the WS endpoint — Chapter 112's "one Router, two endpoint kinds"); output JSON (Chapter 32) for scripts to consume. **Observability** — connection count / channel count / broadcast rate / slow count periodically emitted (Chapter 37's logger or direct stdout JSON Lines — Chapter 138's file_json shape); stats (Chapter 6) watches the memory watermark. **Graceful restart** — the seven shutdown steps + systemd-style supervision: across the restart window clients reconnect (Chapter 109: reconnecting is the client's duty) + the sequence re-sync backfills lost frames — **restart without losing semantics** is the sequence protocol's operations dividend.

### Re-examining the boundary against SSE push

With pushd done, Chapter 106's selection judgment deserves re-examination: when is SSE enough, when is WebSocket required? Of pushd's three requirements, **channel subscription** (the client telling the server what it wants) and **kick management** (bidirectional control) are beyond SSE — SSE can only push server-side; the client has no uplink (subscription would ride URL parameters — unrealistic with many channels); **sequenced push** itself SSE can do too (SSE also has the id field and Last-Event-ID resume — Chapter 106's mechanism). So the dividing line sharpens: **pure downlink push uses SSE** (simpler — port 80, automatic browser reconnect); **client uplink control (subscription/acknowledgment/custom heartbeats) needs WebSocket**. pushd's subscription semantics put it on the WS side — but if the requirement degrades to "broadcast the same stream to everyone" (a site-wide notice), SSE is the lighter answer. **Selection review** (looking back at the alternative after implementing one solution) is part of design ability — it makes the next selection faster and sharper.

### Stress methodology and capacity planning

Chapter 135's method landed for a long-connection service: **the stress question** ("at 1000 connections × 3 channels × 100 frames/second of broadcast, what are the P99 frame latency and memory watermark?" — write the question before building the experiment); **the load model** — virtual clients (executor-driven: connect / subscribe / receive / reconcile, four phases) + producers (timers at fixed rates) — **both ends deterministic scripts** (no real-network dependency, local loopback — Chapter 103's mock idea, stress edition); **measurement** — frame-latency distribution (a send-to-receive delay histogram — Chapter 41 timestamps), memory watermark (periodic stats sampling), slow-count trajectory; **sweeps** — a workload of connection-count gradients (100/500/1000) × a rate gradient (10/100/1000 frames per second) — the inflection point (which dimension saturates first: memory? fan-out CPU? Engine Workers?) names the scaling direction. **Capacity planning output** — a single instance's recommended load band (say "≤800 connections and ≤200 frames/second with P99 < 50ms") + actions beyond it (add instances / lower the frame rate / fragment channels).

### Retrospective: Project 5's new lessons

pushd's additions atop the twelve: **design fan-out before optimizing** (the broadcast path's reference send/snapshot/skip are design decisions, not performance patches — settle them at architecture time); **long-connection final-state reconciliation** (the cleanup verification of three state classes — connections/groups/subscription sets — can only rest on counts reaching zero — Chapters 109–111's per-layer "exactly once" converge into one reconciliation table at project level); **the sequence number is a cheap self-healing protocol** (one monotone integer buys loss detection + re-sync — two orders of magnitude cheaper than an acknowledgement-retransmission mechanism; good enough is good protocol); **the test client is itself a system under test** (a concurrent test framework's correctness cannot rest on timing assumptions — sequence reconciliation applies to the test code itself); **the operations interface is part of the product** (listing channels/kicks/slow counts are not "add later" but part of the delivery definition — Chapter 103's five timeouts likewise: operability is decided at design time). Five more, seventeen cumulative — the final project (the static-server finale) will test their resilience under **full-stack combination**.

## Pitfalls

### Pitfall 1: sending while iterating the group (no snapshot)

Symptom: joins and leaves during the broadcast loop — iteration scrambling or misses (new joiners miss this frame; the just-left get sent outside the snapshot).

Cause: the group's Count/members are instantaneous — the set changes mid-iteration. **Snapshots are the only correct stable iteration** (Chapter 111 pitfall 1): allocate outside the lock + ordered reference increments — joins/leaves during the loop never touch this snapshot.

```c bad
n = xrtWsGroupCount(pGroup);
for ( i = 0; i < n; i++ ) { 发(第 i 个成员); }   /* joins/leaves during the loop: scrambling */
```
```c good
xwsgroupsnapshot* Snap = xrtWsGroupSnapshotCreate(pGroup);
for ( i = 0; i < xrtWsGroupSnapshotCount(Snap); i++ ) {
	发送(xrtWsGroupSnapshotGet(Snap, i));
}
xrtWsGroupSnapshotDestroy(Snap);
```

### Pitfall 2: waiting on a slow connection inside the broadcast loop

Symptom: one client's TCP window fills — the whole broadcast loop stalls — every channel member's subsequent messages delayed.

Cause: treating "send" as synchronous semantics. The group's async operation object never waits for a single connection; synchronously waiting in your own loop serializes the fan-out. **Skip + mark slow** is the default; chronically degraded connections are handled by the kick policy.

```c bad
for ( each member ) {
	while ( send(conn, msg) == AGAIN ) { wait(); }  /* the slow one drags everyone */
}
```
```c good
/* the group operation object: returns once all accepted; a slow one's AGAIN belongs to its own connection backpressure */
xwsgroupop* Op = xrtWsGroupTextAsync(pGroup, msg);
xrtWsGroupOpDestroy(Op);   /* fire and go (or wait on the aggregate as needed) */
```

### Pitfall 3: destroying the group at shutdown (no Seal, connections unclosed)

Symptom: at the shutdown instant an in-flight upgrade/broadcast — access is left dangling after destruction; or with connections unclosed, Engine Destroy hangs.

Cause: shutdown is an **ordered flow**, not one call. Chapter 111's seven-step template, no step skippable — Seal blocks new members, the farewell broadcast signals, per-connection Close walks the protocol, and only counts-at-zero destroy.

```c bad
stop_listener();
xrtWsGroupDestroy(pGroup);   /* in-flight upgrades / connections unclosed: dangling and deadlock */
```
```c good
stop_listener();
xrtWsGroupSeal(pGroup);          /* 1. block new */
broadcast("shutting down");       /* 2. signal */
close_all_members(pGroup);       /* 3. protocol-driven close */
wait_close_count_zero();         /* 4. final-state count */
xrtWsGroupDestroy(pGroup);       /* 5. destroy */
xrtNetEngineDestroy(pEngine);    /* 6. stop the Engine last */
```

### The channel-capacity design trade-off

`xrtWsGroupCreate(Limit)`'s capacity parameter weighed in pushd's context: **how much to cap** — a hot channel (broadcast to all) caps at the maximum concurrent connections (guarding against anomaly storms, not normal peaks); a fine-grained channel caps small (guarding one channel from flooding); **what when full** — `GroupAdd` returns a capacity error (AGAIN-class, retryable): the subscription receipt reports "channel full" (the client backs off and retries or degrades with a summary channel); **the relation to authorization** — capacity is an undifferentiated hard gate, authorization (Authorize) an identity gate — two layers in sequence (authorize first, capacity second), semantics non-overlapping. This design point is Chapter 111's "capacity is an overload line, not a protocol error" made project-concrete: **every resource class needs an explicit rejection path**, and rejection semantics (retryable/non-retryable) bind to error classes — Chapter 4's error model, resource edition.

## Exercises

### Basic: the single-channel broadcast loop

Fixed endpoint + one group: clients join automatically on connect, the server broadcasts sequenced frames on a timer. Acceptance criteria: three clients' sequences continuous (no losses); after a drop and reconnect, rejoining works.

### Advanced: the full subscription protocol

Three operations sub/unsub/ping + channel-name validation + error-frame replies. Multiple clients cross-subscribing multiple channels. Acceptance criteria: cross-subscribed broadcasts never cross channels; illegal channel names get error frames with the connection kept; the subscription set matches group membership (reconciled).

### Challenge: the full pushd + stress

Kick/member queries/all seven graceful-shutdown steps implemented; 100 connections × 3 channels stress (sequence reconciliation + miss statistics); slow-client injection (receiving, not reading) verifying isolation. Acceptance criteria: all six acceptance criteria pass; shutdown final-state counts reconciled; stats zero leaks throughout.

### A checkpoint at the book's second-to-last chapter

With pushd done, only the final chapter remains — Project 6 (the comprehensive static file server, Chapter 144). The six projects' progression revisited: **logstat** (streaming — the data-scale constraint), **configd** (long-lived state — the lifecycle constraint), **xdl** (remote resources — the network-uncertainty constraint), **pushd** (long-connection concurrency — the fan-out and final-state constraints). Each project's acceptance checklist lengthened, each lesson replayed across shapes — the closing rhythm of a spiral curriculum. The final chapter will **stack all these constraints** (the static server = long-connection lifecycle × disk IO × protocol correctness × caching policy × security boundaries — the intersection of five dimensions) and carry the book finale's double duty: technically the full-stack synthesis, narratively the knowledge map's final gathering. After Chapter 144, you should hold a complete work that **runs, tests, operates, and extends** — and your own answer to "what did this book actually teach".

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Shape | pushd: a subscription-based push gateway — channels/sequences/bidirectional control |
| Entrance | ServerRoute fixed endpoint: Origin default + Authorize; Open requires Ref |
| Channel model | one group per channel — membership semantics free; subscription sets managed separately at the application layer |
| Broadcast pipeline | snapshot → reference send → snapshot destroy; one payload for N connections |
| Slow clients | skip + mark slow / kick / no unbounded waiting — isolation is the acceptance |
| Sequence | increasing per channel — clients check losses and re-sync |
| Message leniency | a single bad frame gets an error, no disconnect — errors locatable |
| Shutdown, seven steps | stop accepting → Seal (idempotent) → farewell broadcast → close per connection → counts to zero → destroy groups and routes → stop the Engine |
| New lessons | design fan-out before optimizing; long-connection cleanup by final-state count reconciliation; the sequence is cheap self-healing |
| Kick | Remove first, then Close — after leaving, broadcasts never reach |
