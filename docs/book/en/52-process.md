---
num: 52
slug: process
title: Child Process Management
volume: 卷六 进程与并发
type: practice
lead: The Shell/Spawn twin families, output capture and pipeline chaining, a three-state exit status — the engineered posture for calling external programs like functions.
api: process, signal
---

## Orientation

The process module engineers "launch an external program": the **Shell family** (`xrtProcessShell` — the whole command string goes through the system shell, the easy path for tool invocation) and the **Spawn family** (`xrtProcessSpawn` — direct launch from an argv array, no injection surface, the safe path for untrusted input); **output capture** (`Result.Stdout` — an owning buffer capturing the child's standard output); **pipeline chaining** (`xrtProcessPipeline` — each stage's stdout wires into the next stage's stdin, the equivalent of the shell's `a | b`); and a **three-state exit status** (normal exit + exit code / killed by signal / launch failure — distinguished by `xrtProcessResultSuccess`). Environment inheritance, working directory, and timeout kill (the SIGTERM→SIGKILL ladder) are declared in the configuration.

## Introduction

Three scenes. Scene one: an image-processing service must call external `ffmpeg` — the naive `system()` call: command-string concatenation (a filename with spaces is disaster), no way to capture output, no way to set a timeout (ffmpeg hangs, the service hangs with it). Scene two: a build tool runs `gcc main.c -o app` and needs the error output — stderr capture and the exit code; `system` gives one int. Scene three: the text pipeline `cat | grep | sort` — multi-stage process chaining, where hand-wiring pipes means handle management and two-ended deadlocks (upstream blocks on a full pipe, downstream never reads) — pits everywhere.

The three scenes map to three capability sets: the Spawn family (`xrtProcessSpawn(配置)` (config) returning a handle) plus timeout configuration (scene one, made safe), dual-stream capture plus three-state verdicts (scene two), Pipeline auto-wiring (scene three). The divide between Shell and Spawn is **trust**: the command string comes from your own code (trusted) — Shell for convenience; from user input (untrusted) — Spawn+Argv is mandatory — command injection is a sibling of SQL injection, and the defense line is "never splice input into an interpreter".

## Concepts

### The twin families: Shell and Spawn

| Dimension | Shell family | Spawn family |
| --- | --- | --- |
| Entrance | `xrtProcessShell(命令串, 结果)` (command string, result) | `xrtProcessSpawn(配置)` (config) |
| Interpretation | cmd/sh interprets metacharacters | direct kernel launch, zero interpretation |
| Fits | tool invocation, trusted commands | untrusted input, precise control |
| Injection surface | command-string concatenation is the risk | none - arguments passed as-is |

The configuration (`xprocessconfig`) declares the runtime environment: working directory, environment variables (incremental override of Chapter 43's process environment), standard-stream handling (discard/inherit/capture/pipe), timeout and kill ladder (on timeout send SIGTERM first, SIGKILL after the grace period — the sending end of Chapter 49's send/receive division).

### The three-state exit status

```diagram flow
- Launch failure: executable missing/permissions - a parent-process error (Chapter 4's error slot)
- Normal exit: the child's exit(code) - the exit code goes into the struct
- Signal termination: killed by a signal (timeout kill/crash) - the signal number goes into the struct
- Verdict: xrtProcessResultSuccess = normal exit AND code 0
```

Why the three states matter: a non-zero exit code is the **child's semantic signal** (gcc reporting a compile error); being killed by a signal is **abnormal termination** (timeout, crash) — the two are handled completely differently (the former: read the output; the latter: diagnose). `Success` is the convenience verdict "code 0 and normal"; the full three states live in the struct's fields.

### Pipeline chaining

`xrtProcessPipeline(阶段数组, ...)` (stage array) creates and chains N stages in one call — the stdout→stdin between stages is a **real OS pipe** (data never passes through the parent — throughput and isolation both optimal); the last stage's output can optionally be captured. Contrast with hand-wired Spawn piping: handle-inheritance configuration, close timing at both ends, two-ended buffer deadlock — three classic pits all taken over by the implementation (the pipeline sample's comment says so verbatim).

### The lifecycle of captured streams

