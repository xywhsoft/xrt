---
num: 71
slug: net-interface
title: Network Interfaces and Local Machine Info
volume: 卷七 网络
type: practice
lead: Interface snapshots, name/index conversion in both directions, the local trio — the cross-platform chokepoint layer that lets a service "see what network it runs on".
api: net_interface, net
---

## Orientation

Chapters 63 through 69 solved "how data flows out": addresses, ports, buffers, DNS, TCP, UDP, proxies. This chapter supplies the other half of the picture — **seeing yourself**: which NICs this machine has, which addresses hang on each, what the loopback interface is called, what the machine's preferred address is. This information is indispensable in four scenarios: startup logs printing the network environment, admin diagnostic pages showing node info, choosing a bind address on multi-NIC machines, and the scope identifier IPv6 link-local addresses must carry. Their shapes on native APIs are famously fragmented — Windows' adapter GUIDs, POSIX's `if_nametoindex`, per-platform naming schemes — and XRT chokes them down into a small module with zero engine dependencies: no Sockets, threads, or DNS introduced, freely combinable with any transport layer.

## Introduction

Suppose you are writing a distributed service's startup flow. Operations requires every startup log to carry "hostname + local address + hardware identifier" so a node can be located instantly among thousands of log lines; the admin endpoint `/health` shows the same set; on a multi-NIC machine the config file gives an interface name `eth0` and you must look up its index for IPv6 multicast; and there is the classic error — `fe80::1` won't connect, because a link-local address is complete only written as `fe80::1%3` (3 being the interface index).

Individually these needs are easy; together they are a cross-platform swamp: a Windows interface's "name" is a GUID, its display name "Ethernet"; Linux's canonical name is `eth0`; the loopback interface is named entirely differently on each side. Hardware addresses come from `GetAdaptersAddresses` on Windows and `ioctl` on POSIX, and the length is not necessarily 6 bytes. Doing the chokepoint yourself costs one branch per platform, while XRT's interface module has already done it: one call yields a consistent snapshot structure, names and indices convert both ways, and the local trio (address/hardware/hostname) comes in three convenience tiers each.

## Concepts

### The interface snapshot: one allocation, all views borrowed

`xrtNetInterfaces` produces a **consistent snapshot** of the system's current interfaces and addresses: one XRT allocation holds the interface array, the address array, names, and hardware addresses; afterwards every field is a borrowed view.

```diagram flow
- Enumerate: xrtNetInterfaces(&List) — one system query builds the snapshot
- Read: Items[i].Name / Addresses[j] — all borrowed from snapshot storage
- Free: xrtNetInterfacesFree(&List) — returns all storage and zeroes the struct
```

The snapshot's value is **consistency**: interfaces added or removed during enumeration cannot misalign entries; the price is that it is a momentary slice, not a change subscription. Per-interface metadata: `IPv4Index`/`IPv6Index` (Windows maintains them separately; POSIX usually identical), `Flags` bit flags, `Mtu`, `Name` (canonical) and `DisplayName` (display) views, `HardwareAddress` raw bytes, and the `Addresses` array — each address carries `PrefixLength` (0..32 for IPv4, 0..128 for IPv6; `XNET_INTERFACE_PREFIX_UNKNOWN` when the platform can't tell).

Two usage disciplines: the order of interfaces and addresses is OS-decided — **never treat order as identity**; views die immediately after freeing, so all reads must precede `Free`.

### Flags: the stable cross-platform intersection

`Flags` exposes only the six flags every platform can observe reliably: `UP`, `RUNNING`, `LOOPBACK`, `BROADCAST`, `POINT_TO_POINT`, `MULTICAST`. A flag with no reliable source on some platform stays unset — **unset does not mean the capability is absent**, only that the platform can't see it. Judging "can join a multicast group" reads `MULTICAST`; "is loopback" reads `LOOPBACK` — each a single bit-and.

### Names and indices: two-way conversion

Interfaces have two identity systems. The **index** is the kernel world's key (IPv6 scope and multicast membership management use it); the **name** is the human world's key (config files write it):

- `xrtNetInterfaceIndex(名称, 族)` returns the index; index zero is not a valid result — a miss reports `XERR_NOT_FOUND`.
- `xrtNetInterfaceName(索引, 族, 缓冲, 容量)` outputs the canonical name in reverse, returning the needed length; a null buffer measures first, and an insufficient buffer truncates safely, zero-terminates, and returns the full needed length — the same two-part convention as Chapter 63's text APIs.

