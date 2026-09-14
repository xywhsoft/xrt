---
num: 89
slug: http
title: HTTP Foundations: Fields, Methods, and Targets
volume: 卷九 Web 协议核心
type: practice
lead: The transport-independent foundation layer: field parsing, token iteration, Host splitting, and parameter coding — the vocabulary every http* module shares.
api: http, http_connection
---

## Orientation

Volume 9 starts at HTTP's **foundation layer** — no networking, no versions, just the vocabulary every upper layer shares: "what a field looks like, how methods are recognized, how targets are split". `xrtHttpFieldParse` strictly parses a single header field into borrowed views; `xrtHttpTokenNext` slices token-lists by HTTP rules; Host splitting and parameter coding handle the URL layer's two kinds of structured text; `FieldBlockWrite` writes field views back verbatim. Chapter 90's HTTP/1 parser, Chapter 98's xhttp runtime, your own gateways and middlewares — all stand on this set of zero-allocation primitives. After this chapter you will recognize: HTTP header handling's safety and performance are, at heart, three things — **strict parsing + borrowed views + capacity atomicity**.

## Introduction

What is the innermost operation of writing a reverse proxy? "Change one header and forward" — parse the request head, look up a field by name, change its value or add a field, then write the whole head block back to the byte stream. Sounds simple, but every step carries historical pits: how to treat field-name case? How to slice token-list items with spaces and quotes? Do comments in values (obsoleted by RFC 7230 but still on the wire) blow up the parser? What if the buffer runs out mid-rewrite — write half?

The foundation layer's answer distills into three disciplines. **Strict parsing**: illegal input is rejected on the spot (never silently repaired) — half the HTTP parsing CVEs in the databases stem from "lenient parsing" turning into ambiguity in an attacker's hands. **Borrowed views**: parse results are all views of the original input (zero-copy, zero allocation) — a high-concurrency gateway's per-request memory cost approaches zero. **Capacity atomicity**: a write-back either succeeds completely or touches not one byte — the nastiest intermediate state, "half a message written", is eliminated at the API layer.

## Concepts

### The field model: name, value, and borrowed views

`xhttpfield` is just two views: `Name` and `Value`. `xrtHttpFieldParse(文本, &字段)` (text, &field) strictly parses "name: value" — before the colon is the field name (token character-set validated), after it a value trimmed of surrounding OWS (optional whitespace). The parse result is borrowed: `Name.Data`/`Value.Data` point inside the original text — no allocation, no copy. Three companions: `xrtHttpFieldCount`/`FieldFind`/`FieldNext` (search and traversal over field arrays), and `xrtHttpFieldBlockWrite(字段数组, 数量, 输出, 容量, &长度)` (field array, count, output, capacity, &length) — writing the field array back as a complete "name: value\r\n...\r\n" head block; insufficient capacity returns failure and **writes not a single byte** (atomicity).

### token-list: the iterator of comma culture

HTTP is a comma-separated world: `Connection: keep-alive, Upgrade`, `Accept-Encoding: gzip, deflate`, `Cache-Control: no-cache, no-store`. `xrtHttpTokenNext(值视图, &偏移, &token)` (value view, &offset, &token) is the standard iterator — splitting on `,` and `;`, skipping whitespace, returning `XHTTP_NEXT_ITEM` (got one) / `XHTTP_NEXT_END` (clean end).**How parameters ride**: in `gzip;q=0.5` the `q=0.5` is a parameter after the semicolon — the iterator stops at the semicolon, and parameter parsing belongs to the per-field layers (like Chapter 91's TE q-values). This "tokens first, parameters after" model runs through every list field.

### Methods, targets, and Host splitting

- **Methods**: `xrtHttpMethodParse` recognizes standard methods (GET/POST/...) and extension methods (custom tokens) — strict token charset; `GET;` with a separator is rejected outright.
- **Targets**: the four request-target shapes (origin-form `/path?query`, absolute-form full URL, authority-form, asterisk-form) are distinguished at the parsing layer; this layer provides `xrtHttpTargetParse`'s shape recognition and path/query splitting.
- **Host splitting**: `xrtHttpHostParse(值, &authority)` (value, &authority) yields an `xhttpauthority` view (host/port) — handling domains, IPv4, and **bracketed IPv6** (the brackets of `[2001:db8::1]:8443` must pair, the IPv6 literal is validated group by group — ambiguous inputs like a domain-colon-port are never misjudged).

### Parameter coding: the two roads of the query string

`xrtHttpParamBuild`/`xrtHttpParamWrite` (encoding side) and `xrtHttpParamCount`/`xrtHttpParamNext`/`xrtHttpParamFind` (parsing side) handle `a=1&b=2`-shaped key-value pairs: encoding escapes arbitrary bytes into percent form, decoding restores. Two disciplines: **the semantics of `+` and `%20`** — decoding follows the `application/x-www-form-urlencoded` rules (`+` → space); **spaces and reserved characters** — the encoding side escapes only what is necessary (preserving `&`/`=` as structural separators). Forms, query strings, and Cookie values share this set.

### The isomorphism with Chapter 25's strings and Chapter 78's DER

You will find this chapter's APIs isomorphic with Chapter 25 (Trim/Cut's view pipeline) and Chapter 78 (the DER cursor): **borrowed input → strict validation → view output → convert on demand**. It is XRT's library-wide philosophy of text processing — zero-copy runs through to the protocol layer; "parse once, reference everywhere".

