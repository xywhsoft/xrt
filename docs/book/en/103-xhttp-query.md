---
num: 103
slug: xhttp-query
title: Query Strings and Forms: urlencoded and multipart
volume: 卷十 扩展库：xhttp
type: practice
lead: Zero-allocation query traversal (distinguishing missing values from empty ones), owning QueryParams rewriting and rebuilding, the encoding layer, and the dual form shapes — a and a= are not the same thing.
api: xhttp-query, xhttp-query_params, xhttp-form, xhttp-form_data
---

## Orientation

URL query strings and HTTP forms are two similar-but-different key-value grammars, and xhttp gives two tool layers.**The zero-allocation layer** (the `query` module): `xrtQueryNext` traverses directly over the raw string — `xquerypair`'s `XQUERY_HAS_VALUE` precisely distinguishes `a` (no value), `a=` (empty value), `=v` (empty key); `xrtQueryFind/Count/Validate` complete the family; enabling `query_codec` adds percent encoding/decoding.**The owning layer** (the `query_params` module): the `xqueryparams` container parses, rewrites (`Set` position-preserving dedup / `Append` append / `Sort` stable sort / `Compact` compaction), and rebuilds — the standard path of "change one parameter and stitch it back". The form side has two shapes: `application/x-www-form-urlencoded` (the same grammar + the `+`-space semantics) and `multipart/form-data` (Chapter 92's quoted-string in live combat). The raw layer (borrowing views) and owning layer (containers), are fully isomorphic with Chapters 89/92's layering — grammar layer first, containers after.

## Introduction

High-frequency API-client operations: pagination (`page=2` becomes `page=3`), adding filters (append `tag=network`), copying a link with changed parameters. Hand string-splicing stumbles on **duplicate keys**: in `?tag=c&tag=xlang&debug` tag has two values — append a third? Replace all? Keep which position? `xqueryparams`' semantics are explicit: `Set` keeps the first same-name position and drops the rest, `Append` adds at the tail, `Find` iterates all same-named in order — three intents, three verbs. The zero-allocation layer's value is the **read-side hot path**: a proxy routes by the `debug` flag and must glance at the query of every request — view traversal costs nothing.

The `a` vs `a=` distinction runs through both layers: `XQUERY_HAS_VALUE` unset = no value (no `=` written in the URL), set and empty = explicit empty value — whether to re-add the `=` on round-trip writing is decided by it. The same philosophy as Chapter 102's presence bits: **grammatical structure is never guessed from view length**.

## Concepts

### The zero-allocation layer: iteration, finding, and validation

`xrtQueryNext(串, &偏移, &对)` (string, &offset, &pair) iterates in three states (`XQUERY_NEXT_ITEM/END/ERROR`); duplicate keys keep original order; the leading `?` may be omitted; empty segments from consecutive/leading/trailing `&` are skipped. `xrtQueryFind` iterates same-named items by name from an offset (the standard read for duplicate keys); `xrtQueryCount` counts; `xrtQueryValidate` decides legality.**This layer does not decode** — `%2F` is just three characters without interpretation; decoding values goes through `query_codec` (leniency switches like `XQUERY_PARAMS_LENIENT_PERCENT` live there).

### The owning layer: the container's four verbs

```diagram flow
- Parse: ParamsParse (form rules: + -> space, percent decoding, ErrorOffset reports the percent position)
- Rewrite: Set (keep first same-named position, drop the rest) / Append (tail) / AppendPair-SetPair (preserve the no-equals shape)
- Tidy: Sort (stable sort by name bytes - same names keep order) / Compact (drop dead bytes left by deletions)
- Rebuild: Build -> zero-terminated string freed with xrtFree (writing yields the canonical form)
```

