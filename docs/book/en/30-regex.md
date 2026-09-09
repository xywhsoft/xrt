---
num: 30
slug: regex
title: The Regex Engine
volume: 卷四 文本与结构化数据
type: practice
A non-backtracking engine with worst-case linear time, immutable compiled objects with exclusive matchers, named captures and replacement — closing with its restricted cousin pattern for routing-shaped matching.
api: regex, pattern
---

## Orientation

`xregex` is XRT's regex engine; three engineering decisions define its character: a **non-backtracking engine** — matching time is linear in input length, catastrophic backtracking (ReDoS) is structurally impossible, and untrusted text can be matched safely; **compile/match separation** — `xrtRegexCompile` produces an immutable reference-counted object (compile once, share everywhere), while `xregexmatcher` is an execution cache exclusive to each traversal (repeated scans of the same text do zero re-allocation); and **named captures** — `(?<name>...)` read by group name, replacing fragile numeric indices. This chapter makes the usage and payoff of the three decisions clear; the regex syntax itself is covered as a standard-subset cheat sheet. The chapter closes by bringing in its **restricted cousin** — `xpattern`: when the matching shape is a `{name}`-capture structure like routes or configuration keys and one text must dispatch against dozens of patterns, it is faster and simpler than regex.

## Introduction

Two real incidents. Incident one: a log service matched user-submitted filter expressions with a backtracking engine; a `^(a+)+$`-style pattern made a single match run for thirty seconds — queues backed up, the service went catatonic. This "catastrophic backtracking" is a structural defect of backtracking engines: the way a pattern is written determines its worst-case complexity. Incident two: in config parsing, the capture groups of `(\w+)=(\d+)` were read by number — a maintainer inserted one group in the middle, every index shifted by one, all downstream reads misaligned, and the compiler complained about nothing.

`xregex` is immune to both incidents: a non-backtracking engine's worst case is linear — there are no "evil patterns", only "slightly slower patterns"; the named captures of `(?<key>\w+)=(?<value>\d+)` are read by name — adding or changing groups never disturbs existing read code. Add the lifecycle design of "compiled objects immutable and shareable, matchers exclusive caches", and you have an engine built for long-running servers.

## Concepts

### Lifecycle: the compile and match layers

```diagram flow
- Compile: xrtRegexCompile(pattern) -> immutable object (reference counting, shared via Retain/Release)
- Matcher: MatcherCreate(compiled object, text) -> exclusive execution cache
- Scan: Find(offset) for the first / Next advances to the next - loop for a full scan
- Capture: CaptureNamed(group name) reads text and range
- Wrap-up: MatcherFree returns the cache; Release returns the compiled object
```

The compile layer is immutable: the same `xregex*` is shared by multiple threads and multiple matchers and never modified — reference counting (the same primitive as Chapter 30) manages its life and death. The matcher layer is exclusive: each traversal holds its own cache (backtrack positions, capture slots), and **repeated scans of the same text reuse that cache** — a hundred scans initialize once. The two-layer separation makes the "compile once, match a million times" cost model hold.

### Non-backtracking: the linear guarantee

A backtracking engine, on failure, tries every path — some patterns make the path count explode exponentially (the root of ReDoS). A non-backtracking engine (NFA/DFA descent) **tracks all possible states simultaneously** and processes each input character exactly once — worst-case time is linear in input length. The engineering meaning: matching time has a hard upper bound, so it can be used confidently on request paths and untrusted input. The cost is a slightly larger constant on some patterns — imperceptible for the overwhelming majority, in exchange for the certainty that "there are no evil patterns".

### Named captures and replacement

`(?<name>[A-Za-z_]+)=(?<value>\d+)` defines two named groups; `xrtRegexMatcherCaptureNamed(匹配器, "value", ...)` (matcher, "value", ...) reads out the text view and range by name. Replacement `xrtRegexReplace(编译对象, 文本, 替换, ...)` (compiled object, text, replacement, ...) supports `$1`/`$2` numeric references (named groups carry implicit numbers too); `xrtRegexEscape` escapes arbitrary text into a "match literally" form — the mandatory step before splicing user input into a pattern. `xrtRegexFullTest` decides "whole-string exact match", complementing Find's "find a substring" semantics.

### When regex is too heavy: structured pattern matching (pattern)

