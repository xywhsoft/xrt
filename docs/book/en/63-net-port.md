---
num: 63
slug: net-port
title: The Event Port and the Five Backends
volume: 卷七 网络
type: practice
lead: The unified abstraction over IOCP/epoll/kqueue/io_uring/select — the readiness and completion dual shape, capability declaration, and control events.
api: net, tcp
---

## Orientation

The event port (`xnetport`) is the network engine's **heart**: five platform backends (IOCP/epoll/kqueue/io_uring/select) unified into one interface, `xrtNetPortBackend` queries the current backend, and `Capabilities` declares the capability set. Two programming shapes (previewed in Chapter 62): **readiness** (Watch readable/writable notifications — epoll/select/kqueue) and **completion** (Connect/Accept/Recv/Send delivering results directly — IOCP/io_uring); plus **control events** (Post custom events, Wake, Cancel of in-flight operations — the three doors that link with schedulers and channels). Most business code uses the port indirectly through the Stream layer (Chapter 66) — this chapter is the reference layer for understanding engine internals and for using the port directly.

## Introduction

Each of the five platforms has its own native event mechanism: Windows IOCP (completion ports — the completion model), Linux epoll (readiness notifications) and io_uring (completion queues — completion), macOS kqueue (readiness), and the cross-platform select fallback. Writing five bare adapters is a network library's classic workload — and the capability differences (IOCP has no readiness, select has no completion) make "one codebase, five platforms" require capability branching.

The XRT port's answer: **one interface + capability declaration** — the interface covers all operations of both shapes (Watch/Unwatch for readiness; the Connect/Accept/Recv/Send family for completion), `Capabilities` (returning bit flags) queries what the current backend supports, and cross-platform code branches on capabilities or moves up to the Stream layer (which branches internally already). The port_tour example's output shows it directly: on Windows `iocp=completion select=readiness` — two ports, one shape each; on Linux epoll and uring fall on either side — platform differences turn from "traps" into "declarations".

## Concepts

### The full landscape of port operations

| Family | Entry points | Shape | Notes |
| --- | --- | --- | --- |
| Introspection | Backend/Capabilities/GetConfig | — | Backend and capability queries |
| readiness | Watch/Unwatch | Readable/writable notifications | Callback when an fd is ready |
| Stream completion | Connect/Accept/ReadProbe/Recv/RecvVec/Send/SendVec | completion | Results delivered directly |
| Datagram completion | RecvFromVec/RecvMsg(Vec)/SendToVec/SendMsgVec | completion | UDP, message-oriented |
| Control | Post/Wake/Cancel/RecvError | — | Custom events / wake / cancel / error pickup |

