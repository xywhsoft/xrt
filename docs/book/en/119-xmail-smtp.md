---
num: 119
slug: xmail-smtp
title: SMTP: Sending Mail
volume: 卷十一 其他扩展库
type: practice
lead: Reply parsing and capability negotiation, command-injection defense, streaming DATA with dot transparency, the CHUNKING fast path and STARTTLS — the complete outbound wire.
api: xmail-smtp, xmail-smtp_client, xmail-mail
---

## Orientation

The data layer (Chapter 116) defines what a mail message is; SMTP defines **how to send**. xmail's SMTP has two layers: **protocol primitives** (`smtp`: reply-line parsing, EHLO capability merging, command building — no network dependency; the official client and custom state machines share one implementation); **the client** (`smtp_client`: a synchronous session state machine — the `READY → MAIL → RECIPIENT → DATA → READY` envelope transaction; no hidden Engine/DNS thread/fixed buffers, and it does not build MIME). Four main threads: **injection defense** (the command layer rejects CR/LF and control characters — SMTP is the worst header-injection zone); **streaming DATA** (dot transparency as an incremental state machine — line-start state preserved across chunks, no whole-message copy); **CHUNKING** (BDAT sends raw by declared byte count — no scanning, no escaping); **TLS in two forms** (implicit TLS and STARTTLS upgrade — the post-upgrade capability snapshot is not carried over).

## Introduction

SMTP is a 1982 conversational protocol: the client sends commands (`EHLO`/`MAIL FROM`/`RCPT TO`/`DATA`), the server answers three-digit replies (`250 OK`/`550 拒绝` (rejected)). Beneath the surface simplicity sit three modern engineering problems. **First: injection** — early implementations spliced user input straight into command lines; input containing CRLF can "smuggle" extra commands (the classic sender-forgery path); `xrtSmtpCommandWrite` rejects CR/LF and control characters at the entrance, with a 512-byte cap. **Second: dot transparency** — in DATA mode, a `.` at a body line's start must be escaped (`.`→`..`), and termination is a lone `.` line; streaming sends (without accumulating the whole message) require the escape state to persist **across chunks** — the previous chunk ended at a line start, the next begins with `.`; that boundary dot must be escaped. **Third: binary** — dot transparency and CRLF requirements make DATA inherently textual; BDAT (the CHUNKING extension) sends raw chunks by declared byte count, so binary attachments pass unescaped.

## Concepts

### Protocol primitives: replies, capabilities, and commands

```diagram flow
- Reply: MailLineRead (take a CRLF-less line) -> SmtpReplyLineParse (three-digit code + multiline separator)
  -> xsmtpreplyparser (code consistency/line-count cap/unique terminating line) -> SmtpReplyRead (incremental)
- Capabilities: SmtpCapabilityParse (borrowed name and parameters) -> CapabilityAdd (merge common extensions +
  three AUTH forms + 64-bit SIZE; unknown extensions parse without taking a slot)
- Command: SmtpCommandWrite (Verb [Args]\r\n - rejects control chars/CR/LF injection/512B excess)
```

**Path validation**: `xrtSmtpPathValid` validates reverse/forward-path content (no angle brackets/whitespace/control separators) — it validates only the inside of the angle brackets and builds no command (responsibility split: content validation belongs to the protocol layer, command assembly to the command layer).

### The client: session state and ownership

`xrtSmtpClientOpen`: validate the `220` banner → send EHLO → (only on an explicit `500/502/504` does it fall back to HELO per configuration — other EHLO failure codes do not fall back). The state machine `READY → MAIL → RECIPIENT → READY` (one envelope); with CHUNKING it enters `CHUNK` after the first chunk, and LAST success returns to READY. **Ownership**: the configuration is borrowed only during Open; the Client holds the transport and last reply, and **borrows** the Engine/Resolver/TLS Context/Verifier (destroying it touches no shared objects). **Threading**: all blocking operations take an absolute `xdeadline` + optional `xcancel`; **they must not be called from the owning Engine's Worker callback** (Chapter 98's same deadlock guard); a single Client supports no concurrent commands. **Three finishes**: `Quit` (the protocol farewell) / `Close` (skip QUIT, await transport close) / `Abort` (immediately abort from any non-empty state, idempotent success, FAILED keeps the last reply for diagnosis).

### The DATA fast path: streaming dot transparency

`DataBegin/DataWrite/DataEnd` accept arbitrary chunking: the internal incremental state machine **preserves CRLF and line-start state across chunks**, sending dot-transparent fragments directly — **no whole-message copy is created** (the sending side of big-attachment streaming). `xrtSmtpClientData` is the contiguous-input convenience entrance: it fully validates CRLF first, then delegates to the incremental machine. **The caller still owns** the MIME fields, transfer encodings, and the server's SIZE limit — the client manages the wire, not the content (content belongs to Chapter 116's modules).

