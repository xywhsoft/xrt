---
num: 121
slug: xmail-imap
title: IMAP: The Server-Side Mailbox
volume: 卷十一 其他扩展库
type: practice
lead: Tagged responses and streaming literal reads, the two-level command model, SELECT summaries and IDLE push, COMPRESS and pipelining — the server-authoritative receiving protocol.
api: ximap-imap, ximap-imap_client, ximap-imap_command
---

## Orientation

IMAP is POP3's (Chapter 118) "server-authoritative" dual: mail stays on the server, and folders/flags/search/FETCH partial retrieval all happen server-side — the cornerstone of multi-device sync. The protocol complexity is an order higher too: **the tagged response model** (every command carries a unique tag; server responses correlate by tag — enabling pipelining); **the literal mechanism** (parameters and responses containing arbitrary bytes go through `{N}` length declarations — synchronous literals wait for a continuation confirmation, `LITERAL+` skips the sync); **the untagged event stream** (EXISTS/RECENT/FETCH updates cut in at any time). ximap layering: protocol primitives (`imap`: replies/literals/capabilities/commands — builds no mailbox objects and constructs no search conditions; **unknown extensions remain reachable as raw text**) → client (`imap_client`: the two-level command model — a low-level explicit-tag pipeline plus a sequential convenience layer) → command convenience layer (`imap_command`: safe construction of SELECT/LIST/SEARCH/FETCH/IDLE) → specialized layers (auth/body/message/append/compress).

## Introduction

Three IMAP mechanisms deserve the opening. **First: streaming literals.** FETCH a 20 MB message and the response carries `{20971520}` followed by raw bytes — the client **must read in explicit-length segments**: `ReadLiteral` takes segments into the caller's buffer, and **until it finishes you cannot read subsequent responses** — "message size never becomes a fixed memory ceiling" is a protocol-layer guarantee (the same family as POP3's line streaming, but by length rather than by line). **Second: IDLE push.** The `IDLE` command lets the server push untagged events as new mail arrives — the server-side version of "a long connection waiting for new mail" (the mail edition of SSE). **Third: pipelining.** The low-level `Send` takes explicit tags — send several commands, then one `Receive` loop consumes events in arrival order and correlates completions by tag — IMAP's concurrency primitive (the throughput shape of connection reuse).

The usual injection defense is present: `CommandWrite` rejects malformed atoms/control characters/line injection/excess; `imap_command`'s mailbox names are "validated and quoted per IMAP string rules — **extra commands cannot be injected via CRLF**".

## Concepts

### The response model and literals

```diagram flow
- Three reply classes: tagged (command completion - correlated by tag) / untagged (starting with * - the event stream cuts in) / continuation (starting with + - literal continuation lines)
- Five status states: OK/NO/BAD/PREAUTH/BYE (stably recognized)
- Three literal forms: synchronous {N} (waits for the client's continuation confirmation) / LITERAL+ {N+} (no sync, send straight) / binary ~{N}
  - the protocol layer parses only lengths and markers; the state machine reads the data by length (zero copy, zero implicit buffering)
```

**Literal budgets run both ways**: the send side (APPEND of a big message — Chapter 120's composition) and the receive side (FETCH of a big literal). The receive-side contract: on a `HasLiteral` event first check `Event.Literal.Size` — over your application budget, **Abort immediately** (no need to consume the body — the memory defense of "read the menu before deciding to eat"); the low-level streaming path has **no default ceiling** (ceilings belong to the application); the `imap_message` convenience layer carries its own 64 MiB budget (the two don't share — each layer, each policy).

### The two-level command model

