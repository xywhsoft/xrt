---
num: 120
slug: xmail-pop3
title: POP3: Receiving Mail
volume: 卷十一 其他扩展库
type: practice
lead: The +OK/-ERR simple world, the mailbox facts of STAT/LIST/UIDL, streaming RETR with dot un-escaping, SASL authentication and STLS — the standard protocol of download-style receipt.
api: xpop3-pop3, xpop3-pop3_client, xmail-mail
---

## Orientation

POP3 is the simplest of the three receiving protocols (POP3/IMAP/Chapter 119): a **download** model — connect, authenticate, list messages, fetch one by one, (optionally) delete, say goodbye. Simple doesn't mean careless: **multiline-response dot transparency** (RETR/TOP body line-start dot escaping and single-dot termination — the symmetric reverse of SMTP DATA); **streaming receipt** (`Begin/Next` yields line by line, **never allocating the whole message** — each line borrows the internal buffer, stable only until the next read: three outlets in direct file writes / incremental MIME / your own storage); **UIDL's identity semantics** (unique IDs underpin the client logic of "fetch only new mail"); **the SASL authentication family** (USER/PASS plaintext rejected by default, SASL PLAIN plus OAuth mechanisms); **STLS upgrade** (CAPA declares → +OK → upgrade in place → **re-run CAPA**). xpop3 layering: protocol primitives (offline) + client (a synchronous state machine: AUTHORIZATION→TRANSACTION→MULTILINE→UPDATE) + an optional message layer (bounded aggregation).

## Introduction

The model difference between POP3 and IMAP decides the choice: POP3 assumes "taken means local" — the server is a temporary delivery point, the client is the authoritative store; IMAP (Chapter 119) assumes "the server is authoritative" — mail stays on the server, folders/flags/searching all server-side. POP3's simplicity brings low resource use and offline friendliness; IMAP's richness brings multi-device sync. Most scenarios today are "receive and archive" (pull into a local database) — POP3's streaming receipt fits exactly.

