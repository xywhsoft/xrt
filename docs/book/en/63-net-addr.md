---
num: 63
slug: net-addr
title: The Network Engine and the Address Model
volume: 卷七 网络
type: practice
lead: An overview of the five-backend network engine + the xnetaddr address model: parsing, endpoints, classification, zone IDs, and sockaddr round-trips.
api: net, tcp
---

## Orientation

Volume 7 opens with two things: **a network engine overview** (xnetengine — an architectural bird's-eye view of the five-backend event engine, the industrial carrier of Chapter 61's skeleton) and **the address model** (`xnetaddr` — the unified IPv4/IPv6 address abstraction: parsing, endpoint serialization, classification, zone IDs, sockaddr round-trips). Addresses are the whole volume's foundation — TCP/UDP/DNS/proxies all use `xnetaddr` as currency; the engine overview tells every later chapter which engine layer it lives on.

## Introduction

The first hurdle of network programming is not the socket — it is the **address**: a string like `"[fe80::1%3]:8080"` — IPv6 needs brackets, `%3` is a zone ID (link-local addresses must name which NIC — Windows uses numbers, Linux uses interface names), and representations differ across platforms. Hand-writing sockaddr_in6 fill and parse is every network programmer's initiation nightmare and the number-one cause of failed IPv6 migrations. `xnetaddr` chokes all this down: **one type holds IPv4/IPv6 + port + zone**, parsing and serialization agree across platforms, and classification checks (loopback/private/multicast/link-local) are one per line.

The second thing — the engine overview: the layering of the XRT network engine (Engine → event port → Stream/Listener → protocol layer) and the five backends (IOCP/epoll/kqueue/io_uring/select). Chapter 61's skeleton spoke of "scheduler pump + offloads" — this chapter unfolds the pump's underlayer as the event port (Chapter 64), and the Stream as the engine form of the skeleton's "connection coroutine" (Chapter 67's gold standard).

## Concepts

### The four pieces of the address model

| Operation | Entry points | Notes |
| --- | --- | --- |
| Parse | `xrtNetAddrParse` / `ParseEndpoint` | Text → address; the endpoint form carries the port |
| Serialize | `xrtNetAddrString` / `EndpointString` | Address → text; endpoints get brackets automatically |
| Classify | `IsLoopback/IsPrivate/IsMulticast/IsLinkLocal/...` | One check per family member |
| Native round-trip | `xrtNetAddrToNative` / `FromNative` | Convert to/from sockaddr structures |

**The zone ID** is the address model's hard nut: link-local addresses (fe80::) must name the NIC, or the same address is ambiguous across NICs — parsing preserves the zone, serialization writes it back verbatim (`[fe80::1%3]:8080`), and `IsLinkLocal` is zone-aware — the cross-platform differences are choked down. **Port 0 semantics**: listening with 0 lets the system assign, and `xrtNetListenerLocal` retrieves the real port (the standard posture of Chapter 67's gold standard).

### The engine layering at a glance

```diagram flow
- Engine layer: xnetengine — Worker pool + lifecycle (Create/Start/Stop/Destroy)
- Event port layer: xnetport — five backends (IOCP/epoll/kqueue/io_uring/select) behind one interface
- Stream/Listener layer: the callback model of connections and listening (Chapter 67 gold standard)
- Protocol layer: TCP/UDP/proxy/framing (Chapters 67-70) + upper TLS/HTTP (Volumes 8/9)
```

The layering's value is **freedom of replacement**: the event port's uniform interface lets the engine auto-select the best backend (Windows IOCP, Linux epoll/io_uring, macOS kqueue, select fallback); the business sees only the Stream callback model — zero awareness of backend changes. This carries on Chapter 61's assembly idea that "skeleton components are individually replaceable".

### The readiness and completion dual shape

The event port has two programming models (unfolded in Chapter 64): **readiness** (a "readable/writable now" notification — you go read; the natural form of epoll/select/kqueue) and **completion** ("the read finished" — the result is handed to you; the natural form of IOCP/io_uring). XRT takes both into one interface — capabilities are declared per backend (`xrtNetPortCapabilities`): Windows IOCP is completion-only, Linux epoll readiness-only — **cross-platform network code must branch on capabilities or use the Stream layer (which smooths the difference away)**.

### happy-eyeballs: how address lists are consumed

`xrtNetResolve` returns an **address list** (one domain, many addresses — dual-stack/multi-NIC/load-balancing workload spread) — connection strategies try in order (IPv6 first, falling back to IPv4 — happy-eyeballs). The list model with `xrtNetAddrListCount/Get` traversal is the standard consumption shape; the single-address convenience entry is the list's first-class pick. The resolver's asynchronous form (Future shape) is in the resolver_future example — the skeleton's DNS offload slot from Chapter 61.