### CHUNKING: the BDAT fast path

Once the server declares CHUNKING: `BdatBegin/BdatWrite/BdatEnd` send raw fragments by **declared byte count** — **no content scanning, no dot transparency, no appended CRLF**, zero whole-chunk copies. Each chunk synchronously reads `250`; after `4xx/5xx` further chunks are forbidden (you must RSET/close/abort). **Declared length mismatch**: End refuses to read the reply and the caller may top up the bytes; if it cannot, it must abort. The BINARYMIME combination: the capability bit declares support only, and the message type passes through `Mail(..., "BODY=BINARYMIME", ...)`'s generic parameter — **no envelope API is duplicated for it** (the ability to combine arbitrary MAIL parameters is retained).

### TLS: implicit and upgrade forms

The base client is plaintext; the `smtp_client_tls` layer adds two forms: `XMAIL_SECURITY_TLS` (implicit — the handshake completes before the banner, the port-465 shape); `XMAIL_SECURITY_STARTTLS` (upgrade — EHLO declares STARTTLS, validate the `220` switch, take over TLS on the original stream, **re-send EHLO after the handshake**). Two hard rules: **the upgrade does not carry over the pre-upgrade capability snapshot** (a man in the middle may strip capabilities — the renegotiation is the truth); **a verifier must be provided** — certificate validation is never silently skipped (Chapter 85's "not verifying is not an option", mail edition).

## Examples

### First complete program: capability parsing and command building

The program below is from `examples/smtp/protocol` — an offline loop of the protocol primitives:

```embed path="extlibs/xsmtp/examples/protocol/main.c" title="extlibs/xsmtp/examples/protocol/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xsmtp/examples/protocol/main.c -lws2_32 -liphlpapi
（输出能力合并与 EHLO 命令构建的自检结果）
```

**What just happened.** (1) `SmtpCapabilityParse("SIZE 10485760")` parses a single capability (name + borrowed parameter view) → `CapabilityAdd` merges it into the built-in bits — **SIZE's 64-bit cap** (10 MiB) is recorded: later, before DATA, you can check against it (don't send past the limit in vain). (2) `SmtpCommandWrite("EHLO", "client.example", ...)` produces `EHLO client.example\r\n` — the arguments pass the injection checks (control characters/CR/LF rejected), 512-byte cap. (3) **Offline testability** is this sample's point: the protocol layer touches no network — the reply/capability/command primitive families are all unit-testable (the official client is precisely an assembly of these primitives, one behavior, no second implementation).

### Second complete program: a complete sending session

The second program is from `examples/smtp/client` — a synchronous loop against a real session:

```embed path="extlibs/xsmtp/examples/client/main.c" title="extlibs/xsmtp/examples/client/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xsmtp/examples/client/main.c -lws2_32 -liphlpapi
（对配置的 SMTP 服务器完成发送会话后正常退出）
```

