---
num: 78
slug: der-pem
title: DER and PEM: The Byte Shape of Certificates
volume: 卷八 安全
type: practice
lead: TLV cursor parsing and the textual envelope — the first vocabulary layer for reading every binary structure in the X.509/TLS world.
api: asn1, pem, codec
---

## Orientation

The four chapters starting here handle "the data structures of trust": how certificates are encoded (this chapter), parsed into views (Chapter 79), how signatures are verified (Chapter 80), and how trust chains string together (Chapter 81). The byte foundation of it all is **DER** — ASN.1's canonical encoding format: X.509 certificates, most structured fields in TLS messages, and key files are all DER. DER's shape is the uniform **TLV triple** (type, length, value); master the one model of "cursor reading layer by layer" and you hold the key to parsing the whole certificate ecosystem. **PEM** is DER's textual garment — the `-----BEGIN CERTIFICATE-----` block Base64 that lets binaries travel safely through email, config files, and environment variables. Both modules are zero-allocation borrowing-style: not one byte copied, only views produced.

## Introduction

You receive a `.pem` file from a colleague, said to be the service certificate. Opening it shows several Base64 texts starting with `-----BEGIN CERTIFICATE-----` — how does this become something a program can use? Conversely, your TLS service must store an ECDSA private key in the config center, which accepts text only. These two directions are PEM's encode/decode.

Drill deeper: what are the bytes that Base64-decode out of a PEM block? A DER-encoded certificate. To read "who issued it, when it expires, what algorithm the public key is" — you must parse DER. DER's rules live in the ASN.1 standards, but engineering-wise you need only one model: **every element is a "type-length-value" triple**, the type determining the value's structure (integer, string, OID, or a constructed type "containing more elements"). Unfold recursively and any certificate can be read. XRT's `asn1` module turns this model into a zero-allocation cursor — Chapter 79's certificate parser and Chapter 82's TLS message parsing are all built on it.

## Concepts

### TLV: the atomic shape of every DER document

A DER element is always three parts:

```diagram flow
- Type (Tag): class (UNIVERSAL/context...) + constructed bit + tag number, 1 to several bytes
- Length: content byte count; canonical rules demand the shortest encoding — 0x05 is 5, never 0x00 0x05
- Value: content bytes; a constructed type's (SEQUENCE/SET) value is itself a nested TLV sequence
```

For example, a SEQUENCE holding two INTEGERs: `30 06 02 01 07 02 01 2A` — `30` is "UNIVERSAL constructed SEQUENCE", `06` is the 6-byte content length, and the two `02 01 xx` are INTEGERs of 1 content byte each. **Recursiveness** is all of DER: a certificate is one big SEQUENCE nesting Issuer, Subject, public key, extensions... — the same TLV at every layer.

### DER's "strictness": why lenient parsing won't do

ASN.1 has a looser sibling, BER (Basic Encoding Rules): it allows multiple encodings of the same data — length-leading zeros, non-minimal integers, indefinite lengths. Loose formats are transport-hostile (the same certificate can have different byte sequences), so DER (**Distinguished** Encoding Rules) mandates **exactly one encoding per value**. Every read of XRT's cursor checks in strict DER mode: rejecting BER indefinite lengths and truncation, non-minimal lengths, non-minimal or overflowing high tag numbers, wrong primitive/constructed forms of common types, and non-canonical BOOLEAN/INTEGER/BIT STRING/NULL/OID. The cursor mode suits the "protocol structure known" high-performance path; for **untrusted standalone DER documents** (downloaded certificates, key files), run `xrtDerValidate` once at the entrance for whole-document checking — eliminating "discovering an overrun halfway through".

### The cursor model: zero allocation, zero copy, three-state returns

Four value objects constitute the whole parsing state: `xasn1tag` (class + constructed bit + 32-bit tag number), `xdervalue` (type + byte-range **view**; Raw includes the full TLV, Value only the content — both borrowing the input), `xdercursor` (input + bounds + next-item offset; copyable by value and independently traversable), `xderresult` (three states). Six core entries:

