---
num: 39
slug: console
title: The Console and Terminal Detection
volume: 卷五 系统服务
type: practice
lead: stdout/stderr routing, terminal detection with color degradation, in-place progress refresh without flooding — the CLI's first impression.
api: console
---

## Orientation

The console is a CLI tool's first interface with the user; the console module handles its three engineering problems: **stream selection** (normal output goes to stdout, diagnostics to stderr — no mutual pollution under pipe composition), **terminal detection** (`xrtConsoleIsTerminal` — colors and progress animations turn on only for an interactive terminal, degrading automatically to plain text when redirected), and **in-place refresh** (progress bars/spinners overwrite via carriage return without newline — logs and progress share the screen without trampling each other). A small module, a big experience — the trust gap between a tool that sprays escape codes after redirection and one that degrades gracefully is enormous; and the code difference may be a few detection branches.

## Introduction

Three crash scenes. Scene one: after `tool > out.txt`, the file is full of `\033[32m` color escape sequences — the author hard-coded ANSI colors without considering that the output target isn't a terminal. Scene two: the CI log has three thousand lines of progress bar — the spinner's every refresh emitted a newline, because it didn't know stdout had been piped away. Scene three: diagnostics printed to stdout — the user's `tool | grep result` pipeline gets warn mixed in, and the script's parsing crashes on the spot.

The common root cause: **treating the console as a fixed thing instead of an environment with two streams, two shapes, and a need for runtime adaptation**. In reality it has two streams (stdout/stderr, different semantics) and two shapes (interactive terminal / redirected pipe, different capabilities) — correct code detects and adapts at runtime. The console module standardizes this adaptation: stream selection, terminal queries, and an output policy of "refresh only takes effect on a terminal".

## Concepts

Framework first, details after: this chapter's concepts unfold in three layers — "two streams → detection and degradation → refresh discipline" — mapping exactly onto the introduction's three crash scenes; after reading the concepts, you should be able to recite the fix for each scene aloud.

### The semantic division between the two streams

| Stream | Semantics | Consumer |
| --- | --- | --- |
| stdout | the program's **product** — result data, normal output | downstream pipes (grep/jq/files) |
| stderr | the program's **aside** — diagnostics, warnings, progress | human readers, log collection |

The discipline of the division (fifty years of Unix sediment, worth obeying to the letter): **stdout is the machine interface** — stable format, parseable, never mixed with human-facing content; **stderr is the human interface** — diagnostics as chatty as you like, color no problem. Violating the division (diagnostics into stdout) is not a style issue — it **destroys pipe composability**, and Unix philosophy's punishment always arrives late.

### Terminal detection and capability degradation

`xrtConsoleIsTerminal(流)` (stream) queries whether the target is an interactive terminal — note the per-stream query: stdout may be piped while stderr still sits on a terminal; the two are judged independently. The degradation decision tree: color — on for terminal, off for redirect; progress animation — in-place refresh on a terminal, throttled printing when redirected (one line per N items or per second); interactive prompts — may wait for input on a terminal, fail and exit immediately when redirected (nobody is at the keyboard at the other end of the pipe).

### The degradation decision tree

```diagram flow
- Color output: on for terminal / off for redirect - escape sequences only mean something to a terminal
- Progress animation: in-place refresh on terminal (rate-limited) / throttled milestones when redirected (every 25%)
- Interactive prompt: may wait for input on terminal / fail-and-exit in a pipe - nobody on the other end
- Output width: wrap at detected width on terminal / no wrapping when redirected (downstream formats itself)
```

### In-place refresh

