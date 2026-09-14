---
num: 142
slug: project-chat
title: The Chat Service: Implementation
volume: 卷十三 实战项目
type: project
lead: Landing the room and user entities, dispatching the line+JSON wire protocol, sequence numbering and ring catch-up, final-state wrap-up and the stress audit — six decisions cashed into code and eight acceptance criteria reconciled.
api: value, net, core
---

## Orientation

Chapter 141 settled six design decisions (room model / wire protocol / ordering / state machine / storage / degradation); this is chatd's **implementation chapter** — the same methodology as Chapter 140 (configd's implementation): **decisions land row by row, acceptance reconciles row by row, revisions leave traces**. The implementation's terrain: **the entity layer** (three structs — user/room/message ring — and their containers — Decision 1 landed); **the protocol layer** (line-delimited + JSON frame receive/dispatch/send — Decision 2); **the session layer** (the handshake→join→speak→leave state machine — Decision 4); **ordering and catch-up** (number-taking/broadcast/cursor/ring wraparound — Decisions 3+5); **wrap-up and degradation** (final-state guarantees/kick/refusal — Decisions 4+6). The chapter's capstone is the **stress audit** — the machine form of acceptance 8, a 100-virtual-user × 10-room proof of room-wide sequence consistency — the design chapter's "no sequence gaps" semantics audited frame by frame under concurrency.

## Introduction

Implementation starts from the **frame dictionary** (not from sockets) — Chapter 141 exercise 4's frame-type dictionary is the protocol authority: seven uplink types (handshake/join/leave/say/dm/who/quit) and nine downlink types (ok/err/broadcast/dm/history_tail/too_far/you_left/member_list/unknown). **Writing the dictionary before the dispatcher** makes the protocol consultable from day one — every `on_frame` branch corresponds to a dictionary row, review reads them side by side. **The entity skeleton precedes the network**: the user/room structs and container operations (join/remove/find) are pure data logic — unit-testable without any socket (Chapter 133's layered-testing dividend); the network layer (Engine/Listener/Stream) attaches last — by then the business logic is already test-protected. This order matches configd's "bottom-up" — **pure data layers first** is the shared rhythm of both implementation chapters.

## Concepts

### The implementation's module map

chatd's layered map (isomorphic with Chapter 140's boundary map):

```diagram flow
- Entity layer (~150 hand-written lines): three structs + container operations (join/remove/find/ring management)
- Protocol layer (~120 lines): frame-receive engine + op dispatch + frame generation (JSON Writer)
- Session layer (~180 lines): the seven handlers handshake/join/leave/say/dm/who/quit
- Ordering layer (~60 lines): number-taking/broadcast/catch-up/cursor
- Network layer (0 hand-written lines): Engine/Listener/Stream - Chapters 64/67 own it all
  about 510 hand-written lines total - zero self-written network stack
```

Five hundred lines implement a chat service with ordering guarantees and a catch-up protocol — cross-checked with configd (280 lines) and webserv (under 400): **an XRT project's code volume is proportional to business complexity** (chatd's entities + protocol + ordering are three real business blocks) — the system complexity of networking and concurrency is fully absorbed by the library layers. This number is also the last group of evidence for Chapter 145's "assembly, not writing".

### The entity layer: three structs and containers

```c
struct chuser {
	char Name[32];            /* identity (independent of the connection - Decision 1) */
	xnetstream* Stream;        /* nullable = offline */
	uint64 Cursor;            /* largest sequence number seen */
	struct chroom* Room;      /* current room (nullable = lobby) */
};
struct chroom {
	char Name[32];
	uint64 NextSeq;           /* ordering counter (Decision 3) */
	struct chmsg Ring[1024];  /* message ring (Decision 5) */
	size_t RingHead;          /* oldest position */
	void* Members;            /* member table (Chapter 18 map) */
};
struct chmsg {
	uint64 Seq;
	char From[32];
	char Text[256];
};
```

