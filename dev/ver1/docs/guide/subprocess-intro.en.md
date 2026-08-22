# XRT Guide: Subprocess and Command Execution

> A practical guide for using `subprocess` as the foundation for local command execution, tool orchestration, and agent-host shell workflows.

[中文](subprocess-intro.md) | [Back to Guides](README.en.md)

---

## 1. Four Rules First

1. Prefer `XPROC_TARGET_EXEC` for normal tools
2. Switch to `XPROC_TARGET_SHELL` only when shell semantics are required
3. Prefer `xrtExecCapture()` for one-shot commands
4. Prefer `xrtProcessSpawn()` for streaming or interactive control


## 2. Two Main Paths

### 2.1 One-shot execution with captured output

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

	if ( xrtExecCapture(&tConfig, &tResult, 5000u) ) {
		printf("exit=%d\n", tResult.ExitInfo.iExitCode);
		printf("kind=%d\n", tResult.ExitInfo.iKind);
		printf("stdout=%s\n", tResult.pStdout ? (char*)tResult.pStdout : "");
		printf("stderr=%s\n", tResult.pStderr ? (char*)tResult.pStderr : "");
		xrtProcessResultUnit(&tResult);
	}

	xrtUnit();
	return 0;
}
```

Use this for short commands where you want a complete result object at the end.


### 2.2 Spawn and control the process yourself

```c
static void procStdout(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData)
{
	(void)pProcess;
	(void)pUserData;
	fwrite(pData, 1u, iSize, stdout);
}

int main(void)
{
	xprocessconfig tConfig;
	xprocessevents tEvents;
	xprocess* pProcess;
	str arrArgs[] = { (str)"--version" };

	xrtInit();

	memset(&tEvents, 0, sizeof(tEvents));
	tEvents.OnStdout = procStdout;

	xrtProcessConfigInit(&tConfig);
	tConfig.sProgram = (str)"git";
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 1u;
	tConfig.Stdout.iMode = XPROC_STDIO_PIPE;
	tConfig.pEvents = &tEvents;

	pProcess = xrtProcessSpawn(&tConfig);
	if ( pProcess != NULL ) {
		(void)xrtProcessWait(pProcess);
		xrtProcessDestroy(pProcess);
	}

	xrtUnit();
	return 0;
}
```

Use this when you need streaming callbacks, manual stdin, or explicit stop control.


## 3. When to Use Shell Mode

Switch to shell mode only when the command depends on:

- shell builtins
- redirection or pipes
- `&&` or other shell syntax
- shell-specific expansion rules

```c
xrtProcessConfigInit(&tConfig);
tConfig.iTargetKind = XPROC_TARGET_SHELL;
tConfig.sCommand = (str)"echo shell-out";
```

You do not need to manually prepend:

- `cmd /c` on Windows
- `/bin/sh -c` on POSIX

The subprocess layer selects the default shell automatically.  
If you want a specific shell such as `pwsh`, assign it to `sProgram`.


## 4. Writing to Child Stdin

```c
xrtProcessConfigInit(&tConfig);
tConfig.sProgram = (str)"python";
tConfig.Stdin.iMode = XPROC_STDIO_PIPE;
tConfig.Stdout.iMode = XPROC_STDIO_PIPE;
```

Then:

```c
xrtProcessWriteText(pProcess, (str)"line-1\nline-2\n", 0u);
xrtProcessCloseStdin(pProcess);
```

Closing stdin matters because many tools do not continue until they observe EOF.


## 5. Safe Incremental Output Polling

For agent hosts, polling output incrementally is often more useful than taking full snapshots.

```c
xprocessreadinfo tReadInfo;
size_t iSize = 0u;
uint64 iOffset = 0u;
ptr pChunk;

memset(&tReadInfo, 0, sizeof(tReadInfo));
pChunk = xrtProcessReadStdoutSince(pProcess, iOffset, 0u, &iSize, &tReadInfo);
if ( pChunk != NULL ) {
	fwrite(pChunk, 1u, iSize, stdout);
	xrtFree(pChunk);
}
iOffset = tReadInfo.iNextOffset;
```

Key fields:

- `iBaseOffset`
	- earliest retained offset
- `iNextOffset`
	- next offset to read from
- `iStreamEndOffset`
	- current stream tail
- `bDone`
	- stream has finished


### 5.2 Unified event timeline

If you need a single ordered timeline for process lifecycle and output, poll the event stream directly:

```c
xprocesseventreadinfo tEventInfo;
xprocessevent* arrEvents = NULL;
uint32 iEventCount = 0u;
uint64 iSeq = 0u;

