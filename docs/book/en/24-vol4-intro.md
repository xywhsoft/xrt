---
num: 24
slug: vol4-intro
title: Volume 4 · Text and Structured Data: Introduction
volume: 卷四 文本与结构化数据
type: intro
lead: The triple jump from bytes to meaning — strings, numbers, encodings, compression, and up to the gateway of structured data.
api: string, codec
---

## Orientation

Volume 4 answers "how does a program process arbitrary non-code data". It comes in two halves: the first (Chapters 25–29) is **text and byte infrastructure** — string views and builders, strict number parsing and formatting, charsets and Unicode, the codec trio, compression; the second (from Chapter 31) is **structured data** — dynamic values, JSON, XSON, line-delimited formats (JSONL/XSONL), templates, and regex. The first half is the second's foundation: parsers tokenize with the string family, convert with the numeric family, handle multilingual text with the charset family, and defend against bombs with resource limits — every chapter gets named and reclaimed in the second half.

### One through-line: strictness at boundaries

More important than "which functions you can use" is the same through-line recurring throughout Volume 4: **strictness at boundaries**. Number parsing is all-or-fail (Chapter 26), transcoding defaults to STRICT (Chapter 27), decoding leaves no half-result (Chapter 28), decompression caps its input (Chapter 29) — four modules, one philosophy: **vague leniency at boundaries is a hazard; explicit failure is the contract**. This is no accidental stylistic unity: text and bytes are the program's contact surface with the outside world, and attacks (injection, bombs, encoding traps) and accidents (truncation, drift, misalignment) all enter at boundaries. After the first half of Volume 4, every "eats external data" function you write should naturally carry this strictness — that is the muscle memory this volume most wants to leave behind.

### A quick mnemonic for the first five chapters

If you can take away only one line, take this: **"views don't allocate, strictness doesn't stay overnight, boundaries aren't lenient, expansion is budgeted, decompression is capped"** — five chapters, one phrase each. Chapter 25's views spare text operations from copying; Chapter 26's strictness exposes impurities the same day; Chapters 27/28's boundary discipline keeps encoding traps and half-results out; Chapter 28's capacity formulas make expansion budgetable; Chapter 29's caps keep bombs from detonating. Five phrases cover every pit of the first half — quotable directly in interviews, reviews, and code.

### The triple jump: bytes → text → structure

Volume 4's knowledge stack is a triple jump. **Level one: bytes ↔ text** — views (Chapter 3) are the currency, the string family (Chapter 25) the operations, encodings (Chapters 27/28) the boundary translation. **Level two: text ↔ numbers** — parsing and formatting (Chapter 26), where strictness takes shape. **Level three: text ↔ structure** — the second half's JSON/XSON turning character streams into value trees, templates turning value trees back into text. Each level stands on the last; skipping levels (say, jumping straight to JSON without views and strict parsing) bills you the understanding cost at debugging time.

### Reading advice and self-checks

Reading in order is this volume's best path — the triple jump's order is the chapter order, and skipping any level means paying the fare later in the second half's debugging. Readers already familiar with C text handling can fast-forward Chapter 25 but should stop and read closely at Chapters 26/27's strictness philosophy (it runs opposite to the standard-library habits most bring along). Before leaving the volume, self-check five questions: when to use views versus owning strings? Why is strict parsing safer than atoi's leniency? When must the three measurements (bytes/units/scalars) be distinguished? What channels do Base64 and Percent each serve? Why must decompression be capped? Five fluent answers pass the first half — the structured data of the second half waits ahead, and there you will find every layer of the JSON parser bearing this volume's function names.

### The volume's knowledge map

```diagram flow
- Strings (25): view pipeline zero allocation / builder O(n) / the find-and-edit full set
- Numbers (26): strict parsing / shortest round trip / format strings
- Charsets (27): UTF-8 mainline / all-direction transcoding / per-scalar operations
- Codecs (28): Base64 / Hex / Percent three bridges; capacity always computable by pen
- Compression (29): one-shot and streaming postures / deterministic artifacts / overhead ledger and decompression caps
- Second half (from 31): dynamic values / JSON / XSON / JSONL and XSONL (line-delimited records) / templates / regex — all standing on the first five chapters
```

### An intuition table of expansion and cost

The first half's "ledger intuition" merges into one small table: view operations allocate zero (Chapter 25), strict parsing surprises zero (Chapter 26), transcoding output is pen-computable (Chapter 27), encoding expansion has formulas (Chapter 28: 33%/100%/≈0%), compression has fixed overhead (Chapter 29: ~18-byte container) — every chapter turns "cost" from guesswork into arithmetic. Read together: **the first half of Volume 4 is a toolbox that makes all implicit costs of text processing explicit** — you always know what each step pays and why.

### Three kinds of readers, three routes

**Application developers** (mostly business code): read Chapters 25/26 closely — strings and numbers are daily tools; Chapters 27/28/29 need only the philosophy, with the cheat sheet at use time. **Protocol and infrastructure developers**: read everything closely — every layer you write runs on this volume's functions, and the strictness and performance views directly shape the interfaces you design. **Readers migrating from dynamic languages**: adjust two expectations first — C strings are not objects but views plus conventions (Chapter 3), and "number to string" has no implicit magic, only explicit parsing and formatting (Chapter 26); once adjusted, this volume's APIs will feel especially tidy — because they make explicit everything dynamic-language runtimes do implicitly.

### A performance view: this volume doesn't chase micro-optimization

Volume 4's functions mostly run at data boundaries (parse entry, protocol exit, presentation layer), and their performance follows one law: **boundary operations run far fewer times than data operations** — configuration parses once, request headers parse once per request, while the business processing they trigger may run a million times. So this volume's optimization focus is "don't make unnecessary allocations" (view pipeline, builder) rather than micro-optimizing each function; genuinely high-frequency text processing (protocol streams) gets dedicated streaming designs in Volume 7. Invest optimization effort at the right layer — that is the performance-view division between Volume 4 and Volume 3 (containers are the hot-path regulars).

### Links to the volumes before and after

Looking back: Volume 1's error model (Chapter 4) carries all of this volume's failure paths — behind every "fails whole" is the error slot recording category and position; the memory system (Chapters 5/7) carries string and buffer ownership — the division between owning artifacts and borrowed views is Volume 1's ownership language landing at the text layer; resource limits (Chapter 3) serve twice here (decompression cap, parse boundary). Looking forward: Chapter 30's dynamic values build on this volume's numbers and encodings — the float shortest round trip becomes JSON serialization's default output strategy directly; Volume 9's HTTP header parsing, content encoding, and URL handling are this volume's full family at protocol scale, and you will flip back to this volume's cheat sheets more often than you expect. Text is the lingua franca of the data world — this volume teaches not "a few string functions" but the grammar of that lingua franca and its safety rules: grammar lets data flow, rules keep the flow out of trouble.
