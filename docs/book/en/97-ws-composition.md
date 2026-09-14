---
num: 97
slug: ws-composition
title: WebSocket Composition: The Full Bidirectional Chain
volume: 卷九 Web 协议核心 · 卷九收官
type: practice
lead: Close-code semantics, permessage-deflate negotiation and streaming compression, the full-chain assembly — the closing composition of Volume 9's nine chapters of building blocks.
api: websocket, websocket_stream, http
---

## Orientation

Volume 9's closing chapter. The previous three gave upgrade (93), frames (94), streams (95); this chapter adds the bidirectional channel's last two engineering pieces and strings the whole chain together.**The close protocol**: `xrtWsCloseWrite/CloseParse`'s round trip, the close codes' semantic domains (1000 normal/1001 going away/1002 protocol error...), write-side enforcement (the reason must be legal UTF-8, total length within the cap, reserved codes never reach the wire) — a graceful close is the handshake where "both sides know it's over".**permessage-deflate**: WebSocket's compression extension — the negotiation layer (offer/response strictly distinguished, window bits, context takeover), the `xwsinflater`/`xwsdeflater` streaming objects (fragment-advancing without aggregation, dual caps, RSV1 only on the first frame), and the Stream layer's compressed send path. Finally, "one message's complete journey" strings Volume 9's nine chapters from HTTP parsing to compressed fragmented frames — from Chapter 98 we enter xhttp's high-level world.

## Introduction

Two engineering questions close Volume 9. First: how does a connection "say goodbye properly"? Dropping TCP outright leaves the peer unable to distinguish "normal ending" from "network failure/middlebox truncation" — monitoring alarms and reconnect strategies all fail. The Close control frame carries a status code and reason, making the close a **mutually confirmed protocol event**: the initiator sends Close, the peer answers Close, each enters its final state — the `CloseInfo` digest snapshot tells you "why the peer left". Second: JSON messages are highly repetitive (key names, structure); text compression of this workload saves 70-90% of bandwidth — but HTTP's Content-Encoding approach (Chapter 93) needs redesigning on a bidirectional stream: compression per **message** not per connection, the dictionary reused across messages (context takeover), control frames never compressed — this is permessage-deflate, whose negotiation and streaming handling are both protocol-layer engineering.

## Concepts

### The close handshake and code semantics

```diagram flow
- Initiate: CloseWrite(code, reason) produces the control-frame payload -> Close sent (single-send guarantee)
- Peer: receives Close -> the state machine auto-answers Close (the CloseInfo snapshot goes to the application)
- Final state: both Close exchanges complete -> CLOSED; timeout (no peer answer) forces the wrap-up
```

The close codes' semantic domains fall in three bands.**1000-1003**: the normal-close family (1000 normal, 1001 endpoint going away, 1002 protocol error, 1003 unacceptable data type);**1008-1011**: the error family (1008 policy violation, 1009 message too big, 1010 missing extension, 1011 internal error);**3000-4999**: application-defined (the library does not interpret);**reserved codes** (1005 "no code", 1006 "abnormal closure", 1015 "TLS handshake failure") are **API expressions only, forbidden on the wire** — they describe "situations with no Close frame", and the write side refuses them outright. Payload rules: code 2 bytes + reason (legal UTF-8, total ≤123 — `XWS_CLOSE_PAYLOAD_MAX`) — the write side fails on violations, the read side rejects malformed payloads.

### permessage-deflate: the negotiation layer

The extension name is `permessage-deflate`, with four parameters: `server_max_window_bits`/`client_max_window_bits` (compression window 9-15 — smaller saves memory, compression ratio dips slightly), `server_no_context_takeover`/`client_no_context_takeover` (dictionary reset at message boundaries — saves memory, cross-message ratio drops). The negotiation rules are **strictly enforced** by the negotiation layer: offer and response validated separately, duplicate parameters rejected, illegal window bits rejected, the response may not introduce capabilities the offer never proposed, context takeover consistent at both ends. The answering principle is the **minimal compliant subset** — reply only the necessary parameters (the offer proposes `server_max_window_bits=10`; accepting replies exactly that). `xrtWsDeflateDirection` turns the negotiation result into single-direction runtime parameters — the receive and send directions' parameters may differ.

### The streaming compression objects

`xwsinflater` (decompress) / `xwsdeflater` (compress) are streaming objects, in the same posture as Chapter 29's decompression and Chapter 93's decoding:

- **Fragment advancing**: input and output both per fragment, never aggregating a whole message — big messages keep constant memory.
- **Callback output**: decompressed data goes straight into the caller's buffer or the send queue.
- **Dual caps**: total decompression is bounded by both the message cap and the Inflate cap — compression-bomb defense (3 bytes of input claiming to inflate into 4 GB? The cap intercepts first).
- **no-context-takeover**: when negotiated, the dictionary resets at message boundaries.
- **Capacity planning**: `xrtWsDeflaterBound` gives the upper bound before writing — an input to SendLimit budgeting.
- **Failure paths**: callback failure/OOM/data error have clear reset/abort/destroy semantics.