**What just happened.** (1) `xrtSmtpClientData` sends the complete message (headers + empty line + body) — the convenience entrance validates CRLF first, then enters the incremental state machine (dot transparency automatic). (2) `xrtSmtpClientQuit` is the protocol farewell — the normal close path; the failure path switches to Abort (the sample's Cleanup shape). (3) The whole session is bound by `Deadline` + optional Cancel — the standard two parameters of blocking operations (the common shape since Chapter 82). (4) Compare with the `submit` sample: that one is the **highest layer** (`xrtSmtpSubmit` — one struct submission of sender/recipients/subject/body; MIME assembled and the session run inside); this chapter's sample shows the middle layer (the complete message self-supplied) — **the higher the layer, the more it does for you; the lower, the more freedom** — Chapter 120 completes the layer diagram.

## Contracts

- **Protocol/client layering**: the primitives touch no network; the client is a state-machine assembly of the primitives — no second implementation.
- **Injection defense**: the command layer rejects control characters/CR/LF, 512B cap; path validation covers only the inside of the angle brackets.
- **Reply discipline**: three-digit codes consistent, line-count cap, unique terminating line; the incremental path `MailLineRead`→`ReplyRead`.
- **EHLO fallback**: only an explicit `500/502/504` falls back to HELO per configuration; other failures are not guessed at.
- **Ownership**: configuration borrowed during Open; shared objects (Engine/Resolver/TLS/Verifier) borrowed, not destroyed; a single Client runs no concurrent commands.
- **Blocking discipline**: absolute deadline + optional cancel; calling from the owning Worker's callback forbidden.
- **Three finishes**: Quit protocol farewell / Close skips QUIT and awaits close / Abort aborts immediately (idempotent; FAILED keeps the reply).
- **Streaming DATA**: dot transparency across chunks; zero whole-message copies; CRLF belongs to the caller.
- **BDAT**: declared bytes sent raw; no scanning, no escaping, no CRLF; each chunk reads 250; a length mismatch may be topped up or the session aborted.
- **BINARYMIME**: capability bit + generic MAIL parameter — no envelope API duplication.
- **TLS in two forms**: implicit (before the banner) / STARTTLS (re-EHLO after upgrade, no old snapshot carried); a verifier is mandatory.

## Pitfalls

### Pitfall 1: splicing user input directly into a command

Symptom: input with CRLF in the recipient address "successfully" sends — actually smuggling extra commands (forgery/amplification).

Cause: SMTP injection's home turf: the command is a text-line protocol; bare splicing is injection. `CommandWrite`/path validation are the defense line — splicing strings around them bypasses the defense.

```c bad
snprintf(Cmd, "RCPT TO:<%s>\r\n", UserInput);   /* injection straight through */
```

```c good
if ( !xrtSmtpPathValid(view(UserInput), false) ) {
	return reject("invalid address");   /* validate the content */
}
xrtSmtpCommandWrite(XRT_STR_LITERAL("RCPT TO"),
	AngleForm, Out, sizeof(Out), &n);   /* the entrance with injection checks */
```

### Pitfall 2: keeping the old capability snapshot after STARTTLS

Symptom: a "supports SIZE" cached before the upgrade is used to decide after it — on a connection whose capabilities a man in the middle stripped, an oversized mail gets rejected (good case) or CHUNKING is wrongly enabled (bad case).

Cause: the plaintext-phase EHLO response is **untrustworthy** (a MITM can alter it); after upgrading you must re-send EHLO for the true snapshot — the client contract does this automatically; caching capabilities yourself bypasses it.

```c bad
caps_before_tls = parse_ehlo();   /* cached during plaintext */
starttls();
	use_caps(caps_before_tls);  /* untrustworthy snapshot */
```

```c good
starttls();
caps = client_renegotiated_ehlo();   /* the client re-sent EHLO after upgrade - use the new one */
```

### Pitfall 3: forcing End when BDAT's declared length mismatches

Symptom: the server hangs waiting for the remaining bytes or reports a protocol error — you declared 1024, actually sent 1000, then called End.

Cause: BDAT's contract is "declaration = promise"; when End detects a mismatch it refuses to read the reply and offers a top-up chance — if you can't top up, abort (the connection's chunk counting is already wrong; there is no continuing).

```c bad
BdatBegin(1024);
BdatWrite(实际 1000 字节);
BdatEnd();   /* 24 bytes short, hard-ended: the server hangs */
```

```c good
BdatBegin(1024);
sent = BdatWrite(Buf, 1000);
if ( sent < 1024 ) {
	if ( !BdatWrite(Buf + 1000, 24) ) {  /* top up */
		xrtSmtpClientAbort(pClient);      /* can't: abort */
	}
}
BdatEnd();
```

## Exercises

### Basic: the reply-parsing matrix

Construct four reply byte streams — single-line/multiline/inconsistent codes/no terminating line — through LineRead→ReplyRead. Acceptance criteria: the legal groups parse correctly; the illegal groups yield distinguishable structured errors.

### Advanced: injection-defense experiments

Feed `CommandWrite` and `PathValid` inputs containing CRLF/control characters/excessive length — verify all rejected. Acceptance criteria: five hostile input classes (CRLF/bare CR/control bytes/over 512B/angle brackets) each rejected by the correct entrance.

### Challenge: streaming a big attachment

DATA path: a 10 MB attachment streamed in 8 KiB chunks (dot transparency across boundaries) — compare with the BDAT path on the same attachment. Measure peak memory and wire bytes (DATA slightly more due to escaping). Acceptance criteria: both paths deliver the same content; the DATA path's peak constant; the boundary cases (chunk boundary exactly at a line start / before a dot) pass their test vectors.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Layering | protocol primitives (offline-testable) → client (state-machine assembly) — no second implementation |
| Injection defense | commands reject CR/LF/control/512B; paths validate inside the angle brackets |
| Session state | READY→MAIL→RECIPIENT→DATA→READY; EHLO falls back only on explicit rejection |
| Ownership | shared objects borrowed, not destroyed; a single Client has no concurrency; Worker callbacks forbidden |
| Blocking shape | absolute deadline + optional cancel |
| Three finishes | Quit/Close/Abort (immediate, idempotent, FAILED keeps diagnostics) |
| DATA | dot transparency across chunks; zero whole-message copies; CRLF is the caller's |
| BDAT | declared bytes sent raw, unescaped; each chunk reads 250; mismatch topped up or aborted |
| BINARYMIME | capability bit + generic MAIL parameter — no envelope API duplication |
| TLS | implicit/STARTTLS two forms; re-EHLO after upgrade; a verifier is mandatory |