`Result.Stdout/Stderr` are **owning buffers** — released by `xrtProcessResultUnit` (buffers included); forgetting Unit is one leak per process run. The composition rule of capture and piping: the last stage may capture (intermediate stages' output went into the pipe — the parent never sees it, which is precisely the point of a pipeline).

## Examples

### Complete program: Shell execution and output capture

From the repository sample `examples/process/capture/main.c`:

```embed path="examples/process/capture/main.c" title="examples/process/capture/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/process/capture/main.c -lws2_32 -liphlpapi
captured output
```

**What just happened.** (1) `xrtProcessShell("echo captured output", &Result)` — the whole command goes through the system shell; platform differences (Windows echo versus POSIX printf) are handled by the sample's conditional compilation, runtime behavior identical. (2) `Result.Stdout` carries the captured owning buffer — printed directly to verify; `xrtProcessResultSuccess` decides "normal exit and code 0". (3) `xrtProcessResultUnit` frees the result — **the capture buffer travels with the result**, and Unit is its only exit (forgetting Unit leaks one block per run). (4) This shape is the shortest path to "external program as function call": a command string in, the pair (buffer + status) out.

### Complete program: a multi-stage pipeline

From `examples/process/pipeline/main.c`:

```embed path="examples/process/pipeline/main.c" title="examples/process/pipeline/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/process/pipeline/main.c -lws2_32 -liphlpapi
pipeline output
```

**What just happened.** (1) A two-stage configuration array (one `xprocessconfig` per stage) is handed to `xrtProcessPipeline` in one call — creation, wiring, launch, and joining are all its work. (2) The pipe between stages is OS-level (the Windows leg uses findstr, the POSIX leg tr — each platform picks the equivalent tool); the data flow **never passes through the parent** — throughput is not dragged down by parent relaying. (3) `Result.Stdout` captures the last stage's output and `xrtProcessPipelineSuccess` decides all-stages success — any stage failing fails the whole chain (a pipeline's failure semantics: all clear or nothing). (4) Against hand-wired piping: handle inheritance, two-ended deadlocks — the pits are named in the sample's comments; the implementation taking them over is exactly why Pipeline exists. The file sample demonstrates output redirection to a file (`xrt-process-output.txt`) — capture and redirection are the two destinations of standard-stream handling.

## Contracts

- **Family choice**: trusted commands — Shell for convenience; untrusted input — Spawn+Argv mandatory; the injection defense is kin to SQL parameterization.
- **Three-state semantics**: launch failure (parent error) / normal exit (code) / killed by signal (signal) — Success = code 0 and normal.
- **Result balancing**: `ResultUnit` frees the capture buffers included — every Result pairs with a Unit.
- **Timeout ladder**: the configuration declares timeout and kill (TERM→grace→KILL) — a stuck child never drags the parent down with it.
- **Pipeline semantics**: OS pipes wire stages directly, data bypasses the parent; the last stage may capture; any stage's failure fails the whole chain.
- **Environment control**: working directory and environment variables declared in the configuration — the child's runtime environment is an explicit contract, not an inheritance lottery.

### From examples to engineering: three hosts of child processes