Regex answers "**one text against one pattern**"; pattern answers "**one text against a set of patterns**" — the standard shape of routing tables, configuration keys, and command dispatch. Its syntax has exactly three elements: literal fields match bytes exactly; `{name}` captures one non-empty field (never crossing a separator — `/` by default; `xpatternconfig.Separators` configures the byte set); `{*name}` may only sit at the end, capturing the remainder and allowing empty. `{{` and `}}` escape literal braces. No metalanguage, no quantifiers or alternation — three immediate consequences: **non-backtracking holds by construction** (nothing to explode); **no escaping traps** (user input is always the text being matched, never a pattern ingredient — pitfall 2 structurally disappears here); **matching is anchored to the full input** (no path normalization, URL decoding, or case folding — those are Chapter 101/104 jobs; layering is the point).

The outcome uses the three-state `xpatternresult`: `XPATTERN_ERROR` / `XPATTERN_NONE` / `XPATTERN_MATCH` — **a miss is not an error** (the same philosophy as Chapter 9's `xwaitresult`: control flow separated from errors). The multi-pattern lifecycle mirrors regex: `xrtPatternBuilderCreate` registers dozens of patterns incrementally, `xrtPatternBuilderCompile` compiles them once into a single deterministic program (immutable, long-lived; the builder is freed right after); on a hit, `Match.Value` directly returns the business identity given at registration (say `"user"`) and `Match.Index` the pattern number — **the dispatch key is built in**, no capture-group semantics to re-parse. One-shot cases can use `xrtPatternExtract` for whole-string extraction without compiling. Four budget ceilings (pattern bytes / pattern count / captures / compiled bytes) block untrusted patterns from ballooning memory; overflow errors carry the in-pattern byte offset (`xrtPatternErrorOffset`). The division in one line: **need "shape description plus quantifiers and alternation" — regex; need "field splitting plus multi-pattern dispatch" — pattern** — Chapter 104's routing table and Chapter 137's log classification both stand on this side.

### Syntax-subset cheat sheet

| Syntax | Meaning |
| --- | --- |
| `.` `\d` `\w` `\s` | wildcard/digit/word/whitespace classes |
| `[abc]` `[^abc]` | character set and negated set |
| `*` `+` `?` `{m,n}` | quantifiers |
| `(...)` `(?<name>...)` | grouping and named groups |
| `^` `$` `\b` | line start/end and word boundary |
| `a\|b` | alternation |

The syntax is the common subset of mainstream regex — no backreferences and no lookahead (forms the non-backtracking engine does not support; the documentation says so explicitly rather than silently ignoring them).

## Examples

### Complete program: compile, match, and named capture

From the repository sample `examples/text/regex/main.c` — extracting `width=128` and `height=72` from configuration text:

```embed path="examples/text/regex/main.c" title="examples/text/regex/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/text/regex/main.c -lws2_32 -liphlpapi
width = 128
height = 72
```

**What just happened.** (1) `xrtRegexCompile` compiles once to an immutable object — this example uses it locally and `Release`s at the end; in long-running scenarios it hangs on a global (compiled at startup, released at shutdown), and each match merely borrows a reference. (2) After the matcher is created, `Find` locates the first match from offset 0 — the `(?<key>...)` group captures the name pair. (3) `CaptureNamed("key")` and `CaptureNamed("value")` read out text views by name — more refactor-proof than `$1`/`$2` numeric reads. (4) In the loop, `Next` advances to the next match — `width` and `height` each scanned and printed; the matcher's cache is reused across matches. (5) The whole process carries zero "catastrophic backtracking" risk — the non-backtracking engine scans linearly for any pattern.

### Complete program: replacement and escaping

From `examples/text/regex_replace/main.c` — batch-rewriting `name=value` into `name: value`:

```embed path="examples/text/regex_replace/main.c" title="examples/text/regex_replace/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/text/regex_replace/main.c -lws2_32 -liphlpapi
width: 128 height: 72
```

**What just happened.** (1) The same named-group pattern, this time through `xrtRegexReplace` — `$1` and `$2` reference the capture groups (named groups carry the implicit numbers 1, 2). (2) The replacement product is an owning string (freed with `xrtFree`). (3) This sample doesn't use `Escape`, but note it here: **before user input is spliced into a pattern, `xrtRegexEscape` is mandatory** — the parentheses in a user input like `a(b` are syntax characters; unescaped, they form an illegal pattern or a wrong match (Pitfall 2's protagonist).