### The parsing pipeline panorama

```diagram flow
- Input: on-the-wire header field text (borrowed buffer, the zero-copy starting point)
- Strict parsing: FieldParse validates the token charset and trims OWS -> Name/Value views
- Structured queries: TokenNext slices lists / HostParse splits the authority / ParamCount counts parameters
- Write-back: FieldBlockWrite produces the canonical head block (capacity atomicity) - the complete read-modify-write loop
```

## Examples

### First complete program: field parsing, token iteration, and write-back

The program below is from `examples/http/base/main.c` — the minimal loop of the foundation trio:

```embed path="examples/http/base/main.c" title="examples/http/base/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/base/main.c -lws2_32 -liphlpapi
keep-alive
Upgrade
Connection: keep-alive, Upgrade
```

**What just happened.** (1) `FieldParse` parses one field into borrowed views (Name=`Connection`, Value=`keep-alive, Upgrade` — the leading whitespace after the colon is trimmed). (2) The `TokenNext` loop slices two tokens: `keep-alive` and `Upgrade` — the offset is held by the caller, the iterator stateless; `NEXT_END` is a clean end, not an error. (3) `FieldBlockWrite` writes the field back verbatim as a complete head block — note the canonical form after the colon in the output (one space): **parsing normalized the OWS, and write-back yields the canonical shape** — after a proxy "reads, modifies, writes", the on-the-wire bytes are deterministic. When capacity runs short it writes not one byte — atomicity lets you safely "try a small buffer first, swap in a bigger one on failure".

### Second complete program: the strictness of Host splitting

The second program is from `examples/http/host/main.c` — the standard split of bracketed IPv6 and port:

```embed path="examples/http/host/main.c" title="examples/http/host/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/host/main.c -lws2_32 -liphlpapi
host=2001:db8::1 port=8443
```

**What just happened.** (1) Input `[2001:db8::1]:8443` splits into host `2001:db8::1` and port `8443` — brackets validated in pairs, the IPv6 literal checked group by group. (2) Strictness boundaries: `example.com:8443` (domain with port) is legal; `2001:db8::1` (bare IPv6 without brackets plus an intended port) is ambiguous — the parser refuses rather than guesses.**Why so fussy**: Host/Authority feeds routing and identity decisions (Chapter 80's SAN matching of IP shapes, a proxy's target selection); ambiguous parsing means founding routing decisions on an understanding that differs from the peer's. The companion `examples/http/field_tour` (3 output lines) tours the complete API surface of field-block counting, finding, token iteration, and write-back — this chapter's "executable cheat sheet".

## Contracts

