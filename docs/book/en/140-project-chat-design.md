---
num: 140
slug: project-chat-design
title: The Chat Service: Design
volume: 卷十三 实战项目
type: concept
lead: The room model and message sequence numbers, the TCP wire-protocol choice, broadcast-consistency semantics, the connection-lifecycle state machine, capacity and degradation — the last design exercise of concurrent networking.
api: core, value, net
---

## Orientation

Project 3 is a **TCP chat room** (chatd): multi-room real-time chat — users connect, join rooms, speak, rooms broadcast, private messages, offline sequence catch-up. In design it stands between configd (Chapters 138–139: long-lived state and concurrent handover) and pushd (Chapter 143: fan-out and final states) — the purest sample of **connection state × room state × message ordering** interwoven. This chapter reuses Chapter 138's **five-element decision template** (problem/options/decision/rationale/verification) — design-methodology reuse precedes code reuse. Six design domains: **the room model** (the shape of the room-member relation); **the wire protocol** (message delimiting over a TCP byte stream — the selection of Chapters 71/90 replayed); **broadcast consistency** (sequence numbers and catch-up — the prequel of pushd's sequence protocol); **the connection state machine** (the handshake→join→chat→leave lifecycle); **the storage boundary** (the split between online memory and offline history); **capacity and degradation** (the policy matrix for slow members and full rooms).

## Introduction

Chat is the classic "looks simple" trap: isn't it just "forward each message to everyone in the room"? Behind every "simple" of a real system sit decisions: **A speaks one second before B joins — should B see it** (join-time semantics: the history-visibility boundary); **between A's message and B's receipt, C leaves — how does C's session close** (session final state); **two people speak in the same second — what order does the room see** (ordering authority: who decides the order); **B reconnects after a drop — what about the missed messages** (the catch-up protocol: the sequence cursor). Answer any of these wrong and it becomes "messages lost", "order scrambled", "ghost sessions" at the product layer — yet all of them are **design-time questions**, not coding-time questions. This chapter turns each into an explicit decision — the implementation chapter (Chapter 141) cashes them one by one.

## Concepts

### Requirements and acceptance criteria

