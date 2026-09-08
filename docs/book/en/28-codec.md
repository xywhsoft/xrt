---
num: 28
slug: codec
title: The Codec Trio: Base64 / Hex / Percent
volume: 卷四 文本与结构化数据
type: practice
lead: Three bridges between the binary and text worlds — derivable capacities, no half-written results on failure, variants declared explicitly.
api: codec
---

## Orientation

Before binary data can enter a text channel (HTTP headers, JSON fields, URL parameters), it must first be "translated" into pure-text bytes — the codec module provides three standard translations: **Base64** (3 bytes into 4 characters, 33% expansion — data headers, JWTs, mail attachments), **Hex** (1 byte into 2 characters, 100% expansion — debug dumps, key fingerprints, short binary values), and **Percent** (escapes only illegal characters, minimal expansion — URLs and forms). The trio shares one set of engineering conventions: **capacity rules are derivable** (no guessing buffer sizes), **failures never write half a result** (atomicity), and **variants are declared explicitly** (URL-safe, letter case, and custom alphabets are parameters, not guesses). These conventions share ancestry with Chapter 26's strict parsing — every entrance and exit of the text world is exact to the byte.

## Introduction

Three real needs. A mail attachment is binary, but SMTP accepts only text — Base64 turns every 3 bytes into 4 printable characters, one-third more volume in exchange for passage through the mail channel. Debugging requires printing a short stretch of key material into the log — Hex turns each byte into two hexadecimal characters, human-readable, copyable, comparable. User input must go into a URL query parameter — in `reports/July 2026`, both `/` and the space are reserved or illegal URL characters; Percent encoding turns them into `%2F` and `%20` while every other character passes through untouched — three encodings for three demands: wholesale conversion, readable conversion, minimal conversion.

