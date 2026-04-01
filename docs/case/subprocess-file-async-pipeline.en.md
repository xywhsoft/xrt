# XRT Case Study: Subprocess + Async File Tooling Pipeline

> A practical tooling chain: launch an external tool, wait through a future, persist stdout/stderr asynchronously, then reopen the result through the async file layer.

[中文](subprocess-file-async-pipeline.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you are building a small tool orchestrator with these constraints:

1. start an external CLI tool
2. write input into its stdin
3. capture stdout and stderr
4. save the output into a workspace directory
5. continue the rest of the pipeline asynchronously after the tool exits

Without one clear mainline, this usually degrades into:

- `system()` or shell-pipe string assembly
- no formal object for exit code, timeout, or process completion
- synchronous file writes scattered after the process exits
- ownership of captured stdout/stderr becoming unclear

This case solves the exact problem of how an external tool should be attached to the XRT async/future mainline in a stable way.

---

## 2. Why This Mainline

### Why not `system()` or shell-string assembly?

Because the real problem is not just "run a command".

The pipeline needs:

- direct argv execution
- explicit stdin/stdout/stderr pipes
- a formal process-completion future
- output artifacts that keep flowing into async file work

`system()` is too coarse for that.

### Why not switch back to synchronous file I/O after the subprocess finishes?

Because the goal here is not only to get one short demo running. The goal is:

- keep process completion inside the future chain
- let file persistence stay composable and concurrent
- keep later steps on the same waiting model

### Why not `task group` this time?

`task group` is better when the main problem is:

- one structured scope of child tasks
- one fan-out / join boundary

This case is more linear:

- one subprocess finishes
- then its artifacts continue into async file work

So the better fit is:

- `subprocess` for external execution
- `future` for waiting and composition
- `file_async` for persistence and later reads

---

## 3. What Each Layer Does in This Chain

| Layer | Role in this case | What it really gives you |
|------|-------------------|--------------------------|
| `xprocess` | launch and manage the external tool | direct argv, stdin/stdout/stderr, exit code |
| `xrtProcessWaitFuture()` | turn "process completed" into a future | timeout and unified waiting |
| `xrtFilePutAllAsync()` | persist stdout and stderr concurrently | file artifacts stay on the future mainline |
| `xFutureWhenAll()` | one barrier over several file-write futures | unified waiting for several artifact writes |
| `xrtFileMoveAsync()` | publish the final output artifact | move from temp file to final report |
| `xasyncfile` | reopen and preview the artifact by explicit offset | handle-layer async file work |

One-sentence version:

> `subprocess` runs the tool, `future` keeps completion in the unified wait model, and `file_async` keeps the produced artifacts moving forward instead of falling back to blocking file I/O.

---

## 4. The Target Flow

The complete flow of this case is:

1. the parent process chooses parent-mode vs child-tool mode
2. the parent asynchronously creates the work directory
3. the parent launches itself again as a child tool through direct argv
4. the parent writes text into child stdin and waits through `xrtProcessWaitFuture()`
5. the parent copies captured stdout/stderr and persists them through async file futures
6. the parent publishes the final report by moving `stdout.tmp` into `report.txt`
7. the parent reopens the report with `xasyncfile` and reads back a preview by explicit offset

This is not:

- "the subprocess ends, then we return to synchronous file work"

It is:

- `subprocess -> wait future -> file-async futures -> final preview`

---

## 5. The 4 Most Important Boundaries

### 5.1 Process Execution Boundary

The parent does not inline the tool logic into itself. It explicitly:

1. launches a child through direct argv
2. writes to child stdin through `xrtProcessWriteText()` and `xrtProcessCloseStdin()`

So:

- the child owns the actual external-tool logic
- the parent owns orchestration of the tooling chain

### 5.2 Waiting Boundary

This case does not stop at:

- `xrtProcessWait()`

It turns completion into:

- `xrtProcessWaitFuture()`

So timeout, failure, and later composition stay inside the future mainline.

### 5.3 Capture-Buffer Boundary

This is a very important rule.

From the current implementation and tests:

- `xrtProcessGetStdout()`
- `xrtProcessGetStderr()`

return borrowed buffers owned by `xprocess`, not heap objects owned by the caller.

So this case first:

- copies stdout/stderr into caller-owned heap buffers

and only then passes those copies into:

- `xrtFilePutAllAsync()`

That prevents later async file tasks from depending on `xprocess` lifetime.

### 5.4 File-Layer Boundary

The case intentionally uses both async file layers:

- path helper layer:
	- `xrtDirCreateAllAsync()`
	- `xrtFilePutAllAsync()`
	- `xrtFileMoveAsync()`
- handle layer:
	- `xrtAsyncFileOpen()`
	- `xrtAsyncFileReadAt()`

That makes the split explicit:

- use path helpers for one-shot artifact handling
- use `xasyncfile` when offset-based reads or handle reuse are needed

---

## 6. Why This Model Works Well for Tool Chains

Many tooling orchestrators start runnable, but soon degrade into:

- longer and longer shell strings
- stdout and stderr becoming mixed
- artifact persistence falling back to blocking file writes
- no clean boundary for timeout, exit code, and result files

This model keeps those pieces explicit:

- direct argv for launch
- process completion as a future
- artifact persistence through `file_async`
- multi-file writes still composable through `WhenAll`

That makes it much easier to grow into:

- batch tooling
- automation pipelines
- agent-style tool execution chains

---

## 7. Common Mistakes

### Mistake 1: Defaulting to shell command strings

That blurs quoting, cross-platform behavior, and error attribution.

### Mistake 2: Treating `xrtProcessGetStdout()` as caller-owned memory

In the current implementation, it behaves like borrowed data.

### Mistake 3: Destroying `xprocess` immediately after capture without first copying the buffers

That is dangerous if later async tasks still depend on those buffers.

### Mistake 4: Returning to synchronous file I/O halfway through the chain

That breaks the unified waiting model and makes later composition harder.

### Mistake 5: Looking only at stdout and ignoring stderr or exit code

Many tools put the real failure details into stderr.

---

## 8. Suggested Reading

- [Guide: Subprocess and Tool Execution](../guide/subprocess-intro.en.md)
- [Guide: Async File and Directory Operations](../guide/file-async-intro.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
- [Case: Queue + Future Multi-Producer Worker](queue-worker-future.md) (Chinese for now)
- [Case: Thread / Coroutine / Future Coordination](thread-coroutine-future.en.md)

---

**One-sentence summary:** the key is not just "run the command"; the key is that subprocess completion enters the future chain, the produced artifacts continue through `file_async`, and the whole tooling pipeline stays on one unified waiting model.