User stories: **UC1 connect & handshake** (TCP connects → a handshake frame carrying the nickname → server confirms/rejects); **UC2 join/leave** (join/leave frames — after joining, a history tail arrives); **UC3 speak & broadcast** (a say frame → everyone in the room receives a numbered broadcast frame); **UC4 private message** (a dm frame → only the target user receives it); **UC5 reconnect catch-up** (reconnect carrying the last sequence number → the server resends the missing frames); **UC6 online list** (a who frame → the room's member list). **Acceptance**: (1) six UCs with positive and negative cases; (2) broadcast ordering: a room-wide consistent order for concurrent speech (the numbers every member of a room sees strictly monotone); (3) catch-up correct: frames from the drop window all arrive after reconnect, without duplicates; (4) session final state: no ghost frames after leave/disconnect (the closing frame arrives exactly once); (5) a slow member never drags the room (isolation); (6) explicit rejection paths for a full room and nickname conflicts; (7) OOM and per-point failure with zero residue; (8) a 100-concurrent × 10-room load testing run stable.

### Decision 1: the room model

**Problem**: what relates rooms to members, members to connections? **Options**: A a room = a set of connections (manages connections directly); B a room = a set of users, users independent of connections (reconnect swaps the connection, not the identity); C no room entity (messages dispatched by a routing table). **Decision: B**. Rationale: **reconnect semantics** (UC5 requires keeping identity and sequence cursor across reconnects — identity cannot bind to the connection); **private-message routing** (UC4 routes by user, not connection); **offline history** (UC5's catch-up target is the user, not the connection). A rejected: connection drops = identity lost — reconnect becomes a new user (catch-up impossible). C rejected: the room is a product concept (history/member lists are room attributes) — a bodiless routing table cannot hold them. **Verification**: the reconnect test (UC5) passes only under B. **Data shape**: `Map<房间名, 房间>` (room name → room); a room holds `Map<用户名, 用户>` (user name → user) + a message ring (the history tail) + a sequence counter; a user holds the current connection pointer (nullable = offline) + the cursor.

### Decision 2: the wire protocol

**Problem**: how are messages delimited on a TCP byte stream? **Options**: A bare text lines (`\n`-separated); B length-prefixed binary; C JSON lines (text lines + a JSON payload). **Decision: C**. Rationale: **human-readable** (teaching and debugging — telnet can talk directly); **structured** (frame type/fields via JSON — Chapter 32 parses in one step); **extensible** (new fields, zero protocol changes). A rejected: fields need hand-rolled escaping (a nickname with a space breaks). B rejected: debugging needs hexdumps; encoding overhead buys nothing here. **Delimiting's implementation**: Chapter 71's line framer (`\n` delimiting) + Chapter 32's JSON parsing — **both layers are ready-made main subjects**. **Verification**: a protocol-matrix test (each frame type's positive and negative cases). This decision is also pushd's mirror (there it chose JSON over WS frames — here JSON over lines — **the payload structure identical, the delimiting layer follows the transport** — layering turns protocol design into a single-layer choice).

### Decision 3: broadcast ordering and consistency

**Problem**: who orders concurrent speech? **Options**: A sender-side ordering (local sequence numbers); B single-point server ordering (a room counter); C vector clocks (distributed ordering). **Decision: B**. Rationale: a room is a single-process **total-order domain** (one ordering point per room) — simple and sufficient; C is for distribution (meaningful only with multiple instances — beyond this design). **Mechanism**: a say frame arrives → take a number under the room lock (counter++) → the broadcast frame carries the number → deliver by number. **Consistency semantics**: the numbers every member of a room sees are **strictly monotone with no gaps** (a gap = loss — triggers catch-up); a member's cursor records the largest number seen. **The catch-up protocol** (UC5): the reconnect frame carries the cursor → the server resends from cursor+1 within the history ring → beyond the ring (too old) returns "too far behind" (the client refreshes wholesale). **Verification**: sequence audits in the stress test (every client verifies monotone-no-gaps — acceptance 2/3).

### Decision 4: the connection state machine

```diagram state
connection established -> handshaking: TCP accept
handshaking -> online: handshake frame (nickname legal and unique)
handshaking -> closed: rejection frame (nickname conflict/illegal - closed after stating the reason)
online -> online: join/leave/say/dm/who any number of times
online -> closed: quit frame / TCP drop / server kick
closed -> (user offline): connection resources released; user identity kept (cursor and history)
(user offline) -> online: reconnect handshake (with cursor -> catch-up)
```

**Final-state guarantees**: a connection's Close exactly once (Chapter 66); a user identity's lifecycle is **longer** than the connection's (offline counting / timeout cleanup — Decision 1's corollary). **The ghost-frame defense**: the leave frame reaches the person after the broadcast queue drains ("you have left" is the last frame) — later broadcasts never reach (the remove-member-before-broadcast order).

### Decision 5: the storage boundary

**Problem**: where are messages stored, for how long? **Options**: A all memory (a ring buffer); B all on disk; C hot in memory + cold on disk. **Decision: A (a memory ring) + an explicit boundary declaration**. Rationale: the teaching form's capacity assumption (a 1024-frame ring per room — about 100 KB per room); **B/C are evolution points, not this design** (disk = Chapter 44 append-write + startup replay — the extension path stays open). **The ring's semantics**: new frames overwrite the oldest; catch-up reaches only within the ring (outside = too far behind). **Verification**: a ring-wraparound test (send beyond the ring — the oldest overwritten, numbers still monotone).

### Decision 6: capacity and degradation

