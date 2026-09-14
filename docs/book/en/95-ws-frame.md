---
num: 95
slug: ws-frame
title: WebSocket Frames: Headers, Masking, and Fragment Reassembly
volume: 卷九 Web 协议核心
type: practice
lead: Frame-header packing and three-state parsing, per-segment payload masking, fragmentation reassembly with interleaved control frames and incremental UTF-8 validation — RFC 6455's byte layer.
api: websocket, http
---

## Orientation

Once the upgrade completes (Chapter 94), what runs on the connection is WebSocket frames — this chapter covers the byte layer's everything.**Frame headers**: `xrtWsFrameWrite` writes a frame description into wire bytes, `xrtWsFrameParse` parses a header back in three states (an incremental model isomorphic with Chapter 72's framing);**masking**: client-to-server frames must be masked, `xrtWsMask` does the cyclic exclusive-or by "payload index mod 4" and **advances per segment** (network fragmentation need not wait for a whole frame);**fragment reassembly**: one message may split across frames (the FIN flag ends it), control frames may interleave — the `xrtWsMessageInit/FrameBegin/Payload/FrameEnd` state machine carries **incremental UTF-8 validation** (a Chinese character split by a frame boundary still gets its legality judged). The masking sample uses RFC 6455 §5.7's canonical example — standard-vector anchoring again.

## Introduction

Why does WebSocket mask? The historical answer: against cache poisoning — back then, intermediate devices cached TCP streams as HTTP, and attackers crafted payload data "looking like HTTP response headers" to poison caches. Masking makes client-emitted data unpredictable (a random 4-byte key per frame), so intermediates cannot mistake frame payloads for protocol text. The spec accordingly rules: **client→server must mask, server→client must not** — the directional asymmetry is the protocol's hard rule, enforced at parse time by `xwsframeconfig`'s `Mask` mode (REQUIRED/FORBIDDEN).

Why do messages fragment? Sending a 100 MB message in one frame forces the receiver to see the header first (total length unknown until FIN) before deciding how to process; fragmentation lets the sender produce as a stream (total unknown) and the receiver consume as a stream (no complete-message buffering) — the same motive as HTTP chunked (Chapter 91). The fragmentation protocol: the first frame carries the opcode (TEXT/BINARY), subsequent frames CONTINUATION, the last frame FIN=1;**control frames (Ping/Pong/Close) may — and only may — interleave between fragments** — heartbeats are not blocked during long-message transfer.

## Concepts

### Frame structure and header parsing

One frame = header + payload. The header starts at two bytes: FIN + RSV(3) + Opcode (4 bits) + MASK + length (7 bits; 126 → a following 16-bit, 127 → a following 64-bit extended length); when MASK=1, a 4-byte mask follows. The `xwsframe` structure describes a frame: `Flags` (`XWS_FRAME_FIN`/`XWS_FRAME_MASKED`/RSV bits — nonzero RSV means an extension was negotiated, Chapter 97 deflate), `Opcode` (TEXT/BINARY/CONTINUATION/PING/PONG/CLOSE), `PayloadSize`, `Mask`.

- `xrtWsFrameWrite(帧, 配置, 输出, 容量, &头长)` (frame, config, output, capacity, &header length): writes only the **header** (the payload is yours to send after) — large payloads pair with Vec sends (Chapter 68).
- `xrtWsFrameParse(输入, &解析, 配置, NULL)` (input, &parsed, config, NULL): three states `XWS_FRAME_READY/MORE/ERROR` — an insufficient header waits for more (incremental); the configured Mask mode enforces the direction rule (a client-direction parser rejects unmasked frames).

### Masking: a cyclic exclusive-or advancing per segment

`xrtWsMask(数据, 长度, 掩码, 起始下标)` (data, length, mask, start index): `data[i] ^= mask[(起始+i) % 4]` (start) — **in place**; masking and unmasking are the same function (the operation is self-inverse). The fourth parameter is the elegance: when data arrives in segments, the first runs from 0, the next from "previous segment's length" — **the offset continues, no waiting for a whole frame**. `XWS_MASK_SIZE` is always 4.

### Fragment reassembly and control-frame bypass

```diagram state
消息开始 -> 消息中: 首帧 FrameBegin（opcode 定类型）
消息中 -> 消息中: Payload × N + FrameEnd（该帧完）
消息中 -> 消息中: 控制帧穿插 → 被状态机旁路（不当消息数据）
消息中 -> 消息完成: FIN 帧的 FrameEnd（校验全过）
```

`xrtWsMessageInit(状态, 可选配置)` (state, optional config) builds the state; every frame walks the three steps `FrameBegin(帧头) → Payload(负载) → FrameEnd` (header, payload). The state machine does four things: **fragment chaining** (the legality of CONTINUATION opcodes); **control-frame bypass** (interleaved Ping/Pong/Close are ignored — the caller handles them outside the message state machine); **incremental UTF-8 validation** (TEXT-message legality judged across frames — when a multi-byte character is split by a frame boundary, the leading part hangs in the state waiting for the trailing part to complete the judgment); **the completion callback** (the message-complete Info: type/total length). The message config can set a maximum message length — defense against malicious infinite fragmentation.

