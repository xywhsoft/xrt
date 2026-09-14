---
num: 15
slug: buffer
title: The Dynamic Buffer: xbuffer
volume: 卷三 容器与数据结构
type: practice
lead: The xarray of the byte world — segmented append, sparse fixed-point writes, zero-copy handoff — the data building block of networking and protocols.
api: buffer
---

## Orientation

`xbuffer` and the previous chapter's `xarray` are fellow students of the same school: `xarray` holds "elements", `xbuffer` holds **bytes**. It is the standard building block of network packet receipt, protocol encoding, and file I/O — data arrives from every direction (variable length, variable timing), `Append` adds to the tail, `Write` writes at a fixed point (auto-zeroing past the end), `Take` takes the assembled content away with **zero copies**. This chapter explains these three actions thoroughly and distinguishes the easily confused pair "logical length" and "physical capacity".

## Introduction

Picture a TLS record decoder: data arrives segmented over TCP, one record possibly split across three packets; the header declares "record body length 512 bytes", and you want to know "how much is missing" when the header arrives, and take the whole record at once when the body completes. With a fixed buffer you must guess the maximum record length; with bare malloc splicing, every growth is an allocation and a copy; with `xbuffer`, `Append` accumulates segments, `Size` compares against the header's length, `Take` takes the complete record — the buffer zeroes and keeps assembling the next. This "accumulate — complete — take" loop is the universal shape of protocol processing, and `xbuffer` was designed for it.

One easily underestimated dividend: buffer zero-and-reuse decouples allocation counts from "connections × peak messages", making them the same order as "concurrent connections" — one buffer per connection, one Take per message, zero new allocations throughout. In Chapter 6's statistics, the pooled-to-backing ratio shows this curve's shape as soon as you run it.

Fixed-point `Write` solves another protocol pain: **fixed layouts** — it makes the natural style of "write the known fields first, backfill the fields that depend on earlier bytes" a two-phase encoding. "Byte 5 is the flag, bytes 10 to 14 the length field" — the body length is unknown when the header arrives; write the flag first, backfill the length later, and the hole in between is auto-zeroed. With a contiguous array you would compute offsets, grow, and zero the gap yourself; `Write` does it in one step.

## Concepts

### The three core actions (memorize them first)

| Action | Semantics | Typical scenario |
| --- | --- | --- |
| `Append` | Add to the end, growing automatically | Receive accumulation, streaming reads |
| `Write` | Fixed-point write, may pass the end, holes zeroed | Fixed-layout protocol headers, offset-based encoding |
| `Take` | Coalesces into a contiguous block and **transfers ownership**, buffer zeroed for reuse | Assemble a complete record and take it for processing |

`Size` and `Data` are public fields — logical length and data pointer. Reads go straight to fields, edits through functions, per last chapter's conventions. Contrast with `xarray`'s `Count`: the array's public field is the element count, the buffer's the byte count — "how many records" versus "how many bytes", a difference that decides their consumers: arrays consume structured records, buffers consume raw byte streams.

### Logical length and physical capacity

```diagram flow
- Logical length Size: bytes written so far; advanced by Append/Write
- Physical capacity: internally held storage; grows only, never shrinks (except Trim)
- Reserve: pre-grow capacity, avoiding repeated allocation as segments arrive
- Trim: return spare capacity; tighten before a long residency
```

`Reserve(4096)` arrives in one step when the magnitude is known — in a receive loop this single line drops allocations from "one per packet" to "one or two for the whole run". `Resize` explicitly changes the logical length (growth zero-fills, truncation cuts). This concept pair exists in every container but is clearest on the byte buffer.

### Segmented storage and the coalescing moment

`xbuffer`'s internal storage may be segmented — consecutive Appends do not necessarily live in one block of memory. The external contract is always "you get a contiguous block when taking": `Take` coalesces before handoff. The design trade-off is clear: **the accumulation phase chases allocation efficiency** (new segments hang on directly, no old data moved), **the take phase alone pays for contiguity** (one coalesce). If every Append maintained global contiguity, every arriving segment would move everything — a disaster in receive scenarios. Conversely, if after every Append you read the whole content, segmentation loses its point — that means you actually need "one buffer per message" rather than "one buffer accumulating many messages".