Two postures for progress output: **append with newlines** (one line each — log-style; the only correct posture under redirection) and **overwrite in place** (carriage return without newline — animation-style, terminal only). The disciplines of in-place refresh (animation-style, terminal only — overwriting the previous frame via carriage return): rate-limit the refresh (10fps is enough for the human eye; more is waste); print the final-state newline on exit (or the next shell prompt sticks to the progress bar's tail); when logs and progress share the screen, progress owns the last line (clear the progress line before printing a log — the coordination shape the variants sample demonstrates).

## Examples

### Complete program: routing output across streams

From the repository sample `examples/console/output/main.c`:

```embed path="examples/console/output/main.c" title="examples/console/output/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/console/output/main.c -lws2_32 -liphlpapi
$ ./a.exe
（stdout）service started
（stderr）example diagnostic
$ ./a.exe 2>/dev/null
（stdout）service started
（stderr 行被丢弃：stdout 保持纯净可管道）
```

**What just happened.** (1) Normal output goes to stdout, diagnostics to stderr — the `2>/dev/null` contrast experiment verifies the routing directly. (2) Pipe composability: `./a.exe | grep service` hits only the stdout lines — machine interface and human interface never cross into each other's territory. (3) This routing converges with Chapters 37/38's Logger: the Logger's console Sink likewise follows "INFO and above to stdout, ERROR and below to stderr" or per configuration — the console module is its underlying stream convention.

### Complete program: terminal detection and degradation

From `examples/console/variants/main.c`:

```embed path="examples/console/variants/main.c" title="examples/console/variants/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/console/variants/main.c -lws2_32 -liphlpapi
$ ./a.exe | cat
write-ok=1
is-terminal=0
```

**What just happened.** (1) The write interface works on both streams (`write-ok=1`). (2) `is-terminal=0` — under pipe redirection the detection returns false; running the same program on a real terminal yields 1 (real-terminal behavior can't be shown in this output block — detection varying with the target is its very point). (3) This one if is the foundation of all degradation logic: `if ( xrtConsoleIsTerminal(stdout) ) { 彩色+动画 } else { 纯文本+节流 }` (colors + animation, else plain text + throttling) — the rest of the variants sample demonstrates the output variants (width, alignment, prefixes) across streams.

### Three easily overlooked terminal behaviors

**Line-buffered vs fully buffered**: stdout is line-buffered when attached to a terminal (flushed every line) and becomes fully buffered when piped (flushed only when full) — this is exactly why "under a pipe, stdout output is slow to appear while stderr comes out first" (stderr is never buffered). Comparison tools either `fflush(stdout)` before critical output or send progress to stderr. **Width detection**: beyond `xrtConsoleIsTerminal` there is a width query — a table wrapped at the terminal's width should switch to a fixed width under a pipe (the 80-column tradition); when the width read fails (non-terminal), use the default. **Encoding assumptions**: this chapter's output assumes a UTF-8 terminal (Chapter 27's mainline); transcoding for legacy Windows code-page consoles is the host layer's responsibility — cross-platform tools are advised to state "UTF-8 terminal recommended" in the README, more pragmatic than building a full transcoding stack into the tool. All three behaviors are reefs you "one day run into if you don't know they exist" — knowing they exist matters more than memorizing the details.

## Contracts

- **Stream division**: stdout = product (machine interface), stderr = aside (human interface); diagnostics never enter stdout.
- **Terminal detection**: color/animation/interaction degrade per `IsTerminal`; plain text under redirection.
- **Refresh discipline**: in-place refresh rate-limited, a newline at the end state, coordination with logs over the last line.
- **Pipe friendliness**: output format stable and grep/jq-able; progress goes to stderr or is throttled.

### From examples to engineering: three principles of CLI experience

**Principle one: quiet by default** — say little on success (beyond result output, stderr speaks only when something happens); `-v` increases verbosity, rather than chatty-by-default plus `-q` for silence. **Principle two: actionable errors** — every error on stderr carries "what to do next" (missing file → point out the path, bad argument → show usage); paired with Chapter 4's error chain: the cause chain unfolds level by level into human-readable action guidance. **Principle three: the exit code is an interface** — 0 for success, non-zero distinguished by error category (aligned with Chapter 4's general error categories — argument errors, IO errors, timeouts each taking a value range); scripts branch on exit codes, never parse output text to guess. Together: a CLI's "user experience" is not flash but **predictability** — pipes, scripts, and humans can all predict its behavior.

### The relation to Chapters 37/38

The console module is the underlying convention provider for the Logger's console Sink (the bottom layer of the stack is the operating system's two streams; this chapter wraps the queries and conventions; the Logger assembles policy above): the Sink's stdout/stderr choice (the common INFO→stdout, ERROR→stderr policy) rests on this chapter's stream division; the Sink's colored text format degrades automatically under redirection (the IsTerminal check happens inside the Sink — you configured a colored text Sink, and under a pipe it becomes plain text by itself). Once you understand this layering, the "color" option in log config is no longer a magic switch — it merely sinks the if-branch you would have hand-written (Pitfall 1's good form) into the library. Conversely, when you write CLI output with the console module directly, you carry the same detection capability as the Logger — tools and services behave consistently across the whole library.