Naming differences are absorbed by the module: on Windows the canonical name comes from the system adapter name (GUID-like) and the display name is the interface alias; on POSIX both are `if_name`. `Index` accepts both canonical and display names, and a `UNSPEC` family query prefers the IPv6 index, falling back to IPv4.

### The local-info trio: three tiers

The diagnostic set "hostname, local address, hardware address" has three usage tiers, and the API splits accordingly:

| Tier | Address | Hardware | Hostname | Fits |
| --- | --- | --- | --- | --- |
| Struct/bytes out-param | `xrtNetLocalAddress` | `xrtNetLocalHardware` | `xrtNetHostName` | Continuing with binary forms (connections, comparisons) |
| Two-part text | `xrtNetLocalAddressText` | `xrtNetLocalHardwareText` | — (HostName is already text) | Fixed buffers, allocation-free contexts |
| String convenience | `xrtNetLocalAddressString` | `xrtNetLocalHardwareString` | `xrtNetHostNameString` | Startup logs and diagnostic pages, printed in one go |

`xrtNetLocalAddress`'s choice is a **deterministic preference query**: it prefers in turn `UP`, `RUNNING`, non-loopback, non-link-local interfaces, with `UNSPEC` preferring IPv6 on ties. It suits startup logs and node reports, but is **not the public egress address and does not represent the default route** — for precise choices, walk `xrtNetInterfaces` and decide yourself. Hardware-address text is uppercase separator-free compact HEX (a 6-byte MAC is 12 characters); for colon-separated and other display formats, read the raw bytes and format yourself. The String tier returns strings freed with `xrtFree`, where null means unavailable (query semantics — no thread error is set).

### An unexpected bonus: IPv6 scope parsing

With this module enabled, Chapter 63's `xrtNetAddrParse` and `xrtNetAddrParseEndpoint` accept both **numeric and interface-name scopes**: `fe80::1%3` and `fe80::1%eth0` both parse, and so does `[fe80::1%eth0]:8080`. The parse result stores only the stable numeric interface index; textual output (`xrtNetAddrText`) keeps its canonical system-query-free form — names change with system configuration, while the index is the persistently comparable identity.

## Examples

### First complete program: enumerating interfaces and address prefixes

The following program comes from `examples/network/interface/main.c`, walking every local interface and printing name, dual-stack indices, MTU, and each address's prefix — a one-shot panorama for multi-NIC inspection:

```embed path="examples/network/interface/main.c" title="examples/network/interface/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/interface/main.c -lws2_32 -liphlpapi
（输出随机器变化：每个接口一行"名称 (显示名) index4=.. index6=.. mtu=.."，
其下每条地址一行"地址/前缀位数"，如 127.0.0.1/8 与 ::1/128）
```

**What just happened.** Three points. (1) `xrtNetInterfaces` enumerates and snapshots in one call — `List.Count` and `List.Items` are the whole interface set; no handles to close one by one. (2) Interface names and display names are both `xstrview`, printed with `%.*s` and `Size`; addresses reuse Chapter 63's `xrtNetAddrText` for text, with `XRT_NPOS` signaling an insufficient buffer. (3) `PrefixLength` appends straight after the address — `127.0.0.1/8`, `192.168.1.10/24` CIDR-style lines are the snapshot's real prefixes. Output varies by machine, so the example header's expected output is a description, not fixed text.

### Second complete program: name/index round-trip and the local trio

The second program comes from `examples/network/interface_tour/main.c`, chaining the name → index → name round-trip and the two-part queries of local address, hardware, and hostname into one checkup:

```embed path="examples/network/interface_tour/main.c" title="examples/network/interface_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/interface_tour/main.c -lws2_32 -liphlpapi
iface: loopback index!=0 name=[[GUID]] ok
iface: local addr=[192.168.0.66] hw=00E04C7D7E66 host=[DESKTOP-DQCMPS1] ok
```

**What just happened.** (1) The loopback interface first tries the POSIX name `loopback4`, falling back to the Windows display name `Loopback Pseudo-Interface 1` — the standard way of "one fallback absorbing cross-platform naming differences at the call site". (2) `InterfaceName`'s two-part form: measure the needed length with `NULL`/0 first, then write and cross-check the two returns; with the canonical name in hand, reverse-lookup the index as a round-trip check (platforms where display and canonical names differ may skip the reverse lookup). (3) The local trio verified in turn: `LocalAddress` struct out-param with family and zero-port checks, `LocalAddressText` output necessarily containing a dot (an IPv4 text trait), `LocalHardware` raw bytes at least 6, `LocalHardwareText` exactly twice the byte count in HEX characters, `HostName` non-empty and writable. Any step failing goes through the unified `Cleanup` returning non-zero — writing the "checkup" as a fail-fast chain reads far better than scattered ifs.