The relation to Chapter 67's `xnetbuf` is settled here: `xnetbuf` is the network layer's receive buffer (references, returns, pooling — Volume 7 goes deep), `xbuffer` a general byte accumulator. The common protocol pipeline is `xnetbuf` receives segments → copy or reference into `xbuffer` to complete → `Take` out a contiguous block for the parser. Different duties, cooperative, not interchangeable.

### Take's ownership semantics

`Take` returns an **owning** contiguous block (`bytes` pointer + length out-param); the buffer's logical state zeroes and stays reusable; the returned block is freed with `xrtFree`. Note what "coalesce" means: with segmented internal storage, `Take` coalesces once before handing out the contiguous block; this guarantees you always receive contiguous memory — decoders never handle segmentation. This and Chapter 67's network-stream "receive buffer direct handoff" are the same design idea at different layers.

## Examples

### Complete program: append, sparse write, and handoff

From the repository example `examples/containers/buffer/main.c`:

```embed path="examples/containers/buffer/main.c" title="examples/containers/buffer/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/buffer/main.c -lws2_32 -liphlpapi
61 62 63 00 00 7a
```

**What just happened.** (1) `XRT_BYTES_LITERAL("abc")` builds a byte view (trailing zero auto-removed — Chapter 3's convention's counterpart in the byte world); after `Append`, `Size = 3`. (2) `Write(5, "z")` writes at offset 5 — past the then-end of 3, offsets 3 and 4 auto-zeroed: in the dump `61 62 63 00 00 7a` the two `00`s are the holes and `7a` is `'z'`. Fixed-layout protocol headers' "write the flag first, backfill the length" is exactly this posture. (3) `Take` takes all 6 bytes — ownership of a contiguous block returned, the `iSize` out-param carries the length; the buffer zeroes afterward and `Unit` is safe (no double free). (4) The taken block is freed with `xrtFree` — the owning convention fully consistent with Chapter 5.

### Complete program: capacity and editing in full (the buffer's checkup sheet)

From `examples/containers/buffer_tour/main.c`, covering Reserve/Resize/Trim, the edit family, and three take styles:

```embed path="examples/containers/buffer_tour/main.c" title="examples/containers/buffer_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/containers/buffer_tour/main.c -lws2_32 -liphlpapi
buffer: reserve/resize/trim size=2
buffer: edit add+insert+remove+append assign=9
buffer: take set-take=6 create-take=3 from=2
```

**What just happened.** (1) Line one verifies the capacity trio: Reserve pre-grows, Resize sets length explicitly, Trim tightens with `Size` semantics unchanged — note the detail "Size unchanged after Trim": the capacity cut removes only idle space, written content stays as is. (2) Line two verifies the edit family: add, insert, remove, append, and assign (Assign wholesale-replaces content) execute in order with correct values. (3) Line three compares three take postures: `Take` on an existing buffer, construct-then-take on a fresh buffer, and conversion from a view — three paths converging on a contiguous owning block. Ninety percent of network code uses the first: one reused buffer + one Take per message.

### Putting the three actions into a real rhythm

Alone, Append/Write/Take are three functions; inside protocol processing's rhythm they are three beats of one loop. Take "each message = 4-byte length header + body": on any arriving segment, first `Append` into the buffer; use `Size` against "the header's declared length already assembled" to judge a message's completeness; when complete, `Take` it for the parser and the buffer zeroes for the next. In the three-beat loop the buffer is the sole state carrier — no message list, no per-message allocation, no double copying. In Chapter 67 you will see this loop running at a million-per-second cadence inside network-engine callbacks, shaped exactly like the toy version here.

## Contracts

- **Public fields**: `Size` (logical length) and `Data` (data pointer) read directly; edits via functions.
- **Write zero-fill**: holes past the end auto-zero; the logical length jumps to the write's end.
- **Take handoff**: returns an owning contiguous block (freed with `xrtFree`); the buffer zeroes and is reusable; `Unit` never double-frees.
- **No side effects on failure**: a failed edit leaves the buffer untouched; cleanup with `Unit` is always safe.
- **Capacity discipline**: known magnitude, `Reserve` first; before long residency, `Trim`; Trim cuts only idle space, never written content.

