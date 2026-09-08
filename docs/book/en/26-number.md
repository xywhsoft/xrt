---
num: 26
slug: number
title: Number Parsing and Formatting
volume: 卷四 文本与结构化数据
type: practice
lead: Strict parsing that is all-or-nothing; shortest round-trip floating-point output; Python-style format strings that declare everything in one pass.
api: number
---

## Orientation

Converting between numbers and text looks like a small thing, yet it is an accident-prone zone: `atoi` silently returns 0 on non-numeric input, `strtoll` "parses as far as it can" and leaves half-consumed garbage behind, `%.17g` output is verbose while `%.6f` loses precision, and the `INT64_MIN` boundary breaks any "make positive first, negate later" implementation. XRT's number module answers these problems with three designs: **strict parsing** (the whole span is legal or the whole call fails — leniency is enabled explicitly via flags), **shortest round-trip** (floating-point output carries exactly the digits that guarantee parsing back to the original value), and **format strings** (Python-style — grouping, base, and percentage declared at once). This chapter works all three lines through, and along the way unfolds the idea of boundary-value testing — the mirror image of Chapter 6's fault injection in the numeric world: both deliberately manufacture extreme conditions to verify that implementations are as reliable beyond the happy path as on it.

## Introduction

Three accident scenes. Scene one: a config file has the port written as `"8080x"`; `atoi` returns 8080 — the trailing x is silently dropped, and the service starts on a port that "looks right". Scene two: floats in the logs printed with `%.6f` lose three digits when read back, so regression comparisons never pass; switching to `%.17g` produces things like `0.10000000000000001` that nobody wants to read. Scene three: parsing `" -1_234.567_890e-2 "` — surrounding whitespace, underscore digit grouping, and scientific notation, a trio the standard library won't accept any part of, while hand-writing it eats an afternoon.

The common root cause across all three scenes: the standard library's conversion functions were born in a lenient era and made "fault tolerance" the default behavior — while engineering wants exactly the opposite: **strict by default, lenient explicitly**. XRT's parse functions do not stop halfway: the input view must be legal as a whole before a value is returned, and any impurity fails the call and leaves a clear category in the error slot; to allow whitespace and underscores, you declare it explicitly with flags.

## Concepts

### Strict parsing: all-or-nothing

```diagram flow
- Input view: the whole span must form one legal number
- Leniency optional: whitespace/underscores allowed explicitly via parse flags, rejected by default
- Boundary-correct: INT64_MIN and other boundary values handled exactly, no intermediate overflow
- Failure diagnostics: whole-input failure + error-slot record, no half-parsed results
```

The strictness of `xrtIntParse(视图, 基数, 标志, 出参)` (view, base, flags, out-param) has three layers: no surrounding whitespace unless `XNUMBER_PARSE_SPACE`, no underscores inside digits unless `XNUMBER_PARSE_SEPARATOR`, and success only when the whole span is consumed — `"8080x"` is eliminated at the first layer. The floating-point version `xrtNumParse` shares the semantics, plus explicit handling of scientific notation and special values (inf/nan). The **boundary-value discipline** deserves its own emphasis: test inputs should use `INT64_MIN` — any implementation that "converts to positive first, then negates" gets it wrong exactly there (the positive 9223372036854775808 overflows); only implementations that accumulate in the negative throughout can pass. Writing boundary values into tests is a hard requirement of this chapter's exercises.

### Shortest round-trip: the de facto standard for serializing floats

`xrtNumString(值, 精度)` (value, precision) at its default (precision 0) outputs the **shortest round-trip representation**: exactly the decimal digits that guarantee `Parse` brings back a double equal to the original, bit for bit. It is far shorter than `%.17g` (`0.1` comes out as `0.1`, not `0.10000000000000001`) and, unlike `%.6f`, loses nothing — the serialize-deserialize loop closes with zero error. When should you specify precision by hand? When displaying to humans (two decimals in a report); machine round-trips always use the shortest round-trip.

### Format strings: declare every formatting wish at once

| Format | Effect | Standard-library equivalent |
| --- | --- | --- |
| `,d` | Thousands-grouped decimal: `-123,456,789` | none (hand-written loop) |
| `#_X` | 0X prefix + underscore grouping + uppercase hex | several flags combined |
| `,.2f` | Thousands + two-decimal fixed-point | none |
| `.1%` | Percentage (auto ×100 plus %) | compute by hand, then paste |
| `c` | Integer code point rendered as a UTF-8 character | wctomb conversion |

