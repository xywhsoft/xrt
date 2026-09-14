---
num: 70
slug: net-proxy
title: Proxies: SOCKS5 and HTTP CONNECT
volume: 卷七 网络
type: practice
lead: Hosted proxy dialing (whole-chain reclamation), the SOCKS5/HTTP CONNECT incremental handshake protocols, address types, and authentication forms.
api: proxy, net, tcp
---

## Orientation

The proxy module engineers "establishing a connection through a proxy": **hosted dialing** (`xnetproxydial` — a managed chain of DNS + handshake + connection: on failure, every established resource is reclaimed automatically); **SOCKS5** (the incremental handshake state machine of greeting/auth/CONNECT — pure protocol layer); **HTTP CONNECT** (method line + headers + 2xx confirmation); **address types** (IPv4/IPv6/domain — the proxy protocol's specialty: the target can be a domain, resolved proxy-side); and authentication forms (none / username-password). The proxy_dial_http_connect example is the complete practice slot.

## Introduction

Under corporate networks or compliance requirements, outbound connections must go through a proxy: client → proxy → target. Hand-rolling it is three-stage drudgery: connect to the proxy (address resolution + TCP setup) → handshake protocol (SOCKS5 two round-trips or CONNECT one — state machines for two encodings, binary and textual) → only after success do you hold "a channel to the target". If any stage fails — the TCP already built, the handshake already sent — everything must be reclaimed correctly (partial-state cleanup is a bug hotbed).

The proxy module's answer: `xnetproxydial` **hosts the whole chain** — dial (proxy address) → handshake (protocol state machine) → deliver (a Stream to the target); **automatic reclamation on failure** (the meaning of "hosted" — the same promise as the gold standard's tcp_dial example, replayed for the proxy scenario). Protocol details (message encoding/state machine/address types) are wrapped inside the module — the user declares "through which proxy, to which target", and the module does the rest.

## Concepts

### The proxy dialing shapes

| Shape | Entry | Notes |
| --- | --- | --- |
| Configured | proxy config + dial | Type/address/auth all configured |
| Incremental handshake | step-by-step message produce/consume | Pure protocol layer (socks5 example) |
| HTTP CONNECT | method line + confirmation | The textual protocol slot |

**Configured** is the most common shape: declare the proxy (type + address + optional auth) and the target (an xnetaddr or a domain) — one dial delivers a Stream. The **incremental handshake** faces special scenarios (custom handshake insertion, testing/teaching): greeting/CONNECT messages are produced step by step and replies consumed step by step — the state machine laid bare but controllable.

### The SOCKS5 incremental handshake

```diagram flow
- greeting: 05 01 00 (version 5 / 1 method / no auth) -> reply selects the method
- auth (optional): username-password sub-negotiation (RFC 1929)
- CONNECT: 05 01 00 + address type (01 v4 / 03 domain / 04 v6) + target + port
- reply: 05 00 (success) + bound address — from here the channel runs straight to the target
```