**The three control doors' uses**: `Post` delivers USER events (external logic injecting into the event stream — timer expiry, configuration changes); `Wake` wakes a waiting port (thread-safe — the landing point of Chapter 60's skeleton Wake channel); `Cancel` cancels an in-flight operation (ending in CANCELLED — the port dialect of Chapter 53's cancellation tree).

### readiness vs completion in depth

```diagram flow
- readiness (epoll/select/kqueue): register interest -> readiness notification -> you read/write
  Responsibility on the caller: read/write timing, buffer management, non-blocking handling
- completion (IOCP/io_uring): submit operation + buffer -> result delivered on completion
  Responsibility on the port: operation queueing, buffer pinning, completion delivery
- Capability declaration: query Capabilities — never assume the backend supports both shapes
```

The most notable difference between the shapes is **buffer ownership**: under readiness the buffer is yours (when ready, you fill/read it); under completion the buffer is **frozen** from submission to completion (the same discipline as Chapter 47's async files — the port edition). On Windows, IOCP is completion-only — Watch is unavailable; select is readiness-only — submitted operations are unavailable; on Linux, epoll and uring sit on opposite sides. **The pattern for using the port directly across platforms**: capability branching (if Capabilities.completion → submit style, else → Watch style) or moving uniformly up to the Stream layer.

### Event types and waiting

The port's waits return **events** (structs carrying a type + associated data): READABLE/WRITABLE (readiness), CONNECT completed / ACCEPT arrived / read-write results (per completion operation), USER (Post), WAKE (Wake), ERROR (platform errors like ICMP unreachable — RecvError gated by platform capability). Waiting forms: blocking wait (next event) and timeout wait (the engine pump's cadence — the underlayer of Chapter 60's PollFor). The event-consumption loop is exactly the skeleton of the engine Worker's main loop.

### The port and the Stream

Chapter 66's Stream sits on top of the port: the Stream's callbacks (Accept/Read/Close) are driven by port events; the Stream internally does dual-shape adaptation (completion backends submit directly; readiness backends read/write after readiness) + the state machine (Chapter 66's four states) + reference counting. **The layer-selection criterion**: need fine control (custom protocol foundation, UDP batch send/receive — the udp_batch example uses the port directly) → this chapter's port; standard connection handling → the Stream layer. The five examples port_epoll/port_iocp/port_kqueue/port_select/port_uring each demonstrate one backend used directly.

## Examples

### Complete program: dual shape and capability declaration

From the repository example `examples/network/port_tour/main.c` — two ports demonstrating one shape each:

```embed path="examples/network/port_tour/main.c" title="examples/network/port_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/port_tour/main.c -lws2_32 -liphlpapi
port: backends iocp=completion select=readiness ok
port: watch -> READY -> unwatch ok
port: tcp connect + accept + readprobe + recv/send ok
port: stream vec recv/send ok
port: dgram recvfromvec/recvmsg/recvmsgvec ok
port: dgram sendtovec/sendmsgvec ok
port: cancel in-flight recv -> CANCELLED ok
port: post USER + wake WAKE ok
port: recv-error gated by platform ok
```

**What just happened.** (1) The direct testimony of `Backend/Capabilities`: `iocp=completion select=readiness` — on Windows, one shape per port (capability declaration prevents assumptions). (2) The readiness route: Watch registers → event READABLE → Unwatch deregisters — three steps of a readiness observation. (3) The completion route, whole family: TCP's Connect/Accept/ReadProbe/Recv/Send (with Vec vector variants — multiple buffers in one submission) and UDP's RecvFrom/SendTo family (datagram-oriented — one datagram per operation). (4) The three control doors verified: an in-flight receive Cancel ends in CANCELLED (Chapter 53's dialect); Post/Wake's custom and wake events arrive. (5) `recv-error gated by platform` — RecvError's platform capability gate (calls outside the capability are refused rather than crashing — declarative design's enforcement).

### Complete program: using a single backend directly

From `examples/network/port_select/main.c` — direct use of a readiness backend:

```embed path="examples/network/port_select/main.c" title="examples/network/port_select/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/port_select/main.c -lws2_32 -liphlpapi
（select 后端的就绪观察输出——跨平台兜底形态）
```

**What just happened.** (1) The single-backend example focuses on the readiness loop: Watch → wait for event → handle → deregister — the minimal port-usage skeleton. (2) select's value is **fallback** (works on any platform) — the landing point for development debugging (environments without a dedicated backend) and teaching; production prefers IOCP/epoll/uring (engine auto-selection). (3) The five port_X examples (epoll/iocp/kqueue/select/uring) share one structure — **the same calls across five backends** is the living proof of the unified interface's value: switching backends costs zero code changes (or only a creation parameter).

## Contracts

- **Capability declaration**: before using the port directly, query Capabilities — never assume both shapes; calls outside capabilities are refused (declarative enforcement).
- **Buffer freeze**: from submission to completion under completion, the buffer must not be touched (the port edition of Chapter 47's discipline).
- **The three control doors**: Post injects USER / Wake wakes cross-thread / Cancel cancels in-flight — mapping one-to-one onto the skeleton's three (offload injection / pump wake / cancellation tree).
- **Vec variants**: RecvVec/SendVec submit multiple buffers at once — the amortizing entry of batched IO (the IO edition of Chapter 21's batching idea).
- **Layer choice**: standard connections move up to the Stream (Chapter 66); custom foundations / UDP batching use the port directly.
- **Event loop**: the port's wait is the engine Worker's main loop — event consumption is the skeleton pump's underlying form.

## Pitfalls

### Pitfall 1: using Watch on a completion backend

Symptoms: the call is refused ("no readiness capability") or looks fine at compile time and fails at runtime — there is no readiness notification on Windows IOCP.

Cause: assuming the backend has both shapes — IOCP is completion-only, select readiness-only; calling without checking capabilities.

```c bad
xnetport* Port = xrtNetPortCreate(...);   /* Windows defaults to IOCP */
xrtNetPortWatch(Port, Fd, EVENTS, Callback, Data);   /* refused — no readiness capability */
```

```c good
uint32 Caps = xrtNetPortCapabilities(Port);   /* bit flags */
if ( Caps & XNET_PORT_CAP_READINESS ) {
	xrtNetPortWatch(Port, Fd, EVENTS, Callback, Data);
} else if ( Caps & XNET_PORT_CAP_COMPLETION ) {
	xrtNetPortRecv(Port, Fd, Buffer, Size);   /* submit style — capability branching */
}
```

### Pitfall 2: touching the buffer after submission

Symptoms: dirty data received or occasional completion errors — the same family as Chapter 47's async-file Pitfall 2; timing-dependent and hard to reproduce.

Cause: under the completion shape the buffer is pinned by the port from submission to completion — overwriting it early is a data race.

```c bad
xrtNetPortRecv(Port, Fd, Buffer, Size);   /* submit a receive */
memset(Buffer, 0, Size);                    /* zeroed immediately — the port is writing into it */
```

```c good
xrtNetPortRecv(Port, Fd, Buffer, Size);
/* buffer frozen until the completion event arrives; to reuse early, switch to a buffer pool (Chapter 22) — each block bound to one IO */
```

## Exercises

### Basic: capability reporter

Create a port → print Backend/Capabilities/GetConfig — run it on every platform you can reach and record the five-backend capability matrix.

### Advanced: readiness echo

A select port + Watch: listen on a UDP socket, recvfrom after readiness and echo back — the minimal complete loop of the readiness model (compare the port_select example).

### Challenge: a dual-shape TCP client

Write the same TCP connection logic twice: a completion edition (IOCP/uring — submit style) and a readiness edition (select/epoll — ready style) — compare code shapes and buffer ownership, and produce a comparison report. Acceptance: both editions run on supporting backends; the buffer-freeze discipline is honored in the completion edition (pooled buffers); the report compares code volume and complexity of the two models.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Five backends | IOCP/epoll/kqueue/select/uring — one interface, capability declaration |
| Dual shape | readiness (ready notification, you read/write) / completion (submit and await results); branch on Capabilities |
| Three control doors | Post USER / Wake wakes / Cancel → CANCELLED — the skeleton trio's landing points |
| Vec family | RecvVec/SendVec submit multiple buffers at once — batched IO |
| Buffer discipline | untouchable during the completion freeze — pool with one IO per block |
| Layer criterion | standard TCP → Stream layer; custom foundation / UDP batching → use the port directly |
| Platform examples | port_epoll/iocp/kqueue/select/uring — five isomorphic direct-use examples |
