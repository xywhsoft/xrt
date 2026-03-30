# XRT Guide: Subprocess and Tool Execution

> A practical introduction to `xrtExecCapture()`, `xrtProcessSpawn()`, stdin/stdout/stderr piping, and async wait via `xrtProcessWaitFuture()`.

[中文](subprocess-intro.md) | [Back to Guides](README.en.md)

---

## What This Page Explains

The current XRT mainline now includes a small subprocess layer for common tool-style workflows:

- run another program directly
- capture its stdout / stderr
- stream output while it runs
- write to the child process stdin
- wait synchronously or through a future

This is especially useful for CLI orchestration, build tools, and AI-agent tool chains.

---

## The Two Main Entry Points

### 1. `xrtExecCapture()`

Use this when you want:

- one-shot execution
- full stdout / stderr capture
- an exit code at the end

```c
#include "xrt.h"

int main(void)
{
	xprocessconfig tConfig;
	xprocessresult tResult;
	str arrArgs[] = { (str)"status", (str)"--short" };

	xrtInit();

	xrtProcessConfigInit(&tConfig);
	tConfig.sProgram = (str)"git";
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 2;

	if ( xrtExecCapture(&tConfig, &tResult, 5000) ) {
		printf("exit = %d\n", tResult.iExitCode);
		printf("stdout = %s\n", (char*)tResult.pStdout);
		printf("stderr = %s\n", (char*)tResult.pStderr);
		xrtProcessResultUnit(&tResult);
	}

	xrtUnit();
	return 0;
}
```

### 2. `xrtProcessSpawn()`

Use this when you want:

- streaming callbacks
- manual stdin writes
- explicit lifetime control
- async waiting

---

## Minimal Streaming Pattern

```c
static void procStdout(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData)
{
	(void)pProcess;
	(void)pUserData;
	fwrite(pData, 1, iSize, stdout);
}

int main(void)
{
	xprocessconfig tConfig;
	xprocessevents tEvents;
	xprocess* pProcess;
	str arrArgs[] = { (str)"-c", (str)"print('hello from child')" };

	xrtInit();

	memset(&tEvents, 0, sizeof(tEvents));
	tEvents.OnStdout = procStdout;

	xrtProcessConfigInit(&tConfig);
	tConfig.iFlags = XPROC_F_PIPE_STDOUT;
	tConfig.sProgram = (str)"python";
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 2;
	tConfig.pEvents = &tEvents;

	pProcess = xrtProcessSpawn(&tConfig);
	if ( pProcess ) {
		xrtProcessWait(pProcess);
		xrtProcessDestroy(pProcess);
	}

	xrtUnit();
	return 0;
}
```

---

## Writing to Child Stdin

If the child expects input, enable `XPROC_F_PIPE_STDIN` and then:

- call `xrtProcessWrite()` or `xrtProcessWriteText()`
- call `xrtProcessCloseStdin()` when you are done

That final close matters because many tools continue only after they receive EOF.

```c
xrtProcessWriteText(pProcess, (str)"line-1\nline-2\n", 0);
xrtProcessCloseStdin(pProcess);
```

---

## Waiting with a Future

When the network/future layer is enabled, the process can also be observed through a future:

```c
xfuture* pFuture = xrtProcessWaitFuture(pProcess);
if ( pFuture ) {
	if ( xFutureWaitValueTimeout(pFuture, 3000) == pProcess ) {
		printf("exit = %d\n", xrtProcessExitCode(pProcess));
	}
	xFutureRelease(pFuture);
}
```

This is useful when you want to compose subprocess completion with other XRT async flows.

---

## Important Behavior Notes

- `xrtProcessDestroy()` is not a kill call. Destroy only after the process has already exited.
- `OnStdout`, `OnStderr`, and `OnExit` run on internal worker threads, not automatically on the caller thread.
- `XPROC_F_MERGE_STDERR` routes stderr into stdout.
- `XPROC_F_NO_CAPTURE` disables in-memory capture when you only want streaming callbacks.
- `iMaxCaptureBytes == 0` means no capture limit.
- `XPROC_F_USE_SHELL` switches from direct argv mode to shell mode.

For most tool execution paths, direct argv mode should be the default.

---

## Recommended Defaults for Tool Chains

For general tool orchestration, a good starting point is:

- prefer direct argv mode
- enable `XPROC_F_PIPE_STDOUT | XPROC_F_PIPE_STDERR`
- enable `XPROC_F_PIPE_STDIN` only when needed
- set `XPROC_F_KILL_TREE` when the tool may fan out child workers
- use `xrtExecCapture()` for short, one-shot commands
- use `xrtProcessSpawn()` for long-running or interactive commands

---

## Related Reading

- [Base Runtime](../api/api-base.en.md)
- [Thread](../api/api-thread.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
- [Runtime and Thread Attach](runtime-thread-attach.en.md)
