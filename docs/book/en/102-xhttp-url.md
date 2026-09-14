---
num: 102
slug: xhttp-url
title: URL Parsing: xurl and RFC 3986
volume: 卷十 扩展库：xhttp
type: practice
lead: Zero-copy generic URI-reference parsing, presence-bit semantics, port lexics, target writing, and reference resolution — more than scheme://host/path.
api: xhttp-url, xhttp-query, net
---

## Orientation

`xurl` is xhttp's URL layer, but it is **not merely HTTP URLs**: `xrtUrlParse` parses the complete RFC 3986 URI-reference — `mailto:user@example.com`, `urn:isbn:...`, network paths `//host/path`, relative references `../items?page=2` all supported, never forcing the `scheme://host/path` shape. Design continuous with the whole library: **zero copy** (all views borrow the input), **presence-bit semantics** (`Path` always present; whether Query/Fragment/UserInfo/port exist must be read from `Flags` — never from view length), **lexic preservation** (`PortText` keeps `:00080` verbatim; `port` is arbitrary-length decimal text — `:65536` is not rejected for exceeding uint16), **allocation-free writing** (the Write family refuses overlap; short buffers write zero). Add RFC 3986 reference resolution (`xrtUrlResolve` — relative address expansion) and target writing (the HTTP request-line form), and you have URL's complete vocabulary.

## Introduction

"Parsing a URL" sounds like string splitting — where are the professional pits?**First: presence vs empty** — `https://host` has no query, `https://host?` has an **empty** query: both views are length 0, yet the semantics differ (the latter keeps its question mark when written back). `xurl` solves it with `Flags` presence bits: `XURL_HAS_QUERY` distinguishes "absent" from "empty".**Second: port lexics vs value** — the RFC port is arbitrary-length decimal text: `:65536` is a **legal URL** (parsing must not reject), it merely has no uint16 value; `XURL_PORT_VALUE` flags "the Port member holds an expressible value" — `:00080` keeps its lexics, value 80.**Third: relative references** — an HTTP redirect's `Location: ../health?full=1` must be expanded against the current URL (Chapter 100); reference resolution is a component of protocol correctness.

All three problems point to "URL is structured text, not a string" — the parser completes the grammar verdict, and the caller receives an unambiguous structure.

## Concepts

### Structure and presence bits

`xurl`'s fields are all views: `Scheme/Authority/UserInfo/Host/PortText/Path/Query/Fragment` + `Flags` (presence bits) + `Port` (uint16 value). Core discipline: **`Path` is always present but may be empty**; the presence of Query/Fragment/UserInfo/port is read from `XURL_HAS_QUERY`-style flags, **never from view length or numeric value** — an explicit empty query (`?`) and an absent query are two structures. The symmetric rules for hand construction: a numeric port sets `XURL_HAS_PORT | XURL_PORT_VALUE`, fills `Port`, leaves `PortText` empty; a lexical port fills `PortText` without setting `PORT_VALUE`.

### Parse boundaries and illegal input

- **ASCII URIs**: non-ASCII text must first pass through the IRI/percent-encoding layer — unencoded UTF-8 bytes are not a legal URI (Chinese domains need punycode first; spaces need `%20`).
- **IPv6 ZoneID**: RFC 9844 withdrew extensions like `[fe80::1%25eth0]` — the parser rejects (local-scope expression belongs to Chapter 71's network-address layer; two separate things).
- Views borrow the input (no reading after the input dies); parse output never covers the input; writable-range end-address wraparound is rejected as a parameter error.

### The Write family: allocation-free, atomic, overlap-refusing

`xrtUrlWrite/UrlAuthorityWrite/UrlHostWrite/UrlTargetWrite`: structure → text, **no `\0` written**, `NULL/0` capacity queries the exact length, short buffers write zero, and **output overlapping the input object/views/length outputs is always rejected**. `Target` writes the origin-form used by the HTTP request line (`/api/items?page=2`) — a direct part for request construction. `PathNormalize` supports in-place and limited overlap; `Resolve` refuses output covering Base/Reference.**The Build family** (`UrlResolveBuild` etc.) returns zero-terminated strings freed with `xrtFree` — pay that one allocation only when you truly need "an independent string".

### Reference resolution: relative address expansion

`xrtUrlResolve(Base, Reference, 输出)` (Base, Reference, output) expands a relative reference per RFC 3986 §5: `../health?full=1` against `https://example.test/api/items?page=2` resolves to `https://example.test/api/health?full=1` — path segments resolved step by step (`..` pops up), query overwritten, no scheme inherits the base's scheme/authority. Fragment semantics belong to the caller (HTTP redirect's inherit/block rules live in Chapter 100 — that is protocol-layer policy; this layer only does grammatical expansion). Length queries and direct writes allocate nothing; failure does not modify the output.