**Container selection**: the member table `成员表 map` (lookup by name — the hot path of who/join, Chapter 18); the room table one global map (the same family). **The ring's implementation**: an array + head pointer (`Ring[RingHead]` is the oldest) — on overwrite the head advances; catch-up replays in order from cursor to ring tail. **These three structs are chatd's entire state** — no hidden copies anywhere (SetData on the connection hangs the user pointer — Chapter 67's context mechanism).

### The protocol layer's frame-generation side

Beyond receiving frames there is sending them (nine downlink types) — the generation form: the JSON Writer (Chapter 32) producing one line per frame. **Frame template functions** (one small function per type — broadcast/dm/err/ok share a skeleton): fill op, fill payload fields, (broadcast family) fill seq, Writer Finish produces legal JSON — **atomicity guaranteed by the Writer** (no half frame leaves the door). **The send path**: frame text → `xrtNetStreamSend` (line-buffered + `\n`) — symmetric with the receive side's line delimiting (**both ends of one protocol use the same pair of primitives**). **The error frame's unified format**: `{"op":"err","code":N,"why":"..."}` — code is an enum (BADJSON/NOOP/NICK_TAKEN/ROOM_FULL/NOT_IN_ROOM/...) and why is for humans — **machines judge by code, humans read why** (Chapter 4's error model, frame edition).

### The protocol layer: frame receive and dispatch

The receive engine = the line framer (Chapter 72) + JSON parsing (Chapter 32):

```c
/* the connection readable event: line arrives -> JSON -> dispatch by op */
static void chat_on_data(xnetstream* Stream, ptr Ctx)
{
	chuser* U = ctx_user(Ctx);
	xlineview Line;
	while ( xrtLineReaderNext(U->Lines, &Line) ==
		XLINE_NEXT_LINE ) {
		xvalue* Msg = NULL;
		xvalue* Op = NULL;
		xstrview Op;
		if ( (Msg = chat_parse_json(Line)) == NULL ) {
			chat_send_err(U, CH_E_BADJSON);
			continue;            /* a bad frame gets an error, no disconnect */
		}
		Op = xrtValueObjectGet(Msg, XRT_STR_LITERAL("op"));
		if ( (Op == NULL) ||
			!xrtValueGetString(Op, &Op) ) {
			chat_send_err(U, CH_E_NOOP);
		} else if ( xrtStrEqual(Op, XRT_STR_LITERAL("hello")) ) {
			chat_do_hello(U, Msg);
		} else if ( xrtStrEqual(Op, XRT_STR_LITERAL("join")) ) {
			chat_do_join(U, Msg);
		} /* ... say/dm/who/quit isomorphic ... */
		xrtValueRelease(Msg);
	}
}
```

**The lenient/strict boundary** (Chapter 141's protocol decision): the line layer lenient (a bad line never affects the connection), the JSON layer half-lenient (bad JSON gets an error but no disconnect), **the business layer strict** (nickname conflict / full room refused — but by protocol frame, not disconnect). **The dispatch shape** is isomorphic with pushd's on_message — the two projects share one pattern at the protocol layer (learn once, use twice — the project sequence's compounding).

### Ordering and broadcast: take-number-deliver-cursor

```c
static void chat_do_say(chuser* U, xvalue* Msg)
{
	chroom* R = U->Room;
	chmsg* M;
	if ( R == NULL ) { chat_send_err(U, CH_E_NOT_IN_ROOM); return; }
	/* take a number: single-point ordering inside the lock (Decision 3) */
	lock(R);
	M = &R->Ring[R->RingTail];
	M->Seq = ++R->NextSeq;
	copy_from(M, U->Name, Msg);
	R->RingTail = (R->RingTail + 1) % 1024;
	unlock(R);
	/* deliver: broadcast with the number + advance the speaker cursor */
	chat_broadcast(R, M);
	U->Cursor = M->Seq;
}
```

**Broadcast's ordering guarantee**: frames carry `Seq` — members' cursors advance on receipt; **the catch-up path** (reconnect): `hello` carries `cursor` → within the room, replay from `cursor+1` to the ring tail → after everything is resent, reply `ok`; a cursor older than the ring head → `too_far` (the client refreshes wholesale). **Ring wraparound's** sequence continuity: overwriting the oldest frame never stops `NextSeq` from being monotone — the catch-up window = the ring capacity (drops within 1024 frames are recoverable — the design capacity assumption cashed).

### Three transition details of the state machine

The three transitions most easily fumbled when implementing the state machine: **handshake failure → closed** (after the rejection frame, actively Close — not waiting for the client to drop: the server's explicit attitude toward protocol errors; draining the send queue before close is guaranteed by Stream semantics — Chapter 67); **leave then rejoin the same room** (the user entity is reused — the cursor continues (Decision 1 cashed again: leaving does not clear the cursor — rejoining continues from the history tail)); **the quit frame and a TCP drop handled equivalently** (graceful exit and abnormal disconnect walk the same cleanup path — differing only in whether you_left is sent — **one cleanup path, two entrances** is the standard technique against state forking).

### The session state machine and final states

Handshake (nickname legal and unique → register the user; conflict → `err` + close): **join/leave's order sensitivity** (Chapter 141 pitfall 3, implemented): remove the member first → send the person `you_left` (the last frame) → the room broadcasts `member_left`. **Connection drop** (the TCP-layer Close event): the user enters the offline state (`Stream=NULL`, cursor kept) — **not destroyed** (the resource of reconnect catch-up); only the offline timeout (configurable) truly cleans up. **Server kick** (slow member — Decision 6): sends persistently AGAIN over a threshold → kick frame → Close — the no-gap semantics forbids the "skip" grade (the key difference from pushd — the implementation face of Chapter 141's division table).

### The stress test's execution details

The 100-double × 10-room stress run's concrete shape on the implementation side: **doubles driven in-process** (Chapter 59's executor with 100 tasks — each double a state-machine loop: random actions (speak 70% / listen 20% / dm 10%) + frame intervals (a Poisson distribution simulating human rhythm — Chapter 11's randomness) + the auditor (verify `Seq == Cursor+1` on receipt)); **the server in the same process or a subprocess** (local loopback — real network, controllable); **the injector** (designated doubles drop/slow at designated frame numbers — fault injection invisible to the server); **the verdict** (after 10 minutes: all audits zero violations + catch-up joins correctly + the slow-kick count matches the injections + stats zero leaks). **This topology's credibility argument**: the loopback-TCP-versus-real-network difference (latency/loss) does not affect **correctness verification** (the sequence audit is a logical property, not a timing property) — performance conclusions need real networks (that is post-deployment — Chapter 136's baseline comparison). **Correctness stress in CI, performance stress in the environment** — the two measurement classes find their separate homes.

### Retrospective: the implementation chapter's complete state

With chatd implemented, the project sequence's pedagogical intent closes here (webserv already did the whole-book finale in Chapter 145 — this chapter fills the sequence's missing link): **the six projects' constraint ladder** fully presented — data scale (logstat) → lifecycle (configd) → connection identity and ordering (chatd) → remote uncertainty (xdl) → fan-out and final states (pushd) → full-stack synthesis (webserv). **chatd's unique contribution** is "ordering" — the other projects' data is either unordered (tools), single-writer (config), or losable (push) — only chat makes **room-wide consistent ordering of many-party concurrent writes** its correctness core — it is the closest project to "database" complexity (the single-point orderer = a miniature WAL, the ring = a circular log, catch-up = log replay). **The twenty engineering common-sense rules** checked in chatd: all applicable, two (final-state reconciliation / design fan-out first) take the lead again — the checklist's stability is established on its sixth replay.

### The stress audit: acceptance 8's machine form

**The auditor's position**: every virtual client (Chapter 141's double design) carries its own check on receipt — Seq == Cursor+1 (strictly increasing frame by frame) — a violation reports (which room, where the gap opened). **The stress topology**: 100 doubles across 10 rooms, random speech for 10 minutes, 10 disconnect points injected, 5 slow clients. **The audit output**: every room's sequence chain complete (gapless), cursors joined after catch-up, and after slow clients are kicked the rest of the room unaffected. **This audit is the design chapter's "no gaps" semantics in its ultimate form** — not analysis but **enumerative runtime checking** (every frame verified) — Chapter 133's testing philosophy applied concurrently.

### The reconciliation with the design chapter

The eight acceptance criteria one by one: **(1) six UCs, positive and negative** (the frame dictionary + dispatch layer's cases all green); **(2) ordering consistent** (the stress audit — room-wide monotone); **(3) catch-up correct** (disconnect-injection tests: cursors join after reconnect, no duplicates, no losses); **(4) final states clean** (ghost-frame tests after leave/disconnect — zero ghosts); **(5) slow members isolated** (slow injection: other members' sequences unaffected); **(6) full-room/nickname refusal** (capacity tests: rejection frames carry reasons); **(7) OOM zero residue** (point by point: any failure leaves user/room state unchanged); **(8) stress stable** (the 100×10-minute audit all green). **All eight pass — chatd is deliverable**.

## Examples

### First complete program: the receive engine's atom

The program below is from `examples/io/line` — the teaching atom of chatd's frame-receive engine (the line-delimiting layer):

```embed path="examples/io/line/main.c" title="examples/io/line/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/io/line/main.c -lws2_32 -liphlpapi
1: INFO server started
2: WARN queue is busy
3: ERROR request failed
```

**What just happened.** (1) The two steps `ReaderFromMemory→LineReaderTake` and the `Next` three states — chatd's `chat_on_data` body (swap the Reader for the connection's receive buffer — Chapter 67's Stream data event is the Reader's network form). (2) View borrowing (each line used as it arrives) — released after dispatch, never cached — Chapter 141's "frames never overnight" discipline. (3) This atom recurs across three projects (137/139/141) — **foundation atoms reused is layered design's direct yield**.

### Second complete program: broadcast frame generation

The second program is from `examples/logging/file_json` — the reference for JSON frame generation (one JSON object per line as the output form):

```embed path="examples/logging/file_json/main.c" title="examples/logging/file_json/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/logging/file_json/main.c -lws2_32 -liphlpapi
wrote example_logger_json.log
```

**What just happened.** (1) The JSON-Lines output form (one complete object per line) is **isomorphic** with chatd's downlink frames — broadcast/dm/err, one JSON line per frame. (2) Logging's fields mechanism (structured fields riding the record) corresponds to chat frames' payload fields — **the same serialization need in different systems** (logs are frames for machines; chat is frames for clients). (3) chatd's frame generation uses Chapter 32's Writer to produce JSON directly (`{"op":"broadcast","seq":N,...}`) — this sample is its logging-side cousin.

### The revision record against the design chapter

One revision and two refinements to Chapter 141's design during implementation (the revisable-decisions clause cashed): **revision** — catch-up's lock policy moves from "hold the room lock during catch-up" to "snapshot inside the lock, replay outside" (the design had not refined the lock scope — implementation found that replaying under the lock would block the whole room's number-taking — the snapshot pattern gets both); **refinement one** — the slow-member threshold refines from "persistently AGAIN" to "N consecutive deliveries AGAIN plus a timeout T" (configurable quantification — a product of stress calibration); **refinement two** — the offline timeout refines from "configurable" to a 300-second default (the reconnect window's product assumption — a reasonable IM-convention default). **None of the three touches the core decisions** (room model / ordering / no gaps) — the design's peripheral parameters get calibrated by reality during implementation — exactly the value of "decisions and parameters layered": the core stable, the periphery adjustable.

### Implementation walkthrough: the catch-up protocol's source

Catch-up (reconnect cursor joining) is chatd's subtlest path — key segment walked:

```c
/* reconnect handshake: with cursor -> snapshot replay -> back to live */
static void chat_do_resume(chuser* U, chroom* R, uint64 Cursor)
{
	chmsg Pending[1024];
	size_t n = 0;

	if ( R == NULL ) { chat_send_err(U, CH_E_NOT_IN_ROOM); return; }
	lock(R);
	if ( (R->NextSeq - Cursor) > 1024u ) {
		unlock(R);
		chat_send(U, "{"op":"too_far"}");
		return;                 /* older than the ring head: wholesale refresh */
	}
	/* snapshot inside the lock: copy frames Cursor+1..NextSeq out */
	{
		uint64 s;
		for ( s = Cursor + 1u; s <= R->NextSeq; s++ ) {
			Pending[n++] = ring_at(R, s);
		}
	}
	unlock(R);                  /* release the lock once the snapshot is done */
	/* replay outside the lock: a slow client never blocks the room */
	{
		size_t i;
		for ( i = 0; i < n; i++ ) {
			chat_send_frame(U, &Pending[i]);
		}
	}
	U->Cursor = R->NextSeq;    /* cursor joins up */
	chat_send_ok(U);
}
```

**Walkthrough points.** (1) `(NextSeq - Cursor) > 1024`'s **overflow safety**: NextSeq only grows, Cursor ≤ NextSeq always holds (a cursor only ever points at frames seen) — the difference is non-negative. (2) The snapshot array sits **beyond the stack limit** (1024 × 300 bytes ≈ 300 KB — heap-allocated or replayed in batches) — the teaching form sketches only the logic; a real implementation replays in batches (64 frames per batch to avoid stack/memory spikes). (3) The cursor advances **after** the replay — another drop mid-catch-up restarts catch-up from the top next time (idempotent — catch-up has no side effects). (4) `ring_at(R, s)` fetches a frame by sequence number (random ring access — circular index arithmetic).

### The test-asset checklist

chatd's four-quadrant tests (the delivery definition isomorphic with Chapter 140's): **positive** (test_chat.c — each of the six UCs' positives + one per each of the frame dictionary's sixteen types); **race** (test_chat_threads.c — concurrent-speech ordering audit (multi-threaded 1000 frames, then room-wide sequence chains monotone) + catch-up interleaving (speech continuing through a disconnect/reconnect) + ghost-frame checks (zero frames after leave)); **negative** (nickname conflict / full room / bad JSON / unknown op / missing fields — each returns err with the connection kept); **OOM** (test_chat_oom.c — entity operations point by point: on failure, room/user state unchanged). **The double library** (test_chat_client.c — virtual-user driver + built-in sequence auditor) shares protocol code with the system under test — no second implementation. All hung on the manifest's tests field — `build.py --suite chatd` on both tracks.

## Contracts

- **Three entity structs**: user (identity + nullable connection + cursor) / room (orderer + ring + member table) / message (sequence number + sender + text) — all state, no copies.
- **Containers**: member table and room table as maps (lookup by name) — Chapter 18.
- **The receive engine**: line delimiting (three-state loop) + JSON (ObjectGet fetches op) — the lenient/strict boundary per Chapter 141.
- **Dispatch shape**: string routing by op — isomorphic with pushd (pattern reuse).
- **Number-taking**: `++NextSeq` inside the lock, single-point ordering; frames carry Seq; member cursors advance on receipt.
- **Catch-up**: hello with cursor → snapshot inside the lock, replay outside; older than the ring head → too_far wholesale refresh.
- **Ring semantics**: 1024 frames/room; overwrites the oldest but numbers stay monotone; the catch-up window = the ring capacity (drops beyond the window are too_far).
- **join/leave order**: remove first → you_left to the person → room broadcast — zero ghost frames.
- **Disconnect semantics**: enter the offline state (cursor kept); cleanup only on timeout — identity outlives the connection.
- **Slow members**: kicked (no skip grade) — the no-gap semantics forbids skipping.

### Delivery form and operations

chatd's delivery (the service form shared with pushd/webserv): **a standalone process** (static-library or single-header link — listen address / room capacity / ring length / slow threshold / offline timeout read from the environment — five parameters cover the whole tunable face); **the operations interface** (management TCP commands or local signals: room list / member counts / sequence watermarks / connection counts — the minimal form of an ops endpoint); **observability** (per-room sequence rate / connection count / kick count periodically emitted — JSON Lines into the same log stream); **restart semantics** (an in-memory service — restart loses rooms (the teaching boundary); clients reconnect + wholesale refresh to recover — **stateless clients are the source of restart tolerance**). Against pushd's operations face: isomorphic (counts + lists + kicks) — **service projects' operations face converges into one pattern** — Chapter 144's lesson cashed once more.

### The ring capacity's product trade-off

The ring length 1024's product semantics deserve a final look: it decides **the catch-up window** (a drop beyond 1024 frames is too_far) — an active room (100 msg/s) has a window of only 10 seconds — weak-network users refresh wholesale constantly. **The adjustment space**: ring length ↑ (memory ↑: 300B per frame × 1024 = 300KB/room — a 10× ring = 3MB/room × 100 rooms = 300MB — infeasible); **history on disk** (Decision 5's evolution point: ring + disk two layers — catch-up hits memory first, then disk — the capacity problem becomes an IO problem); **snapshot + delta** (on wholesale refresh, send a compressed snapshot instead of frame-by-frame — refresh cost drops). The shared lesson of the three directions: **the memory ring is the right choice for the teaching form and the starting point, not the endpoint, of a production form** — the boundary declaration's (Chapter 141) honesty cashed here: the design said plainly from the start that it is not the production answer.

## Pitfalls

### Pitfall 1: broadcasting inside the lock

Symptom: one slow client's send blocks — the whole room stalls (sending one-by-one before releasing the lock).

Cause: confusing the lock scopes of "take a number" and "deliver". The lock protects only **number-taking** (mutating the counter and ring); delivery (network sends) happens outside — a slow send never blocks others' number-taking.

```c bad
lock(R);
seq = ++R->NextSeq;
store(R, seq, msg);
broadcast(R, seq, msg);   /* sending inside the lock: the slow one drags everyone */
unlock(R);
```
```c good
lock(R);
seq = ++R->NextSeq;
store(R, seq, msg);
unlock(R);
broadcast(R, seq, msg);   /* sending outside the lock: number-taking never blocked */
```

### Pitfall 2: catch-up racing live broadcast (new frames mid-catch-up)

Symptom: reconnect catch-up is in progress while new speech broadcasts also arrive — the client's frames scramble (old caught-up frames interleave with new).

Cause: catch-up replays history while broadcast pushes new frames — the two run concurrently without coordination. The correct order: **catch-up snapshots inside the lock and replays** (or buffers new frames until catch-up completes) — guaranteeing everything caught up precedes everything new.

```c bad
replay_from(cursor);       /* catch-up begins */
/* new broadcasts arriving simultaneously - interleaved */
```
```c good
lock(R);
snapshot_ring(R, cursor, &pending);   /* snapshot the pending frames inside the lock */
unlock(R);
send_pending(U, &pending);            /* catch-up done */
resume_live(U);                        /* then receive live */
```

### Pitfall 3: destroying the user struct on disconnect

Symptom: after a user drops, the room's member table dereferences a dangling pointer — crash; or on reconnect the old identity is missing (the cursor lost).

Cause: treating disconnect as destruction. Disconnect is **going offline** (connection resources released, identity kept) — destruction waits for the offline timeout. Chapter 141's "offline state" corresponds in implementation to **not destroying**.

```c bad
on_close(U) { destroy_user(U); }    /* destroy on disconnect: cursor/room ties all lost */
```
```c good
on_close(U) {
	U->Stream = NULL;    /* offline: connection null, identity and cursor kept */
	schedule_timeout(U);  /* real cleanup only on timeout */
}
```

### Implementation walkthrough: the complete ordering-broadcast chain

The complete chain of a say frame from arrival to room-wide receipt (the broadcast side of the say handler, supplemented):

```c
/* deliver: the whole room (outside the lock) + each cursor advances */
static void chat_broadcast(chroom* R, const chmsg* M)
{
	size_t n = member_snapshot(R, NULL, 0);   /* count members */
	chuser** Members = malloc(n * sizeof(void*));
	if ( Members == NULL ) { return; }        /* OOM: the frame is already in the ring -
		reconnect catch-up backstops (numbers never lost) */
	member_snapshot(R, Members, n);           /* stable snapshot */
	for ( size_t i = 0; i < n; i++ ) {
		chuser* U = Members[i];
		if ( (U->Stream == NULL) ||
			!chat_send_frame(U, M) ) {
			/* offline: cursor + catch-up cover it; online send failure: mark slow */
			mark_slow(U);
			continue;
		}
		U->Cursor = M->Seq;    /* the receiver cursor advances */
	}
	free(Members);
}
```

**Walkthrough points.** (1) The member snapshot is two-phase (count then fetch — a miniature of Chapter 112's pattern: snapshot the room's member table under lock). (2) **OOM's backstop semantics**: the frame is already in the ring (do_say stores before broadcasting) — a send-side failure loses no data (reconnect catch-up replays from the ring) — **the ring is broadcast's persistence layer**. (3) The send failure's `mark_slow`: the slow count accumulates to the threshold and triggers the kick (Decision 6 quantified). (4) The broadcast holds no room lock (pitfall 1's positive form).

