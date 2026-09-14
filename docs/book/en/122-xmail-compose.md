---
num: 122
slug: xmail-compose
title: Composed Sending: Attachments, Encodings, and the Send-Receive Loop
volume: 卷十一 其他扩展库
type: practice
lead: A one-line Compose of structured mail, three-layer automatic multipart selection, the SMTP submission loop and the Bcc boundary — the concluding fusion of xmail's five chapters.
api: xmail-mail_compose, xmail-smtp_submit, xmail-mail
---

## Orientation

xmail's concluding chapter. The previous four gave the data layer (115) and three protocols (116–118); this chapter fuses them: **`mail_compose`** (high-level composition — `xmailmessage`/`xmailattachment` borrow all caller data; `xrtMailCompose` produces a complete message in one line: **three-layer automatic multipart selection** (mixed/alternative/related combined per content, boundaries distinct from each other), QP/Base64 automatic encoding, **Bcc goes only to the submission layer, never into the message**); **`smtp_submit`** (the highest-level send — one struct submission, the envelope derived from the message); **the address syntax layer** (`mail_address`: addr-spec validation, ASCII default with SMTPUTF8 explicitly enabled); **the loop** (Compose → Valid static validation → Submit session → attachments streamed and encoded with zero whole-message copies throughout). The layer map: from `xrtSmtpSubmit` (one line) down to `CommandWrite` (bytes), every level free to drill into.

## Introduction

"Send an HTML mail with attachments" in the hand-made MIME era was twenty lines of glue: pick the multipart structure (mixed wrapping related wrapping alternative?), generate non-colliding boundaries, QP the body, Base64 the attachments with line folding, encoded-word the Chinese Subject, compute Content-Type… Chapter 116 gave primitives for every one — but should common combinations be hand-spliced every time? `mail_compose`'s answer is **structured input + automatic selection**: you declare "this is the body, this is the HTML version, these are two inline images, these are three attachments" — the structure is **automatically derived** from the content (three-layer nesting assembled automatically, boundaries auto-generated and distinct), encodings automatic by type (text QP / attachments Base64 in chunks), and you inject Date/Message-ID/boundary only when you need **deterministic output** (testing/archiving).