memset(&tEventInfo, 0, sizeof(tEventInfo));
arrEvents = xrtProcessReadEventsSince(pProcess, iSeq, 0u, &iEventCount, &tEventInfo);
if ( arrEvents != NULL ) {
	for ( uint32 i = 0u; i < iEventCount; i++ ) {
		printf("seq=%llu kind=%d stream=%d size=%llu\n",
			(unsigned long long)arrEvents[i].iSeq,
			arrEvents[i].iKind,
			arrEvents[i].iStream,
			(unsigned long long)arrEvents[i].iSize);
	}
	xrtFree(arrEvents);
}
iSeq = tEventInfo.iNextSeq;
```

In terminal mode, the output stream kind becomes `XPROC_STREAM_TERMINAL`.


## 6. Environment and Working Directory

### 6.1 Environment overlay

```c
str arrEnv[] = {
	(str)"LANG=C",
	(str)"XRT_TOOL_MODE=agent"
};

xrtProcessConfigInit(&tConfig);
tConfig.sProgram = (str)"python";
tConfig.arrEnv = arrEnv;
tConfig.iEnvCount = 2u;
tConfig.bInheritEnv = true;
```

For an isolated environment:

```c
tConfig.bInheritEnv = false;
```


### 6.2 Strict working directory

```c
xrtProcessConfigInit(&tConfig);
tConfig.sProgram = (str)"git";
tConfig.sWorkDir = (str)"/opt/project";
```

If the directory is invalid, launch fails with structured exit info:

- `iKind = XPROC_EXIT_SPAWN_FAILED`
- `iStage = XPROC_STAGE_WORKDIR`

It does not silently fall back to the old current directory.


## 7. Terminal Mode

Use terminal mode when you need an interactive shell, a REPL, or a tool that explicitly requires a TTY:

```c
xrtProcessConfigInit(&tConfig);
tConfig.iTargetKind = XPROC_TARGET_SHELL;
tConfig.bUseTerminal = true;
```

Check support first:

```c
if ( xrtProcessTerminalSupported() ) {
	/* spawn */
}
```

You can also resize the terminal after startup:

```c
xrtProcessResizeTerminal(pProcess, 100u, 32u);
```

Important detail:

- terminal mode returns raw terminal bytes
- the stream may contain ANSI / VT control sequences and title updates
- strip escape sequences yourself if your UI needs plain text


## 8. Reading Structured Exit Info

Prefer this over checking only `xrtProcessExitCode()`:

```c
xprocessexitinfo tExitInfo;

if ( xrtProcessGetExitInfo(pProcess, &tExitInfo) ) {
	printf("kind=%d\n", tExitInfo.iKind);
	printf("exit=%d\n", tExitInfo.iExitCode);
	printf("signal=%d\n", tExitInfo.iSignal);
	printf("stage=%d\n", tExitInfo.iStage);
	printf("os_error=%d\n", tExitInfo.iOsError);
	printf("stop=%d\n", tExitInfo.iStopReason);
	printf("timed_out=%d\n", tExitInfo.bTimedOut);
	printf("cancelled=%d\n", tExitInfo.bCancelled);
}
```

This is especially important for distinguishing:

- launch failure
- normal non-zero exit
- timeout-driven shutdown
- signal-based exit on POSIX


## 9. Stop Semantics

The subprocess layer exposes four levels of stop requests:

- `xrtProcessInterrupt()`
- `xrtProcessTerminate()`
- `xrtProcessKill()`
- `xrtProcessKillTree()`

Recommended escalation:

1. `xrtProcessWaitTimeout()`
2. `xrtProcessInterrupt()`
3. `xrtProcessTerminate()`
4. `xrtProcessKillTree()`

`xrtExecCapture()` follows a similar internal timeout shutdown sequence.


## 10. Futures

When the future layer is enabled:

```c
xfuture* pFuture = xrtProcessWaitFuture(pProcess);
if ( pFuture != NULL ) {
	if ( xFutureWaitValueTimeout(pFuture, 3000) == pProcess ) {
		printf("done\n");
	}
	xFutureRelease(pFuture);
}
```


## 11. Common Mistakes

- `xrtProcessDestroy()` is not a stop call
- `OnExit` now receives `xprocessexitinfo*`
- `GetStdout/GetStderr` return new heap buffers and must be freed with `xrtFree()`
- `PIPE` mode is required for stdin writes and retained stream reads
- terminal mode returns raw terminal bytes instead of pre-cleaned plain text
- prefer direct exec over shell unless shell syntax is actually needed


## 12. Related Pages

- [Subprocess API](../api/api-subprocess.md)
- [Thread](../api/api-thread.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
- [Runtime and Thread Attach](runtime-thread-attach.en.md)