## Exercises

### Basic: implementing the frame dictionary

Implement Chapter 141 exercise 4's frame dictionary as a dispatch function (one branch per sixteen types) + positive and negative cases per type. Acceptance criteria: negatives all return err/unknown without disconnecting; missing-field errors are locatable.

### Advanced: the catch-up protocol test

A disconnect injector (doubles drop after frame N) + reconnect with cursor: verify catch-up frame order, the too_far boundary (drops beyond 1024 frames), and correct buffering of speech during catch-up. Acceptance criteria: all three cases correct — the machine proof of acceptance 3.

### Challenge: the complete chatd + the stress audit

Implement all six UCs + the frame dictionary + ordering + catch-up + final states + the slow kick; 100 doubles × 10 rooms × 10 minutes of stress (the auditor verifies monotone frame by frame). Acceptance criteria: all eight acceptance criteria pass; the audit zero gaps zero ghosts; stats zero leaks (acceptance 7).

### The one-page acceptance sheet (eight items)

The same one-page reconciliation as Chapter 140: (1) six-UC cases all green (protocol-layer dispatch tests); (2) ordering consistent (stress audit zero violations); (3) catch-up correct (disconnect-injection three-state tests); (4) final states clean (zero ghost frames); (5) slow members isolated (slow injection, others unaware); (6) full-room/nickname refusal (capacity and conflict tests); (7) OOM zero residue (entities point by point); (8) stress stable (the 100×10-minute audit). **All eight ticked, each with its evidencing section** — the reconciliation page to paste into a PR.