## Pitfalls

Both pitfalls orbit one thing: `Size` and `Data` are "snapshots at the moment of reading", and any edit (Append/Write/Take) can stale the snapshot. Understand this and the two pitfalls are one pitfall in two guises.

### Pitfall 1: reading the old Size after Take

Symptoms: post-take processing logic reads zero or wrong lengths; sporadic empty messages.

Cause: `Take` zeroes the buffer for reuse — the pre-call `Size` is stale, already 0 afterward; using the old variable as the length is naturally wrong.

```c bad
size_t iWant = tBuffer.Size;          /* saved the old length */
bytes pBlock = xrtBufferTake(&tBuffer, &iSize, NULL);
process(pBlock, iWant);               /* iWant is the pre-Take length? correct only by coincidence */
```

```c good
bytes pBlock = xrtBufferTake(&tBuffer, &iSize, NULL);
if ( pBlock == NULL ) {
	return false;
}
process(pBlock, iSize);              /* length from Take's out-param — always this take's true length */
```

### Pitfall 2: holding the Data pointer past a growth

Symptoms: same family as last chapter's Pitfall 1 — dangling pointers, random content; high incidence on the buffer's growth paths.

Cause: `Data` points into internal storage, and `Append`/`Write`/`Reserve` can all trigger relocation; the pointer has an expiry, just like the length.

```c bad
bytes pView = tBuffer.Data;            /* noted the pointer */
xrtBufferAppend(&tBuffer, pMore, 4096);/* growth relocates */
consume(pView, tBuffer.Size);          /* pView dangles */
```

```c good
xrtBufferAppend(&tBuffer, pMore, 4096);
consume(tBuffer.Data, tBuffer.Size);  /* take at use; pointer and length same source, same time */
```

## Exercises

### Basic: reproduce the dump

First, with the book closed, write the expected six bytes on paper; then hand-build `61 62 63 00 00 7a` per the main example: append three bytes, write one byte at offset 5, and dump hex byte by byte to verify the zero-filled holes.

### Advanced: a TLV decoder skeleton (protocol world's first toy)

Implement a minimal TLV (type-length-value) accumulator with `xbuffer`: `feed(字节段)` (feed a byte segment) appends; `next()` checks whether a complete record is assembled (a 4-byte header declaring length), and when complete, `Take`s that record with the buffer's remainder carried forward. Hint: Take is all-or-nothing — when you need "take the first N bytes", remember the record boundary first, consume via a view, then process whole.

### Challenge: a receive-performance comparison (let numbers speak)

Write a simulated receive loop: random 64–1500 byte segments, 10 MB total, accumulating under two postures — "no Reserve" and "Reserve(1MB) at startup" — Taking every 64 KB. Compare the two versions' system allocation counts with Chapter 6's statistics. Acceptance: the Reserve version allocates significantly less; both versions' final assembled results are byte-identical.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three actions | `Append` tail-add / `Write` fixed-point zero-fill / `Take` zero-copy handoff |
| Public fields | `Size` logical length, `Data` data pointer; stale after any edit |
| Capacity | `Reserve` pre-grow, `Resize` set length, `Trim` tighten |
| Take semantics | returns an owning block (`xrtFree`), buffer zeroed for reuse |
| Building views | `XRT_BYTES_LITERAL` feeds literals straight into Append/Write, per `XRT_BYTES_LITERAL` |
| The protocol loop | Append accumulates → Size compares → Take takes → buffer reused |
| Division with the array | arrays consume structured records (element count), buffers raw bytes (length) |
| Versus xnetbuf | xnetbuf receives segments (network layer), xbuffer completes (general) — pipeline partners |
| Literals | `XRT_BYTES_LITERAL` auto-drops the trailing zero; Append/Write eat it directly; constant segments coalesce with zero intermediate allocation |