### The division with Chapter 89's Host and Chapter 103's queries

Who consumes `xurl`'s `Host` view? The HTTP side's Host-field splitting (Chapter 89's `xhttpauthority`) handles the authority shape of the `Host:` header; a URL's host is a URI component — the two share the "bracketed IPv6/port" grammar but differ in entrance. Query-string key-value iteration (`xrtQueryNext`) operates on the `Url.Query` view — Chapter 103 unfolds it.**What this layer does not do**: encoding/decoding (the percent layer), punycode (the charset layer) — xurl handles structure only.

### xurl's place in the layering

```diagram flow
- xurl (this chapter): RFC 3986 structural parse/write/reference expansion - zero copy, presence bits, lexics preserved
- query two layers (Chapter 103): zero-allocation traversal + owning containers (& domain)
- HTTP specializations (Chapters 89/92): Host-header authority, semicolon parameters of field values (; domain)
- codecs (Chapter 28): percent/punycode - the character layer above xurl
```

## Examples

### First complete program: parse, target write, and reference resolution

The program below is from `examples/url/main.c` — three steps through the core face:

```embed path="extlibs/xhttp/examples/url/main.c" title="extlibs/xhttp/examples/url/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/url/main.c -lws2_32 -liphlpapi
host: example.test
target: /api/items?page=2
resolved: https://example.test/api/health?full=1
```

**What just happened.** (1) `UrlParse` parses the full URL in one pass — the `Host` view (`example.test`) and Query (`page=2`) all borrow the input, zero allocation. (2) `UrlTargetWrite` writes the origin-form target: `/api/items?page=2` — directly usable in an HTTP request line (the fragment `#result` never enters the target — it is client-local semantics). (3) `UrlResolveBuild` expands the relative reference `../health?full=1` against the current URL: the path `api/items` is popped by `..` up to `api/`, `health` appended, query swapped for `full=1` — one allocation produces only the final string (no intermediate merged object). These three lines are the complete underlay of redirect Location handling (Chapter 100).

### Second complete program: URI-Reference parameter validation

The second program is from `examples/url_param/main.c` — reference validation without building temporary strings:

```embed path="extlibs/xhttp/examples/url_param/main.c" title="extlibs/xhttp/examples/url_param/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/url_param/main.c -lws2_32 -liphlpapi
valid: yes
```

**What just happened.** (1) The input is a URI-reference in HTTP parameter-value shape (`../items?page=2` inside a quoted-string — the usual carrier of the `Link` header); `xrtUrlParamValid` validates legality directly **on the post-decode view** — no copy, no temporary string. (2) Such "validate without landing" entrances are the high-frequency path of middleware/proxies: every URL in every request head must be validated, and allocation is overhead. (3) Companion tours live in `examples/url/query*` (Chapter 103 unfolds) and `examples/http/origin`/`forward`/`link` (origin computation, Forwarded handling, Link-header parsing — HTTP specializations of the same grammar family).

## Contracts

- **Parse range**: the complete RFC 3986 URI-reference (not just http shapes); ASCII input; ZoneID IPv6 rejected (RFC 9844).
- **Presence bits**: `Path` always present, may be empty; the presence of Query/Fragment/UserInfo/Port read from `XURL_HAS_QUERY`-style flags — explicit empty and absent are different structures.
- **Port semantics**: `PortText` keeps lexics (`:00080` verbatim); `XURL_PORT_VALUE` marks Port holding a uint16 value; no port/zero/empty/out-of-range can all yield Port==0 — judge with presence bits; the two symmetric hand constructions.
- **Write family**: no `\0`; empty capacity queries length; short buffers write zero; overlap with input rejected; Target produces origin-form (no fragment).
- **PathNormalize/Resolve**: zero allocation; failure leaves output untouched; overlap contracts (in-place/limited overlap/refusing to cover Base).
- **Build family**: freed with `xrtFree`; ResolveBuild allocates only the final result.
- **View borrowing**: valid while the input lives; output never covers the input object.
- **Unaligned safety**: structure and size_t outputs allow unaligned storage; writable-range wraparound rejected.
- **Division boundaries**: percent encoding/decoding, punycode, Host-header splitting, query key-values — each an independent layer; xurl handles structure only.

## Pitfalls

### Pitfall 1: judging component presence by view length

Symptom: `https://host?`'s empty query read as "no query" — the question mark is lost when rewriting the URL; or conversely: an absent fragment written as `#`.

Cause: empty vs absent is distinguished by `Flags`; both views are length 0. This is the key to URL round-trip (parse→modify→write) fidelity.

```c bad
if ( Url.Query.Size != 0 ) {   /* empty query (has ?) misjudged as absent */
	write_url_with_query(...);
}
```

```c good
if ( (Url.Flags & XURL_HAS_QUERY) != 0 ) {
	/* has a query (possibly empty) - writing preserves the question mark */
}
```

### Pitfall 2: reading the Port value as "has a port"

Symptom: `:0`, empty port `:`, and `:99999` all yield `Port` of 0 — treated as "no port", default-port decisions wrong.

Cause: the `Port` value is meaningful only when `XURL_PORT_VALUE` is set; the three Port==0 inputs differ in semantics (explicit zero/empty/out-of-range lexical). Presence bit + value bit, two steps.

```c bad
uint16 Port = Url.Port ? Url.Port : 443;   /* all three Port==0 take the default */
```

```c good
uint16 Port;
if ( !xrtUrlPort(&Url, &Port) ) {   /* value availability in one step (with default fill-in) */
	Port = 443;                      /* only a non-expressible port takes the default */
}
```

### Pitfall 3: unencoded UTF-8/Chinese straight into a URL

Symptom: a Chinese path spliced into a URL fails to parse or garbles on the wire — the peer parses it as an ASCII URI into something else entirely.

Cause: URI is ASCII grammar; non-ASCII must first be percent-encoded (path/query) or punycoded (host). xurl neither guesses nor repairs — **illegal input is rejected explicitly**; repair belongs to the layer above (charset/codec).

```c bad
snprintf(Url, "%s/search?q=%s", Base, "中文关键词");  /* bare UTF-8 into a URL */
```

```c good
/* percent-encode before entering the URL (Chapter 28's codec-layer duty) */
percent_encode("中文关键词", Encoded, sizeof(Encoded));
snprintf(Url, "%s/search?q=%s", Base, Encoded);
```

## Exercises

### Basic: the five-shape parse matrix

Parse five inputs: a full https URL, `mailto:user@example.com`, `urn:isbn:0451450523`, `//host/path` (network path), `../rel?x=1` (relative reference) — print each one's Flags and non-empty components. Acceptance criteria: the structural differences (scheme/authority/relativity) match RFC 3986.

### Advanced: round-trip fidelity test

Pick ten "wicked" URLs (with empty query, empty fragment, `:00080` port, mixed-case scheme, IPv6 host) — parse→write→parse again, comparing both structures field by field. Acceptance criteria: all ten round-trip faithfully (presence bits of empty components never lost); `:00080` keeps its lexics.

### Challenge: a redirect Location expander

Implement `next_url(当前URL, Location头值)` (current URL, Location header value): cover the five Location shapes (absolute, network path, absolute path, relative path, empty-fragment inheritance), expanding with `UrlResolve` — handling `#` per Chapter 100's inherit/block rules. Replay a real site's redirect chain (recorded via curl -L) for verification. Acceptance criteria: expansions match curl's followed final URLs; fragment inheritance follows RFC/Chapter 100; zero intermediate allocations throughout (Resolve direct-write form).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Parse range | RFC 3986 full URI-reference (mailto/urn/network-path/relative); ASCII; ZoneID rejected |
| Presence bits | Path always present, may be empty; other components via XURL_HAS_QUERY-style flags - empty ≠ absent |
| Port | PortText keeps lexics; PORT_VALUE marks the value; three Port==0 cases differ; UrlPort decides in one step |
| Write family | no \0; empty capacity queries length; short buffers write zero; overlap refused; Target = origin-form without fragment |
| Reference resolution | UrlResolve expands ../rel shapes; zero allocation, failure untouched; Build family one allocation for the final string |
| Unaligned safety | structure/length outputs allow unaligned; wraparound rejected |
| View discipline | everything borrows the input; output never covers the input |
| Division boundaries | percent/punycode/Host header/query key-values - independent layers; xurl handles structure only |
| HTTP specializations | origin/Forwarded/Link header parsing are same-family independent modules |