**The RSV1 rule**: the compression flag RSV1 may appear only on a compressed message's **first data frame** — continuations don't repeat it, control frames are never compressed. This is Chapter 95's "nonzero RSV = extension negotiated" being honored.

### One message's complete journey (Volume 9's panorama)

```diagram flow
- Request arrives: TCP/TLS (66/86) -> RequestParse (89) -> upgrade check + handshake (93)
- After the 101: StreamAttach takes over (95) - the frame state machine goes live (94)
- Message arrives: FrameParse three states -> unmask (segmented phase) -> message reassembly (incremental UTF-8)
- Compressed message: RSV1 -> the inflater streams decompression (dual caps) -> the MessageData view
- Application: consume inside the callback (forward/parse/echo)
- Return trip: Text/Ref/Compressed sends (unified SendLimit) -> fragmented frames + masking (client direction)
- Farewell: Close(code, reason) -> the peer answers Close -> both sides' final-state snapshots
```

Nine chapters' APIs each in place within one message — this is the "building-block structure's" acceptance form.

## Examples

### First complete program: the close-payload round trip

The program below is from `examples/websocket/close/main.c` — writing and parsing back code and reason:

```embed path="examples/websocket/close/main.c" title="examples/websocket/close/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/close/main.c -lws2_32 -liphlpapi
code=1000 reason=shutdown
```