## Pitfalls

### Pitfall 1: hard-coded color escapes (disaster under redirection)

Symptom: the redirected file is full of `\033[..m`; downstream parsing scripts broken by the escape characters.

Cause: the color decision was hard-coded into the output statement, without the "color only on a terminal" detection branch.

```c bad
printf("\033[32mOK\033[0m done\n");   /* unconditional color - disaster under a pipe */
```

```c good
if ( xrtConsoleIsTerminal(stdout) ) {
	printf("\033[32mOK\033[0m done\n");   /* terminal: color */
} else {
	printf("OK done\n");                   /* pipe: plain text */
}
```

### Pitfall 2: the progress bar flooding CI logs (in-place refresh degraded by the pipe)

Symptom: tens of thousands of progress lines in the CI log file; the useful information drowned by the spinner.

Cause: in-place refresh (carriage-return overwrite) degenerates into plain newlines on a non-terminal — one line per frame; the detection branch didn't cover this scenario.

```c bad
while ( working ) {
	printf("\rprogress: %d%%", pct);   /* on a non-terminal \r doesn't overwrite - one line per frame */
	fflush(stdout);
}
```

```c good
bool bTerm = xrtConsoleIsTerminal(stderr);
while ( working ) {
	if ( bTerm ) {
		fprintf(stderr, "\rprogress: %d%%", pct);   /* terminal: overwrite */
	} else if ( pct >= next_milestone ) {
		fprintf(stderr, "progress: %d%%\n", pct);   /* pipe: throttled milestone */
		next_milestone += 25;
	}
}
if ( bTerm ) { fprintf(stderr, "\n"); }   /* end-state newline */
```

## Exercises

### Basic: the redirection triple (stream-routing experiments)

Write a program printing results to stdout and progress to stderr; verify with `> file` (stdout to disk), `2> file` (stderr to disk), and `| grep` (takes only stdout) what each stream receives — one line of conclusion per experiment.

### Advanced: a gracefully degrading progress bar (terminal animation and pipe milestones)

Implement progress output: a colored spinner + in-place percentage refresh on a terminal (10fps rate limit); one milestone line per 25% when redirected; a final end-state line on the terminal at completion. Compare a run through `| cat` against a real terminal.

### Challenge: a pipe-friendly report tool (the three principles, fully landed)

Implement `report`: a result table (stdout, stable format, aligned columns), progress and statistics (stderr, throttled with milestones), and a `--quiet` mode (stderr silent, errors only). Acceptance criteria: `report | grep 关键字` (keyword) hits exactly the result rows with no progress residue; `report > out.txt` produces zero escape codes and zero progress lines, ready for diff; under `--quiet`, stderr holds only error lines; three exit codes (success/argument error/IO error) each correct.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Stream division | stdout product (machine) / stderr aside (human); diagnostics never enter stdout |
| Terminal detection | `xrtConsoleIsTerminal(流)`; color, animation, interaction degrade on it |
| Refresh | in-place overwrite rate-limited to 10fps, end-state newline, throttled milestone lines under redirection |
| Pipe friendliness | stdout output grep/jq-able; progress to stderr or throttled by milestone |
| With Logger | the console Sink follows the same stream conventions - this module is the underlying foundation |
| Buffering differences | stdout line-buffered on terminal / fully buffered on pipe; stderr never buffered - mind this in comparisons |
| CLI three principles | quiet by default / actionable errors / the exit code is an interface - predictable beats flashy |