### The closing position in the whole book

With chatd's implementation chapter filled in, Volume 13's eight sub-chapters (137–144) are all in place — the six-project sequence complete: every chapter follows the structure "acceptance first → selection/decisions → implementation walkthrough → reconciliation → retrospective", every chapter's lesson list grows and replays in the next. **Along this sequence the reader has walked six system constraints** (scale / lifecycle / identity & ordering / remote / fan-out / full stack) — each constraint genuinely handled in at least one project. That is Volume 13's complete state as the book's closing volume — not the endpoint of knowledge but **the starting point of the ability to combine it**.

### A supplement to the data-structure quick reference

The three structs' field-semantics table (for implementation-time lookup): **user** (Name the identity key / Stream the nullable connection = offline / Cursor the last-seen sequence / Room the current-room pointer); **room** (Name the room key / NextSeq the ordering counter, only growing / Ring the circular array of 1024 / RingHead the oldest position / Members the member-table handle); **message** (Seq the in-room sequence / From the speaker name / Text the body). Three invariants: Cursor ≤ same room's NextSeq (receipt never runs ahead); Seq within the ring is continuous (overwriting starts at the oldest — continuity is promised only within the window); the member table's key = the user name (a duplicate name is the same user — nickname global uniqueness is the handshake's precondition).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Implementation rhythm | frame dictionary → entities → protocol → session → stress (pure data first) |
| Three entity structs | user (nullable connection + cursor) / room (ordering + ring + member table) / message |
| Frame receive | line three-state loop + JSON op routing — isomorphic reuse with pushd's dispatch |
| Number-taking | ++NextSeq inside the lock; frames carry numbers; delivery outside |
| Catch-up | replay from cursor+1; too_far wholesale refresh; snapshot inside the lock prevents interleaving |
| Ring semantics | 1024 frames/room; overwrites the oldest but numbers stay monotone; catch-up window = ring capacity |
| join/leave order | remove first → you_left to the person → room broadcast — zero ghost frames |
| Disconnect semantics | enter the offline state (not destroyed); real cleanup only on offline timeout |
| Slow members | kicked (no skip grade) — the no-gap semantics forbids skipping |
| Stress audit | doubles verify Seq==Cursor+1 frame by frame — runtime enumerative proof of concurrent correctness |
