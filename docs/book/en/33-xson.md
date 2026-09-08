---
num: 33
slug: xson
title: XSON Extended Serialization
volume: 卷四 文本与结构化数据
type: practice
lead: A strict superset of JSON — bytes/time/set/intmap as first-class types; full-type round trips for configs and internal data.
api: xson, value
---

## Orientation

JSON can't express far too much: binary needs hand-rolled Base64, timestamps need string conventions, sets and integer-keyed maps degrade into arrays — every gap is one more layer of custom convention, and every convention layer is an interoperability mine. **XSON is a strict superset of JSON**: legal JSON is automatically legal XSON, plus four additional first-class types — `bytes("...")` (inline Base64), `time("...")` (ISO timestamps), `set[...]` (deduplicated sets), and `intmap{...}` (integer-keyed maps). The parser is the same one as Chapter 31's (event callbacks distinguish by type field; extended types no longer degrade into strings), and serialization writes the four types back as-is — **full-type round trips at zero convention cost**. When to use XSON and when to stick with JSON: this chapter gives the decision rules.

## Introduction

The true shape of one internal config: the certificate key is binary (in JSON, only a Base64 string — the consumer must know "this string needs decoding"); the expiry is a timestamp (string or number? whose timezone?); the allowed ports are a set (what about duplicates in a JSON array? does the consumer deduplicate?); the connection parameters are a port-number-to-description map (JSON object keys can only be strings — who converts between "80" and 80?). Four fields, four conventions, two pages of documentation — and someone still got it wrong.

XSON's answer is to make these four shapes **first-class types**: `bytes("AAEC...")` carries its own encoding declaration, `time("2026-07-31T00:00:00Z")` unifies on UTC microseconds, `set[80, 443]` deduplicates by semantics, `intmap{80: "http"}` maps with native integer keys — the value tree's `xvalue` (Chapter 30) already has these four types; XSON simply stops serialization from flattening them. The **superset design** guarantees smoothness: existing JSON toolchains keep working, and the JSON portions of an XSON document are legal to any JSON parser.

## Concepts

### The four extended types

```diagram flow
- bytes("AAEC/w=="): binary inlined as Base64 - no external references; reads back as a bytes value
- time("2026-07-31T00:00:00Z"): ISO 8601 - parsing normalizes to UTC, events carry a microsecond integer
- set[...]: deduplicated set - semantics as in Chapter 19's sets, serialized round-trips as-is
- intmap{ 80: "http" }: integer-keyed map - keys no longer forced into strings
```

All four types have counterparts in the value tree (Chapter 30): Bytes/Time values, set/intmap containers — XSON serialization is simply the complete mapping between "value tree ↔ text", losing no type information. The event stream (SAX) likewise distinguishes: `xxsonevent`'s Type field marks extended types directly, so consumers never have to guess "is this string secretly a time".

### What strict superset means

Three properties, worth stating separately. **Forward compatible**: any legal JSON document is legal XSON — renaming existing `.json` configs to `.xson` costs nothing. **Not backward compatible**: an XSON document with extended types is illegal to a pure JSON parser (`bytes(` is not JSON syntax) — XSON belongs only to channels where "both ends are XRT or have agreed on XSON". **Non-interfering**: one parser suite handles both, parsing by actual document content — a JSON document's value tree contains no extended types, an XSON document's may; the config system maintains no second code path for the second format.

### Selection rules against JSON

| Scenario | Choice | Reason |
| --- | --- | --- |
| Public-facing APIs | JSON | the lingua franca, any client can parse |
| Internal configs | XSON | binary/time/sets expressed natively, zero conventions |
| Data between internal services | XSON | both ends controllable, full-type round trip |
| Human-edited configs | JSON or XSON | humans write ISO timestamp strings more kindly than microsecond numbers |

The rule's core variable is **whether the receiver is controllable**: controllable → XSON for full types; not → JSON for interoperability. Mixed shapes are common too: the public API's fields use JSON types while the internal processing representation uses value trees (which always carry full types) — convert at the boundary, don't compromise inside.