- `xrtDerInit`: binds the input into a root cursor — records pointer and length only, parsing no bytes.
- `xrtDerRead`: sequentially takes the next element, returning three states — `XDER_VALUE` (got one), `XDER_DONE` (read to the normal end, **not an error**), `XDER_ERROR` (illegal structure; sets the thread error).
- `xrtDerExpect`: asserts "the next must be a certain type" and takes it — paths with fixed tags in protocols use it (one branch fewer than "read then check type").
- `xrtDerEnter`: enters the interior of a SEQUENCE/SET/explicit tag for a child cursor — no content copied, descending layer by layer.
- `xrtDerDone`: asserts the cursor consumed exactly — one extra byte counts as illegal structure (against trailing garbage).
- `xrtDerIs`: peeks without advancing — dispatch decisions for CHOICE and OPTIONAL fields.

Two iron rules: **a failed call is a no-op** (`Read`/`Expect` advance the cursor and publish outputs only on success); **zero allocation and zero-copy throughout** (value conversion triggers on demand via the type-helper layer).

### The type-helper layer: the last step from view to number

`xrtDerUInt64/Int64` (INTEGER view → integer), `xrtDerUnsigned`, `xrtDerBoolean`, `xrtDerOctets`, `xrtDerBitString`, `xrtDerOid` (object identifier) and `xrtDerOidEqual` (OID comparison) — the cursor gives "byte ranges"; whether and what to convert into numbers is the caller's decision. This design lets "structure-only, no numbers" paths (like certificate-fingerprint traversal) pay zero conversion cost.

### PEM: the block protocol of the textual garment

PEM wraps arbitrary bytes into labeled text blocks:

```text
-----BEGIN CERTIFICATE-----
MIIB...(Base64 at 64 characters per line)
-----END CERTIFICATE-----
```

`xrtPemFind(文本, 长度, 标签, 块)` locates the **next** block of the given label in the text — explanatory text around blocks is allowed (comments common in certificate-chain files), LF/CRLF/CR all count as line breaks, and the begin and end labels must **match exactly**; nested boundaries, missing end boundaries, and illegal labels are rejected outright. The returned `xpemblock` holds only borrowed views (Label/Body/Raw all point into the original text). `xrtPemDecodeNew(块, &长度)` Base64-decodes the Body into an **owning** byte buffer (freed with `xrtFree`) — this is the step that allocates; `xrtPemEncodeNew(标签, 字节, 长度)` generates canonical PEM text in reverse (64-character wrapping). Multiple blocks in one file (private key + chain): loop `Find` over the same text — cursor semantics identical to DER's.

## Examples

### First complete program: cursor-reading a DER SEQUENCE

The following program comes from `examples/asn1/der/main.c`, reading two integers from an 8-byte minimal DER document — a miniature of certificate parsing:

```embed path="examples/asn1/der/main.c" title="examples/asn1/der/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/asn1/der/main.c -lws2_32 -liphlpapi
7 + 42 = 49
```

**What just happened.** (1) `xrtDerValidate` checks the whole first — length prefixes and nesting boundaries in one pass; for untrusted input this is the entrance action — a broken structure stops right there. (2) `Expect` asserts the root element is `(UNIVERSAL, SEQUENCE, 构造式)` — three parameters fully specifying the TLV's type part; then `Enter` descends for a child cursor, not one byte of the original copied. (3) Two `Read` calls each take an `xdervalue` (type + range view); `xrtDerUInt64` converts to numbers on demand — 7 and 42. (4) `Done` asserts exact consumption: one extra 0x00 at the input's end fails it — "against trailing garbage" is the safety baseline of protocol parsing (the later certificate parsing and TLS message parsing all close with Done). Any failing link takes the same error branch — the chained parsing style is this chapter's recommended form.

### Second complete program: PEM encode, find, decode round-trip