### Complete program: pattern — routing-style {name} capture

From the repository example `examples/text/pattern/main.c` — one route pattern; a hit is a dispatch:

```embed path="examples/text/pattern/main.c" title="examples/text/pattern/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/text/pattern/main.c -lws2_32 -liphlpapi
kind=user group=admin id=42
```

**What just happened.** (1) The `xpatternspec` quadruple: template, business identity (`"user"` — the dispatch key of the hit), budget flags; a real routing table registers dozens before one compile. (2) Builder incremental registration → `Compile` yields an immutable object → **the builder can be freed immediately** — the same lifecycle discipline as the regex compiled object. (3) `xrtPatternMatch` writes captures into the caller's array in template declaration order (`group` first, `id` second); captures are **views borrowing the input text** (Chapter 3's discipline). (4) One output line yields all three dispatch essentials: `Match.Value` (which route class), two captured segments (the parameters) — the entire input of an HTTP route handler is in that line.

## Contracts

- **Linear guarantee**: non-backtracking engine; worst-case matching time is linear in input length — no catastrophic-backtracking patterns exist.
- **Two-layer lifecycle**: the compiled object is immutable and shared by reference counting; the matcher's cache is exclusive, one per traversal.
- **Named captures**: defined with `(?<name>...)`, read with `CaptureNamed`; the numeric reference `$1` remains available.
- **Escaping discipline**: dynamic content must pass `xrtRegexEscape` before entering a pattern.
- **Semantic distinction**: `Find` finds substrings, `FullTest` decides whole strings — choose by need; don't simulate with `^...$`.
- **Syntax subset**: the mainstream common subset; backreferences and lookahead unsupported — documented explicitly, not silently.
- **pattern boundary**: full-string anchored; captures borrow the input text; errors live in the `xrt.pattern` domain with `xrtPatternErrorOffset` giving the in-pattern byte offset; a miss is NONE, not an error.

### From examples to engineering: three hosts of regex

**Validation layer**: input legality checks (username rules, SKU formats) — patterns compile statically into globals; each request only builds a matcher, and `FullTest` decides whole strings. Three disciplines for validation patterns: anchor whenever possible (`^...$` or FullTest semantics), whitelist character sets over blacklists, and failure messages that explain the format. **Extraction layer**: pulling data from semi-structured text (log fields, config lines) — named captures plus a full `Next` scan, with extraction results going into a value tree or business structs; this is one midway station of Chapter 35's text pipeline. **Replacement layer**: templated rewriting (format conversion, data masking) — `Replace`'s capture references keep the reassembly logic inside the replacement string. All three layers share one performance discipline: **compile at startup, match on the hot path** — Pitfall 1's good form is the shared code skeleton of all three.

### The division of labor with the string family and globs

When do Chapter 25's three text-matching tools yield to regex? **Literal search** (finding a fixed `ERROR`) uses `xrtStrFind` — no regex overhead needed; **simple globs** (`*.log`) use `xrtStrGlob` — simpler and faster; **structural patterns** ("three digits per group, joined by hyphens") are regex territory. The mnemonic: **if the pattern can be stated in one sentence, a string tool suffices; only "shape descriptions" need regex**. The cost of regex abuse is not just performance — readability, maintainability, and escape traps (Pitfall 2) all lie in wait; code that uses the right tool explains itself in review.

### A testing view: patterns are code too

A regex pattern is condensed code — it deserves code's treatment. Three habits: **name your patterns** — `static const char* PATTERN_SKU = "..."` plus a one-line comment on intent, not scattered across call sites; **boundary case sets** — every pattern ships with "should match / should not match / boundary" sample groups (a transplant of Chapter 26's boundary-value thinking), regression-run when the pattern changes; **performance sampling** — run representative inputs through Chapter 6's statistics or simple timing to confirm no constant surprises. Chapter 11's reproducible randomness (fixed-seed malformed input generation) hands pattern fuzzing a ready-made tool — these disciplines will be systematized again in Chapter 132's testing framework.

## Pitfalls

### Pitfall 1: recompiling the pattern on every request

Symptom: abnormal hot-path CPU; the profiler shows the hotspot inside `xrtRegexCompile` — every request compiles the same pattern from scratch.

