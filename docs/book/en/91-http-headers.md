---
num: 91
slug: http-headers
title: The Header Field Families: Parameters, Connection, and Negotiation
volume: 卷九 Web 协议核心
type: practice
lead: Semicolon parameters and quoted-strings, Connection/Upgrade capability, Expect and trailers — the per-family layer's complete handling surface.
api: http, http_te, http_connection, http_expect, http_trailer
---

## Orientation

Chapter 88 covered "how to parse one field"; this chapter covers "how to use one family of fields". HTTP headers include a batch of fields with their own **structured syntax**: `Content-Type`'s semicolon parameters (`charset=UTF-8; boundary="part;42"` — values may contain quotes and semicolons), `Connection`/`Upgrade`'s token lists (connection capability negotiation — the deciding input of Chapter 94's WebSocket handshake), `Expect` (client expectations — the 100 Continue flow), `TE`/`Trailer` (transfer coding and trailer capability, Chapter 90). Each family has its own parsing traps (quoted-string escapes, token case, repeatable fields), and XRT provides an independent small module per family — `ParamNext` parameter iteration, `FieldTokenFind` token lookup, `xrtHttpTeParse` negotiation aggregation. The daily toolbox of proxy, middleware, and protocol implementers.

## Introduction

Implementing a multipart form parser, the first step is extracting boundary from `Content-Type: multipart/form-data; boundary="part;42"` — the value carries quotes, and **inside the quotes is a semicolon** (boundaries may contain semicolons!). Anyone splitting with `strchr(';')` crashes here: `part;42` is cut in half. Or take a WebSocket upgrade check: does `Connection` contain the `upgrade` token? Case, spaces, quotes — a hand-written check is another pile of edge cases.

The common thread of the two scenes: **a field value's syntax is more complex than "looks like text"** — RFC 9110 defines a different grammar per field family (token-list, semicolon parameters, quoted-string), each with canonical parsing rules and historical implementation holes. XRT's per-family layer implements each family's rules as small independent APIs: the parameter layer's quote decoding, the token layer's case-insensitive lookup, the TE layer's q-value negotiation (seen in Chapter 90). This chapter walks each family: what the syntax is, how to use the API, where the pits lie.

## Concepts

### The parameter layer: the semicolon world and quoted-string

The "semicolon parameter" syntax of field values (Content-Type, Content-Disposition, and friends): `主值; key=value; key2="quoted string"` (main value; key=value; key2="quoted string"). The API trio:

- `xrtHttpParamNext(值, &偏移, &参数)` (value, &offset, &param): iterates yielding `xhttpparam` — a `Name` view, `Flags` (`XHTTP_PARAM_HAS_VALUE` — valueless parameters allowed), and the raw Value view.
- `xrtHttpParamValueWrite(&参数, 输出, 容量, &长度)` (&param, output, capacity, &length): **decodes the quoted-string then writes the value** — quotes stripped, `\"` unescaped, capacity atomic. boundary=`"part;42"` decodes to `part;42` — the semicolon survives intact.
- `xrtHttpParamCount`/`ParamFind`/`ParamBuild`: count, find by name, build a new parameter string.

Why "iteration yields raw views, decoding is a separate step": most consumers only compare parameter names or lengths — skipping the unquote costs nothing; only when the value content is truly needed is it decoded.

### The token layer: connection capability

`Connection`/`Upgrade`/`Transfer-Encoding` are all token-lists (Chapter 88's TokenNext territory); the per-family layer adds **semantic lookup**: `xrtHttpFieldTokenFind` — whether a specified token exists in a field value (case-insensitive). "Does Connection contain upgrade" answered in one line; Chapter 94's WebSocket handshake check and a proxy's hop-by-hop field stripping (the field names `Connection` lists must be deleted before forwarding — `xrtHttpConnectionNext` iterates out that list) both stand on it.

### Expect and 100 Continue

Before sending a large body the client asks "will the server take it": `Expect: 100-continue`. `xrtHttpExpectFields`/`ExpectValid` decide whether a request carries a legal 100-continue expectation — the server may answer `100 Continue` first and then receive the body, or refuse with 417.**Why a dedicated family**: 100's timing interweaves with the body stream (the server may answer 100, receive the body, then answer the final status), and implementers easily miss the lenient path of "sending the body without waiting for 100" — the per-family layer standardizes the verdict; timing belongs to the connection layer.

### The full lookup family over field arrays

The field arrays parsed in Chapter 89 come with a lookup family (implemented at the foundation layer, reused at the header layer): `xrtHttpFieldFind` (first by name), `xrtHttpFieldNext` (same-name multi-value iteration — the Set-Cookie scene), `xrtHttpFieldGet`/`FieldGetUnique` (take-value/unique-value semantics — `GetUnique` is for fields that "semantically must appear once"; duplicates are an error), `xrtHttpFieldNameEqual` (case-insensitive comparison), `xrtHttpFieldTokenCursorInit`+`FieldTokenCount` (token counting and cursor). This family is the complete vocabulary of "querying the head" — the foundation of middleware decisions by Route/Cache-Control.

### The trimming surface of the family modules

One feature macro per family (`HTTP_HOST`/`HTTP_PARAM`/`HTTP_TARGET`/`HTTP_TE`/`HTTP_CONNECTION`/`HTTP_EXPECT`/`HTTP_TRAILER`/`HTTP_UPGRADE`...): an embedded client carries only Host+Param without upgrade and Expect; a gateway carries all. The per-family modules introduce no interdependencies — trimming is truly independent.

### The layering of the field families

```diagram flow
- Foundation (Chapter 88): FieldParse field views + TokenNext list iteration
- Parameter family: ParamNext/ValueWrite - semicolon parameters and quoted-string decoding
- Semantic lookup family: FieldFind/FieldGetUnique/FieldTokenFind - case-insensitive retrieval
- Per-field families: TE/Encoding/Upgrade/Expect/Trailer/Connection - independently trimmable negotiation mini-modules
```

## Examples

### First complete program: parameter iteration and quote decoding

The program below is from `examples/http/param/main.c` — the standard cure for the quoted-string trap:

```embed path="examples/http/param/main.c" title="examples/http/param/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/param/main.c -lws2_32 -liphlpapi
charset = UTF-8
boundary = part;42
```

**What just happened.** (1) Input `charset=UTF-8; boundary="part;42"` — two parameters. (2) `ParamNext` yields them one by one: the first has a value (the `HAS_VALUE` flag), Name=`charset`, raw Value=`UTF-8` (a bare token needing no decode, though going through ValueWrite uniformly is also correct); the second has Name=`boundary` and the raw value **quoted `"part;42"`**. (3) `ParamValueWrite` decodes: quotes stripped, the inner `;` preserved whole — output `part;42`.**This is the flip side of the strchr-split crash scene**: semicolons inside quotes don't split, quote pairing is validated, escape sequences restored — all handled by the decoder. Insufficient capacity writes zero — consistent with the library-wide atomicity.

### Second complete program: the field lookup and write-back tour

The second program is from `examples/http/field_tour/main.c` — four self-checking lines over field blocks, lookup, tokens, and write-back:

```embed path="examples/http/field_tour/main.c" title="examples/http/field_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/field_tour/main.c -lws2_32 -liphlpapi
block-count=3 next=3
get=keep-alive, Upgrade value-valid=1 count=1 unique=hit find=1 find2=2
tokens: gzip deflate count=2 find=1
write=C blockwrite=ok
```

**What just happened.** (1) Line 1: field-block counting and iteration — the batch-processing surface of multi-field arrays. (2) Line 2 runs the lookup family in one pass: value by name, view validity, counting, `GetUnique` unique-value semantics (the positive use of this chapter's Pitfall 3), and two `Find` calls verifying same-name multi-value returns in order (find2=2 — the second same-named field). (3) Line 3, the token family: Accept-Encoding's list iteration, counting, and named-token lookup — a runnable footnote to this chapter's "token semantic lookup". (4) Line 4, the write-back round trip (`write=C` is one field, `blockwrite` the whole block) — re-verifying Chapter 88's atomicity conventions. The companion `examples/http/small_fields` covers the joint tour of the five per-field families TE/Encoding/Upgrade/Expect/Trailer, and `examples/http/host` covers the authority split — every API in this chapter has a runnable sample.

## Contracts

- **Parameter iteration**: raw views first (Name/quoted Value), the `HAS_VALUE` flag distinguishing valueless parameters; decoding (ValueWrite) on demand — quote stripping and escape restoration, capacity atomic.
- **Token semantic lookup**: `FieldTokenFind` case-insensitive; the field names listed by `Connection` iterate via `ConnectionNext` — the basis of hop-by-hop stripping.
- **Unique-value semantics**: `FieldGetUnique` for single-value fields — duplicates error rather than "take the first" (silent selection = accepting ambiguity).
- **Expect**: `ExpectFields`/`ExpectValid` decide the expectation's presence and legality; 100 timing belongs to the connection layer.
- **Trailer declaration**: the trailer list announced by the `Trailer` field is queryable — reconciled against Chapter 90's read-side array.
- **q negotiation**: thousandths integers (TE/Encoding share the model); unmentioned q=0.
- **Independent family trimming**: the per-family macros are mutually independent; `ParamValueCursorInit/ValueNext` provide a value-iteration shape without decoding.
- **Zero allocation**: the family APIs are all borrowed views + caller buffers.

## Pitfalls

### Pitfall 1: splitting parameters with strchr — the semicolon inside quotes crashes

Symptom: a multipart boundary comes out wrong — `boundary="part;42"` cut into `boundary="part` and `42"`, and form parsing goes entirely wrong.

Cause: in the semicolon-parameter syntax, a semicolon inside a quoted-string is not a separator. Hand-written splitting knows nothing of quote context.

```c bad
char* p = strtok(Value, ";");   /* part;42's semicolon treated as a separator */
```

```c good
xhttpparam Param; size_t iOff = 0;
while ( (N = xrtHttpParamNext(Value, &iOff, &Param)) ==
		XHTTP_NEXT_ITEM ) {
	char Buf[64]; size_t iSize;
	if ( (Param.Flags & XHTTP_PARAM_HAS_VALUE) &&
			xrtHttpParamValueWrite(&Param, Buf,
				sizeof(Buf), &iSize) ) {
		use(Param.Name, Buf, iSize);   /* decoded value: part;42 whole */
	}
}
```

### Pitfall 2: case-sensitive token lookup

Symptom: checking `Connection: keep-alive, Upgrade` for upgrade with `strstr(value, "upgrade")` — returns NULL, and the WebSocket handshake is refused.

Cause: HTTP token semantics are case-insensitive, and on-the-wire casing is all over the map (`Upgrade`/`upgrade`/`UPGRADE` are all legal).

```c bad
if ( strstr(Field.Value, "upgrade") != NULL ) {
	/* "Upgrade"'s casing never gets in: a legal handshake refused */
}
```

```c good
if ( xrtHttpFieldTokenFind(Fields, iCount,
		XRT_STR_LITERAL("upgrade")) ) {
	/* case-insensitive + quote context handled by the family API */
}
```

### Pitfall 3: handling duplicates of a single-value field by "take the first"

Symptom: `Content-Length: 11` and `Content-Length: 12` both appear; the code handles the first and ignores the second — front and back ends understand differently: a smuggling variant.

Cause: a semantically unique field appearing twice is erroneous input. "Take the first" silently dissolves the ambiguity — choosing an understanding on the attacker's behalf.

```c bad
xstrview V = first_field("Content-Length", ...);
parse_length(V);   /* the second CL goes unquestioned */
```

```c good
xstrview V;
if ( !xrtHttpFieldGetUnique(Fields, iCount,
		XRT_STR_LITERAL("Content-Length"), &V) ) {
	reject("conflicting content-length");  /* duplicate means reject */
}
```

## Exercises

### Basic: run the five-family tour

Run both samples, small_fields and param, annotating each output line with its family and API. Change param's input to `boundary="a\"b"` (escaped quote) and verify decoding. Acceptance criteria: escape restoration correct (`a"b`); the valueless-parameter path takes the HAS_VALUE-false branch.

### Advanced: a Content-Type consumer

Implement `content_type(字段值, &主类型, &子类型, 参数数组, 容量)` (field value, &main type, &sub type, param array, capacity): split out the two segments of `multipart/form-data` and all parameters (quote-decoded). Verify against `text/plain` (no parameters) and `multipart/mixed; boundary="x;y"; charset=UTF-8` (multi-parameter with a quoted semicolon). Acceptance criteria: both outputs match hand-derivation; main/sub types compared after case normalization.

### Challenge: a hop-by-hop field stripper

Implement a proxy's hop-by-hop handling: iterate `Connection` for the hop-by-hop field-name list + the standard hop-by-hop set (Connection/Keep-Alive/TE/Trailer/Upgrade), delete hits from the field array (compacting it), then `FieldBlockWrite` the block back. Acceptance criteria: after stripping a request with `Connection: keep-alive, X-Push`, `X-Push` is gone while `Connection` itself remains but without the deleted item; end-to-end fields (Accept etc.) untouched; zero heap allocation throughout.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Parameter layer | ParamNext iteration (raw view + HAS_VALUE); ValueWrite decodes (quote strip/escape/atomicity) |
| quoted-string | semicolons inside quotes don't split, `\"` restored - hand-slicing with strchr is an accident source |
| Token lookup | FieldTokenFind case-insensitive; ConnectionNext iterates hop-by-hop field names |
| Lookup family | FieldFind/FieldNext (multi-value)/FieldGet (value)/FieldGetUnique (duplicate = error) |
| Expect | ExpectParse decides the 100-continue expectation; timing belongs to the connection layer |
| Trailer declaration | the Trailer field's announced list is queryable; reconciled against the read-side array |
| q negotiation | thousandths integers; TE/Encoding share the model; unmentioned q=0 |
| Upgrade family | Upgrade iterate/count/build - Chapter 94's handshake parts |
| Trimming | one macro per family (HOST/PARAM/TE/CONNECTION/EXPECT/TRAILER/UPGRADE...) |
| Zero allocation | the whole family borrows views + caller buffers |