## Examples

### Complete program: endpoint parsing and classification

From the repository example `examples/network/address/main.c` — the complete handling of zone IDs:

```embed path="examples/network/address/main.c" title="examples/network/address/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/address/main.c -lws2_32 -liphlpapi
endpoint=[fe80::1%3]:8080
family=6
link-local=yes
```

**What just happened.** (1) `xrtNetAddrParseEndpoint` takes in `[fe80::1%3]:8080` — bracketed IPv6 + `%3` zone ID + port, all parsed by one call. (2) `xrtNetAddrEndpointString` serializes it back verbatim (brackets and zone auto-completed — round-trip consistency). (3) `family=6` — the address-family query (IPv6); (4) `IsLinkLocal` gives the zone-aware verdict — **the complete loop of link-local + zone ID** is the address model's proof of choking down cross-platform differences. The addr_tour example is a full-interface tour: equality/comparison, the whole classification family, IPv6-mapped demapping, and sockaddr round-trips, one set of ok each.

### Complete program: address-list resolution

From `examples/network/dns/main.c` — walking the list of a dual-stack domain:

```embed path="examples/network/dns/main.c" title="examples/network/dns/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/dns/main.c -lws2_32 -liphlpapi
[::1]:443
127.0.0.1:443
```

**What just happened.** (1) `xrtNetResolve("localhost", 443, UNSPEC)` — domain + port + family-unspecified → an **address list** (owning — freed by `xrtNetAddrListDestroy`). (2) `ListCount/ListGet` traversal — localhost is dual-stack (::1 and 127.0.0.1, order may swap); each entry is serialized and printed by `EndpointString`. (3) The list model is exactly happy-eyeballs' input: the connector tries in order until one succeeds — not the assumption "the first address always works". This example belongs to the net module (not a separate DNS module) — Chapter 66 unfolds the async resolver.

## Contracts

- **Type unification**: one `xnetaddr` holds IPv4/IPv6 + port + zone; the whole volume's APIs use it as currency.
- **Zone fidelity**: parsing preserves the zone, serialization writes it back verbatim; link-local checks are zone-aware.
- **Port 0**: listening with 0 lets the system assign and ListenerLocal retrieves — the standard posture for concurrency tests (Chapter 67).
- **List semantics**: resolution returns a list; consume by trying in order (happy-eyeballs), never assuming a single address.
- **Engine layering**: Engine → port → Stream → protocol; business code lives at the Stream callback layer, backends auto-selected.
- **Dual-shape declaration**: readiness/completion per backend capability; cross-platform direct code checks Capabilities first.

### From examples to engineering: the address model's three hosts