The first question of selection is always **how much will the channel tolerate**: 33% expansion for Base64, 100% for Hex, near 0% for Percent — the answer is usually dictated by the protocol itself (`Content-Transfer-Encoding: base64`, the URL spec's percent-encoding); your job is to use it correctly, not to choose freely.

## Concepts

### Shape comparison of the trio

```diagram flow
- Base64: 3 bytes -> 4 characters (33% expansion); the standard for data headers / JWTs / attachments
- Hex: 1 byte -> 2 characters (100% expansion); dumps / fingerprints / short values, human-readable
- Percent: escapes only the target set (~0% expansion); minimal rewriting for URLs and forms
```

### Capacity rules: output size is always computable by hand

Base64 output = `4 × ceil(n / 3)` (padding included); Hex output = `2n`; Percent output's upper bound = `3n` (every byte might be escaped). **Compute the capacity before handing over a buffer** is this chapter's fundamental drill — the `Encode` family writes into the caller's buffer and fails **wholesale without writing half a result** when capacity is short (atomicity: the output buffer never retains a half-result that someone could misuse), while the `EncodeNew` family returns an owning buffer directly (freed with `xrtFree`), letting the library allocate for you. Choosing between the two routes is fully isomorphic to Chapter 25's "caller's buffer vs owning".

### Explicit variants: standard and URL-safe

The standard Base64 alphabet contains `+` and `/` — reserved characters in URLs, hence the **URL-safe variant** (swapped to `-` and `_`), enabled via `xbase64config`'s alphabet parameter (pass NULL in the `Alphabet` field for the default standard table); decoding validates strictly against the same configuration — an alphabet mismatch is a failure. Hex has two output cases, upper and lower (decoding accepts both). Percent's **escape set** is declared per scenario (URL path segments and query parameters reserve different characters). Same principle as Chapter 26: variants are explicit parameters, never guessed defaults.

### Strictness on the decoding side

Decoding is just as strict: illegal characters (outside the alphabet), misaligned lengths (Base64 not a multiple of 4), truncated escape sequences (an isolated `%` in Percent) all fail wholesale and leave an error — garbage can't get in, half-results can't get out. This is the same design philosophy as the charset chapter's STRICT policy (Chapter 27) and the number chapter's consume-the-whole-span (Chapter 26): **the text boundary is where the attack surface meets the accident surface; fuzziness here was never tolerance, always a hazard**.

## Examples

### Complete program: Base64 in standard and owning postures

From the repository sample `examples/codec/base64/main.c`:

```embed path="examples/codec/base64/main.c" title="examples/codec/base64/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/codec/base64/main.c -lws2_32 -liphlpapi
xrt codec -> eHJ0IGNvZGVj
```

**What just happened.** (1) Input is 9 bytes; the capacity rule `4 × ceil(9/3)` = 12 characters — the buffer is sized by formula, and `Encode` writes it in one step. (2) `sizeof-1` excludes the trailing zero — the old rule from Chapter 12's hashing: the input to encoding is a byte string, and the zero terminator is merely a C-string accident. (3) Decoding takes the `DecodeNew` owning route — a round trip verifies the encoding. (4) "Atomicity" deserves hands-on verification: shrink the buffer on purpose, and `Encode` fails with the output area untouched — no half base64 left lying around for someone to misuse.

### Complete program: a Percent encode-decode round trip

From `examples/codec/percent/main.c`:

```embed path="examples/codec/percent/main.c" title="examples/codec/percent/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/codec/percent/main.c -lws2_32 -liphlpapi
encoded: reports%2FJuly%202026
decoded: reports/July 2026
```

**What just happened.** (1) The encoding touched exactly two characters: `/` → `%2F`, space → `%20`, letters untouched — the shape of minimal rewriting at a glance. (2) The decode round trip is lossless — Percent's decoding is the strict inverse of its encoding; an isolated `%` or illegal hexadecimal fails wholesale on the decoding side. (3) The URL landing point: chain it after Chapter 25's query-string parser and you have the complete URL toolkit — parse (string) → encode (codec) → transmit.

### The trap terrain on the decoding side

Decoding carries three more trap classes than encoding, each worth naming. **Alphabet drift**: the sender uses URL-safe, the receiver decodes by the standard table — `-`/`_` are not in the standard table, so wholesale failure is the lucky case; sneakier is when both sides "leniently" handle their own halves and the data quietly misaligns. **Padding ambiguity**: whether Base64's `=` padding is present is dictated by protocol (JWT segments carry none); an implementation that leniently forgives omissions — strict mode's rejection of non-aligned input exists precisely to keep such ambiguity outside the door. **Nested-encoding detection**: seeing `%2520` should tell you this is the fossil of double Percent encoding — a decoder cannot fix an upstream process error; it can only fail, strictly. The common solution for all three is one line: **the same configuration on both ends, strict mode, and failures treated as process signals, not data noise**.

### The dividing line with Chapter 27's transcoding

A confusable pair: charset's `xrtTranscode` (Chapter 27) and this chapter's encoding are both "byte-form transformations" — where is the boundary? **Transcoding changes the byte representation of the same character** (the same Chinese character written in UTF-8 and in UTF-16 is still one character): semantics unchanged, losslessly round-trippable. **Encoding changes the presentation form of bytes** (a binary byte stream becomes printable characters): the semantics are "equivalent transport", not "the same character". The mnemonic: are input and output two spellings of the same thing — if yes, transcoding; if not, encoding. Chaining the two is also common: binary → Base64 → UTF-16 handed to a Windows API, each step independently correct.

## Contracts

- **Derivable capacity**: Base64 `4⌈n/3⌉`, Hex `2n`, Percent bounded by `3n`; compute first, provide second.
- **Atomicity**: insufficient caller buffer fails wholesale; half a result is never written.
- **Two routes**: `Encode/Decode` write the caller's buffer; `EncodeNew/DecodeNew` produce owning products (`xrtFree`).
- **Explicit variants**: URL-safe, letter case, and escape set are parameters; decoding validates strictly against the declared alphabet.
- **Strict decoding**: illegal characters, misaligned lengths, and truncated escapes fail wholesale.

### From examples to engineering: three hosts of encoding

Three ways encoding hangs in engineering. **Protocol exits**: JWT assembly, binary carried in HTTP headers — encode at the exit, decode at the entrance, once per side of the channel, never twice (double encoding is a real incident: Percent-encoded twice, `%20` becomes `%2520`). **Log dumps**: Hex, one-shot, short, for human eyes — paired with Chapter 26's format strings for offset annotation. **Persisted fields**: binary inside JSON/config files (Chapter 31 will show JSON's Base64 support) — encode once on store, decode once on load, the field semantics declared in the schema. The shared discipline of the three hosts: **encoding is a one-time boundary action, not an intermediate state** — "encode-then-encode-again" and "half-decoded" both indicate a process-design problem.