### Third complete program: the diagnostic-page overview (String convenience tier)

The third program comes from `examples/network/local_info/main.c`, gathering the startup-log trio in one go via the String tier, with placeholder text when the hardware address is missing:

```embed path="examples/network/local_info/main.c" title="examples/network/local_info/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/local_info/main.c -lws2_32 -liphlpapi
（输出随机器变化：host = 主机名、address = 本机地址、
hardware = 大写紧凑 HEX 或 unavailable）
```

**What just happened.** The String tier's contract shows here: it returns `str` (freed with `xrtFree`), and `NULL` means unavailable **without setting a thread error** — so a null check is the entire error handling. A missing address or hostname counts as an environmental anomaly returning 1, while the hardware address may legitimately be absent in a pure-loopback environment (containers, NIC-less VMs), printing `unavailable` and carrying on. The three strings are freed together at the end — even a mid-way failure frees them all, since `xrtFree(NULL)` is a no-op needing no per-string null checks.

## Contracts

- **Snapshot ownership**: `Interfaces` owns all storage in one allocation; field views only borrow; `InterfacesFree` returns it and zeroes the struct, callable repeatedly on an empty list; failure leaves the output untouched.
- **No order guarantee**: interface and address order is OS-decided and must not serve as stable identity; identity is the index (numeric) and the canonical name.
- **Name/index**: index zero is not a valid result; `Index` accepts canonical and display names, `UNSPEC` preferring the IPv6 index; `Name` follows the text APIs' two-part convention (measure → write; insufficient truncates safely and returns the full needed length).
- **Convenience-tier boundary**: `LocalAddress` is a deterministic preference query (UP/RUNNING/non-loopback/non-link-local, UNSPEC preferring IPv6) — not the public egress, not the default route, no substitute for listening and source-address policy; for precise choices, walk `Interfaces`.
- **Text tier**: hardware-address text is uppercase compact HEX; the String tier returns strings freed with `xrtFree`, `NULL` meaning unavailable, no thread error set (query semantics).
- **Errors**: failures are reported via `xrtGetError()` — `XERR_NOT_FOUND` (interface/address/hardware absent), `XERR_IO` (system enumeration failure), `XERR_RANGE` + `XNET_ERROR_BUFFER` (insufficient buffer), domain `xrt.net`.
- **Dependencies and trimming**: `XRT_MODULE_NET_INTERFACE` depends only on the network address foundation — no Sockets, Engine, threads, DNS, or containers; `XRT_MODULE_NET_INTERFACE_TEXT` is a separate thin layer adding HEX encoding.

## Pitfalls

### Pitfall 1: treating LocalAddress as the public egress or bind address

Symptoms: the service is unreachable in multi-NIC/NAT environments — the "local address" in the logs belongs to some intranet interface, not the address peers can reach; or binding to it leaves the service inaccessible outside the container.

Cause: `xrtNetLocalAddress` is a **preference pick** (preferring online, running, non-loopback, non-link-local unicast addresses); it knows nothing of the routing table, let alone the public egress behind NAT. Treating it as the "externally reachable address" or the sole bind address mistakes diagnostic information for a routing decision.

```c bad
xnetaddr Addr;
xrtNetLocalAddress(&Addr, XNET_FAMILY_UNSPEC);
/* directly registering Addr as "the address peers can connect to me on" */
register_service(xrtNetAddrText(&Addr, ...));
```

```c good
/* Use LocalAddress for diagnostic display; use configuration or a live connection for external registration;
   for multi-NIC binding, walk the snapshot and decide yourself */
xnetinterfacelist List;
if ( xrtNetInterfaces(&List) ) {
	for ( size_t i = 0; i < List.Count; i++ ) {
		const xnetinterface* pIf = &List.Items[i];
		if ( (pIf->Flags & XNET_INTERFACE_UP) == 0 ) {
			continue;
		}
		/* pick by business rules: family, prefix, multicast capability... */
	}
	xrtNetInterfacesFree(&List);
}
```

### Pitfall 2: using borrowed views after the snapshot is freed

Symptoms: occasional crashes or garbage bytes, often in code that "caches interface info".

Cause: `Items[i].Name.Data`, `Addresses[j]`, `HardwareAddress.Data` all point into the snapshot's internal storage; after `xrtNetInterfacesFree` they all dangle. The snapshot is a value-semantics momentary slice, not a long-held object.

