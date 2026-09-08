---
num: 27
slug: charset
title: Character Sets and Unicode
volume: 卷四 文本与结构化数据
type: practice
lead: UTF-8 as the mainline, UTF-16 at platform boundaries, all-direction transcoding and strict policies — the correct way to handle multilingual text.
api: charset
---

## Orientation

XRT's text mainline is **UTF-8**: string views (Chapter 3) and the whole parser family consume UTF-8 by default. But the real world has three exits that need other encodings: Windows APIs want UTF-16, legacy data may be GBK/Latin-1, and network protocols sometimes specify UTF-32 — the charset module is the conversion layer at exactly these boundaries. This chapter covers three things: **the relation between scalars and bytes** (why "character count" differs from "byte count"), **all-direction transcoding across UTF-8/16/32** (with BOM wrapping and error policies), and **scalar-indexed safe slicing** (never cutting a multi-byte character in half). The distance/similarity functions previewed in Chapter 25 also live here — they count Unicode scalars, a demonstration of upgrading from "byte thinking" to "character thinking".

## Introduction

Take `"XRT 你好 😀"`: `strlen` counts 14 bytes, but there are only 8 "characters" — each Chinese character is 3 bytes, the emoji is 4. This gap is not an academic matter: truncate a username by bytes and half of a Chinese character turns to garbage; enforce a length limit by bytes and Chinese users get two-thirds fewer characters than English users; sort and compare by bytes and the ordering has nothing to do with what users expect. The common root cause of all three bugs: **treating bytes as characters**.

The charset module's core abstraction is the **Unicode scalar value** (code point) — that is the machine representation of "one character" (approximately; strictly speaking there are also combining characters, see the contracts section). Bytes are the encoded form: UTF-8 encodes a scalar as 1~4 bytes, UTF-16 as 1~2 16-bit units (supplementary-plane characters need a "surrogate pair"), UTF-32 as a fixed 4 bytes. Every API in this chapter revolves around converting between "scalars ↔ the various byte forms".

## Concepts

### Shape differences among the three encodings

```diagram flow
- UTF-8: scalars encoded as 1~4 bytes; ASCII-compatible; XRT's mainline and the network default
- UTF-16: scalars encoded as 1~2 16-bit units; supplementary planes (like 😀) need a surrogate pair
- UTF-32: scalars fixed at 4 bytes; convenient for internal computation, rare on the wire
```