### Error location and strictness

XSON inherits all of Chapter 31's strictness: resource-limit three gates, duplicate-key rejection, errors with line/column positions. Extended-type parse errors are likewise located to the character — the illegal month in `time("2026-13-01...")` is pinpointed on the spot.

## Examples

### Complete program: a full-type round trip

From the repository sample `examples/data/xson/main.c` — the four extended types written and read in one pass:

```embed path="examples/data/xson/main.c" title="examples/data/xson/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/data/xson/main.c -lws2_32 -liphlpapi
{
  "blob": bytes("AAEC/w=="),
  "updated": time("2026-07-31T00:00:00Z"),
  "roles": set[ ... ],
  "ports": intmap{ 80: "http", 443: "https" }
}
bytes = 4
time = 1785456000000000
{"code":200,"tags":set["xrt"]}
```

**What just happened.** (1) Serialization segment: the value tree's four types written out as-is — `bytes("AAEC/w==")` with its encoding declaration, `time(...)` as an ISO string, `set[...]` and `intmap{...}` each with their own bracket semantics. **Type information is visible in the text** — readers of the document need no convention table. (2) Parsing segment: `bytes = 4` — Base64 decoded back to 4 binary bytes; `time = 1785456000000000` — the ISO string normalized to a UTC microsecond integer (Chapter 3's `xtime` measure). (3) The last line is compact serialization (not pretty) — the shape of `set["xrt"]` on one line; machine channels use compact, on-disk review uses pretty, the same choice as JSON's.

### Complete program: file round trip and error location

From `examples/data/xson_tour/main.c`:

```embed path="examples/data/xson_tour/main.c" title="examples/data/xson_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/data/xson_tour/main.c -lws2_32 -liphlpapi
xson: valid +/- and read(config) ok
xson: error location line=1 ok
xson: file roundtrip parse/read -> write/stringify ok
xson: sync write via callback ok
xson: sink writer full value domain ok
```

**What just happened.** (1) Validation and reading: legality validation of an XSON document and config-field reading in one pass. (2) Error location: feed input with a missing comma and `line=1` reports precisely — Chapter 31 JSON's locating ability inherited verbatim. (3) File round trip: parse → read fields → write back → stringify, a closed loop verifying no type is lost. (4) Callback-style writing: serialization output is handed over chunk by chunk through a callback — the shape of streaming to disk or wiring straight to a sink, identical to Chapter 31's Writer sink. (5) The sink write covers the whole value domain: scalars and all four extended types each take a turn through the sink — full types are expressible not only in DOM; streaming writes lose no type either. The five "ok"s together prove: "XSON has the same engineering capability set as JSON, only with more types".

## Contracts

- **Strict superset**: legal JSON is necessarily legal XSON; one parser suite; JSON documents contain no extended types.
- **Four-type round trip**: bytes/time/set/intmap serialize and parse back as-is, at zero convention cost.
- **Time normalization**: `time(...)` parses to normalized UTC microseconds (the `xtime` measure); serialization writes ISO 8601.
- **Selection rule**: receiver controllable → XSON for full types; not → JSON for interoperability; convert at boundaries, don't compromise inside.
- **Inherited strictness**: three gates, duplicate-key rejection, line/column error location — fully consistent with JSON.
- **Compact and pretty**: compact for machine channels, pretty for on-disk review — the same selection rule as JSON's.

### From examples to engineering: three hosts of XSON

**Internal configs** (the main battleground): load at startup → value tree → Chapter 30's merging — binary keys, expiry times, permission sets, and port maps all first-class types, zero conversions in consuming code. This is XSON's direct target scenario. **Internal service bus**: message serialization between services — both ends are XRT, and the full-type round trip deletes the codec layers on both sides; combined with Chapter 21's queues, choose per channel between passing value trees directly (no serialization) and transmitting XSON-serialized (across processes). **The downgrade exit at outer boundaries**: when JSON output is required, downgrade by convention (Pitfall 1's good form) — downgrade logic concentrates in the boundary module while internal value trees keep full types. The three hosts share one point: **type information is always first-class internally, downgrading only at explicitly marked boundaries**.