**Problem**: what about slow members, full rooms, server overload? **The decision matrix**: a slow member (send AGAIN persistently over a threshold) → **kick with notice** (not unbounded waiting — pushd's three grades, chat edition: chat semantics cannot drop frames — dropping breaks the no-gap invariant — so skipping is unavailable, only kicking); a full room (member cap) → reject the join frame (with an explicit reason); overload (a total-connection cap) → reject the handshake. **Every rejection has a protocol frame** (clients can understand and retry) — rejection paths are first-class citizens. **Verification**: a slow-member injection test (isolation + kick semantics — acceptance 5).

### The division of labor against pushd (Chapter 143)

chatd and pushd look alike (rooms/broadcast/sequence numbers) but differ in constraints — a comparison table prevents confusion: **transport** (bare TCP versus WebSocket — chatd builds its own handshake/delimiting, pushd gets them free); **identity** (user independent of connection versus connection-as-identity — chatd's reconnect catch-up forces the user entity); **frame-loss tolerance** (intolerable — no sequence gaps — versus losable with resync — pushd's skip grade is unavailable in chatd). **Conclusion**: chatd teaches "a protocol built up from bytes", pushd teaches "reusing high-level parts" — **the same business replayed on two transports** — a training in transport-sensitivity of the design view.

## Examples

### First complete program: the wire protocol's delimiting atom

The program below is from `examples/io/line` — the runnable atom of the wire protocol's delimiting layer (`\n` line frames):

```embed path="examples/io/line/main.c" title="examples/io/line/main.c"

```
```
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/io/line/main.c -lws2_32 -liphlpapi
1: INFO server started
2: WARN queue is busy
3: ERROR request failed
```

**What just happened.** (1) The line-delimiting three-state loop is exactly chatd's **frame-receiving engine**: LINE arrives → JSON parse → dispatch; END → connection wrap-up; ERROR → structured disconnect. (2) EOL adaptation (all three EOLs recognized) lets telnet/nc and all sorts of clients connect — **a lenient entrance** (the protocol's lowest layer is lenient, the JSON layer above is strict — each layer at its post). (3) This atom is the same one quoted in Chapter 139 — **two projects share one foundation** (direct evidence of layered reuse).

### Second complete program: the frame payload, structured

The second program is from `examples/data/json` — the atom of the frame-payload layer (JSON):

```embed path="examples/data/json/main.c" title="examples/data/json/main.c"

```
```
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/data/json/main.c -lws2_32 -liphlpapi
name = xrt
{
  "name": "xrt",
  "features": [
    "json",
    "http"
  ]
}
member: code
member: ok
{"code":200,"message":"OK"}
```

**What just happened.** (1) DOM parsing yields field access (the `member: code`/`ok` query form) — chatd's frame-dispatch vocabulary directly (routing by the `op` field — isomorphic with pushd's on_message). (2) The serialization round trip (Writer producing legal JSON) is the **broadcast frame's generation** form. (3) The two atoms (line delimiting + JSON) together are the wire protocol's **everything** — Decision 2's implementation preview: two ready-made main subjects assemble one protocol — **zero self-written delimiting code**.

### The design-decision summary table

A one-page glance at the six decisions (for review) — one row per domain, the five elements compressed into three columns:

| Domain | Decision | Key rationale & verification |
| --- | --- | --- |
| Room model | users independent of connections | reconnect keeps identity (UC5 passes); two-level Map |
| Wire protocol | line delimiting + JSON | human-readable + structured + zero self-written delimiting |
| Ordering | single-point numbering per room | a single-process total-order domain; numbers monotone, gapless |
| State machine | six states including offline | identity lifecycle longer than the connection |
| Storage | a 1024-frame memory ring | teaching capacity assumption; disk left as evolution |
| Degradation | kick/refuse two grades + protocol frames | no sequence gaps forbids skipping; rejections understandable |

This table is the implementation chapter's (Chapter 141) reconciliation index — each row's "verification" column is a checklist entry when implementation completes.

### Against Chapter 138's design chapter, methodologically