### The three-level comparison: Chapter 72 framing, Chapter 91 HTTP framing, this chapter

Chapter 72 is generic length-prefix/line framing (custom protocols); Chapter 91 is HTTP body framing (three delimittings + smuggling defense); this chapter is WebSocket framing (masking + direction rules + control-frame interleaving + fragment reassembly). The three levels share one design vocabulary: three-state returns, failure-does-not-advance, incremental absorption of sticky and partial packets — **having learned the first two levels, this chapter's only new things are the protocol-specific rules**.

## Examples

### First complete program: header round trip and segmented unmasking

The program below is from `examples/websocket/frame/main.c` — server-direction header parsing and the RFC masking sample:

```embed path="examples/websocket/frame/main.c" title="examples/websocket/frame/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/frame/main.c -lws2_32 -liphlpapi
opcode=1 payload=5 text=Hello
```

**What just happened.** (1) A frame is constructed: FIN+MASKED, TEXT, 5-byte payload, mask `37 FA 21 3D` (RFC 6455 §5.7's canonical sample); `Config.Mask = XWS_MASK_REQUIRED` is the server direction (received frames must be masked). (2) `FrameWrite` produces only the 7-byte header; `xrtWsMask` masks the 5-byte payload in place. (3) `FrameParse` parses the header back in three states (READY) — `Parsed.Opcode=1` (TEXT), payload length 5, mask restored. (4) **Segmented unmasking**: the first 2 bytes start from 0, the last 3 from 2 — the continuing offset decodes `Hello`. This is the proof that "network fragmentation need not wait for a whole frame": the mask stream's position is decided by the cumulative offset, independent of chunk boundaries.

### Second complete program: a Chinese character split and control-frame interleaving

The second program is from `examples/websocket/message/main.c` — fragmentation reassembly's edge case:

```embed path="examples/websocket/message/main.c" title="examples/websocket/message/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/message/main.c -lws2_32 -liphlpapi
message complete
```

**What just happened.** (1) The split point is deliberately wicked: first frame `{A, 0xE4, 0xB8}`, last frame `{0xAD, B}` — the three UTF-8 bytes `E4 B8 AD` of the sample Chinese character are split in half by the frame boundary. (2) First frame opcode=TEXT (message begins), last frame opcode=CONTINUATION + FIN (message ends) — two FrameBegin/Payload/FrameEnd cycles. (3) The state machine's **incremental UTF-8 validation** shows itself: at the first frame's end, 0xE4 0xB8 is an incomplete sequence — no error, the state hangs waiting for the next stretch; the last frame supplies 0xAD and it is complete and legal, the message passes (assembling `A中B`). A per-frame independent validation would misreport here — that is why "cross-frame state" exists. (4) The control-frame interleaving bypass semantics are covered in the test vectors (tests) — the message state machine directly ignores an interleaved Ping frame, leaving TEXT reassembly untouched.

## Contracts

- **Direction rule**: client→server must mask, server→client must not; the `Mask` config (REQUIRED/FORBIDDEN) is enforced at parse time — a direction-violating frame is rejected outright.
- **Header three states**: READY/MORE/ERROR; MORE consumes nothing and reports no error; failure does not advance (as Chapters 72/91).
- **Write produces only the header**: the payload is sent by the caller (masking your own or advancing per segment); capacity atomic.
- **Mask advancing**: `xrtWsMask` transforms in place, the start index continues — independent of chunk boundaries; XWS_MASK_SIZE=4.
- **Fragmentation protocol**: the first frame sets the opcode, subsequent frames CONTINUATION, the last frame FIN; control frames may only interleave between fragments (not embed mid-message as data).
- **Control-frame bypass**: the message state machine ignores interleaved control frames — the caller handles Ping/Pong/Close outside the message stream.
- **Incremental UTF-8**: TEXT-message legality is judged across frames — multi-byte sequences crossing frame boundaries hang in state awaiting completion; no misreporting.
- **Length defense**: the message config caps maximum message length; frame-level PayloadSize over the cap is rejected (a config item).
- **RSV bits**: nonzero RSV means an extension (Chapter 97's permessage-deflate) — receiving nonzero RSV without a negotiated extension is a protocol error.
- **Trimming**: `WEBSOCKET_FRAME`/`WEBSOCKET_MESSAGE` frame and message layers are independent of the Stream layer (Chapter 96) — usable without a transport.

## Pitfalls

### Pitfall 1: masking each segment from 0

Symptom: with fragmented arrival, the decoded data is "partly right, partly garbled" — the first segment correct, the later ones scrambled, and the scrambling pattern shifts with chunk boundaries.

Cause: the mask cycle's phase is the **absolute index into the whole frame's payload**. Restarting the second segment from 0 misplaces the phase — the result is systematically wrong.

```c bad
recv(seg1); xrtWsMask(seg1, n1, Mask, 0);
recv(seg2); xrtWsMask(seg2, n2, Mask, 0);   /* phase reset: scrambled */
```

```c good
size_t iOff = 0;
recv(seg1); xrtWsMask(seg1, n1, Mask, iOff); iOff += n1;
recv(seg2); xrtWsMask(seg2, n2, Mask, iOff); /* offset continues */
```

### Pitfall 2: feeding the message state machine awry when control frames interleave

Symptom: mid-transfer of a long fragmented message a Ping arrives, and the receiver's reassembly reports "unexpected opcode" — the message breaks.

Cause: control-frame interleaving is a protocol feature (heartbeats must not be blocked by big messages). The message state machine is designed to bypass them — but **the caller must triage correctly**: if the opcode is a control frame, don't feed it to the message state machine (or feed it and trust the bypass — pick one, don't mix).

```c bad
while ( parse_frame(&Frame) == READY ) {
	message_frame_begin(&State, &Frame);  /* Ping fed too: state scrambled */
	...
}
```

```c good
while ( parse_frame(&Frame) == READY ) {
	if ( is_control(Frame.Opcode) ) {
		handle_control(&Frame);   /* Ping/Pong/Close on a separate path */
		continue;
	}
	message_frame_begin(&State, &Frame);   /* only data frames enter reassembly */
	...
}
```

### Pitfall 3: per-frame independent UTF-8 validation

Symptom: legal messages misreported illegal — precisely when a Chinese character/emoji crosses a frame boundary; or worse, "fixing it" by turning validation off (letting illegal text through).

Cause: UTF-8 is a multi-byte sequence; legality judgment is natively cross-frame. Per-frame validation treats incomplete sequences as illegal; disabling validation accepts illegal text (the protocol requires TEXT to be legal UTF-8).

```c bad
on_frame_payload(seg) {
	if ( !utf8_valid(seg) ) {   /* a frame boundary splits a character: misreport */
		close_protocol_error();
	}
}
```

```c good
/* the message state machine has incremental validation built in: legal only when whole after cross-frame assembly */
xrtWsMessagePayload(&State, Seg, NULL);   /* validation is the state machine's business */
/* if anything was illegal, the FrameEnd/Init paths have already errored per protocol */
```

## Exercises

### Basic: the four-direction mask matrix

Using the RFC sample mask on `Hello`, do four splits (1+4, 2+3, 3+2, 4+1): mask per segment then unmask per segment, verifying full restoration. Acceptance criteria: all four splits produce identical output; the start-offset parameters correspond to cumulative lengths.

### Advanced: a large-message streaming sender

Implement `send_text_streamed(write_fn, 数据, 总长, 块大小)` (write_fn, data, total, chunk size): first frame TEXT (no FIN) + N CONTINUATION frames + last frame FIN — each frame FrameWrites the header + write_fn the payload, mocking streaming production. Acceptance criteria: the receiver's message state machine reassembles the complete original (including a multi-byte character crossing frames precisely); chunk sizes 1 and 4096 give identical results.

### Challenge: a bare frame-level echo-server core

Without Chapter 96's Stream layer, hand-build the frame loop on Chapter 67's TCP event surface: receive → FrameParse three states → unmask (advancing per segment) → control-frame triage (Ping auto-Ponged) → data frames into message reassembly → complete messages FrameWrite-n back (server direction, unmasked) + the mask rule enforced (unmasked frames rejected). Acceptance criteria: successful text exchange against a browser/`wscat`; interleaved Pings don't disturb big messages; unmasked frames rejected with Close 1002.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Frame header | FIN+RSV+Opcode+MASK+length (7/16/64-bit extended); Write produces only the header, capacity atomic |
| Three-state parsing | READY/MORE/ERROR; MORE waits for data without consuming; the direction rule enforced by the Mask config |
| Masking | client→server mandatory, the reverse must not; cyclic exclusive-or mod 4; in-place, self-inverse |
| Segmented masking | start index = cumulative payload offset - independent of chunk boundaries; restarting each segment from 0 is the classic error |
| Fragmentation protocol | first frame sets the opcode → CONTINUATION×N → FIN terminates; streaming favors both ends |
| Control-frame interleaving | Ping/Pong/Close may interleave between fragments; the message state machine bypasses, the caller triages |
| Incremental UTF-8 | TEXT legality judged across frames; sequences crossing frames hang in state - per-frame validation misreports, disabling loses the gate |
| Length defense | message-level maximum + frame-level cap; against infinite-fragmentation attacks |
| RSV bits | nonzero = an extension negotiated (Chapter 97 deflate); receiving it un-negotiated is a protocol error |
| Three-level comparison | 71 generic -> 90 HTTP delimiting -> 94 WS masking/fragmentation: one incremental vocabulary |
