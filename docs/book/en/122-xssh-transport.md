---
num: 122
slug: xssh-transport
title: SSH (Part 1): Transport and the Packet Layer
volume: 卷十一 其他扩展库
type: practice
lead: wire encoding, the RFC 4253 binary packet framing, AES-GCM packet encryption, and the buffer-free transport core — the four building blocks at the bottom of the SSH stack.
api: xssh-ssh_wire, xssh-ssh_packet, xssh-ssh_transport_core
---

## Orientation

The SSH series opens. Unlike TLS's (Volume 8) "high-level integration", xssh's design philosophy is **radical layering**: the four bottom building blocks are independent, zero-allocation, and individually testable — **wire** (`ssh_wire`: encoding/decoding of SSH's boolean/uint32/string/mpint basic types); **packet** (`ssh_packet`: the RFC 4253 binary packet framing — length/padding/payload, bound to no cipher and no network); **packet AES-GCM** (`ssh_packet_aes_gcm`: OpenSSH's aes128/256-gcm packet wrapping — the plaintext length header as AAD, in-place encryption, a sixteen-byte tag); **transport core** (`ssh_transport_core`: the buffer-free composition of packet codec + protocol ordering + rekey budgets — synchronous, callback, Future, and coroutine clients drive **one and the same state contract**). The next six chapters all build on this layer.

## Introduction

SSH's wire shape is an "onion": application data (inside channels) wrapped in encrypted packets, encrypted packets wrapped in TCP. The packet framing (RFC 4253 section 6) fixes the onion's shape — `packet_length（4B）| padding_length（1B）| payload | padding` (packet_length (4B) | padding_length (1B) | payload | padding), with padding keeping the whole packet block-aligned (cipher blocks or traffic-analysis resistance). **Why padding callbacks are mandatory**: `xrtSshPacketWrite` requires the caller to provide an `xsshpaddingproc` and never silently uses zeros — the core carries no RNG dependency (trimming freedom); transport uses the session PRNG, tests use deterministic padding.

The transport core's key design is the **three-phase send/receive transaction** (Prepare/Commit/Abort): a send first runs `WritePrepare`, writing the final wire packet into the caller's buffer (at this point sequence/nonce/budget **have not advanced**); once the network queue accepts it, `WriteCommit` advances; on `XNET_RESULT_AGAIN` the same wire packet is kept for retry on writable (**never re-prepared** — a sequence cannot be reused); abandoning runs `WriteAbort`, discarding the newly added bytes. At most one packet pending commit per direction — during backpressure no memory is held and no counters reused.

## Concepts

### wire: SSH's type system