- **Parse semantics**: `application/x-www-form-urlencoded` rules — strict by default, illegal percents rejected (`ErrorOffset` gives the percent position); lenient mode (`XQUERY_PARAMS_LENIENT_PERCENT`) keeps invalid percent signs as ordinary bytes — browser-`URLSearchParams`-compatible scenarios.
- **Modification semantics**: append/set copy the name and value (the input may borrow from the container itself — in-place modification is safe); `Get` reads the first same-named, `Find` iterates all; container modification invalidates existing borrowed views (Chapter 18's general rule).
- **Caps**: `xqueryparamsconfig` caps pairs/name length/value length/total decoded bytes — the physical string region compacts before touching `MaxBytes` (the cap logic excludes growth headroom).
- **Atomicity**: `ParseAppend` parses on a working copy — quota/syntax/memory failures never expose a partial append.

### The encoding layer and writing

`query_codec` depends on query and the percent codec: build an RFC 3986 query directly from "unencoded fields" — the `xrtQueryBuild`/`QueryAppendWrite` family bakes in the "which characters to encode" rules (`&`/`=` structure preserved, space form chosen). The raw layer (parse and forward without rewriting) and the codec layer (constructing new strings) **trim independently** — a pure proxy needs no encoder; trimming is per-face.

### The dual form shapes

- **urlencoded**: the body is just query grammar (`a=1&b=2`) — all of this chapter's vocabulary applies directly; `Content-Type: application/x-www-form-urlencoded`.
- **multipart/form-data**: a boundary-delimited part structure (each part with its own headers and content) — the carrier for file uploads; the boundary comes from the Content-Type parameter (that frequent consumer of quoted-string decoding, Chapter 92); the `xrtFormDataConfigInit`/`FormDataAppendBody` family provides building and parsing. `examples/http/form_data`/`multipart`/`multipart_stream`/`multipart_write` cover building, streaming, and writing.

### The division with Chapter 92's parameter layer

Chapter 92 covered **semicolon parameters inside field values** (`Content-Type; key=value` — HTTP header grammar); this chapter covers **URL query and forms** (`&`-separated — URI/form grammar). The two iterators share a shape (offset + three states) but differ in grammar domain — picking the wrong domain is a parse error. The `+` semantics hold only in the form domain (urlencoded `+` = space; in a pure URI query `+` is an ordinary character — the divide from Chapter 89).

## Examples

### First complete program: zero-allocation traversal and semantic distinctions

The program below is from `examples/url/query/main.c` — four key-value shapes traversed in one pass:

```embed path="extlibs/xhttp/examples/url/query/main.c" title="extlibs/xhttp/examples/url/query/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/url/query/main.c -lws2_32 -liphlpapi
tag = c
tag = xlang
debug = <missing>
empty = 
```

**What just happened.** (1) Input `?tag=c&tag=xlang&debug&empty=` — four items: the duplicate key tag yields both values in original order, `debug` has no value (flag unset — printed `<missing>`), `empty` is an explicit empty value (flag set, length 0 — printed as empty). (2) **Four shapes, one loop**: one `XQUERY_HAS_VALUE` bit distinguishes the two pairs of semantics (no value vs empty value). (3) Zero allocation throughout — Key/Value borrow the original string; this is the cost shape of a proxy/gateway reading queries. (4) This output shape doubles as a **verification checklist**: any query-handling code should branch explicitly on these four inputs just like this.

### Second complete program: container rewriting and rebuilding

The second program is from `examples/url/query_params/main.c` — the parse, Set/Append, Build loop:

```embed path="extlibs/xhttp/examples/url/query_params/main.c" title="extlibs/xhttp/examples/url/query_params/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/url/query_params/main.c -lws2_32 -liphlpapi
page=2&tag=c&tag=network&tag=xlang
```

**What just happened.** (1) `ParamsParse("page=1&tag=c&tag=network")` parses three items under form rules (+ and percent decoding happen here; ErrorOffset reports the percent position on failure). (2) `Set("page","2")` — page changes value in place; `Append("tag","xlang")` — a third tag appended at the tail: the **intent difference of Set vs Append toward duplicate keys** shows itself (Set would clear the other same-named; tag uses Append here, so three values coexist). (3) `Build` rebuilds `page=2&tag=c&tag=network&tag=xlang` — a canonical-form zero-terminated string, freed with `xrtFree`. Pagination, filter-adding, copy-and-change — the three-line template is these two Set/Append plus one Build.

## Contracts

- **Zero-allocation layer**: iterate/find/count/validate all views; three-state return; duplicate keys in order; empty segments skipped; leading ? optional; no decoding.
- **Semantic distinction**: `XQUERY_HAS_VALUE` distinguishes no-value/empty-value/empty-key, four shapes; round-trip writing preserved by the flag.
- **Container parsing**: form rules (+→space, percent decoding); strict by default (+ErrorOffset position); lenient mode keeps invalid percents as bytes.
- **Four rewrite verbs**: Set (position-preserving dedup) / Append (append) / Pair forms (preserve no-equals items) / Sort (stable by name bytes) / Compact (clear dead storage).
- **Ownership**: the container copies append/set inputs (may borrow itself); modification invalidates borrowed views; Build returns an xrtFree string.
- **Caps**: four lines — pairs/name length/value length/total decoded bytes; compact before judging the cap; ParseAppend atomic (failures expose no partial result).
- **Encoding layer**: `query_codec` independent (raw and construction trim separately); structural-character rules built in.
- **Forms**: urlencoded = query grammar directly; multipart = part structure (FormData family); boundary from the Content-Type parameter.
- **Domain boundary**: this chapter's `&` domain; Chapter 92's `;` domain — iterators same-shaped, different grammar.

## Pitfalls

### Pitfall 1: rewriting "no value" as "empty value" (or vice versa)

Symptom: `?debug` rewritten into `?debug=` — semantics shift from "flag present" to "value empty"; or the reverse, dropping the `=`.

Cause: no value and empty value are two grammatical structures; judging by `Pair.Value.Size == 0` conflates them — judgment reads `XQUERY_HAS_VALUE`, writing uses `AppendPair/SetPair` to preserve the shape.

```c bad
if ( Pair.Value.Size == 0 ) {
	treat_as_flag(Pair.Key);   /* debug and empty= treated alike */
}
```

```c good
if ( (Pair.Flags & XQUERY_HAS_VALUE) == 0 ) {
	treat_as_flag(Pair.Key);         /* truly no value */
} else {
	treat_as_value(Pair.Key, Pair.Value);  /* has a value (possibly empty string) */
}
```

### Pitfall 2: using container parsing as URL parsing (the + ambiguity)

Symptom: using `ParamsParse` on a **non-form** URI query — `a+b` decodes into `a b`, but under pure URI semantics `+` is just a plus character.

Cause: `+`→space is a urlencoded **form** rule; in RFC 3986 query `+` is an ordinary character. The two layers differ: reading URI queries uses the zero-allocation layer (no decoding) + percent decoding as needed; only form bodies use ParamsParse.

```c bad
/* handling a search URL's query: + wrongly taken as space */
pParams = xrtQueryParamsParse(Url.Query, NULL, &iErr);
```

```c good
/* URI query: zero-allocation traversal + explicit percent decoding */
while ( xrtQueryNext(Url.Query, &iOff, &Pair) == XQUERY_NEXT_ITEM ) {
	percent_decode_value(Pair.Value, Buf, sizeof(Buf));   /* decode %xx only */
}
```

### Pitfall 3: page-link generation while iterating loses the other parameters

Symptom: the pagination link carries only page — tag, sort, and other parameters all vanish.

Cause: building a new query from scratch (writing only page) instead of modifying the existing container. Correct path: Parse existing → Set("page", N) → Build — **change one parameter, touch nothing else**.

```c bad
snprintf(Next, "%s?page=%u", Path, Page + 1);  /* other parameters evaporate */
```

```c good
pParams = xrtQueryParamsParse(Url.Query, NULL, &iErr);
xrtQueryParamsSet(pParams, XRT_STR_LITERAL("page"), PageText);
sNext = xrtQueryParamsBuild(pParams, &iSize);
/* page changed in place; tag/sort and friends fully preserved */
```

## Exercises

### Basic: four-shape round trip

Construct a query with the four shapes (no value/empty value/empty key/normal); traverse and print with the zero-allocation layer; then `ParamsParse` → `Build` and compare round-trip differences (which shapes ParamsParse normalizes). Acceptance criteria: you can point out the boundary between semantic preservation and normalization in the round trip.

### Advanced: a pagination-link generator

Implement `next_page(当前URL文本, &下一页文本)` (current URL text, &next-page text): `UrlParse` → `ParamsParse` → `Set("page")` → `Build` → stitch the URL back. Verify against URLs with duplicate tag keys and a `debug` no-value flag. Acceptance criteria: pagination touches only page; duplicate keys and the flag preserved verbatim; the produced zero-terminated string freed correctly.

### Challenge: a dual-form converter

Implement `urlencoded_to_multipart(字段容器, 输出, 容量)` (field container, output, capacity): render the Params container's fields as multipart parts (boundary self-generated, each part with `Content-Disposition: form-data; name="..."` — names passing quoted-string rules). Implement the reverse, parsing multipart back into a container. Cross-check with the `form_data` sample. Acceptance criteria: bidirectional conversion preserves fields (including values with quotes/semicolons); the boundary satisfies the delimiter-uniqueness requirement (never appearing in the body).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Two-layer structure | zero allocation (Next/Find/Count/Validate, no decoding) + owning (Params container) |
| Four shapes | a = no value / a= = empty value / =v = empty key / normal - one XQUERY_HAS_VALUE bit sorts all |
| Container parsing | form rules (+→space/percent decoding); strict default + ErrorOffset; lenient mode available |
| Four verbs | Set position-preserving dedup / Append append / Sort stable / Compact clears dead bytes |
| Atomicity | ParseAppend working copy - failures expose no partial append |
| Caps | four lines - pairs/name/value/total bytes; compact before judging the cap |
| Encoding layer | query_codec independent (constructs new strings); raw and construction trim separately |
| The + domain | form = space; URI query = ordinary character - never mix the two layers |
| Dual form shapes | urlencoded = query grammar; multipart = parts + boundary (FormData family) |
| Boundary with Chapter 92 | this chapter's & domain (URI/forms); Chapter 92's ; domain (header field values) |