The second program comes from `examples/asn1/pem/main.c`, wrapping a 5-byte payload into PEM text and restoring it losslessly:

```embed path="examples/asn1/pem/main.c" title="examples/asn1/pem/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/asn1/pem/main.c -lws2_32 -liphlpapi
-----BEGIN XRT DATA-----
AQIDBAU=
-----END XRT DATA-----
```

**What just happened.** (1) `xrtPemEncodeNew("XRT DATA", ...)` generates the complete PEM text with a custom label (trailing newline included) — swapping the label for `CERTIFICATE` or `PRIVATE KEY` yields the standard files. (2) `xrtPemFind` locates the block of that label in the generated text — note it returns a **borrowed view**, allocating nothing; the real scenario is looping Find over a whole text read from disk, processing the chain block by block. (3) `xrtPemDecodeNew` Base64-decodes into owning bytes; a triple check (non-empty, length, byte-by-byte memcmp) proves the lossless round trip. (4) The two owning resources (text and bytes) are `xrtFree`d one by one — Encode/Decode are the only two allocation points; Find and the DER cursors allocate nothing throughout.

## Contracts

- **Cursor semantics**: a failed call is a no-op for both cursor and outputs; `Read/Expect` advance only on success; `XDER_DONE` is a normal end that sets no error.
- **Strict DER**: every read rejects indefinite lengths, non-minimal encodings, overflowing tag numbers, wrong constructed forms, and non-canonical primitive types; untrusted standalone documents get `Validate` at the entrance.
- **Borrowing lifetime**: `xdervalue`/`xpemblock` all borrow the input; the input memory must stay valid until all views are done — the borrowing lifecycle (the same rule as Chapter 15's buffer views).
- **Separated value conversion**: views and numeric conversion are two steps (`Read` gives a view; `UInt64` etc. convert on demand) — paths wanting no numbers pay no conversion.
- **PEM strict boundaries**: begin/end labels match exactly; nested, missing boundaries, and illegal labels rejected; explanatory text around blocks allowed; the three line breaks equivalent.
- **Ownership**: `PemEncodeNew`/`PemDecodeNew` return owning objects (`xrtFree`); `PemFind`/the cursor family allocate zero.
- **Threading**: all value objects hold no shared mutable state; cursors/views held separately are concurrent on any threads.
- **Trimming**: `XRT_MODULE_ASN1_DER` (the DER cursor) and `XRT_MODULE_PEM` (depending on Base64) trim independently; PEM drags in no DER and vice versa.

## Pitfalls

### Pitfall 1: parsing untrusted DER without Validate

Symptoms: parsing an external certificate reports "length overrun" only midway — several layers of views already published, and error handling must roll back a pile of state; worse, accepting BER-lax data as DER, two machines parse the same file differently.

Cause: the cursor is lazy — `Init` parses nothing, and `Read` validates each layer as it reaches it. For input of unknown structure, "discovering illegality while reading" means half the state is already published.

```c bad
xrtDerInit(&Root, pUntrusted, iSize);   /* start directly */
xrtDerRead(&Root, &Value);               /* fails only midway */
```

```c good
if ( !xrtDerValidate(pUntrusted, iSize) ) {
	return false;   /* entrance whole-check: bad structure stops here */
}
xrtDerInit(&Root, pUntrusted, iSize);
```

### Pitfall 2: finishing without asserting Done, swallowing trailing garbage

Symptoms: the protocol parse "succeeds", but the peer appended arbitrary bytes after the legal structure — the next parse position scrambles, or unaudited data gets injected.

Cause: `Read` returning `XDER_DONE` only says "this cursor's bounds are read"; if an outer container's length disagrees with actual content, or the input has extra trailing data, not asserting means missing it.

```c bad
while ( xrtDerRead(&Items, &Value) == XDER_VALUE ) {
	handle(&Value);   /* loop ends and done: trailing garbage unquestioned */
}
```

```c good
while ( xrtDerRead(&Items, &Value) == XDER_VALUE ) {
	handle(&Value);
}
if ( !xrtDerDone(&Items) ) {
	return false;   /* exactly consumed — one extra byte is illegal */
}
```

### Pitfall 3: PEM labels by assumption, mismatched case or spacing

Symptoms: `PemFind` cannot find a block that clearly exists — the file's label is `TRUSTED CERTIFICATE` or carries surrounding whitespace, while the search used `CERTIFICATE`.

Cause: PEM boundary matching is **exact** — `-----BEGIN CERTIFICATE-----` and `-----BEGIN TRUSTED CERTIFICATE-----` are two different blocks (history has many more label variants coexisting). A loose "contains means hit" mistakes a private-key (plaintext-form) block for a certificate block.

```c bad
/* label from memory, without checking the file's actual boundaries */
xrtPemFind(Text, iSize, "CERTIFICATE", &Block);
/* the file actually has -----BEGIN TRUSTED CERTIFICATE-----: not found, or the wrong block */
```

```c good
/* search by the file's real label first; for multi-label files try each or walk all blocks */
if ( !xrtPemFind(Text, iSize, "CERTIFICATE", &Block) &&
	!xrtPemFind(Text, iSize, "TRUSTED CERTIFICATE", &Block) ) {
	return false;   /* an explicit failure beats silently finding the wrong block */
}
```

## Exercises

### Basic: hand-decode a DER document

Hex `30 09 02 01 0A 02 04 00 A1 B2 C3` — first hand-derive the structure (SEQUENCE holding two INTEGERs), then read both values with a cursor program and print them (10 and 0xA1B2C3). Then deliberately change the middle `04` to `05` (broken length) and verify `Validate` rejects it. Acceptance: hand-derived results match the program output; the breakage experiment returns failure.

### Advanced: a certificate-chain file unpacker

Read a PEM file holding several certificates (self-generate with OpenSSL), loop `PemFind` + `PemDecodeNew` to decode each block into DER bytes, printing per block "block N: X bytes, DER validation passed". Acceptance: a three-certificate chain file outputs three lines; `Validate` passes for every block; decode buffers freed one by one with no leaks (Chapter 6's stats).

### Challenge: a recursive structure dumper

Implement `dump(xdercursor* Cursor, int iDepth)`: recursively print the structure tree of any DER document — constructed types unfold by indentation, primitive types print type and length (INTEGERs also print their value). Use it to print the built-in certificate of Chapter 79's inspect example, annotating layer by layer against RFC 5280's Certificate structure (tbsCertificate/subjectPublicKeyInfo/extensions). Acceptance: unfolds completely to the leaf layer; on BER-lax input (hand-built non-minimal length) it errors out rather than crashing.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| TLV triple | type (class + constructed bit + tag number) + length (shortest encoding) + value (constructed types nest TLV) |
| DER vs BER | DER has one encoding per value; the cursor's strict mode rejects indefinite lengths / non-minimal / overflowing tags |
| Six cursor entries | Init (bind) / Read (three-state) / Expect (assert type) / Enter (enter constructed) / Done (exactly read) / Is (peek) |
| Three-state returns | `XDER_VALUE` got one / `XDER_DONE` normal end (not an error) / `XDER_ERROR` illegal structure |
| Failed no-op | Read/Expect failures do not advance the cursor or publish outputs |
| Separated conversion | view → number in two steps: UInt64/Int64/Boolean/Oid triggered on demand |
| Entrance validation | untrusted standalone DER gets `Validate` first; cursor mode suits known structures |
| PEM structure | exact BEGIN/END labels + 64-character-wrapped Base64; explanatory text allowed around blocks |
| Three PEM entries | Find (borrowed view, zero allocation) / DecodeNew (owning bytes) / EncodeNew (owning text) |
| Line-break compatibility | LF / CRLF / CR all equivalent; nested and missing boundaries rejected |
| Trimming | ASN1 and PEM (depends on Base64) independent; neither drags in the other |