**Tool glue** (the Shell family's home turf): transcoding, compression, notification scripts — when mature command-line tools already exist, calling them beats rewriting a library, faster and steadier; the command string is a code constant (trusted), output captured, timeout ladder — three configuration items in one step.**The security boundary** (the Spawn family's home turf): user-triggered file handling, plugin execution — argv-ification eliminates injection, working directory and environment declared explicitly, resources (timeout + output cap — the `StdoutTruncated` field tells you it was cut) controlled on three sides.**Data pipelines** (Pipeline's home turf): ETL steps, build chains — multi-stage chaining handed to OS pipes (throughput beats parent relaying), all-stages success verdict (build semantics). The three hosts share one bottom line: **a child process is code outside the boundary** — timeout, output cap, and exit three-state are the three things to think about on every call; skipping any one for convenience gets repaid in production another way.

### The complete ledger of the result struct

`xprocessresult`'s fields are richer than "output + exit code" by a stretch; worth naming one by one. `Status` — the three-state body (normal/signal/failure); `Wait` — the waiting outcome (timeout kills surface here); `InputWritten` — bytes fed to the child's stdin (pipe writes can be partial); `Stdout/Stderr` — each dual stream carries size and a **truncation flag** (`StdoutTruncated` — set when a capture cap was configured and exceeded: what you read may not be all of it, check before judging); `Duration` — elapsed microseconds (Chapter 42's measure — a ready-made field for performance observation). Read the fields as a ledger: **every field is a fossilized "something could have gone wrong here"** — the truncation flag prevents misjudgment, duration prevents blind spots, dual-stream separation prevents mixed reads. Design your own wrappers to this completeness — a result struct's field density is the API author's pit-density fossil.

### The junction with Volume 6: processes are also concurrency units

This chapter opening Volume 6 is no sorting coincidence — **the process is the oldest concurrency unit**. Against threads: processes isolate strongly (independent address spaces — crashes don't propagate) and start expensively (the fork/exec price); threads share memory (zero-copy communication but synchronization needed — Chapter 53) and start cheaply. Selection follows isolation needs: running untrusted/crash-prone code (plugins, third-party tools) — processes; cooperative computation — threads plus the concurrency system of the chapters after this one. The process's interaction points with the other concurrency units: inter-stage pipeline concurrency (OS pipes as channels — a process edition of the Channel); timeout kill meets the cancellation system (Chapter 54's token with SIGTERM — the "cancellable wait" form for external processes); the capture buffer's Unit balancing (the same discipline as value trees and tokens).**Processes, threads, and coroutines are three concurrency granularities** — this chapter the coarsest, Chapter 55 the finest, and the floor between them is the next chapter.

## Pitfalls

### Pitfall 1: user input spliced into a Shell command string

Symptom: a security audit reports command injection — input like `; rm -rf /` or `$(curl ...)` gets interpreted and executed.

Cause: the Shell family hands the command string to a shell for interpretation — metacharacters (`;`, `|`, `$`, backtick) are all attack surface; user data and command syntax share one layer.

```c bad
char sCmd[512];
snprintf(sCmd, sizeof(sCmd), "convert %s -resize 100 %s", sUserInput, sOut);
xrtProcessShell(sCmd, &Result);   /* input containing ";" injects arbitrary commands */
```

```c good
/* argv declared in xprocessconfig's fields; Spawn returns a handle, wait then take the result */
xprocess* P = xrtProcessSpawn(&Config);   /* arguments passed as-is - zero interpretation */
```

### Pitfall 2: forgetting ResultUnit leaks the capture buffer

Symptom: a long-running service creeps upward — one block leaked per external call; Chapter 6's live-byte staircase climbs.

Cause: `Result.Stdout` is an owning buffer and `ResultUnit` its only exit — Result's usage path was never balanced.

```c bad
if ( xrtProcessShell(sCmd, &Result) && xrtProcessResultSuccess(&Result) ) {
	Use(Result.Stdout, Result.Size);   /* used and dropped - the capture buffer leaks */
}
```

```c good
if ( xrtProcessShell(sCmd, &Result) ) {
	if ( xrtProcessResultSuccess(&Result) ) {
		Use(Result.Stdout, Result.Size);
	}
	xrtProcessResultUnit(&Result);   /* released on both success and failure paths */
}
```

## Exercises

### Basic: a three-state machine

Construct child processes with three endings each (normal code 0, normal non-zero code, killed by signal) — run each once with `xrtProcessShell`, printing Result's three-state fields and the Success verdict.

### Advanced: a caller with timeout

`run(命令, 超时秒数)` (command, timeout seconds): on timeout walk the TERM→KILL ladder; capture output; return the three states. Use it to call a sleep command and verify the signal fields on the timeout path.

### Challenge: a secure transcoding service

Implement an image-transcode interface on the Spawn family: filenames and parameters all argv-ified (zero concatenation), output captured, timeout ladder, concurrency cap (Chapter 22's semaphore or task pool). Acceptance criteria: an injection test set (twenty malicious filenames) executes nothing; timed-out processes always reaped (no zombies); the concurrency limit holds; transcode results match exit codes.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Twin families | Shell (command string + result in one step, trusted) / Spawn (config → handle, untrusted input) |
| Three states | launch failure / normal exit (code) / killed by signal (signal); Success = code 0 and normal |
| Result balancing | ResultUnit frees the capture buffers included - every Result pairs with a Unit |
| Pipeline | Pipeline takes a stage array and chains in one call; OS pipes wired directly; the last stage may capture |
| Timeout | the config declares the TERM→grace→KILL ladder - the sending end, across from Chapter 49 |
| Environment | working directory/environment variables in config - the runtime environment is a contract, not a lottery |