**What just happened.** (1) `CloseWrite(1000, "shutdown")` produces a 9-byte payload (code 2 + reason 8) — the write side's triple enforcement at the entrance: reason legal UTF-8, total ≤123, code not in the reserved band. (2) `CloseParse` parses the round trip into `xwsclose`{Code, Reason view} — code 1000 semantically "normal closure", the reason for humans (logs/audit). (3) Reserved-code experiment: attempting to write 1005/1006/1015 fails outright — they are the API layer's "expressive codes" (the Stream's CloseInfo uses 1006 for "abnormally closed without receiving Close"), not sendable values. This "write-side refusal" blocks the protocol error implementers most often make. (4) The Stream layer's (Chapter 96) `Close` is internally exactly this pair — the layer relation: this chapter is the payload layer, 95 the connection layer.

### Second complete program: the compression-negotiation answer

The second program is from `examples/websocket/deflate/main.c` — from offer to the minimal compliant response:

```embed path="examples/websocket/deflate/main.c" title="examples/websocket/deflate/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/deflate/main.c -lws2_32 -liphlpapi
response=permessage-deflate; server_max_window_bits=10
```

**What just happened.** (1) The offer proposes `server_max_window_bits=10` (a 1 KB server compression window) and notifies that the client may carry `client_max_window_bits`. (2) The answer **accepts 10 and writes back the minimal compliant subset** — only the necessary parameter: `permessage-deflate; server_max_window_bits=10`, without echoing the client-side "capability notice".**Why minimal**: every parameter the response echoes is a **promise** — echoing `client_max_window_bits` promises a window cap; more echoed = more promised = less room to maneuver. The negotiation layer's strict validation (duplicate parameters/illegal values/unoffered capabilities) is all built in. (3) The answer string goes into the `Sec-WebSocket-Extensions` response field — composed with Chapter 94's handshake answer. The companions `deflater`/`inflater` demonstrate the streaming objects running in both directions, and `extension_tour` is the extension layer's complete tour.

## Contracts

- **Close payload**: code 2 bytes (network order) + optional reason (UTF-8); total ≤ `XWS_CLOSE_PAYLOAD_MAX` (125); the write side enforces three rules, the read side rejects malformed.
- **Code semantic domains**: 1000-1003 normal family / 1008-1011 error family / 3000-4999 application-defined / 1005/1006/1015 reserved (API expression only, forbidden on the wire).
- **Close handshake**: Close single-send, peer auto-answer, timeout-forced wrap-up, CloseInfo final-state snapshot (Chapter 96's connection-layer guarantee).
- **Compression negotiation**: offer/response each strictly validated; duplicate/illegal/unoffered means reject; the answer is the minimal compliant subset; `DeflateDirection` converts to single-direction parameters.
- **Compression parameters**: window bits 9-15; context takeover consistent at both ends; no-takeover resets the dictionary at message boundaries.
- **Streaming objects**: fragment-advancing without aggregation; callback output; decompression under the message + Inflate dual caps (bomb defense); `DeflaterBound` capacity planning; clear reset/abort/destroy.
- **RSV1 rule**: the compression flag only on the first data frame; continuations don't repeat; control frames never compressed.
- **Send path**: the `TextCompressed` family creates a Deflater on demand (Chapter 96); the budget includes the compression output upper bound.
- **Trimming**: `WEBSOCKET_CLOSE`/`_DEFLATE`/`_DEFLATER`/`_INFLATER`/`_EXTENSION` independent macros — uncompressed connections carry zero compression code.

## Pitfalls

### Pitfall 1: stuffing reserved codes or over-long reasons into Close

Symptom: `CloseWrite(1006, ...)` fails outright; or stuffing a 200-byte "detailed explanation" as the reason — equally fails.

Cause: 1005/1006/1015 are API values expressing "situations without a Close frame"; the protocol forbids them on the wire; the payload cap is 125 bytes (code 2 + reason 123). Over-long reasons belong in application-layer messages; the Close reason carries only a summary.

```c bad
xrtWsCloseWrite(1006, XRT_STR_LITERAL("abnormal"), ...);  /* reserved code: rejected */
xrtWsCloseWrite(1000, LongParagraph, ...);                 /* over the cap: rejected */
```

```c good
xrtWsCloseWrite(1000, XRT_STR_LITERAL("policy: quota"), ...);
/* a code within the semantic domains; the reason one sentence; details via application messages or logs */
```

### Pitfall 2: the compression answer echoing un-promised parameters

Symptom: a hand-written answer echoes the offer's parameters wholesale — the client initializes as if "the server promised client_max_window_bits", but the server never implemented that window; the compression stream scrambles.

Cause: every response parameter is a promise. The minimal compliant subset is the protocol's safe posture — reply only what you will truly honor.

```c bad
/* echoing the offer back as-is */
response = offer_string;   /* echoed un-promised capabilities */
```

```c good
/* answer with the negotiation layer: the minimal compliant subset and all validation built in */
xrtWsDeflateInit(&Offer);
parse_offer(Fields, &Offer);
accept = deflate_accept(&Offer);   /* replies only the necessary parameters */
```

### Pitfall 3: no Inflate cap — eating the compression bomb

Symptom: a malicious peer sends 3 bytes of compressed fragment claiming to inflate into 4 GB — the decompressor dutifully produces until OOM kills it.

Cause: the compression ratio is an attack surface. The message cap (Chapter 95's config) and the Inflate cap as a **dual constraint** are the protocol stack's standard defense — setting only one leaves a bypass (fragmented messages bypass the message cap; multiple messages bypass the Inflate cap).

```c bad
xrtWsInflaterConfigInit(&Config);    /* build without setting a cap? dangerous */
xrtWsInflaterCreate(&Config, NULL);
feed_everything(&Infl, PeerData);     /* the bomb reaches memory directly */
```

```c good
/* both the message cap and the Inflate cap set explicitly (magnitudes per business: e.g. 1MB / 8MB) */
MessageConfig.MaxMessage = 1024 * 1024;
InflateLimit = 8 * 1024 * 1024;
/* over the cap is a protocol error: answer Close 1009 (message too big) and disconnect */
```

## Exercises

### Basic: the close-code matrix

Five experiments with `CloseWrite/Parse`: 1000 + short reason, 1001 + empty reason, 1005 (reserved), reason with illegal UTF-8, 130-byte reason — the first three verify write-side enforcement, the last two the rejection paths. Acceptance criteria: the legal groups round-trip identically; the illegal groups' errors distinguish the three causes.

### Advanced: hand-assembling the compression chain

Adapt the `deflater/inflater` samples: the send side compresses a 10 KB JSON into an RSV1 frame (first frame RSV1, fragmented, FIN); the receive side decompresses and does reassembly. Tally bytes before/after compression and peak memory (Chapter 6). Acceptance criteria: the round-tripped original is identical; fragmentation boundaries falling mid-compressed-data still work; peak memory independent of message size (streaming).

### Challenge: the echo service's full-feature version

On Chapter 96's echo service add everything: compression negotiation (minimal answer to the offer), compressed send/receive (RSV1 rule), the close protocol (both paths — normal Close 1000 and policy refusal 1008), the message cap plus the Inflate dual cap, Ping/Pong (heartbeats during backpressure included). Test against a browser's WebSocket (developer console). Acceptance criteria: a big message (1 MB text) round-trips correctly compressed; malicious input (over-cap messages/malformed frames/reserved-code Closes) all rejected per protocol with clean closes; the whole chain buffers no whole message.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Close payload | code (2B network order) + UTF-8 reason; ≤125 bytes; the write side's three enforcements, the read side rejects malformed |
| Code semantic domains | 1000-1003 normal / 1008-1011 error / 3000-4999 application / 1005·1006·1015 reserved, forbidden on the wire |
| Close handshake | single send, auto-answer, timeout wrap-up, CloseInfo snapshot |
| Negotiation rules | offer/response each strictly validated; duplicate/illegal/unoffered means reject; the minimal compliant subset answers |
| Negotiation parameters | window bits 9-15; context takeover consistent; no-takeover resets per message |
| Streaming objects | inflater/deflater fragment-advancing, callback output, clear reset/abort/destroy |
| Bomb defense | message cap + Inflate cap dual constraint; over the cap answers 1009 |
| RSV1 rule | the compression flag only on the first data frame; continuations don't repeat; control frames never compressed |
| Capacity planning | DeflaterBound computes the upper bound before entering the SendLimit budget |
| Panorama | one message's journey strings together nine chapters' APIs, 89->93->94->95->96 |