- **Low level** (`Send/SendParts/Write/Continue/Receive/ReadLiteral`): explicit-tag pipelining — send multiple commands, consume events in order, correlate by tag; `SendParts` writes argument pieces with inserted spaces **straight to the wire** (no whole temporary command constructed). Views borrow the line buffer — **invalidated after the next Receive/Next/ReadLiteral** (the same family discipline as Chapter 118's line views; here literal reads are also invalidation points).
- **Sequential layer** (`Begin/BeginParts/Next`): automatic unique tags; `Next` yields untagged events (`XMAIL_NEXT_ITEM`) and the target completion (`XMAIL_NEXT_END`) **as separate states** — a unified loop shape; you cannot start the next sequential command while one is active.

### The command convenience layer: SELECT summary and streaming results

`imap_command` covers common commands but **builds no heavyweight objects**: `Select/Examine` produce an `ximapmailboxinfo` with zero allocation (a digest of the mailbox facts; the **Present bit distinguishes "not returned" from "value zero"** — Chapter 102's presence bit, mailbox edition; ReadOnly takes the final completion's access mode; `MailboxInfoUpdate` merges subsequent EXISTS/RECENT events into the same summary). **Management commands** (CREATE/DELETE/RENAME/SUBSCRIBE/CHECK/UNSELECT/CLOSE) fully consume their synchronous completions; **streaming commands** (LIST/STATUS/SEARCH/FETCH/STORE/COPY/MOVE/EXPUNGE) only build safely and start — results stream through `Next`+`ReadLiteral` (the convenience layer never cuts the stream). **IDLE**: after entering, `Done` ends it — pushed events flow through the same Receive loop.

### TLS, compression, and failure semantics

**TLS**: implicit/STARTTLS (the handshake only after the tagged OK is fully consumed; **re-run CAPABILITY after upgrade** — the plaintext snapshot is not carried over; a verifier is mandatory) — the same family trio as SMTP/POP3. **COMPRESS** (`imap_compress`): after authentication, negotiate a DEFLATE-compressed wire — a bandwidth optimization for high-latency links. **Failure semantics** in three: **line FAILED** (network/cancellation/timeout/protocol misordering/parse failure — "the next byte's position at a command boundary cannot be guaranteed"; the connection is not reusable); **command NO/BAD** (the server's negation of a complete command — the client **keeps a recoverable state**, returns a structured error — the next command continues); **Abort** (abort without waiting, repeatable; FAILED and the most recent completion are kept until Destroy for diagnosis).

## Examples

Run these commands from the repository root. The builder selects modules, public headers and platform libraries from the manifest, and also prints build and dependency-test logs. Terminal blocks show the example output. The second program starts a loopback server within the process and uses fixed demonstration data. See the client library README for real-service host, port, CA and runtime credential configuration.

### First complete program: protocol primitives

The program below is from `examples/imap/protocol` — an offline loop of replies and commands:

```embed path="extlibs/ximap/examples/protocol/main.c" title="extlibs/ximap/examples/protocol/main.c"
```

```term
$ python tools/build.py --manifest extlibs/ximap/config/modules.json --suite imap --no-single --jobs 4
status=1 command=A002 SELECT "INBOX"
```

**What just happened.** (1) The program parses a fixed tagged OK reply with `xrtImapResponseParse`, then builds SELECT with an explicit tag and mailbox argument through `xrtImapCommandWrite`, printing the status and command bytes. (2) The mailbox argument already includes double quotes; the builder validates wire syntax and injection boundaries. Callers choose a quoted string or literal according to the argument type. The same suite tests other reply, literal and escaping boundaries; body reads remain the client state machine responsibility.

### Second complete program: a read-only mailbox query

The second program is from `extlibs/ximap/examples/offline/main.c`.

```embed path="extlibs/ximap/examples/offline/main.c" title="extlibs/ximap/examples/offline/main.c"
```

```term
$ python tools/build.py --manifest extlibs/ximap/config/modules.json --suite imap_offline_example --no-single --jobs 4
offline IMAP EXAMINE: INBOX has 2 messages (read-only)
```

**What just happened.** (1) The program creates an in-process loopback IMAP server. The client reads the greeting and establishes a sequential command session. LOGIN uses a fixed demonstration account, with plaintext authentication allowed only for this local fixture. (2) The client calls `xrtImapClientExamine`, reads the read-only INBOX state and a count of two messages, checks the selected state and mailbox facts, and waits for tagged completion. EXAMINE does not change server message flags and demonstrates a short read-only query. (3) After LOGOUT the program waits for the server thread, then closes and destroys the listener, resolver and network engine. Success means both the command result and server script were checked. Compression requires a separate authenticated negotiation and stream-decoding flow; see the library README for compression tests and real-client configuration.

## Contracts

- **Primitives without models**: no mailbox objects / no search/FETCH models built — unknown extensions reachable as raw text.
- **Three classes, five states**: tagged/untagged/continuation; OK/NO/BAD/PREAUTH/BYE stably recognized; all views via borrowing.
- **The literal contract**: parsing yields only lengths and markers; data is read in segments; until finished you cannot read subsequent responses — message size ≠ memory ceiling.
- **Literal budgets**: no default ceiling at the low level (the application checks Size itself and Aborts immediately over budget); the message layer's independent 64 MiB budget.
- **Two-level model**: low-level explicit-tag pipeline (SendParts writes straight to the wire) / sequential layer automatic tag + state-split Next; sequential commands do not run concurrently.
- **View invalidation points**: any of Receive/Next/ReadLiteral invalidates line-buffer views.
- **Command convenience layer**: SELECT's zero-allocation summary (Present bit/event merging); management commands complete synchronously; streaming commands never cut the stream.
- **Injection defense**: command/mailbox-name injection rejected; control data goes via literal.
- **TLS**: implicit/STARTTLS; re-CAPABILITY after upgrade; a verifier is mandatory.
- **Compression**: COMPRESS after authentication; configuration Validated first.
- **Failure in three**: line FAILED not reusable / command NO-BAD recoverable / Abort idempotent with diagnostics kept.

## Pitfalls

### Pitfall 1: reading the literal before checking its size

Symptom: on `HasLiteral` you start ReadLiteral right away — the next message is 500 MB (a historical archive), and halfway through, memory or budget blows.

Cause: the contract says plainly "check `Event.Literal.Size` first" — the size is in the event, the reading comes after the decision. No default ceiling at the low level = the ceiling is your responsibility.

```c bad
if ( Event.HasLiteral ) {
	while ( ReadLiteral(...) ) { consume(); }   /* size unchecked: 500MB pours straight in */
}
```

```c good
if ( Event.HasLiteral ) {
	if ( Event.Literal.Size > MyBudget ) {
		xrtImapClientAbort(pClient);   /* over budget: abort immediately, skip the body */
		return;
	}
	while ( ReadLiteral(...) ) { consume(); }
}
```

### Pitfall 2: waiting for pipelined completions in send order

Symptom: dead-waiting on command A's tagged response — the server actually answered B's first (or inserted untagged events first).

Cause: IMAP responses arrive **in the server's arrival order**, with no promise of send order; untagged events cut in at any time — correlate by tag, consume by arrival, never wait by send order.

```c bad
send(A); send(B);
wait_tagged(A);   /* the server may answer B first: hung waiting */
wait_tagged(B);
```

```c good
send(A); send(B);
while ( Receive(&Event) ) {
	if ( Event.kind == TAGGED ) { dispatch_by_tag(Event.Tag); }
	else { handle_untagged(Event); }   /* inserted events handled immediately */
}
```

### Pitfall 3: views crossing the next Receive

Symptom: saved response views/fragments read as garbage — the line buffer was overwritten by the next read.

Cause: the invalidation points are **three** (Receive/Next/ReadLiteral) — finer than POP3's "next read": literal reads also move the buffer. To keep across calls, copy.

```c bad
Subject = Event.ResponseView;   /* stored a view */
Receive(&Event2);               /* the buffer has changed */
use(Subject);                   /* garbage */
```

```c good
Subject = copy_view(Event.ResponseView);   /* copy before crossing calls */
Receive(&Event2);
use(Subject);
```

## Exercises

### Basic: the reply and literal parsing matrix

Construct tagged/untagged/continuation and synchronous/LITERAL+/binary literal samples through the primitives. Acceptance criteria: the five states recognized correctly; the three literal forms' lengths and markers parse consistently.

### Advanced: SELECT and new-mail watching

SELECT takes the summary (Present bit verified) → enter IDLE → simulate an EXISTS push → `MailboxInfoUpdate` merges → Done exits. Acceptance criteria: the summary's EXISTS/RECENT updates with events; untagged handling during IDLE splits states correctly.

### Challenge: a pipelined FETCH fetcher

An explicit-tag pipeline of 4 concurrent FETCHes (different message ranges) — consume by arrival, correlate by tag, write literals to disk in segments. Acceptance criteria: out-of-order responses correlated correctly; a 20 MB message with constant memory; the over-budget path aborts cleanly.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Model | server-authoritative: folders/flags/search/FETCH partial retrieval all server-side |
| Replies | tagged (correlated by tag) / untagged (inserted events) / continuation (literal continuation lines) |
| literal | synchronous {N} / LITERAL+ {N+} / binary ~{N}; parsing yields lengths only — data read in segments |
| Budget discipline | check Size in the event before deciding; no default low-level ceiling; the message layer's independent 64 MiB |
| Two-level model | low-level pipeline (explicit tags) / sequential layer (automatic tag + state-split Next) |
| View invalidation | Receive/Next/ReadLiteral — three invalidation points; copy across calls |
| Command layer | SELECT's zero-allocation summary (Present bit); streaming commands never cut the stream |
| IDLE | server push — Done ends it; events flow through the same Receive |
| TLS/compression | re-CAPABILITY after STARTTLS; COMPRESS with configuration Validated first |
| Failure in three | line FAILED not reusable / NO-BAD recoverable and continuing / Abort idempotent |