`xrtIntFormat` / `xrtUIntFormat` / `xrtNumFormat` — three entries covering signed, unsigned, and floating-point — all produce owning products (freed with `xrtFree`). The format-string style follows Python's format spec — readers who have written Python pay zero learning cost, and everyone else is still spared memorizing which of printf's `%08X` and `%'d` is supported on which platform.

### The division of labor with the printf family

`xrtFormat` (met in Chapter 2) and this chapter's Format family are not competitors: the former assembles arbitrary mixed content ("order %s, %d items"), the latter specializes in display formats for numbers (thousands, base, percentage). Rule of thumb: **Format for text templates, this chapter's Format family for numeric display** — both return owning products, and both accept a printf-style syntax subset (rejecting `%n`, the security discipline mentioned in Chapter 4).

## Examples

### Complete program: strict parsing and multi-base output

From the repository sample `examples/number/integer/main.c` — a double verification on the boundary value INT64_MIN:

```embed path="examples/number/integer/main.c" title="examples/number/integer/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/number/integer/main.c -lws2_32 -liphlpapi
-9223372036854775808
-0X8000000000000000
```

**What just happened.** (1) The parse input is the decimal text of `INT64_MIN` — the sample explicitly allows whitespace and underscores with two flags (both appear in its input); the leniency is spelled out in black and white. (2) Empirical proof of boundary correctness: it parses out `-9223372036854775808` and echoes it — if the implementation "made positive first, then negated", the positive side would overflow at 9223372036854775808 and the result would be off by one; this output is the signal of (non-)compliance. (3) `xrtIntString` outputs in base 16 with the `0X` prefix and uppercase — the output format is declared by flags, no hand-assembled prefix. **Breaking this sample on purpose** is part of this chapter's exercises: drop the parse flags, feed impure input, and watch strict failure instead of silent truncation.

### Complete program: the formatting family

From `examples/number/format/main.c` — five format strings in one show:

```embed path="examples/number/format/main.c" title="examples/number/format/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/number/format/main.c -lws2_32 -liphlpapi
-123,456,789
0XDEAD_BEEF
1,234,567.90
12.5%
你
```

**What just happened.** (1) Thousands-grouped integer — the readability staple of reports and logs; the standard library would need a hand-written comma-inserting loop, here it is one `,d`. (2) Hex with underscore grouping — the debugger-style `0XDEAD_BEEF`; prefix, grouping, and uppercase, three wishes in one format string. (3)(4) Fixed-point and percentage — note the percentage is "auto ×100 plus %": input 0.125 outputs `12.5%`, no hand math, no hand assembly. (5) Character rendering: integer 20320 renders by Unicode code point as the UTF-8 character on the fifth line — a first meeting with the charset family's code-point-to-character conversion (Chapter 27). All five output lines are owning products, each freed with `xrtFree`.

## Contracts

- **Strict by default**: success only when the whole span is legal; leniency (whitespace/underscores) enabled explicitly via flags; failure sets the error and writes no output.
- **Boundary-exact**: boundary values such as INT64_MIN/UINT64_MAX parse exactly; no "positivize first" path exists in the implementation.
- **Shortest round-trip**: `xrtNumString`'s default precision is the shortest round-trip; machine serialization never specifies precision by hand.
- **Owning products**: the String/Format families return `str`, freed exactly once with `xrtFree`.
- **Explicit bases**: parse radix and output base/prefix are declared by parameters/flags — no guessing, no defaulting to hexadecimal.

### From examples to engineering: three hosts of parse functions