Measured on `"XRT 你好 😀"` (the main example's output): UTF-16 is **9 units** — of the 8 scalars, 7 occupy 1 unit each, while 😀 sits in a supplementary plane and takes a surrogate pair of 2 units. **Unit count is not character count, and byte count even less so** — these three metrics (bytes/units/scalars) always differ in multilingual text; before answering "how long", first ask "measured in what".

### All-direction transcoding and the BOM

`xrtTranscode` converts in every direction among UTF-8/16/32 (each with LE/BE): input and output are both byte views, the target encoding is declared by an `xencoding` enum (byte order lives in the enum too), and BOM wrapping is an independent boolean parameter. The BOM trade-off: when the protocol already declares byte order, don't add one (HTTP headers and JSON have their own rules); when writing files to disk, adding one is recommended — text editors open them without mojibake.

### Error policies: STRICT / REPLACE

What if the input contains illegal sequences? Two policies, chosen explicitly: `XUTF_STRICT` fails outright (the error slot records the position — right for boundary validation where "input must be legal"); `XUTF_REPLACE` substitutes the replacement character U+FFFD (right for display paths — the user sees □ instead of a crash); there are only the two grades, STRICT and REPLACE. **The default should be STRICT** — silent substitution masks upstream encoding bugs; REPLACE is reserved for the last step that is already confirmed to face human eyes.

### Scalar-based safe operations

A byte index sliced into the middle of a multi-byte character is a source of garbage text. `xrtUtf8Slice` slices UTF-8 by **scalar index** — the cut point auto-aligns to character boundaries; `xrtUtf8Distance` / `xrtUtf8Similarity` count edit distance by scalars (Chapter 25's cheat sheet foreshadowed that they live here); `xrtUtf8Count` returns the scalar count. User-visible truncation, counting, and comparison all use this family; byte operations on internal transfer stay with the string family.

## Examples

### Complete program: strict UTF-8 ↔ UTF-16 round trip and scalar slicing

From the repository sample `examples/charset/unicode/main.c`:

```embed path="examples/charset/unicode/main.c" title="examples/charset/unicode/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/charset/unicode/main.c -lws2_32 -liphlpapi
UTF-16 units: 9
XRT 你好 😀
scalar slice: 你好
```

**What just happened.** (1) `xrtUtf8ViewTo16` converts the UTF-8 view into an **owning** UTF-16 array (strict mode); the out-param carries the unit count 9 — not 8: 😀's surrogate pair occupies 2 units, the most intuitive teaching aid for "unit count ≠ scalar count". (2) `xrtUtf16ViewTo8` converts back and the output is unchanged — a lossless round trip; strict mode guarantees legal sequences in both directions. (3) `xrtUtf8Slice` slices by scalar index to extract `你好` — the cut lands on a character boundary; even if the given byte position happens to be the middle byte of a Chinese character, the function aligns to the boundary instead of cutting out half a character. The standard usage at the Windows boundary is exactly these two steps: To16 before leaving into the system call, To8 after coming back.

### Complete program: transcoding wrapped with a BOM

From `examples/charset/transcode/main.c`:

```embed path="examples/charset/transcode/main.c" title="examples/charset/transcode/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/charset/transcode/main.c -lws2_32 -liphlpapi
UTF-16 LE bytes with BOM: 14
```

**What just happened.** (1) `xrtTranscode` targets UTF-16 LE with a BOM — the output byte stream starts with `FF FE`, and the receiver identifies encoding and byte order at a glance. (2) The byte count 14 is computable by hand: 6 in-BMP code points at 2 bytes each = 12, plus the 2-byte BOM — **the size of transcoding output is derivable**; buffer planning needs no guessing. (3) All-direction conversion means UTF-32 → UTF-8, Latin-1 → UTF-16, and every other combination goes through the same entry — the parameters declare both ends' encodings; you don't learn one function per pair.

## Contracts

- **UTF-8 mainline**: the XRT string family and the parsers default to UTF-8; conversion happens only at system and protocol boundaries.
- **Strict by default**: illegal sequences fail wholesale under `XUTF_STRICT`; REPLACE is an explicit choice, for display paths only.
- **Three metrics**: byte count / UTF-16 unit count / scalar count can always differ — ask the metric before the length.
- **Scalar safety**: user-visible truncation/counting/comparison uses the Utf8 family by scalars; byte operations are for internal transfer.
- **Owning products**: transcoding and To16/To8 return owning arrays, freed per their declarations (`xrtFree`).
- **BOM trade-off**: skip it when the protocol already declares byte order; recommended when writing files to disk.

### Streaming decode and incremental validation

Chunked data (network streams, large files) cannot wait for "all bytes assembled, then validate" — a multi-byte character may fall exactly on the boundary between two chunks. The `xutf8state` streaming decoder exists for this: `xrtUtf8StateInit` opens, `xrtUtf8StateFeed` feeds chunk by chunk (the `bFinal` flag marks the last chunk), `xrtUtf8StateError` reads the error position — an incomplete character straddling a chunk boundary is remembered by the state machine and never misjudged as illegal. This is where "strict validation" and "streaming processing" converge; Chapter 34's line reader and Volume 7's protocol streams both use this shape. Validating a large file's encoding legality the right way is exactly this: feed streaming, check the state after the last chunk, zero whole-file buffering throughout.

### Combining characters and user-perceived characters

This chapter has been careful to say "scalar" rather than "character", because in Unicode the two are not always equal: an accented letter can be two scalars ("base letter + combining mark"), and emoji families have ZWJ sequences (family and profession combinations) that run to a dozen-plus scalars. **Scalar counting is sufficient for the vast majority of business needs, but "user-perceived character count" (grapheme cluster) is the stricter measure** — the input box's "N characters left" hint and cursor stepping, strictly speaking, should both follow clusters. XRT's position: the scalar layer is the common foundation (this chapter's family); cluster-level needs are handled case by case at the display layer (most projects are fine with scalars; upgrade when complaints arrive — but you must know the difference exists to recognize it when it does). Keep this passage in mind: the day a "wrong length" user report finds you, you will know where to look — it also explains why this chapter consistently uses the precise word "scalar" instead of the colloquial "character".

### Reconfirming the division of labor with Chapter 25

One-sentence division: **byte operations go to string (Chapter 25), character operations go to charset (this chapter)**. Search, splitting, and concatenation — this "content processing" — works by bytes and is safe under UTF-8 (separators are usually ASCII); counting, truncation, width, and distance — these "user-visible" operations — must follow this chapter's family by scalars. The two families share the same view type (`xstrview`) and can be chained seamlessly — locate by bytes with Find, truncate by scalars with Slice: the standard posture for mixing.

## Pitfalls

### Pitfall 1: truncating user text by bytes

Symptom: the tail of a Chinese/emoji username occasionally shows garbage or �; it reproduces deterministically when a particular length lands exactly inside a multi-byte character.

Cause: in UTF-8 one character takes 1~4 bytes, and a byte index has no notion of character boundaries — cutting in the middle produces an illegal sequence.

```c bad
if ( strlen(sName) > 32 ) {
	sName[32] = 0;   /* byte-level truncation: may cut a multi-byte character in half */
}
```

```c good
xstrview Name = (xstrview){ sName, strlen(sName) };
if ( xrtUtf8Count(Name) > 16 ) {
	Name = xrtUtf8Slice(Name, 0, 16);   /* truncate by scalars, boundary auto-aligned */
}
/* Name is a view: Dup it when a zero terminator is required */
```

### Pitfall 2: transcoding errors silently swallowed

Symptom: after multilingual data circulates one loop, some characters become � or vanish into thin air; the upstream encoding bug is never discovered.

Cause: the REPLACE/IGNORE policies "take care of" illegal input — the output looks normal, but the data is already corrupted and no alarm sounds.

```c bad
/* REPLACE for boundary validation - bad data from upstream gets silently laundered */
xrtTranscode(Input, XENCODING_UTF8, XENCODING_UTF16_LE,
	XUTF_REPLACE, true, ...);
```

```c good
/* strict validation at the system boundary first; convert only after legality is confirmed */
if ( xrtTranscode(Input, XENCODING_UTF8, XENCODING_UTF16_LE,
	XUTF_STRICT, true, ...) == NULL ) {
	RejectInput();   /* illegal sequences are rejected here - the bug surfaces at its source */
}
```

## Exercises

### Basic: three metrics

For `"XRT 你好 😀"`, output the UTF-8 byte count, the UTF-16 unit count, and the scalar count (14 / 9 / 8); then build a pure-emoji string and a pure-Chinese string and repeat — the gaps among the three metrics shift with content composition; record one set of numbers for each, worth eyeballing several sets by hand.

### Advanced: a truncator (scalar thinking in practice)

Implement `truncate_utf8(视图, 最大标量数)` (view, max scalars): truncate by scalars, align to boundaries, and append an ellipsis when over length (`…` is also 3 bytes — remember to count it). Verify no garbage with 10 inputs mixing Chinese, English, and emoji.

### Challenge: a Windows filename bridge

Write a pair `open_utf8(路径)/read_dir_utf8()` (path): convert paths to UTF-16 internally and call the wide-character system APIs, convert the returned UTF-16 back to UTF-8 — strict mode, with illegal sequences reported by position. Acceptance criteria: creating/opening/enumerating Chinese-named paths round-trips losslessly throughout; construct an illegal path containing an isolated surrogate byte, and the error report pinpoints the problem unit's position.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Mainline | UTF-8; conversion happens only at system/protocol boundaries, once per side |
| Three metrics | bytes / UTF-16 units / scalars — ask the metric before answering length |
| Transcoding | `xrtTranscode`, all-direction; BOM parameterized; output size derivable |
| Error policy | STRICT by default (boundary validation); REPLACE explicitly for display paths |
| Scalar safety | `xrtUtf8Slice`/`Utf8Count`/`Utf8Distance` operate by characters |
| Platform boundary | Windows APIs go To16 in, To8 out |
| BOM | skip when the protocol declares byte order; recommended for files on disk (independent boolean parameter) |
| Streaming | `xutf8state` validates chunk by chunk (Init/Feed/Error, three steps); no misjudgment across chunk boundaries |
| Character measure | scalar ≠ user-perceived character (cluster); most business uses scalars, display layers upgrade on demand |