**Configuration parsing**: the "0.0.0.0:8080" and "[::]:443" in service config files — `ParseEndpoint` is a fixed link in configuration loading (Chapter 43's configuration assembly pipeline chains to here); test environments with port 0 get the real port reported back by ListenerLocal. **Connection initiation**: resolve to a list → happy-eyeballs tries in order — the fixed opening of Chapter 67's TCP connection chapter; failed addresses are logged (which stack, what error) for network diagnostics. **Network diagnostic tools**: the classification family + the three serialization forms — the iface example and the interface enumeration in this volume's later net-misc chapter are both built on it. The three hosts share one point: **the textual form of an address appears only at boundaries** (config/logs/command line); internally it is always `xnetaddr` passed by value — text↔address conversion is confined to the two boundary spots (Parse at the entrance, String at the exit), zero conversions in between.

### Mapping to Chapter 61's skeleton

Chapter 61 said bolting TCP/TLS/HTTP onto the skeleton yields Volume 7 — this chapter's engine overview completes the first puzzle piece: **scheduler pump → event port** (the skeleton's PollFor ↔ the port's waiting interface); **connection coroutine → Stream callbacks** (the skeleton's xrtCoGo connection coroutine ↔ the Stream's Accept/Read/Close callback table — different expression (straight-line coroutine versus callback table) but the same responsibility carried); **Worker pool → Engine Workers** (the skeleton's task pool ↔ the engine's Worker configuration). The mapping's value: the skeleton's five-item checklist (pump zero-blocking / cancellable waits / three-way final states / aligned lifecycles / observation throughout) applies item by item at every engine layer — Chapter 64's port cancellation, Chapter 67's Stream state machine, Chapter 68's server shutdown — all are the checklist's engine-version enforcement. **The skeleton is Volume 7's map**.

### An engineering checklist for IPv6 migration

XRT's address model makes IPv6 readiness (dual-stack ready) the default rather than a patch — but the road to production still has a checklist to pass. **Address representation**: everything goes through xnetaddr (Pitfall 2's enforcement); config templates listen dual-stack with `[::]` (not 0.0.0.0's v4-only). **List consumption**: happy-eyeballs tries in order (Pitfall 1) — v6 first, v4 fallback. **Zone IDs**: link-local addresses must carry a zone — configuration docs should spell out this environment's NIC identifier rules (numbers/interface names per platform). **Mapped addresses**: a v4 client hitting a dual-stack server may show up as ::ffff:x.y.z.w — Unmap before logging and judging (or v4 clients get misjudged as v6). Pass these four and v6 is a feature switch, not a migration. This checklist gets collected once more at the volume's finale (net-misc) and in Volume 12's engineering chapters.

## Pitfalls

### Pitfall 1: taking only the first address from the list

Symptoms: connection failures on machines with broken IPv6 configuration — the first address is ::1 but v6 routing is dead; works again on another machine — environment-dependent, hard to reproduce.

Cause: assuming "a domain maps to one address" — the list may hold several in unstable order; trying only the first forfeits the fallback chance.

```c bad
xnetaddrlist* List = xrtNetResolve(Host, Port, XNET_FAMILY_UNSPEC);
xnetaddr First;
xrtNetAddrListGet(List, 0, &First);   /* take only the first */
Connect(&First);                       /* v6 broken — the whole connection fails */
```

```c good
xnetaddrlist* List = xrtNetResolve(Host, Port, XNET_FAMILY_UNSPEC);
for ( size_t i = 0; i < xrtNetAddrListCount(List); ++i ) {
	xnetaddr Addr;
	xrtNetAddrListGet(List, i, &Addr);
	if ( TryConnect(&Addr) ) { break; }   /* try in order — v4 fallback happens naturally */
}
```

### Pitfall 2: hand-filling sockaddr to bypass the address model

Symptoms: lost IPv6 zones, byte-order chaos, ifdefs everywhere — the classic triple of hand-written sockaddr_in6 filling.

Cause: bypassing `xnetaddr` to manipulate native structures directly — redoing by hand everything the model choked down.

```c bad
struct sockaddr_in6 Sin;
memset(&Sin, 0, sizeof(Sin));
Sin.sin6_family = AF_INET6;
Sin.sin6_port = htons(8080);
inet_pton(AF_INET6, "fe80::1", &Sin.sin6_addr);
Sin.sin6_scope_id = 3;   /* hand-filled zone — platform differences all yours */
bind(Fd, (struct sockaddr*)&Sin, sizeof(Sin));
```

```c good
xnetaddr Addr;
xrtNetAddrParseEndpoint(&Addr, "[fe80::1%3]:8080", 0);
uint8 Native[64];   /* native sockaddr storage (opaque buffer) */
size_t iSize = 0;
xrtNetAddrToNative(&Addr, Native, &iSize);   /* native round-trip — zone/byte order handled by the model */
BindVia(Native, iSize);
```

## Exercises

### Basic: the round-trip law

For five endpoint forms (v4/v6/v6+zone/address-only/with-port) do parse → serialize → parse again — assert the two parses yield equal addresses (the round-trip consistency law).

### Advanced: the full classification family

Construct one address of each class (unspec/loopback/private/multicast/linklocal/mapped) and print each verdict; for a mapped address (::ffff:1.2.3.4) verify Unmap turns it into v4.

### Challenge: an address-table tool

Implement an `addrinfo 工具` (address-info tool): given any endpoint, output its family/classification/public-or-not plus the three serialization forms (address/endpoint/with-zone); produce a report for ten real-world addresses (local NICs/public DNS/reserved multicast ranges). Acceptance: classifications agree with the system getaddrinfo; zone IDs preserved; every verdict family exercised by tests.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Four pieces | Parse/ParseEndpoint, String/EndpointString, the classification family, ToNative/FromNative |
| Zone ID | mandatory for v6 link-local; parsing preserves, serialization writes back, checks are aware |
| Port 0 | listening gets a system-assigned port + ListenerLocal retrieves it |
| List | Resolve returns a list; try in order (happy-eyeballs), never assume one address |
| Engine layering | Engine → port → Stream → protocol; business lives at the Stream layer |
| Dual shape | readiness ("readable now") / completion ("read done"); Capabilities declare, Stream smooths |