- **Borrowed views**: parse results of fields/methods/targets/Host all borrow the original input; valid while the input lives (Chapter 15's view rules).
- **Strict parsing**: illegal token characters, ambiguous Hosts, unpaired brackets, illegal field-name characters — all rejected on the spot; no repairing, no guessing.
- **Capacity atomicity**: write-back interfaces like `FieldBlockWrite` write zero bytes on insufficient capacity; the length is published only on success — no half messages exist.
- **Token iteration**: split on `,`/`;`, skip whitespace; `NEXT_ITEM`/`NEXT_END` two states; the iterator holds no internal state (the offset belongs to the caller).
- **OWS normalization**: parsing trims surrounding whitespace of values; write-back produces the canonical shape ("name: value").
- **Host semantics**: domain/IPv4/bracketed IPv6; port optional; ambiguous input rejected.
- **Parameter coding**: `+` decodes by form semantics; encoding escapes only necessary characters (`&`/`=` structure preserved).
- **Zero allocation**: every API in this chapter allocates no heap memory — per-request cost is predictable.
- **Trimming**: `XRT_MODULE_HTTP` is independent; subdomains like `HTTP_HOST`/`HTTP_PARAM`/`HTTP_TARGET` trim independently.

## Pitfalls

### Pitfall 1: hunting commas in values yourself

Symptom: hand-writing a `strchr(',')` loop for `Accept-Encoding: gzip, deflate, br` — slicing wrong on quoted tokens (`filename="a,b.txt"`) or empty items (`a,,b`).

Cause: HTTP list comma semantics carry escaping rules for quotes and comments; bare `strchr` knows none of them. `TokenNext` has the rules built in — list handling always goes through it.

```c bad
char* p = strtok(Value, ",");   /* the comma inside quotes gets cut */
while ( p ) { handle(p); p = strtok(NULL, ","); }
```

```c good
size_t iOffset = 0;
xstrview Token;
while ( (Next = xrtHttpTokenNext(Value, &iOffset, &Token)) ==
		XHTTP_NEXT_ITEM ) {
	handle(Token);   /* quote/whitespace/parameter boundaries handled by the iterator */
}
```

### Pitfall 2: rewriting a head block with a buffer too small, writing half a message

Symptom: a homegrown proxy uses an "append-style" rewrite — the buffer fills midway: the first half already sent, the second dropped; the peer receives a truncated head block and the connection is wrecked.

Cause: a writing style without capacity-atomicity awareness. An HTTP head block is one complete unit; half a unit is a protocol error.

```c bad
size_t iUsed = 0;
for ( each field ) {
	iUsed += snprintf(Out + iUsed, Cap - iUsed,
		"%.*s: %.*s\r\n", ...);   /* capacity runs out midway: half a head block */
}
```

```c good
if ( !xrtHttpFieldBlockWrite(Fields, iCount, Out,
		sizeof(Out), &iSize) ) {
	/* swap in a bigger buffer or a chunked strategy - Out stays untouched (zero bytes written) */
}
send(Out, iSize);
```

### Pitfall 3: using the Host value directly as a connection target

Symptom: passing the raw value of `Host: [2001:db8::1]:8443` to a connect function fails or connects to the wrong port; domains differing only in case treated as different hosts (cache-key splits).

Cause: a Host value is **structured text**, not a bare hostname — brackets, port, and case normalization must be split off first. Only after `HostParse` yields host and port do you have usable routing/connection input.

```c bad
connect(g_Target /* the raw Host value used directly */, ...);
/* a raw value with brackets and port is not a hostname */
```

```c good
xhttpauthority Authority;
if ( !xrtHttpHostParse(Field.Value, &Authority) ) {
	return reject("malformed host");
}
/* the split, normalized authority is the input;
   normalize case (domains are case-insensitive) before routing/cache keys */
```

## Exercises

### Basic: run the field tour

Run both samples, base and field_tour, annotating line by line which API each output line verifies. Then change base's input to `Connection: keep-alive,, Upgrade` (an empty item) and observe the iterator's behavior. Acceptance criteria: you can explain whether the empty item is "skipped" or "yields an empty token"; field_tour's three lines all green.

### Advanced: a request-head rewriter

Implement `rewrite_host(字段数组, 数量, 新主机, 输出, 容量, &长度)` (field array, count, new host, output, capacity, &length): find the Host field by name (case-insensitive), replace its value, leave the rest as-is, then `FieldBlockWrite` the block back. Acceptance criteria: an explicit failure when Host is absent; the rewritten block byte-for-byte canonical; zero writes on insufficient capacity (verify with an 8-byte buffer).

### Challenge: the header-processing core of a mini proxy

Read a complete request head (multiple lines) into a field array (your own loop + `FieldParse`), then tally: total fields, duplicated names, Connection's token set, the Host split result, query-string parameter count (ParamDecode). Rewrite "X-Forwarded-For" on the same input and write the block back. Acceptance criteria: zero heap allocations throughout (Chapter 6's statistics); malformed input (field name with a space, ambiguous Host) rejected with the first error position reported.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Foundation disciplines | strict parsing (no repair) + borrowed views (zero-copy) + capacity atomicity (no half messages) |
| Field model | `xhttpfield` = two views Name/Value; Parse trims OWS, validates the token charset |
| Write-back | `FieldBlockWrite` turns a field array into a canonical head block; insufficient capacity writes zero |
| Token iteration | `TokenNext` splits on `,`/`;`; NEXT_ITEM/NEXT_END; the offset belongs to the caller |
| List parameters | parameters after the semicolon (q-values etc.) belong to the per-field layers; the token iterator stops at the semicolon |
| Host split | `HostParse` yields `xhttpauthority`; domain/IPv4/bracketed IPv6; ambiguous rejected |
| Method/target | standard + extension methods token-validated; target four-shape recognition with path/query split |
| Parameter coding | `+`→space (form semantics); encoding escapes only the necessary; `&`/`=` structure preserved |
| Zero allocation | no heap allocation in the whole chapter - per-request cost predictable |
| Trimming | XRT_MODULE_HTTP independent; the fields subdomain trims independently |
