---
num: 118
slug: xmail-mail
title: Mail Messages and MIME
volume: 卷十一 其他扩展库
type: practice
lead: Message views, CRLF discipline, the multipart cursor, encoded words and transfer encodings — every primitive for mail data, zero trees and zero copies.
api: xmail-mail, xmail-mail_message, xmail-mail_multipart
---

## Orientation

The xmail series opens with the **data layer** — what a mail message is and how to read it. The design stance runs through every module: **no object trees, no copying the message, no forced builder** — `xrtMailMessageParse` yields borrowed views (field array + body boundary), multipart is a streaming cursor, encoded words are "boundary parsing + pluggable charsets". Four blocks: **core** (`mail_core`: the `xrt.mail` error domain, CRLF normalization, boundary syntax predicate); **message view** (`mail_message`: strict CRLF parsing + transfer-encoding body decode); **multipart** (`mail_multipart`: zero-allocation MIME cursor + three-mark construction); **encoded words and encodings** (`mail_word`: RFC 2047's `=?utf-8?B?...?=` words; `mail_codec`: QP/Base64 in MIME line-width form). The three protocol chapters (SMTP/POP3/IMAP) and the composition chapter all build on this layer.

## Introduction

Email is the oldest "document format" on the human network — the 1982 RFC 822 bloodline plus the 1996 MIME extensions; forty years of compatibility baggage live in the field zone: **CRLF is iron law** (messages with bare LF get outright rejected by many servers), **encoded words** (`Subject: =?utf-8?B?...?=` — non-ASCII escaped inside ASCII headers), **multipart** (the boundary-delimited part tree — the structure of attachments), **transfer encodings** (QP/Base64 — binary in an ASCII channel's shape). Every one has lenient/strict divergence among historical implementations, and leniency is precisely injection's breeding ground (header-field injection is the classic path of mail forgery).

xmail's answer is **strict parsing + zero-tree views**: CRLF strict (`MailCrlfWrite` normalizes; the parser demands strict CRLF), encoded words strict (empty charsets/illegal markers/control bytes/over-75-bytes rejected — injection bytes rejected on the spot), multipart strict (only a boundary that **fully matches at a physical line start** is accepted — bare-newline forgery rejected), views zero-copy (borrowing the input; "high-level object trees never block the raw message path" — assemble a tree yourself if you want one; the raw bytes are always directly reachable).

## Concepts

### Core: error domain, CRLF, and boundaries

`mail_core` provides three pieces of infrastructure: the `xrt.mail` stable error domain (configuration/line/field/encoding/charset/address/MIME/protocol/limits each with independent stable codes; underlying XRT Base64/network errors **propagate as-is** — the most specific cause reaches the caller); `xrtMailCrlfWrite/Crlf` (CRLF normalization — two-part/allocating dual forms; explicit length **preserves zero bytes**; overlap rejected); `xrtMailBoundaryValid` (a **pure syntax predicate** depending on no RNG/containers/parsers — boundary legality up to 70 bytes; random generation lives in the separate `mail_id` module).

### Message view: parsing and body decoding

```diagram flow
- Parse: MailMessageParse(message, header byte budget, header count budget, &view)
  - the field zone strictly CRLF, must end with an empty line; the view borrows the input wholesale
- Fields: MailMessageHeader(name, ordinal) - ASCII case-insensitive; duplicate fields kept, not merged (Received duplicates legally)
- Transfer encoding: MailMessageTransfer - the sole Content-Transfer-Encoding;
  missing = 7BIT, duplicate or unknown = error
- Body: MailMessageBody(Write) - 7bit/8bit/binary as-is; QP/Base64 decoded (reusing mail_codec)
```

The default budgets (`HEADER_BYTES_DEFAULT/HEADERS_DEFAULT`) guard against maliciously oversized header zones; `SIZE_MAX` cancels them explicitly — a budget is an explicit decision, not an infinite default. **No automatic interpretation of charset/Content-Type/multipart** — those three continue in `mail_word`/`mail_param`/`mail_multipart` (compositional freedom: stop here if you only need the body; take another layer if you want the MIME structure).

### multipart: streaming cursor and three marks

Read side: `xmailmultipartcursor` holds borrowed Source/Boundary/Preamble/Epilogue and the advance position; `MultipartNext` yields per part an `xmailmultipartview` (Headers/Body borrow the original input). **Strictness**: only a boundary **fully matching at a physical line start** (`--b\r\n`) is accepted; bare newlines, illegal part fields, a missing close delimiter, budget overflow — all rejected. **Part bodies are not decoded** — each part's transfer encoding declares and decodes its own (the message view reused per part). Write side: `xmailmultipartmark`'s three marks (FIRST/NEXT/CLOSE) go through `MultipartMarkWrite` to produce directly sendable delimiter fragments — **the caller writes fields and body straight to the network stream, avoiding whole-message splicing** (big attachments stay out of memory — Chapter 108's streaming bodies, mail edition).

### Encoded words: RFC 2047's boundaries and charset policy

The `=?charset?B/Q?text?=` form is the channel for non-ASCII inside ASCII headers. Three-layer design: **view layer** (`MailWordParse`: zero-allocation boundary reading — Charset/Language/Encoding/body views; RFC 2231's `charset*language` split apart) — the **socket** for charsets xmail doesn't build in (GB18030/Big5/Shift_JIS and other big mapping tables stay out of the default build: take the boundary + raw body, apply your own converter); **encoding** (`WordEncodeWrite`: UTF-8 input — pure ASCII passes as-is, non-ASCII or `=?`-misleadable content encodes into ≤75-byte words, **splitting only at UTF-8 scalar boundaries**; Base64 default / Q for the phrase-safe set); **decoding** (`WordDecodeWrite`: mixed text + encoded words; whitespace/folding between adjacent decodable words ignored, other folding normalized to one space; strict mode builds in UTF-8/ASCII/Latin-1/CP1252 aliases and **rejects injectable control bytes**; RELAXED mode keeps unknown charsets verbatim without guessing).

### Transfer encodings: QP and Base64, MIME edition

`mail_codec` is the MIME specialization over the XRT core (Chapter 28): **QP** (`QpWrite` — line width default 76, `=` reserved before soft breaks, trailing spaces/tabs always encoded; BINARY mode treats CR/LF as ordinary bytes, TEXT mode normalizes to CRLF; decoding strictly rejects broken escapes and supports in-place shrinking); **Base64** (`Base64Write` — groups of at most 57 bytes call the core directly **without building a full copy**, each line ends with CRLF, line width a multiple of 4 in 4..76; decoding additionally allows MIME whitespace only, rejecting non-canonical padding and nonzero trailing bits).

## Examples

### First complete program: parsing and body decoding

The program below is from `examples/mail/message` — the minimal mail-reading loop:

```embed path="extlibs/xmail/examples/mail/message/main.c" title="extlibs/xmail/examples/mail/message/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/mail/message/main.c -lws2_32 -liphlpapi
headers=2 body=hello
```

**What just happened.** (1) The input is a complete message of three header lines + empty line + base64 body — `MailMessageParse` parses in one pass: `HeaderCount=2`, the view borrowing the input (zero copy, zero allocation). (2) `MailMessageTransfer` reads the sole encoding declaration BASE64; `MailMessageBody` decodes `aGVsbG8=` into `hello` — **decoding per the message's declaration**, not guesswork. (3) The two output lines are the data layer's minimal loop: structure (header count) + content (decoded body). Continue? `MailMessageHeader` to look up fields, the multipart cursor to split parts, encoded words to decode the Subject — every step an explicit call, no hidden "parse everything automatically".

### Second complete program: the multipart cursor

The second program is from `examples/mail/multipart` — part traversal:

```embed path="extlibs/xmail/examples/mail/multipart/main.c" title="extlibs/xmail/examples/mail/multipart/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/mail/multipart/main.c -lws2_32 -liphlpapi
body=hello
```

**What just happened.** (1) The input `--b\r\nContent-Type: text/plain\r\n\r\nhello\r\n--b--\r\n`: boundary `b`, one part (header + body), the closing delimiter. (2) `MultipartCursorInit` binds source and boundary (zero budget takes the default part cap) → `MultipartNext` yields the first part — `Part.Body` borrows the input's `hello`. (3) The loop shape is fully isomorphic to the DER/HTTP-frame/query-string cursors (Init→Next→ITEM/END) — the Nth sighting of the "offset iteration, borrowed views" pattern. A real part tree = outer message view + multipart cursor + recursion per part (multipart nested in multipart is the shape of an attachment embedding a mail) — the recursion depth is your explicit loop, not the library's automatic tree.

## Contracts

- **Strict CRLF**: parsing demands strict CRLF; `CrlfWrite` two-part form (overlap rejected/capacity atomic); explicit length preserves zero bytes.
- **Explicit budgets**: header bytes/header count/part count default-limited; `SIZE_MAX` is explicit cancellation, not an infinite default.
- **Views borrow**: message/part/encoded-word views all borrow the input — input-lifetime constraint (Chapter 15's general rule).
- **Duplicates kept**: lookup by order without merging — legally duplicated fields like Received stay visible.
- **Transfer encoding unique**: missing = 7BIT; duplicate/unknown = error (no guessing).
- **multipart strict**: full match at physical line start; bare newline/illegal fields/missing close line/budget excess rejected; part bodies not decoded.
- **Streaming construction**: three marks produce delimiter fragments — fields and body written straight to the network stream, no whole-message splicing.
- **Encoded words' three layers**: Parse (zero-allocation boundary + charset socket) / Encode (UTF-8 input, scalar-boundary splitting, ≤75B) / Decode (strict rejects injection bytes; RELAXED keeps without guessing).
- **Big charsets not built in**: GB18030/Big5/Shift_JIS attach application converters at the view layer — no mapping-table baggage.
- **QP/Base64**: MIME line-width forms; 57-byte groups call the core directly; strict decoding (in-place supported); errors propagate as-is.
- **Boundary predicate**: `BoundaryValid` pure syntax; random generation in a separate module.

## Pitfalls

### Pitfall 1: taking a view out of the input's scope

Symptom: a decoded part body occasionally garbles or crashes — the message buffer was a local variable and the view escaped it.

Cause: all views borrow the input (this chapter's third emphasis) — the message's lifecycle dictates the view's lifecycle. To keep long-term, copy (the allocating entrance of `MailMessageBody` is already the copying form).

```c bad
bytes load_and_parse(cstr sRaw) {
	xmailmessageview V;
	xrtMailMessageParse(sRaw, 0, 0, &V);
	return V 的某个视图;   /* the message is on the stack: dangling on return */
}
```

```c good
/* whoever holds the message holds the view - bind both in one struct */
typedef struct { str sRaw; xmailmessageview View; } loadedmail;
bool load(cstr sPath, loadedmail* pOut) {
	pOut->sRaw = read_file(sPath);
	return xrtMailMessageParse(pOut->sRaw, 0, 0, &pOut->View);
}
```

### Pitfall 2: assembling mail and forgetting CRLF (sending bare LF)

Symptom: local tests pass; real servers reject it or a header field gets absorbed into the body — strict servers treat bare LF as illegal.

Cause: the mail wire is CRLF iron law; your `"\n"` "happens" to pass under lenient local parsing. Normalize through `MailCrlfWrite` before sending.

```c bad
snprintf(Buf, "Subject: hi\n\nbody\n");   /* bare LF */
send(Buf);
```

```c good
/* normalize after generation (or write everything with \r\n directly) */
xrtMailCrlfWrite(Normalized, sizeof(Normalized), Raw, RawSize, &n);
send(Normalized, n);
```

### Pitfall 3: the boundary appears inside the body (delimiter collision)

Symptom: an attachment's content happens to contain a `--boundary` line — parts get "split" and parsing scrambles.

Cause: boundary uniqueness is the composer's responsibility (the RFC's 70-byte cap exists so randomness never collides). A fixed short boundary (like the sample's `b`) is for tests only — production uses `mail_id`'s random generation + `BoundaryValid` validation.

```c bad
#define BOUNDARY "b"   /* fixed short value: any "--b" in an attachment collides */
```

```c good
char Boundary[XMAIL_BOUNDARY_MAX + 1];
mail_boundary_random(Boundary);   /* mail_id random generation */
assert(xrtMailBoundaryValid(view(Boundary)));
```

## Exercises

### Basic: the decoding matrix

Construct messages in all four transfer encodings (7bit/8bit/QP/Base64); parse and decode each, then compare. Acceptance criteria: all four bodies round-trip identically; a duplicated encoding field is rejected.

### Advanced: a two-part message traversal

A multipart message of text/plain + text/html: outer message view → multipart cursor → each part's type field (`mail_param`) and body decoded separately. Acceptance criteria: both parts' types and bodies correct; the variant with the closing delimiter missing is rejected.

### Challenge: encoded-word round trip

A mail with a Chinese Subject + sender name: `WordEncode` (UTF-8 input) → assemble → parse → `WordDecode` round trip. A multi-word long Subject (split past 75 bytes). Acceptance criteria: round trip byte-identical; split points on UTF-8 scalar boundaries (no cut characters); RELAXED mode keeps unknown-charset words verbatim.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Design stance | zero trees, zero copies, zero builder — borrowed views, assemble trees yourself, raw message always reachable |
| Core | xrt.mail error domain / CRLF normalization (preserves zero bytes) / boundary pure-syntax predicate |
| Message view | strict CRLF + empty-line separation; headers looked up in order, not merged; budgets explicit |
| Transfer encoding | sole declaration; missing = 7BIT; QP/Base64 reuse the codec |
| multipart | cursor borrows views; full match at physical line start; three marks stream construction |
| Encoded words | Parse (charset socket) / Encode (scalar boundaries, ≤75B) / Decode (rejects injection) |
| Big charsets | view layer attaches application converters — mapping tables stay out of the default build |
| QP/Base64 | MIME line width; 57-byte groups call the core; strict decoding with in-place support |
| Error propagation | underlying Base64/network errors as-is — the most specific cause reaches the caller |
| Recursive structure | the part tree's depth is an explicit loop, not an automatic tree |