The three ways parse functions hang in engineering decide the error-handling strategy. **Entry validation**: config loading and protocol-field decoding call parsing at the system boundary — a failure rejects the whole input, with the error carrying the context of "which field, which impurity" (error formatting's moment to shine, Chapter 4). **Hot-path conversion**: numeric extraction that runs on every request — strictness buys the certainty that "failure means bug"; the hot path needs no fallback branch. **Display-layer formatting**: the Format family faces human eyes (logs, reports, error messages) — thousands and percentage live here, while machine-to-machine transfer uses the shortest round-trip, not format strings. The three hosts correspond to three postures — "boundary rejects, path asserts, display formats" — and using the wrong posture is more common than using the wrong function.

### A testing view: boundary values before coverage

The repeated appearance of INT64_MIN in this chapter is no coincidence; it demonstrates a testing view: **the choice of boundary values matters more than the number of cases**. The boundary checklist for numeric conversion: each type's min and max, zero and negative zero, leading zeros, single characters, empty strings, a lone sign, and maximal-length digit strings — fifteen well-chosen boundaries kill ninety percent of implementation defects, while ten thousand random values may never touch any of them. This idea recurs in Chapter 6 (choosing FailAfter's injection points) and Chapter 30 (JSON numeric boundaries).

## Pitfalls

### Pitfall 1: using strict parsing with an atoi mindset

Symptom: after migrating from code that "always ran" to XRT parsing, the same batch of configs suddenly fails in droves — the errors concentrate on fields with whitespace or impurities.

Cause: the standard library's lenient behavior (skipping whitespace, parsing as far as it can) got treated as a contract; strict parsing turns these problems from "silent wrong values" into "explicit failures" — the failure is not a regression, it is the bug that was always there being seen for the first time.

```c bad
int iPort = atoi(sConfig);   /* "8080x" -> 8080, the "x" silently dropped */
```

```c good
int64 iPort;
if ( !xrtIntParse((xstrview){ sConfig, strlen(sConfig) }, 10, 0, &iPort) ) {
	ReportConfigError(sConfig);   /* "8080x" fails right here - the impurity is seen */
	return false;
}
```

### Pitfall 2: specifying float serialization precision by hand

Symptom: the serialize-deserialize loop check fails intermittently; or floats in the log become seventeen-digit "precision noise".

Cause: manual precision is a pitfall at both ends — too short loses precision (the round trip no longer compares equal), too long outputs noise (the destiny of `.17g`).

```c bad
char sBuf[32];
snprintf(sBuf, sizeof(sBuf), "%.17g", fValue);   /* 0.1 -> 0.10000000000000001 */
```

```c good
str sText = xrtNumString(fValue, 0);   /* shortest round-trip: 0.1 stays 0.1 */
/* ... use sText ... */
xrtFree(sText);
```

## Exercises

### Basic: a strictness physical

Parse the five inputs `"42"`, `" 42"`, `"4_2"`, `"42x"`, `""` with default flags and then with all flags on, and record the ten results — first work out the expectations on paper (which succeed and with what value, which fail), then run and compare; only move to the next exercise when all ten match.

### Advanced: a boundary-value test set

Write a boundary test set for `xrtIntParse`: INT64_MIN/MAX, UINT64_MAX, `-0`, leading zeros, a bare underscore, a single space — assert the exact result for each input. Hint: this is a miniature of the repository's tests; the choice of boundary values matters more than the number of assertions.

### Challenge: a config reader (three modules in concert)

Implement `read_int(配置文本, 键)` (config text, key): split lines (Chapter 25), Cut out key and value, run the value through strict parsing (whitespace allowed, underscores not), and match keys case-insensitively. Acceptance criteria: five kinds of malformed input (impurity, out of range, empty value, missing key, duplicate key) each produce a distinguishable error report; INT64_MIN/MAX config values round-trip exactly.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Strict parsing | `xrtIntParse`/`xrtNumParse`: the whole span legal or fail; leniency via flags |
| Parse flags | `XNUMBER_PARSE_SPACE` (whitespace) / `XNUMBER_PARSE_SEPARATOR` (underscore) |
| Integer output | `xrtIntString`: base + prefix + uppercase declared by flags |
| Shortest round-trip | `xrtNumString(值, 0)`: exactly the digits that guarantee round-trip equality |
| Format strings | `,d` thousands / `#_X` hexadecimal / `,.2f` fixed-point / `.1%` percent / `c` code point |
| Owning | All products are `str`, freed exactly once with `xrtFree`; the Format family also has To caller-buffer variants |
| Boundary discipline | Tests must include INT64_MIN — it specializes in killing "positivize-first, negate-later" implementations |
| Three hosts | Boundary validation rejects / hot path asserts / display layer formats — posture matters more than function choice |
| Testing view | Fifteen well-chosen boundaries beat ten thousand random values — selection precedes coverage |