### An expansion comparison table

| 100 bytes of data | Base64 | Hex | Percent (worst case, all escaped) |
| --- | --- | --- | --- |
| Output size | 136 (with padding, 4⌈100/3⌉) | 200 | ≤300 |
| Channel | mail/data headers/JSON | dumps/fingerprints | URL/forms |
| Readability | none | good | original text preserved |

The table's key reminder: Percent's "minimal expansion" holds only outside the worst case — nearly pure-ASCII safe characters expand almost zero, while fully-escaped binary data hits the 3x worst case. **Choosing Percent for binary data is a selection error** (it is designed for the few special characters inside text); the semantics of its escape set decide its field of applicability.

## Pitfalls

### Pitfall 1: treating Base64 as encryption

Symptom: a security audit finds the "encrypted field" is merely Base64; in penetration testing, tokens are decoded outright.

Cause: Base64 is an **encoding**, not **encryption** — reversible by anyone, with zero confidentiality; the confusion power of the "64" in its name is a sibling of "a hash is not encryption" (Chapter 12).

```c bad
char arrToken[64];
xrtBase64Encode(SecretKey, sizeof(SecretKey), arrToken, sizeof(arrToken), NULL, NULL);
Send(arrToken);   /* "secrecy by encoding" - the receiver decodes once and has the key */
```

```c good
/* the right channel for confidential data: Volume 8 crypto (encryption) or the token system (Chapter 11 SecureText) */
/* Base64 solves only "binary into a text channel", not secrecy */
```

### Pitfall 2: standard Base64 straight into a URL

Symptom: a Base64 value containing `+` or `/` placed in a URL makes server-side decoding fail intermittently or the data misalign — only on particular values.

Cause: `+` is interpreted as a space in query strings and `/` is the path separator — the standard alphabet collides with the URL character set.

```c bad
xrtBase64Encode(Data, Size, arrUrl, sizeof(arrUrl), NULL, NULL);   /* default alphabet contains + / */
AppendQueryParam(Url, arrUrl);   /* + becomes space, / cuts the path - decode must fail */
```

```c good
xrtBase64Encode(Data, Size, arrUrl, sizeof(arrUrl), NULL, &tUrlSafe);   /* URL-safe alphabet: - and _ */
AppendQueryParam(Url, arrUrl);   /* every character URL-safe */
```

## Exercises

### Basic: verify the capacity formula

For inputs of 1, 2, 3, 4, 9, and 10 bytes, compute the Base64 output length by hand and verify by encoding (six groups, hand-computed first, then compared); then use a deliberately short buffer to verify wholesale atomic failure with the output area unwritten.

### Advanced: a Hex dumper

Implement `dump(视图, 前缀)` (view, prefix): output the byte stream in Hex, 16 bytes per line, offset at line start — the debugger's hexadecimal view is exactly this. Hint: annotate offsets with Chapter 26's format strings (`%06X`-style hexadecimal).

### Challenge: a URL builder

Implement `url_encode_query(基准URL, 键值对数组)` (base URL, key-value array): Percent-encode keys and values separately, assemble the query string (Chapter 25's builder), and output the complete URL. Acceptance criteria: ten input groups containing Chinese keys and values plus `&`/`=`/space/`+` encode into URLs that a standard URL parser restores correctly; the decode round trip is byte-for-byte identical.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| The trio | Base64 (33% expansion) / Hex (100%) / Percent (≈0%) — choose by channel rules |
| Capacity | Base64 `4⌈n/3⌉`, Hex `2n`, Percent ≤`3n` — compute first, provide second |
| Atomicity | insufficient buffer fails wholesale; half a result is never written |
| Two routes | To-buffer versions / New owning versions, isomorphic with the container chapters |
| Variants | URL-safe (`-`/`_`), Hex letter case, Percent escape set — explicit parameters |
| Red line | encoding is not encryption; secrets go through Volume 8 or SecureText |
| Three hosts | protocol exit encodes once / log dumps / persisted fields — encoding is a boundary action, not an intermediate state |
| Boundary with transcoding | two spellings of the same character = transcoding; binary made printable = encoding |