The two design chapters' template executions compared — the methodology checking itself: the **five decision elements** are heavier in Chapter 138 (a section per decision), compressed here into table + sections — **depth follows controversy** (genuinely multi-option decisions expand, obvious directions compress — length serves ambiguity removal); the **acceptance-first checklist** is consistent across both (six versus eight items — this chapter adds capacity and stress — a denser-concurrency project naturally has more acceptance); **alternatives retained** (Chapter 138 keeps the template-engine extension point, this chapter keeps disk as an evolution point — **rejection is not destruction**: when the rationale disappears, the way back stays open). Three transferable template lessons: **the template is a skeleton, not a form** — the five elements' order (problem first) matters more than the format; **every decision gets a verification** — a decision without a verification plan is a wish, not a decision; **decisions cross-check** (this chapter's "identity independent" determines the catch-up protocol's shape — individually optimal decisions may contradict as a whole — a design review's main work is finding such crossings).

## Contracts

- **Five decision elements**: every design domain has problem/options/decision/rationale/verification — the implementation chapter cashes them row by row.
- **Room model**: users independent of connections (reconnect keeps identity and cursor); a room = user set + message ring + ordering counter.
- **Wire protocol**: line delimiting (lenient) + JSON payload (strict) — two ready-made layers.
- **Ordering**: single-point per room; numbers strictly monotone, gapless; cursors record the last seen.
- **Catch-up protocol**: reconnect with cursor → resend within the ring; outside it, too far behind.
- **Session final state**: no ghost frames after the leave frame (remove before broadcast); Close exactly once.
- **Identity lifecycle**: longer than the connection; offline timeout cleanup (configurable).
- **Storage boundary**: a memory ring (1024 frames/room) + disk reserved as evolution.
- **Degradation matrix**: slow members kicked (no skipping — no sequence gaps) / full rooms refused / overload refuses handshakes — every rejection has a protocol frame.
- **Division with pushd**: bare TCP builds its own protocol versus WS reusing high-level parts — the same business as a two-transport training pair.

### Compatibility design for protocol evolution

The wire protocol's versioning reserve (a design-time foresight exercise): **a version field** (the handshake frame carries `"v":1` — the server dispatches parsing by version — when v2 adds fields, v1 clients do not break: JSON parsing is naturally lenient about unknown fields — another dividend of choosing JSON cashed); **capability negotiation** (the handshake response returns the server's capability list — clients trim features by capability (the trimming is theirs) — UC6's who can evolve into pagination without breaking old clients); **degradation paths** (an unknown op returns an "unknown op" frame without disconnecting — the same leniency as Chapter 139's message tolerance). The three together cost about twenty lines of design — buying a protocol that **can evolve** instead of a big-bang synchronous upgrade on every change. Real IM systems' protocols live for decades — **compatibility is a protocol's first requirement; features are second**.

### Test doubles and client simulation

chatd's test strategy (decided at design time, executed at implementation time): **client doubles** — scripted virtual users (connect → handshake → join → speak/receive/cursor audit) — Chapter 58's executor drives N doubles; **server-shape configuration** — normal / slow-injection / full-room / ring-wraparound — the test matrix's four server behaviors; **the sequence auditor** — doubles verify monotone-no-gaps on receipt (every client carries acceptance 2's checker); **disconnect injection** — doubles drop deliberately after a given frame number (testing catch-up) and get dropped (testing final states). **The doubles' correctness** comes from sharing the same protocol library as the real client (doubles are not a second implementation — Chapter 129's "no second implementation", test edition).

## Pitfalls

### Pitfall 1: identity bound to the connection

Symptom: after a drop and reconnect the user becomes new — history catch-up impossible (the old cursor finds no owner); a private-message target going offline means unreachable (after their reconnect, routing fails).

Cause: treating the connection as identity. A TCP connection is **the session's carrier, not the session itself** — reconnect semantics require identity independent of the carrier (Decision 1's core).

```c bad
struct user { xnetstream* Conn; ... };
/* Conn dies = the user vanishes: cursor and room ties all lost */
```
```c good
struct user {
	xnetstream* Conn;     /* may be null = offline */
	uint64 Cursor;        /* last-seen sequence number - connection-independent */
	...                   /* identity fields */
};
/* reconnect: a new Conn attaches to the old user - the cursor continues */
```