**The domain target** is the proxy protocol's essence (the socks5 example's `03 0E 6F 72 69 ...` — 03 = domain type, 0E = 14 bytes, then the domain bytes): the target is handed to the proxy as a domain — **resolved proxy-side** (the client never touches the target's DNS — valuable for intranet domains and leak prevention). Three address types plus a port form the unified `xnetproxyendpoint`.

### HTTP CONNECT

The textual protocol slot: `CONNECT host:port HTTP/1.1` + Host header (+ auth headers) → the proxy replies `2xx` → the channel runs straight through. The choice versus SOCKS5 is usually decided by the environment (firewall policy / the incumbent proxy type) — XRT ships both (the http_connect and socks5 examples are symmetric).

### Authentication and security

**Authentication**: SOCKS5 none / username-password (RFC 1929); HTTP Basic / custom headers. **Channel security**: the proxy itself does not encrypt — TLS over proxy (running the TLS handshake after CONNECT — the Chapter 82 composition); **proxy chains**: theoretically chainable (dial through proxy A to proxy B to the target — a configuration-layer composition).

### The hosted semantics

`xnetproxydial`'s hosting = the proxy edition of the gold standard's tcp_dial: DNS (the proxy's domain) → TCP (to the proxy) → handshake (the protocol) → delivery; on failure at any stage — the `xnetproxydialstate` state object records how far it got, and established resources are fully reclaimed; on success — the Stream's lifecycle is identical to a direct connection (later TLS/HTTP notices no difference — **the proxy is transparent to the layers above**).

## Examples

### Complete program: the SOCKS5 incremental handshake

From the repository example `examples/network/proxy_socks5/main.c` — message generation at the pure protocol layer:

```embed path="examples/network/proxy_socks5/main.c" title="examples/network/proxy_socks5/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/proxy_socks5/main.c -lws2_32 -liphlpapi
greeting: 05 01 00
connect: 05 01 00 03 0E 6F 72 69 ...
```

**What just happened.** (1) The greeting message `05 01 00` — version 5, one method (00 = no auth) — the minimal three-byte hello to put on the wire. (2) The CONNECT message `05 01 00 03 0E 6F 72 69 ...` — 05 version / 01 CONNECT / 00 reserved / 03 domain type / 0E fourteen bytes / the domain's first bytes (ori... — the example's domain target) — the **binary encoding of address types** made directly visible. (3) What's printed is "binary ready for the wire" — the incremental handshake makes the protocol layer's byte stream explicit (a teaching/debugging slot); production code uses the configured dial (protocol layer wrapped). The proxy_tour example is the full-interface tour.

### Complete program: HTTP CONNECT in practice

From `examples/network/proxy_dial_http_connect/main.c` — the complete proxy-dialing chain:

```embed path="examples/network/proxy_dial_http_connect/main.c" title="examples/network/proxy_dial_http_connect/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/proxy_dial_http_connect/main.c -lws2_32 -liphlpapi
usage: proxy_dial <proxy-host> ...
```

**What just happened.** (1) With no arguments it prints usage — this example is designed as live verification against a real proxy (contrasted with the pure-protocol socks5 example — one bare protocol, one full chain). (2) With arguments, the complete chain: proxy address resolution → TCP connection → `CONNECT host:port` sent → 2xx confirmation awaited → on success, the channel → target communication (echo verification). (3) The failure path's hosted reclamation — any step failing clears all resources (the proof point of the "managed" semantics). The proxy_dial example is the isomorphic SOCKS5 practice — the two protocols symmetric.

## Contracts

- **Whole-chain hosting**: DNS + TCP + handshake + delivery in one; on failure, established resources fully reclaimed — tcp_dial's hosted semantics, proxy edition.
- **Transparent channel**: after success, the Stream is indistinguishable from direct — TLS/HTTP above never notices.
- **Domain targets**: resolved proxy-side (the client never touches the target's DNS) — three address types, one encoding.
- **Dual protocol**: SOCKS5 (binary incremental handshake) / HTTP CONNECT (textual) — the environment decides; XRT ships both.
- **Optional authentication**: none / username-password / custom headers — declared in configuration.
- **Channel security is your business**: the proxy does not encrypt — TLS over proxy (the Volume 8 composition slot).

### From examples to engineering: the proxy's three hosts

**The corporate egress** (compliance home turf): an intranet service's outbound via designated proxies — configuration declares the proxy pool (Chapter 43's environment variables or config files), a proxy-availability probe maintains the usable table (the challenge exercise), and dialing goes through the hosted chain. **Multi-region access**: choose the proxy by target (intranet domains via A, public via B) — a target routing table + proxy mapping; the domain target type has extra value here (proxy-side resolution — intranet domains resolved by the right proxy). **Privacy/testing infrastructure**: a local SOCKS5 forwarder (the advanced exercise in full bloom) for traffic observation/injection/replay — the proxy slot of testing infrastructure. The three hosts share a configuration face: proxy-pool management (availability/ordering), target routing (which targets through which proxy), failure policy (retry/degrade-to-direct/fail fast) — this layer is built into the xhttp extension library (Volume 10).

### TLS over the proxy: a Volume 8 composition preview

The proxy channel is unencrypted — sensitive data must use TLS over proxy. Composition preview (unfolded in Volume 8): the Stream delivered by CONNECT/SOCKS5 is indistinguishable from a direct Stream → the TLS layer slips straight on (`xtlssession` wrapping) → HTTP slips on again (Volume 9) — **a three-layer nesting where every layer is oblivious to whether the one below is a proxy or direct**. This transparency is no accident — Chapter 67's Stream contract (a lifecycle identical to direct) is its structural guarantee. The reverse composition also exists: the proxy itself can be TLS (a proxy server requiring a TLS connection — TLS dialing of the proxy address — a configuration-layer extension slot). Read Volume 8 with this composition picture — the proxy is the prototype of a "transport middle layer", and transparency is the middle layer's design paradigm.

### A historical footnote: SOCKS' naming and evolution

SOCKS' name comes from "SOCKetS" (a plural nickname for sockets) — born of 1992's firewall-traversal needs. The evolution: SOCKS4 (TCP + v4 only) → SOCKS4a (domain targets) → SOCKS5 (RFC 1928 — v6/UDP/authentication framework) — every step of proxy protocols answered a real gap in the predecessor. HTTP CONNECT grew from the reverse-proxy demand side (early HTTPS traversal) — the two-protocol duopoly is layered history. The value of the lineage: **every proxy-protocol feature (domain targets/auth/UDP forwarding) corresponds to a class of real deployment needs** — read the protocol by needs, not by bytes. XRT's proxy module covers the client face of both protocols (server-side proxies are deployment infrastructure, not kernel — the advanced exercise's local proxy is a teaching form).