Cause: the intent of compile/match layer separation was ignored — compiling is the heavy operation (building the automaton), matching the light one; the heavy operation got into the loop.

```c bad
bool handle(const char* sFilter)
{
	xregex* pRegex = xrtRegexCompile((xstrview){ sFilter, strlen(sFilter) });
	/* ... match once ... */
	xrtRegexRelease(pRegex);   /* compile+release per request - the automaton was built for nothing */
}
```

```c good
/* compile once at startup, share globally */
static xregex* gFilter;
void init(void) { gFilter = xrtRegexCompile(XRT_STR_LITERAL("...")); }
bool handle(const char* sText)
{
	/* build only the matcher (light); the compiled object lends a reference */
	xregexmatcher* M = xrtRegexMatcherCreate(gFilter, Text);
	/* ... Find/Next ... */
	xrtRegexMatcherFree(M);
}
```

### Pitfall 2: splicing user input straight into a pattern

Symptom: user input containing `(`, `*`, or `[` fails compilation or behaves weirdly; worse, carefully crafted input steers the pattern's semantics entirely off course (an injection-class vulnerability).

Cause: regex syntax characters went unescaped — the user's text was treated as part of the pattern.

```c bad
xstrview UserInput = ...;
xstrbuf Pat;
xrtStrBufInit(&Pat);
xrtStrBufAppend(&Pat, XRT_STR_LITERAL("^"));
xrtStrBufAppend(&Pat, UserInput);          /* "(admin)" enters the pattern - syntax characters! */
xrtStrBufAppend(&Pat, XRT_STR_LITERAL("$"));
str sPat = xrtStrBufTake(&Pat);
xregex* R = xrtRegexCompile((xstrview){ sPat, strlen(sPat) });   /* compile fails or injection */
```

```c good
xstrbuf Pat;
xrtStrBufInit(&Pat);
xrtStrBufAppend(&Pat, XRT_STR_LITERAL("^"));
str sEscaped = xrtRegexEscape(UserInput, NULL);
xrtStrBufAppend(&Pat, (xstrview){ sEscaped, strlen(sEscaped) });  /* escaped, it is a pure literal */
xrtFree(sEscaped);
xrtStrBufAppend(&Pat, XRT_STR_LITERAL("$"));
```

## Exercises

### Basic: named extraction (the feel of named captures)

Reproduce the main example: full-scan `"width=128 height=72 depth=24"` and print the three key-value pairs; then verify with a nonexistent key (like `color`) that the scan skips it naturally.

### Advanced: a log filter

Match `[WARN] 2026-04-02 disk 90%` with named groups — level, date, module, and value, each captured separately; count the difference between WARN and ERROR occurrences over a stretch of log. Hint: after capturing the date group, re-parse with Chapter 39's time parsing (if present) or just keep the string.

### Challenge: a search highlighter

Implement `highlight(文本, 查询词)` (text, query term): the query is escaped with `Escape`, compiled into a "whole-word match" pattern (with `\b` boundaries), and all hit positions found across the text, printing the text marked with `<<>>`. Acceptance criteria: query terms containing regex syntax characters (`(`, `*`, `[`) still behave as literal matches; consecutive hits do not overlap; 1000 hits in 100KB of text are linear and measurable in time.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Linear guarantee | non-backtracking engine — worst-case linear, no ReDoS, untrusted text matches safely |
| Two-layer model | compiled object immutable and shared (Retain/Release) / matcher's exclusive cache (Create/Free) |
| Cost discipline | compile once, share globally; inside loops, build only matchers |
| Capture | `(?<name>...)` + `CaptureNamed`; `$1`/`$2` numeric references for replacement |
| Escaping | dynamic content passes `xrtRegexEscape` before entering a pattern (returns an owning string) — the only safe channel to literal matching |
| Semantics | Find substring / FullTest whole string / Next advance / Replace rewrite |
| Three hosts | validation (anchor + whitelist) / extraction (named captures, full scan) / replacement (capture-reference reassembly) |
| Tool division | literal search uses StrFind / simple globs use StrGlob / shape descriptions go to regex |
| Pattern discipline | name patterns + boundary case sets + performance sampling — patterns are code too |
| The pattern cousin | `{name}` capture + literal segments; Builder compiles many patterns once; three states without error; Match.Value is the dispatch key |