SSH's "serialization format" is just five basic types: `boolean` (1B), `uint32` (4B big-endian), `uint64` (8B), `string` (4B length prefix + bytes), `mpint` (string's integer form — canonical non-negative encoding, surplus sign bits stripped). `xsshwriter`/`xsshreader` are a zero-allocation cursor pair (isomorphic to Chapter 77's DER cursors): `WriterInit` binds the caller's buffer, `WriterWrite*` writes each type, `Size` reports output; on the reader side `ReaderInit/Read*` borrows the input and reads type by type. **Strictness**: truncation rejected, trailing data (at the message layer) rejected, non-canonical mpint rejected — the entire vocabulary of an SSH message parser.

### packet: framing and atomicity

`xrtSshPacketMeasure(载荷长, 块长, &padding长, &packet长)` (payload length, block length, &padding length, &packet length) computes the framing: block length zero uses the RFC minimum 8; explicit block length 8..255; padding at least 4 bytes with the whole packet block-aligned; full wire length `4 + packet_length`. `xrtSshPacketWrite(writer, 载荷, ..., padding回调, 上下文)` (writer, payload, ..., padding callback, context) produces the complete wire packet — the callback writes into a stack scratch area first, committing to the target only on success (**writer and sequence number unchanged on failure**). `xrtSshPacketRead(reader, 预算, ..., &packet视图)` (reader, budget, ..., &packet view) returns an `xsshpacketview` borrowing the input: a short packet gives `XSSH_NEED_MORE` (incremental, waiting for data — isomorphic to the library-wide three states); malformed length/alignment/padding is a protocol error; over budget, `XSSH_ERROR_OVERFLOW`. **Sequence numbers**: uint32 wraps naturally; NULL ignores them (standalone framing tool). **Atomicity**: the reader completes all validation on a copy; only full success advances the input, publishes the view, and increments the sequence — failure never half-advances.

### packet AES-GCM: OpenSSH's AEAD packets

The shared wrapping of `aes128-gcm@openssh.com`/`aes256-gcm@openssh.com`: the 4-byte `packet_length` **stays plaintext and serves as AAD**; `padding_length|payload|padding` is encrypted in place; a 16-byte tag trails. `xsshaesgcm` is **one-directional state** (read and write each initialize their own; the same state cannot be advanced concurrently): key 16/32 bytes, initial IV 12 bytes (first 4 the fixed IV + last 8 a big-endian invocation counter); **the counter increments per packet and stops before UINT64_MAX, never wrapping** (zero tolerance for nonce reuse — Chapter 74's discipline made protocol-level). Four entrances: `AesGcmMeasure` (16-byte-aligned framing computation) / `AesGcmWrite` (builds in the writer's uncommitted region and encrypts in place, zero heap allocation) / `AesGcmRead` (**authenticate first, then decrypt** into the caller's buffer — authentication failure leaves the plaintext output untouched; success with an illegal structure zeroes the decrypted body) / `AesGcmInvocation` (query the next packet's counter). Failures (capacity/truncation/callback/authentication/state exhaustion) advance none of reader, writer, sequence, counter.

### transport core: buffer-free composition and NEWKEYS

```diagram flow
- Send: WritePrepare (final packet into the caller's buffer, no advance) -> queue success -> WriteCommit (advance sequence/nonce/budget)
  -> on AGAIN (XNET_RESULT_AGAIN) keep the original packet and retry -> to abandon, WriteAbort
- Receive: Inspect (read the 4B length header) -> aggregate input by WireSize -> ReadPrepare (authenticate + classify + state checks)
  -> the upper layer parses the payload -> ReadCommit / ReadAbort (an authenticated packet rejected -> core closes, no fake rollback)
- NEWKEYS: after this side's packet is queued the write direction closes, SetWriteAesGcm activates; after the peer's packet authenticates, SetReadAesGcm
  -> strict-kex sequence reset + direction rekey counters zeroed + cipher switch in one call
  -> KexComplete is true only after both directions take effect
```

**ReadAbort's semantics deserve chewing**: the codec has already consumed that packet — rejecting an authenticated packet means the protocol layer no longer trusts; transport closes outright, "it will not fake-rollback the sequence number and keep communicating" (rolling back the sequence = manufacturing a replay window). **Automatic recognition**: Prepare automatically recognizes the three special packet classes KEXINIT/NEWKEYS/USERAUTH_SUCCESS — rekey budgets advance at these boundaries. **The network adaptation boundary**: the send buffer can go straight to `xrtNetStreamSend/SendRef`; receiving may borrow a contiguous region of `xrtNetStreamBuffer` — the adaptation layer manages only backpressure/lifetime/deadline/cancel, **never re-implementing SSH rules**.

## Examples

### First complete program: a packet-framing write-read round trip

The program below is from `examples/packet` — building and reading a minimal IGNORE-style packet:

```embed path="extlibs/xssh/examples/packet/main.c" title="extlibs/xssh/examples/packet/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/packet/main.c -lws2_32 -liphlpapi
（输出 packet=16 payload=8 padding=7 形态的包框架自检结果）
```

**What just happened.** (1) `xsshwriter` binds a 32-byte stack buffer; `xrtSshPacketWrite` writes the 8-byte `\2ignore` payload (message number 2 = SSH_MSG_IGNORE) — `exampleSshPadding`'s deterministic callback produces the padding (sample-only; production passes `xrtSshSecurePadding` or the session PRNG — the contract card says plainly "the sample's deterministic padding must not be used directly in a real SSH transport"). (2) `xrtSshPacketRead` runs twice: the first without an out-param (validation only), the second taking the `xsshpacketview` — the `PacketSize/Payload/Padding` views borrow the input. (3) The budget parameter `8u` is an explicit ceiling — a real transport uses the negotiated limit. The round trip matching means the framing is self-consistent: the SSH edition of Chapter 77's DER round-trip pattern.

### Second complete program: the transport core's zero-burden statement

The second program is from `examples/transport_core` — the core's resource shape:

```embed path="extlibs/xssh/examples/transport_core/main.c" title="extlibs/xssh/examples/transport_core/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/transport_core/main.c -lws2_32 -liphlpapi
（输出 transport-core 结构尺寸与默认最大包长的自检结果）
```

**What just happened.** (1) `xrtSshTransportCoreInit(&Core, XSSH_ROLE_CLIENT, 0, NULL, 0)` initializes a client-role core **on the stack** — the zero-argument form takes the default limits; it prints `sizeof(Core)` and `Codec.MaxPacketSize` — the header comment states the design claim: "holds no network and no buffers". **This sizeof output is itself documentation**: the core is a predictable stack object, not a heap monster. (2) `TransportCoreClear` finishes — the core's cleanup touches no handles (it never opened anything). (3) Synchronous, event-callback, Future, and coroutine clients drive **the same** `Core` — the driver shape is pluggable (all four appear in Chapter 126); the state contract exists in one copy only. Companion samples: `packet_codec` (plaintext/GCM dual mode), `packet_aes_gcm` (GCM state and four entrances), `packet_random`/`transport_tcp_random` (random-source integration), `transport_rekey` (the rekey budget path), `transport_state` (the state machine).

## Contracts

- **wire strictness**: truncation/trailing/non-canonical mpint rejected; zero-allocation cursor pair; writer bytes unchanged on failure.
- **packet framing**: block length 8..255 (zero = default 8); padding ≥4 with the whole packet block-aligned; the wire length is `4+packet_length`.
- **padding callback mandatory**: no silent zero padding; the callback goes to the stack first, then commits; `XSSH_ERROR_CALLBACK` distinguished from protocol errors.
- **reader atomicity**: all copy-validation passes before advancing input/publishing the view/incrementing the sequence; a short packet NEED_MORE; over budget OVERFLOW.
- **GCM state**: one-directional, independently initialized; IV = fixed(4) + counter(8 big-endian); the counter stops at UINT64_MAX without wrapping.
- **GCM atomicity**: authenticate before decrypting; failure moves neither reader/writer/sequence/counter; authentication failure leaves the plaintext untouched; success with an illegal structure zeroes the body.
- **GCM aliasing**: pPlain must not overlap the input/state; the written Payload must not overlap this output region (preserving the in-place encryption path).
- **Send transaction**: Prepare does not advance; Commit advances; AGAIN keeps the original packet for retry (no re-preparing); Abort discards; at most one packet pending per direction.
- **Receive transaction**: Inspect→aggregate→ReadPrepare (authenticate + classify)→ReadCommit/Abort; ReadAbort closes the core (an authenticated packet cannot be rolled back).
- **NEWKEYS**: the write direction closes until SetWriteAesGcm succeeds; strict-kex reset + counter zeroing + cipher switch in one call; KexComplete only when both directions are live.
- **Network boundary**: the adaptation layer manages backpressure/lifetime/deadline/cancel — SSH rules belong to the core, not re-implemented.

## Pitfalls

### Pitfall 1: re-Preparing after AGAIN

Symptom: the peer reports MAC/authentication failure or just disconnects — re-preparing produced a new packet whose sequence/nonce conflicts with the already-queued one.

Cause: Prepare's not advancing the counters exists precisely for AGAIN retries: **keep the same wire packet and retry the enqueue unchanged**. Re-Preparing = rebuilding with new padding/nonce, forking counters against the old packet possibly already partially accepted in the queue.

```c bad
if ( prepare(&Core, Buf) == OK ) {
	if ( enqueue(Buf) == AGAIN ) {
		prepare(&Core, Buf);   /* rebuilt: nonce/sequence fork risk */
		enqueue(Buf);
	}
}
```

```c good
xrtSshTransportCoreWritePrepareWithPadding(&Core, Buf, ...);
if ( enqueue(Buf) == XNET_RESULT_AGAIN ) {
	wait_writable();          /* keep the original packet */
	enqueue(Buf);             /* retry unchanged */
}
/* only a decision not to send takes the Abort */
```

### Pitfall 2: forgetting the max-packet budget at the packet layer

Symptom: a malicious server announces a huge packet — the read grows and grows until OOM.

Cause: `iMaxPacketSize` zero takes the default limit, but **the budget is a defense line, not an option**; even standalone tool scenarios should pass it explicitly (even just the default constant — self-documenting).

```c bad
xrtSshPacketRead(&Reader, 0u, 0u, NULL, &Packet);  /* relies on the default - budget intent unclear */
```

```c good
xrtSshPacketRead(&Reader, XSSH_PACKET_MAX_DEFAULT, 0u,
	NULL, &Packet);   /* explicit budget: the code is the documentation */
```

### Pitfall 3: one GCM state shared by read and write

Symptom: encryption and decryption corrupt each other — packet N's decryption used packet N+1's counter.

Cause: `xsshaesgcm` is **one-directional** state — SSH has independent keys, IVs, and counters per direction; one state serving both equals two directions sharing a counter.

```c bad
xsshaesgcm Gcm;
xrtSshAesGcmInit(&Gcm, Key, IV);
write_packet(&Gcm, ...);   /* used for writing */
read_packet(&Gcm, ...);    /* and reading: counter conflict */
```

```c good
xsshaesgcm WriteGcm, ReadGcm;
xrtSshAesGcmInit(&WriteGcm, C2SKey, C2SIv);  /* client -> server */
xrtSshAesGcmInit(&ReadGcm, S2CKey, S2CIv);   /* server -> client */
```

## Exercises

### Basic: round trips of the five wire types

With writer/reader, write and read one each of boolean/uint32/uint64/string/mpint; verify round trips and byte shapes (against RFC 4251's encoding examples). Acceptance criteria: mpint's canonical encoding (sign-bit stripping) derivable by hand; truncated inputs rejected.

### Advanced: packet-framing boundary experiments

Sweep `PacketMeasure` over 0–100-byte payloads, plotting padding/packet_length changes; verify the alignment difference between block lengths 8 and 16. Then construct an over-budget read to verify OVERFLOW. Acceptance criteria: measured values match the RFC 4253 §6 formulas.

### Challenge: a bidirectional GCM channel

Two processes (or two cores in one process) build a GCM channel with KEX-derived per-direction keys: send 1000 packets each way, verify counters are monotonic, and assert that writing is refused after packet UINT64_MAX-1 (state exhaustion — simulate with a small counter). Acceptance criteria: full duplex with zero bad packets; the counter-exhaustion path refuses explicitly instead of wrapping.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Four building blocks | wire (types) → packet (framing) → aes_gcm (encrypted packets) → transport core (composition + ordering + rekey) |
| wire types | boolean/uint32/uint64/string/mpint; zero-allocation cursors; strict rejection |
| Packet framing | 4B length + 1B padding length + payload + padding; block-aligned; wire length = 4+packet_length |
| padding callback | mandatory; stack first, then commit; CALLBACK errors separate from protocol errors |
| reader atomicity | all copy-validation passes before advancing; NEED_MORE/OVERFLOW/protocol-error three states |
| GCM shape | length header plaintext as AAD; payload encrypted in place; 16B tag; counter never wraps |
| GCM atomicity | authenticate before decrypt; any failure moves no counter |
| Send transaction | Prepare (no advance) → Commit / AGAIN keeps / Abort; one packet per direction |
| Receive transaction | Inspect→aggregate→Prepare→Commit/Abort (closes core) |
| NEWKEYS | per-direction activation; strict-kex reset in one call; both live before KexComplete |
| Network boundary | buffers go straight to Send/SendRef/Buffer — the adaptation layer re-implements no SSH rules |