`Begin/Next`'s **per-line borrowing** deserves attention: RETR a 20 MB message, and at any moment only the current line is in memory — the line view `Next` yields **is stable only until the next wire read** (the next line's arrival overwrites it). That is far more aggressive than "buffer the whole message" — it turns "receiving mail" into the shape of "reading a file" (isomorphic to Chapter 41's IO streams).

## Concepts

### Protocol primitives: replies, facts, and commands

- **Replies**: `xrtPop3ReplyParse` distinguishes `+OK`/`-ERR` and borrows the status text; `StatParse` (**64-bit** counts and bytes — big mailboxes don't overflow), `ListParse`/`UidlParse` (multiline items; **UIDL is neither copied nor truncated**).
- **The dot family reused**: multiline termination and dot un-escaping use `mail_net`'s `MailLineRead`/`MailDotLine`/`MailDotDecodeWrite` directly — **no POP3-specific duplicate** (two directions of the same primitive family as SMTP DATA).
- **Capabilities**: `CapabilityParse` returns names and parameters even for unknown extensions; `Capability` gives stable bits only to common standards.
- **Commands**: `CommandWrite` as the generic fallback (capacity includes the trailing zero; rejects control characters/CR/LF injection/512B — the same discipline as the SMTP command layer).

### The client: state machine and layered entrances

```diagram state
AUTHORIZATION -> TRANSACTION: authentication succeeds (USER/PASS or SASL)
TRANSACTION -> MULTILINE: a multiline command (RETR/TOP/LIST/UIDL full)
MULTILINE -> TRANSACTION: the single-dot terminating line (Next auto-recovers)
TRANSACTION -> UPDATE: Quit (commits the DELE marks)
any -> FAILED: a line error (not reusable)
```

**Four levels of layered entrances**: `Send/Line` (the lowest wire level — the opening for SASL continuations and unknown extensions) → `Receive` (+OK/-ERR parsing) → `Command` (build + status response) → `Begin/Next` (multiline, line by line). **Every standard command is built on the public entrances** — STAT/LIST×2/UIDL×2/RETR/TOP/DELE/RSET/NOOP/QUIT are assemblies, not private paths: custom extensions stand on equal footing with official commands.

### Streaming receipt and optional aggregation

RETR/TOP's `Begin/Next`: each line dot-un-escaped, **never allocating the whole message**, the line view borrowing the internal receive buffer (stable until the next read). Three outlets: write straight to a file (Chapter 45), incremental MIME parsing (Chapter 116's modules fed step by step), your own message store. The optional `pop3_message` layer: bounded `RetrWrite/TopWrite` on the same state machine (aggregates into owned bytes within a cap) plus MIME tree entrances — take only the line-by-line path and no aggregation code comes along (trimming freedom).

### Authentication: plaintext rejected by default, the SASL family

`pop3_auth`: traditional USER/PASS plus RFC 5034 SASL (PLAIN and two OAuth mechanisms). **Credential discipline**: the configuration only borrows credentials; Base64 and plaintext scratch buffers **are zeroed before the call returns** (Chapter 77's password engineering honored at the protocol layer). **Sending credentials in plaintext is rejected by default** — `AllowPlaintext` exists only for explicitly controlled compatibility environments (the same stance as Chapter 106's Basic TLS baseline). A server rejecting credentials stays in AUTHORIZATION (retryable); a line error enters FAILED.

### TLS: implicit and STLS

The base client is plaintext on 110; `pop3_client_tls` adds two forms: implicit TLS (handshake on connect); STLS (CAPA explicitly declares → send STLS → **only after the +OK is fully consumed** does the handshake run → **re-run CAPA after the handshake** — pre-upgrade capabilities never leak into the secure session; the same reasoning as SMTP STARTTLS's re-EHLO). A Verifier must be provided.

## Examples

Run these commands from the repository root. The builder selects modules, public headers and platform libraries from the manifest, and also prints build and dependency-test logs. Terminal blocks show the example output. The second program starts a loopback server within the process and uses fixed demonstration data. See the client library README for real-service host, port, CA and runtime credential configuration.

### First complete program: protocol primitives

The program below is from `examples/pop3/protocol` — an offline loop of replies and commands:

```embed path="extlibs/xpop3/examples/protocol/main.c" title="extlibs/xpop3/examples/protocol/main.c"
```

```term
$ python tools/build.py --manifest extlibs/xpop3/config/modules.json --suite pop3 --no-single --jobs 4
messages=12 command=RETR 1
```

**What just happened.** (1) The program parses a fixed STAT reply with `xrtPop3StatParse`, obtains the message count, then constructs and prints RETR with `xrtPop3CommandWrite`. Counts and byte sizes retain 64-bit semantics; command arguments pass protocol validation. (2) Parsing and command construction open no connection and can serve custom clients or test servers. The same suite tests other reply, UIDL and multiline boundaries; this short program demonstrates two entry points.

### Second complete program: local retrieval

The second program is from `extlibs/xpop3/examples/offline/main.c`.

```embed path="extlibs/xpop3/examples/offline/main.c" title="extlibs/xpop3/examples/offline/main.c"
```

```term
$ python tools/build.py --manifest extlibs/xpop3/config/modules.json --suite pop3_offline_example --no-single --jobs 4
offline POP3 retrieval: 3 message lines
```

**What just happened.** (1) The program creates a local listener and server thread, then resolves the demonstration host to loopback. This fixture does not implement CAPA, so automatic capability reads are disabled; select that configuration according to real server behavior. (2) The client completes USER/PASS, executes RETR and consumes `xrtPop3ClientNext` line by line. The program compares the subject, empty line and body byte for byte, requiring exactly three lines and a complete multiline terminator. (3) After QUIT it waits for the server thread and destroys the connection, listener, resolver and engine. Only content checks and successful retirement produce the success line. Fixed plaintext credentials are explicitly allowed only in this loopback fixture; the real-service example uses TLS and runtime credentials. This program does not delete mail: downloading and server deletion are separate operations.

## Contracts

- **Protocol/client layering**: the primitives depend on no network; the client is a state-machine assembly; standard commands are built on the public entrances.
- **64-bit facts**: STAT counts and bytes are 64-bit; UIDL neither copied nor truncated.
- **Shared dot family**: multiline termination/un-escaping reuse the mail_net primitives — no POP3-specific duplicate.
- **Injection discipline**: CommandWrite rejects control/CR/LF/512B — same as SMTP.
- **Streaming receipt**: RETR/TOP borrow line by line (stable until the next read); no whole-message allocation; three outlets (file/incremental MIME/own storage).
- **Optional aggregation**: the message layer's bounded RetrWrite/TopWrite — the line path carries no aggregation baggage.
- **Credential zeroing**: borrowed configuration; scratch buffers zeroed before the call returns; sending credentials in plaintext rejected by default.
- **Authentication semantics**: rejected credentials stay in AUTHORIZATION; a line error enters FAILED, not reusable.
- **TLS**: implicit/STLS; the handshake only after +OK is fully consumed; re-CAPA after upgrade; a verifier is mandatory.
- **UPDATE semantics**: DELE marks take effect at Quit's UPDATE — Abort abandons the deletions.
- **Ownership**: shared objects borrowed, not destroyed; no calling from Worker callbacks; a single Client has no concurrency.

## Pitfalls

### Pitfall 1: storing the line view and continuing to receive

Symptom: after receiving, processing the cached lines — the content is already the next line's bytes.

Cause: the line's stability boundary is "the next wire read" — the POP3 client's most aggressive borrowing. To keep across lines you must copy.

```c bad
while ( Next(&Line) ) {
	rows[n++] = Line;   /* stored views: all invalid at the next Next */
}
process(rows);   /* all the last line */
```

```c good
while ( xrtPop3ClientNext(pClient, &Line) == OK ) {
	process_now(Line);          /* consume immediately */
	/* or copy: append_own_store(Line); */
}
```

### Pitfall 2: sending USER/PASS over a plaintext connection

Symptom: `AllowPlaintext` flipped on and forgotten — intranet captures are full of credentials.

Cause: rejecting by default is protection, not an obstacle; the correct path for old-server compatibility is STLS (upgrade when CAPA declares it), not giving up encryption.

```c bad
Config.AllowPlaintext = true;   /* convenience: plaintext credentials */
Auth(pClient, User, Pass);
```

```c good
if ( capa_has_stls() ) {
	Stls(pClient, ...);   /* upgrade first */
}
Auth(pClient, User, Pass);   /* inside the encrypted tunnel */
```

### Pitfall 3: assuming DELE deletes immediately

Symptom: after DELE the server crashes / the client aborts — after restart the mail is still there; the user thinks it "was deleted and came back".

Cause: POP3's deletion is **two-phase**: DELE only marks; Quit's UPDATE commits — the protocol's defense against accidental deletion. For "definitely deleted", complete the Quit; for "maybe deleted" (tentative processing), Abort rolls back.

```c bad
Dele(pClient, 1);
/* crash/Abort: the mail is undeleted - looks like a bug */
```

```c good
Dele(pClient, 1);          /* mark */
...
xrtPop3ClientQuit(...);    /* UPDATE commits - truly deleted */
/* on processing failure Abort: the marks roll back - a safety net */
```

## Exercises

### Basic: reply and fact parsing

Construct `+OK`/`-ERR`/STAT/List/Uidl multiline samples through the primitive parsers. Acceptance criteria: 64-bit counts correct; UIDL preserved at full length; multiline termination recognized correctly.

### Advanced: fetch only new mail

UIDL listing compared against the locally seen set (Chapter 18's Map) → RETR only new UIDs → record after processing → optionally DELE+Quit. Acceptance criteria: repeated runs fetch zero duplicates; UIDL stability (same mail, same ID); rerun after interruption neither misses nor duplicates.

### Challenge: a mail-archiving pipeline

POP3 streaming receipt → feed line by line into Chapter 116's message view / incremental MIME parsing → extract headers and body → write to a local mbox/JSONL store. A 20 MB message. Acceptance criteria: memory constant throughout (MB scale); after archiving, Chapter 116's parsing restores the whole; resumable receipt (UIDL reconciliation).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Model | download-style: taken means local; UIDL is the identity for "fetch only new" |
| State machine | AUTHORIZATION→TRANSACTION→MULTILINE→UPDATE (Quit commits) |
| Replies | +OK/-ERR two states; STAT 64-bit; UIDL untruncated |
| Dot family | symmetric with SMTP DATA; reuses mail_net primitives, no duplicate |
| Four entrance levels | Send/Line→Receive→Command→Begin/Next — standard commands on equal footing |
| Streaming receipt | per-line borrowing stable until the next read; zero whole-message allocation; file/incremental/store outlets |
| Optional aggregation | message layer's bounded RetrWrite — the line path carries nothing extra |
| Credentials | plaintext rejected by default; scratch buffers zeroed within the call; three SASL mechanisms |
| TLS | implicit/STLS; handshake only after +OK is consumed; re-CAPA; verifier mandatory |
| Deletion semantics | DELE marks, Quit's UPDATE commits — Abort rolls back |