### Pitfall 2: broadcast in arrival order, not by assigned numbers

Symptom: two clients see the same batch of messages **in different orders** (their receiving threads scheduled differently) — conversation logic scrambles (an answer appears before the question).

Cause: mistaking "server processing order" for "room order". Ordering must **take numbers explicitly** (counter++ under a lock) and the number must drive delivery — arrival order is a physical coincidence, not a semantic promise.

```c bad
on_say(user, msg) {
	broadcast(room, msg);   /* sends directly in arrival order: no ordering guarantee outside the lock */
}
```
```c good
on_say(user, msg) {
	uint64 seq;
	lock(room); seq = room->Next++; unlock(room);
	broadcast(room, frame(seq, msg));   /* delivered with the number */
}
```

### Pitfall 3: broadcasts still arriving after leaving (ghost frames)

Symptom: after quitting, the client still occasionally receives room messages — the session closes uncleanly; worse, after reconnect it gets polluted by old frames.

Cause: the remove-member and broadcast order is wrong (broadcast first, remove after — that batch still reaches the leaver).

```c bad
broadcast(room, "user left");   /* broadcast first */
remove_member(room, user);      /* remove after: this batch still reaches the leaver */
```
```c good
remove_member(room, user);      /* remove first */
send_to(user, "you left");      /* the last frame to the user (exactly once) */
broadcast(room, "user left");   /* the room broadcast excludes the leaver */
```

### The frame-type dictionary

The frame-type dictionary finalized for the design (the implementation chapter's protocol authority): **uplink** (handshake/join/leave/say/dm/who/quit — each with a required-field table); **downlink** (ok/err/broadcast/dm/history_tail/too_far/you_left/member_list — each with fields and sequence-number occurrence rules); **bidirectional capability** (an unknown-op receipt). The dictionary uses a three-column table (frame type / direction / fields + constraints) — Chapter 139's expectation-files-as-data idea applied to protocol. Acceptance criteria: every frame sequence of the six UCs is fully findable in the dictionary; error branches' err frames carry locatable reason codes.

## Exercises

### Basic: the six UCs' frame sequences

Write each UC's complete frame sequence (what the client sends, what the server returns — including error branches) — with the JSON frame types defined as a table. Acceptance criteria: one positive and one negative example per UC; frame fields minimally complete (op / required parameters / the sequence field's occurrence rules).

### Advanced: ordering and catch-up timelines

Draw three timelines: two concurrent speakers (number-taking and delivery interleaved — room-wide consistent); reconnect after a 5-frame drop (cursor + ring); catch-up refusal after ring wraparound (too far behind). Acceptance criteria: every timeline's number chain is hand-derivable — no gaps, no duplicates.

### Challenge: the capacity model

Estimate: 100 rooms × 50 users × a 1024-frame ring × 200 bytes/frame average — total memory; 1000 concurrent connections' per-connection buffers (Chapter 64) — the total budget table. Give the relation between a capacity ceiling recommendation and overload-rejection thresholds. Acceptance criteria: every budget row cites its source (chapter reference); thresholds and budget self-consistent.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Design template | five decision elements (problem/options/decision/rationale/verification) — the Chapter 138 template reused |
| Room model | users independent of connections; a room = user set + message ring + orderer |
| Wire protocol | line delimiting (lenient entrance) + JSON payload (strict structure) — two ready-made layers |
| Ordering | single-point numbering per room; numbers monotone, gapless; cursors record the last seen |
| Catch-up | reconnect with cursor → resend within the ring; outside it, too far behind, wholesale refresh |
| Session final state | remove before broadcast; the closing frame to the person exactly once |
| Identity lifecycle | longer than the connection; offline timeout cleanup |
| Storage | memory ring + disk reserved as evolution; ring wraparound overwrites the oldest |
| Degradation | slow members kicked (no skipping) / full rooms refused / overload refused — rejections have frames |
| Division with pushd | bare TCP builds its own protocol versus WebSocket reusing high-level parts — the same business as a two-transport training pair |