Bcc's handling is a sample of security design: **the blind-copy address enters only the SMTP submission layer's envelope recipients** (RCPT TO has it) and **never enters the RFC message** (the To/Cc headers don't) — the protocol-level correct implementation of "blind copy"; skipping this layer (writing Bcc into the headers) is just carbon copy, a privacy incident where recipients see each other.

## Concepts

### Compose: description, validation, and two outlets

```diagram flow
- Describe: xmailmessage (From/Reply-To/To/Cc/Bcc/UTF-8 Subject/Text/Html/
  inline resources/attachments/custom fields) - all borrowing caller data
- Validate: xrtMailMessageValid - checks the description only (no random values generated, no allocation, no sink called)
  - "every static input error found before the first network write"
- Outlet one: xrtMailComposeWrite(description, sink, ...) - write straight to a stream (attachments encoded into the sink in chunks)
- Outlet two: xrtMailCompose(description, &length) - the complete message (released with xrtFree)
```

**The key to deterministic output**: Date/Message-ID/the three boundaries may be provided by the caller — left empty, they use current UTC / secure randomness / secure randomness (with the Message-ID domain empty, derived from From). Testing and archiving inject fixed values → output reproducible byte for byte; production leaves them empty → secure randomness. **Custom fields cannot override the structural fields Compose manages** — ownership of the structural fields (Content-Type/boundary/encoding declarations) belongs to the composition layer; your added fields cannot break the structure.

### Three-layer automatic multipart selection

| Content combination | Structure |
| --- | --- |
| plain attachments present | `multipart/mixed` |
| plain text + HTML coexisting | `multipart/alternative` |
| HTML with inline resources | `multipart/related` |
| all three together | three-layer nesting — **boundaries must differ from each other** (guaranteed by the composition layer) |

Encoding policy: text QP (Chapter 116's MIME line-width form); attachments **Base64-encoded in fixed small chunks** — "no encoding copy that grows with attachment size" (a 10 MB attachment's encoding peak = the chunk, not 13 MB). This is Chapter 108's streaming-body philosophy landing at the encoding layer.

### Address syntax and the internationalization boundary

`xrtMailAddressValid` validates the addr-spec and lends out local-part/domain: **by default it accepts only ASCII** local-parts and DNS-style domains (quoted local-parts and domain-literals supported); **only with `XMAIL_ADDRESS_SMTPUTF8` explicitly set** does it accept internationalized local-parts / UTF-8 domains — and the SMTP client still decides sending by server capability (syntax passing ≠ the peer accepts). **Layer boundary**: this layer owns message syntax only — path length, DNS, IDNA, envelope limits belong to higher layers (Chapter 89's layering, mail edition: the syntax layer carries no transport policy).

### Submit: the highest-level send loop

`xrtSmtpSubmit(客户端, 消息, 截止, 取消)` (client, message, deadline, cancel): internally assembles MIME (via Compose) → derives the envelope (From→MAIL FROM; To+Cc+**Bcc**→RCPT TO) → sends via DATA/BDAT — one line from "structured description" to "wire". Against Chapter 117's three layers: `CommandWrite` (bytes) ← `smtp_client` (session) ← `submit` (business) — **each level down drills one line, each level up delegates one generation**.

### The send-receive loop panorama

```diagram flow
- Send: describe -> Valid (static validation) -> Compose (structure + encoding) -> Submit (envelope + session + streaming send)
- Receive: POP3/IMAP streaming fetch -> MailMessageParse (views) -> multipart cursor -> encoded-word decode -> business
- Legacy engineering: archive on receipt (Chapter 118's pipeline) + assemble on send (this chapter) - zero whole-message residency at both ends
```

## Examples

### First complete program: one-line Compose

The program below is from `examples/mail/compose` — the shortest path to a UTF-8-subject text mail:

```embed path="extlibs/xmail/examples/mail/compose/main.c" title="extlibs/xmail/examples/mail/compose/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/mail/compose/main.c -lws2_32 -liphlpapi
（输出完整 MIME 报文——含编码词主题与 QP 正文）
```

**What just happened.** (1) `MailMessageInit` + five field assignments (sender/recipient/Subject `示例邮件` (sample mail)/body) — **all borrowed** (string literals go straight into the struct, zero copy, zero tree). (2) `xrtMailCompose` produces the complete message in one line: the Chinese Subject automatically encoded-worded (`=?utf-8?B?...?=`), plain text QP-encoded, Date/Message-ID secure-random — **every decision of hand-made MIME's twenty lines happens automatically here**. (3) After fwrite of the output, `xrtFree` — this outlet is the "complete message" form (for streaming, switch to `ComposeWrite`+sink: attachments enter the sink in chunks). (4) Parse this output back with Chapter 116's `MailMessageParse` — **sending and receiving use the same parsing vocabulary**, the loop is self-consistent.

### Second complete program: Submit

The second program is from `examples/smtp/submit` — the highest-level send:

```embed path="extlibs/xmail/examples/smtp/submit/main.c" title="extlibs/xmail/examples/smtp/submit/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/smtp/submit/main.c -lws2_32 -liphlpapi
（对配置的服务器完成一次提交后正常退出）
```

**What just happened.** (1) The same `xmailmessage` description (From/To/Subject/Text) → `xrtSmtpSubmit(pClient, &Message, iDeadline, NULL)` in one line — internally Compose + envelope derivation + session commands + streaming send. (2) Deadline and cancellation run throughout (Chapter 117's blocking shape unchanged — the discipline holds as layers rise). (3) Against Chapter 117's client sample (self-supplied complete message): that one is "I control every byte", this one is "I describe business intent" — **the same session, two altitudes**. The attachment version just fills the `Attachments` array to get multipart/mixed — the composition layer upgrades the structure automatically.

## Contracts

- **Borrowed description**: message/attachment all borrowed — zero hidden links / zero dictionaries / zero recursive owned trees.
- **Valid is pure validation**: generates no random values, allocates nothing, calls no sink — every static error found before the first network write.
- **Two outlets**: ComposeWrite writes straight to a sink (attachments encoded in chunks) / Compose yields the complete message (xrtFree).
- **Structure automatic**: three layers mixed/alternative/related combined per content; boundaries distinct, guaranteed by the layer.
- **Determinism keys**: Date/Message-ID/boundaries injectable (empty = secure random); the Message-ID domain derived from From.
- **Field boundary**: custom fields cannot override structural fields — structural ownership belongs to the composition layer.
- **Encoding policy**: text QP; attachments fixed-chunk Base64 — zero encoding copies growing with size.
- **Bcc semantics**: envelope only, never the RFC message — protocol-level correct blind copy.
- **Addresses ASCII by default**: SMTPUTF8 explicit; syntax passing ≠ deliverable (capability judgment belongs to the protocol layer).
- **The Submit layer**: description → assembly → envelope → session in one line; deadline/cancellation throughout.

## Pitfalls

### Pitfall 1: writing the Bcc recipient into a custom field

Symptom: the "blind copy" becomes a public copy — all recipients see each other in the headers; a privacy incident.

Cause: Bcc's semantics are implemented by **consumption at the submission layer** (envelope recipients) — its absence from the message headers is a feature. Adding the Bcc address as a custom field = hand-writing a carbon copy.

```c bad
message.Bcc = &Secret;
add_custom_field(&message, "Bcc", "secret@x.com");  /* into the message: public copy */
```

```c good
message.Bcc = &Secret;   /* set the structural field only */
/* Submit derives the envelope: RCPT TO includes secret; no Bcc in the headers - a true blind copy */
```

### Pitfall 2: attachments through Compose (not Write) with memory complaints

Symptom: a 50 MB attachment through `Compose`'s complete-message outlet — memory holds the original and the complete message simultaneously.

Cause: the two outlets differ semantically: `Compose` is "give me the complete message" (necessarily aggregating); for streaming use `ComposeWrite` — attachments encode in fixed small chunks straight into the sink (network stream/file); the peak = the chunk.

```c bad
str raw = xrtMailCompose(&Msg, &n);   /* 50MB aggregated: doubled peak */
send_all(raw);
```

```c good
/* ComposeWrite + a streaming sink: chunked output goes straight out */
xrtMailComposeWrite(&Msg, stream_sink, pCtx, ...);  /* small chunks straight to the network */
```

### Pitfall 3: test assertions without injecting the determinism keys

Symptom: two Compose outputs fail comparison — Date/Message-ID/boundaries are random each time.

Cause: empty = secure randomness (the correct production default); tests wanting determinism **inject** fixed values — this is a designed key, not a defect.

```c bad
raw1 = xrtMailCompose(&Msg, NULL);
raw2 = xrtMailCompose(&Msg, NULL);
assert(equal(raw1, raw2));   /* random Date/ID: never equal */
```

```c good
Msg.Date = FixedDate; Msg.MessageId = FixedId;
provide_boundaries(&Msg, "t1", "t2", "t3");
raw1 = Compose(&Msg); raw2 = Compose(&Msg);
assert(equal(raw1, raw2));   /* deterministic: byte-identical */
```

## Exercises

### Basic: three-layer structure verification

Construct three descriptions (plain text + HTML + attachments; + inline image) — Compose's output disassembled layer by layer with Chapter 116's cursor, verifying structure and boundary distinctness. Acceptance criteria: structure selection matches the table; the three-layer nesting order is correct.

### Advanced: the Bcc loop experiment

Submit a message with Bcc to a local test server (or Chapter 104's self-built) — capture the wire: RCPT TO includes the blind copy, the DATA message headers carry no Bcc. Acceptance criteria: both pieces of evidence present; the blind-copy address never appears in messages forwarded by recipients either.

### Challenge: a full send-receive chain tool

Compose: ComposeWrite streams a 10 MB attachment (Chapter 117's DATA streaming) → POP3 streaming receipt (Chapter 118) → incremental parsing restores (Chapter 116) → attachment written to disk and hash-checked. Acceptance criteria: memory constant across the chain (MB scale); attachment hashes match; the encoded-word subject round-trips losslessly.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Positioning | the convenience layer for common scenarios: structured description → automatic MIME — every primitive reachable in Chapter 116 |
| Description borrowing | message/attachment zero copy; no hidden links/dictionaries/owned trees |
| Valid | pure static validation — every error found before the first network write |
| Two outlets | ComposeWrite (streaming sink, chunked encoding) / Compose (complete message) |
| Structure automatic | mixed/alternative/related three layers per content; boundary distinctness guaranteed by the layer |
| Determinism keys | Date/Message-ID/boundaries injectable; empty = secure random |
| Encoding | text QP; attachments chunked Base64 — zero copies growing with size |
| Bcc | in the envelope, not the message — protocol-level correct blind copy |
| Addresses | ASCII by default; SMTPUTF8 explicit; syntax ≠ deliverable |
| Submit | a one-line loop; every layer between it and CommandWrite drillable |