### A comparison: the same config in two spellings

Write the "certificate config" in both formats and the differences leap out. JSON shape: `{"key": "AAEC...", "expire": "2026-07-31T00:00:00Z", "ports": [80, 443, 80]}` — the key needs documentation saying it's Base64, expire needs an ISO-and-timezone convention, the ports array carries duplicates and the consumer must deduplicate. XSON shape: `{"key": bytes("AAEC..."), "expire": time("2026-07-31T00:00:00Z"), "ports": set[80, 443]}` — three self-explanatory fields, one line of exact reading each in consuming code. The comparison's point is not "XSON is shorter" (it may be longer) but that **conventions moved from documents into syntax** — documents forget and get lost; syntax is enforced by the parser. That is the whole meaning of "first-class types".

## Pitfalls

### Pitfall 1: sending XSON to an uncontrollable receiver

Symptom: external integrators report "your JSON format is illegal" — their parser throws a syntax error at `bytes(` or `set[`.

Cause: XSON's extended syntax is illegal to pure JSON parsers — the superset's "super" holds only for XRT and consenting parties.

```c bad
/* return XSON serialization directly on an external API */
str sBody = xrtXsonStringify(pResponse, false, NULL);
HttpReply(200, sBody);   /* client JSON parser: syntax error */
```

```c good
/* JSON only on external channels: extended types convert explicitly at the boundary */
str sBody = xrtJsonStringify(pResponse, false, NULL);
/* bytes/time converted to the agreed JSON forms before the exit (field docs spell it out) */
```

### Pitfall 2: reading a time value as a string

Symptom: reading a time field yields failure or empty — "but the serialization clearly has the string `time("...")`".

Cause: XSON's time is a **first-class type** — the value tree's type is Time, not String; `GetString` fails under Chapter 30's exact-read semantics (type mismatch).

```c bad
xstrview When;
xrtValueGetString(pUpdated, &When);   /* the type is Time - the exact read fails */
```

```c good
xtime When;
xrtValueGetTime(pUpdated, &When);     /* GetTime reads the microsecond integer */
/* for display formatting: format it yourself (Chapter 41's time module) */
```

## Exercises

### Basic: type identification

Parse an XSON document containing one of each of the four extended types; print each field's type name with `xrtValueType`; then serialize back to text and compare — the type annotations survive unchanged.

### Advanced: config migration

Migrate a JSON config (with a Base64 string field, an ISO time string field, and a port array) into XSON's full-type form: upgrade field types (string → bytes/time, array → set/intmap), switch parsing code to exact reads, and compare size and readability before and after. Hint: the migration's value must show up in "consumer code got simpler" — count how many conversion lines you deleted.

### Challenge: a dual-format exporter

Implement `export(值树, 格式)` (value tree, format): XSON output keeps full types; JSON output downgrades extended types by convention (bytes → Base64 string, time → ISO string, set → array, intmap → string-keyed object), declaring the downgrade convention in a header field. Acceptance criteria: XSON import-export round trips equal per type; the JSON export parses under any JSON parser; the field correspondence table of both formats generates automatically.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Superset relation | legal JSON ⊂ legal XSON; the same parser suite and strictness |
| Four types | bytes inline Base64 / time ISO normalized to UTC microseconds / set deduplicated / intmap integer keys |
| Selection | receiver controllable → XSON; not → JSON; convert at boundaries, don't compromise inside |
| Reading time | `GetTime` reads the microsecond integer — time is a first-class type, not a string |
| Error location | line/column positions from the same source as JSON — `line=1`-grade precision |
| Channel shapes | compact for machine channels / pretty for on-disk review / callback streaming writes |