## Pitfalls

### Pitfall 1: using a bare TCP to the proxy as a channel

Symptoms: direct-connection attempts fail or get refused — you only connected the proxy's TCP without the protocol handshake (or sent data before waiting for the CONNECT confirmation — the proxy treats application data as protocol messages and drops/errors).

Cause: a TCP connection to the proxy is only **transport established** — the channel exists only after the protocol handshake confirms it (SOCKS5 two round-trips / CONNECT one confirmation).

```c bad
xnetstream* S = xrtNetStreamConnect(Engine, ProxyAddr, 1, NULL, NULL, NULL);
xrtNetStreamSend(S, TargetData, Size);   /* no handshake — the proxy doesn't know this data */
```

```c good
/* configured dialing — the handshake state machine is wrapped inside the module */
xnetproxydialconfig Dial;
Dial.Type = XNET_PROXY_SOCKS5;
Dial.Proxy = ProxyAddr;
Dial.Target = Target;   /* xnetproxyendpoint (domain form included) */
xnetstream* S = ManagedDial(Engine, &Dial);   /* delivered only after the handshake completes */
```

### Pitfall 2: plaintext credentials on the proxy channel

Symptoms: a security audit — usernames/passwords/business data on the proxy leg are capturable in the clear (visible in packet captures).

Cause: the proxy protocol does not encrypt — SOCKS5 authentication / business data travels plaintext on the proxy leg.

```c bad
/* SOCKS5 username-password auth + plaintext business — visible to the proxy admin and link sniffers */
Dial.Auth = &PlainAuth;
SendSecret(S, Data);
```

```c good
/* sensitive data via TLS over proxy — TLS handshake first after CONNECT (Volume 8), auth also inside TLS */
xnetstream* S = ManagedDial(Engine, &Dial);
/* run TLS on S (the Chapter 82 composition) — the channel's contents are encrypted */
```

## Exercises

### Basic: reproduce message generation

Reproduce the socks5 example's greeting/CONNECT generation; generate one CONNECT for each of the three address types (v4/domain/v6) — verify against hex.

### Advanced: a local loopback proxy

Start a minimal SOCKS5 loopback proxy (greeting reply + CONNECT always succeeds + data passes through) — use an XRT client through it to reach a loopback service. Write both handshake sides yourself — the protocol becomes visible in the middle.

### Challenge: a proxy-availability probe

Implement a proxy-pool probe: N proxies concurrently dial-test a target → record latency/success rate/failure classification (protocol error/timeout/auth refused) → rank the usable table. Acceptance: probe timeout and retry policies configurable; complete failure cause chains (Chapter 4) into logs; the usable table refreshed periodically (Chapter 42); zero leaks in reclaiming dead proxies' resources.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Hosted dialing | DNS + TCP + handshake + delivery in one; on failure, full reclamation |
| Transparent channel | after success the Stream is indistinguishable from direct — the layers above never notice |
| SOCKS5 | greeting → [auth] → CONNECT, binary incremental handshake |
| CONNECT | textual method line + 2xx confirmation — common in firewall environments |
| Address types | 01 v4 / 03 domain (proxy-side resolution) / 04 v6 |
| Authentication | none / username-password / custom headers — declared in configuration |
| Channel security | the proxy doesn't encrypt — TLS over proxy (Volume 8) |