```c bad
xnetinterfacelist List;
xrtNetInterfaces(&List);
const xstrview Name = List.Items[0].Name;   /* borrowed view */
const xnetinterfaceaddress* Addrs = List.Items[0].Addresses;
xrtNetInterfacesFree(&List);                /* storage returned */
printf("%.*s\n", (int)Name.Size, Name.Data); /* dangling read */
```

```c good
xnetinterfacelist List;
if ( xrtNetInterfaces(&List) ) {
	for ( size_t i = 0; i < List.Count; i++ ) {
		const xnetinterface* pIf = &List.Items[i];
		printf("%.*s\n", (int)pIf->Name.Size, pIf->Name.Data);
		/* copy information you must keep into owned storage right now */
	}
	xrtNetInterfacesFree(&List);
}
```

### Pitfall 3: hard-coding the hardware address as 6 bytes

Symptoms: truncated buffers or wrong lengths on platforms with 8-byte EUI-64 addresses or certain virtual interfaces.

Cause: MAC-48's 6 bytes are only the "typical value"; the module contract explicitly says `LocalHardware` returns the **needed byte count** and never hard-codes the length; the `HardwareAddress` view's `Size` is likewise system-decided.

```c bad
uint8 Mac[6];
if ( xrtNetLocalHardware(Mac, sizeof(Mac)) == 0 ) {
	/* assumes 6 bytes: longer addresses silently truncated */
}
```

```c good
size_t iNeed = xrtNetLocalHardware(NULL, 0u);   /* measure first */
if ( (iNeed == 0u) || (iNeed > 16u) ) {
	return 1;
}
uint8 Hardware[16];
size_t iSize = xrtNetLocalHardware(Hardware, iNeed);
/* only iSize == iNeed is a complete address; the HEX text length is 2*iSize */
```

## Exercises

### Basic: loopback interface report

Enumerate the snapshot, find the interface whose `Flags` include `XNET_INTERFACE_LOOPBACK`, and print its name, dual-stack indices, and all addresses. Acceptance: run once each on Windows and Linux (WSL works too), both correctly printing loopback info; the program exits normally after freeing the snapshot.

### Advanced: a multicast-candidate NIC picker

Write a function `uint32 pick_multicast_iface(void)`: walk the snapshot and pick the interface that is "up, multicast-capable, non-loopback, with the largest MTU", returning its IPv6 index, or 0 if none. Hint: test `Flags` for both `XNET_INTERFACE_UP` and `XNET_INTERFACE_MULTICAST`, exclude loopback with `XNET_INTERFACE_LOOPBACK`; don't forget `InterfacesFree` before returning.

### Challenge: a node diagnostics page

Implement `print_diagnostics(void)`: print in one go the hostname (`HostName` two-part), local address (`LocalAddressText` two-part, family `UNSPEC`), hardware address (`LocalHardware` raw bytes self-formatted as colon-separated uppercase HEX, like `00:E0:4C:7D:7E:66`), and the total interface and address counts (from the snapshot). Acceptance: zero leaks across all output (cross-check with Chapter 6's allocation stats); a missing hardware address prints `hardware: unavailable` instead of failing; the two returns of every two-part call cross-checked one by one.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Snapshot | `Interfaces` owns everything in one allocation; views all borrowed; `InterfacesFree` zeroes and is repeatable; order is not identity |
| Interface metadata | dual-stack indices / Flags / Mtu / Name+DisplayName / HardwareAddress / Addresses+PrefixLength |
| Flags | `UP` `RUNNING` `LOOPBACK` `BROADCAST` `POINT_TO_POINT` `MULTICAST`; unset ≠ capability absent |
| Name↔index | `InterfaceIndex` (name → index, 0 = failure) / `InterfaceName` (index → name, two-part); UNSPEC prefers IPv6 |
| Local trio | address (preference query, not the public egress) / hardware (byte count varies) / hostname (local label, not a DNS name) |
| Three tiers | out-param (continue in binary) → Text two-part (allocation-free) → String (logs in one go, `xrtFree` + null check) |
| Hardware text | uppercase compact HEX (6 bytes = 12 chars); format display forms yourself |
| IPv6 scope | with the module enabled, both `fe80::1%3` and `fe80::1%eth0` parse; the stable numeric index is stored |
| Errors | NOT_FOUND (interface/address/hardware) / IO (enumeration) / RANGE+BUFFER (insufficient), domain `xrt.net` |
| Trimming | `XRT_MODULE_NET_INTERFACE` has zero engine dependencies; the TEXT layer separately adds HEX |
