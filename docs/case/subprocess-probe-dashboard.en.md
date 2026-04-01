# XRT Case Study: Upgrade the Local Console Service into a Subprocess Probe Dashboard

> This case shows how a local service should attach an external probe tool to its own runtime mainline, using `config + logging + thread + queue + subprocess + future + xhttpd + template` as one practical dashboard flow.

[中文](subprocess-probe-dashboard.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you already have a local console service, and now the requirements change:

- `POST /api/probe` must trigger one real probe
- the probe is an external CLI tool, not an in-process function
- both stdout and stderr must be preserved
- the browser dashboard must show the latest probe result
- the result must also be written into a latest snapshot file

Without one clear mainline, this quickly degrades into:

- `system()` or shell strings inside the HTTP handler
- blocked requests while the probe runs
- stdout and stderr printed to the console only
- page, JSON, log, and snapshot each inventing their own fields again

This page upgrades the previous local-console-service case into a subprocess-driven probe dashboard.


## 2. Why This Upgrade Matters

The previous page already made these five lines stable:

- config
- logging
- background work
- HTTP
- template rendering

But the background work still stayed in-process.

The real problem here is now:

> background work must call an external tool, and the completion of that external tool must itself join the formal waiting model.

So this page mainly adds two layers:

- `subprocess`
- `future`

That means:

- `queue + thread` moves probe jobs into the background
- `subprocess` runs the real probe tool
- `xrtProcessWaitFuture()` turns "probe completed" into one unified waiting boundary


## 3. What Each Layer Does

| Layer | Role in this case | What it really solves |
|------|-------------------|------------------------|
| `path + file` | config, template, log, and snapshot paths | puts the service into the real filesystem |
| `json + xvalue` | unified config and dashboard model | prevents drift between page, API, and snapshot fields |
| `thread + xmpscqwait` | serial background probe consumption | keeps HTTP from blocking on the external tool |
| `subprocess` | launch the probe CLI | direct argv, stdin/stdout/stderr, exit code |
| `xrtProcessWaitFuture()` | wait for subprocess completion | timeout and unified waiting |
| `xhttpd` | local HTTP entry | trigger probes and inspect current state |
| `template` | HTML dashboard output | browser-readable page output |

One-sentence version:

> HTTP owns the job entry, the worker owns the external probe, `future` owns the completion boundary, and `xvalue` owns the unified export into JSON, HTML, and snapshot files.


## 4. Compact Code Skeleton

The reviewed Chinese page contains the fully expanded version. The English page keeps a compact but source-aligned skeleton.

This version stays self-contained:

- the parent process starts as the local dashboard service
- the worker launches the current program again
- the child process runs in `--probe-child` mode and behaves like a small probe CLI

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
	int32 iNextJobId;
	int32 iPendingJobs;
	int32 iLastExitCode;
	int32 iProbeCount;
	bool bLastProbeOk;
	char sLastTarget[64];
	char sLastStdout[256];
	char sLastStderr[256];
} DemoStats;

typedef struct
{
	int32 iJobId;
	char sTarget[64];
} DemoProbeJob;

typedef struct
{
	char sBindIP[64];
	uint16 iPort;
	char sDefaultTarget[64];
	xmutex hStatsMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	DemoStats tStats;
} DemoApp;

static void procCopyText(char* sDst, size_t iCap, const char* sText, const char* sFallback)
{
	const char* sUse = sText;

	if ( (sUse == NULL) || (*sUse == '\0') ) {
		sUse = (sFallback != NULL) ? sFallback : "";
	}
	if ( iCap == 0u ) {
		return;
	}
	snprintf(sDst, iCap, "%s", sUse);
}

static bool procWaitFutureResolved(const char* sStep, xfuture* pFuture, int64 iTimeoutMs)
{
	const char* sError;

	if ( pFuture == NULL ) {
		printf("%s failed: future missing\n", (sStep != NULL) ? sStep : "future");
		return false;
	}
	if ( !xFutureWaitTimeout(pFuture, iTimeoutMs) ) {
		printf("%s timeout\n", (sStep != NULL) ? sStep : "future");
		return false;
	}
	if ( xFutureState(pFuture) != XFUTURE_RESOLVED ) {
		sError = (const char*)xFutureError(pFuture);
		printf(
			"%s failed: state=%d status=%d error=%s\n",
			(sStep != NULL) ? sStep : "future",
			(int)xFutureState(pFuture),
			(int)xFutureStatus(pFuture),
			(sError != NULL) ? sError : "(null)"
		);
		return false;
	}
	return true;
}

static bool procRunProbe(const char* sTarget, int32* pRetExitCode, char* sStdout, size_t iStdoutCap, char* sStderr, size_t iStderrCap)
{
	xprocessconfig tCfg;
	xprocess* pProcess = NULL;
	xfuture* pWaitFuture = NULL;
	str arrArgs[] = { (str)"--probe-child" };
	char sInput[96];
	ptr pStdout = NULL;
	ptr pStderr = NULL;
	size_t iStdoutSize = 0u;
	size_t iStderrSize = 0u;
	bool bOk = false;

	snprintf(sInput, sizeof(sInput), "%s\n", (sTarget != NULL) ? sTarget : "");
	xrtProcessConfigInit(&tCfg);
	tCfg.iFlags = XPROC_F_PIPE_STDIN | XPROC_F_PIPE_STDOUT | XPROC_F_PIPE_STDERR | XPROC_F_KILL_TREE;
	tCfg.sProgram = xCore.AppFile;
	tCfg.arrArgs = arrArgs;
	tCfg.iArgCount = 1u;
	tCfg.iReadChunkSize = 4096u;
	tCfg.iMaxCaptureBytes = 64u * 1024u;

	pProcess = xrtProcessSpawn(&tCfg);
	if ( pProcess == NULL ) {
		procCopyText(sStderr, iStderrCap, "spawn child failed", "");
		goto cleanup;
	}

	if ( xrtProcessWriteText(pProcess, (str)sInput, 0u) <= 0 ) {
		procCopyText(sStderr, iStderrCap, "write child stdin failed", "");
		goto cleanup;
	}
	(void)xrtProcessCloseStdin(pProcess);

	pWaitFuture = xrtProcessWaitFuture(pProcess);
	if ( !procWaitFutureResolved("wait probe process", pWaitFuture, 5000) ) {
		if ( xrtProcessIsRunning(pProcess) ) {
			(void)xrtProcessKillTree(pProcess);
			(void)xrtProcessWait(pProcess);
		}
		procCopyText(sStderr, iStderrCap, xFutureError(pWaitFuture), "wait probe process failed");
		goto cleanup;
	}

	*pRetExitCode = xrtProcessExitCode(pProcess);
	pStdout = xrtProcessGetStdout(pProcess, &iStdoutSize);
	pStderr = xrtProcessGetStderr(pProcess, &iStderrSize);
	procCopyText(sStdout, iStdoutCap, (const char*)pStdout, "");
	procCopyText(sStderr, iStderrCap, (const char*)pStderr, "");
	bOk = true;

cleanup:
	if ( pWaitFuture != NULL ) {
		xFutureRelease(pWaitFuture);
	}
	if ( pProcess != NULL ) {
		xrtProcessDestroy(pProcess);
	}
	return bOk;
}

static uint32 procProbeWorker(ptr pArg)
{
	DemoApp* pApp = (DemoApp*)pArg;
	DemoProbeJob* pJob = NULL;
	xqueue_result iRet;
	char sStdout[256];
	char sStderr[256];
	int32 iExitCode = -1;
	bool bOk;

	for ( ;; ) {
		pJob = NULL;
		iRet = xrtMPSCQWaitPop(pApp->hQueue, (ptr*)&pJob);
		if ( iRet == XQUEUE_CLOSED ) {
			break;
		}
		if ( (iRet != XQUEUE_OK) || (pJob == NULL) ) {
			continue;
		}

		memset(sStdout, 0, sizeof(sStdout));
		memset(sStderr, 0, sizeof(sStderr));
		bOk = procRunProbe(pJob->sTarget, &iExitCode, sStdout, sizeof(sStdout), sStderr, sizeof(sStderr));

		xrtMutexLock(pApp->hStatsMutex);
		if ( pApp->tStats.iPendingJobs > 0 ) {
			pApp->tStats.iPendingJobs--;
		}
		pApp->tStats.iProbeCount++;
		pApp->tStats.iLastExitCode = iExitCode;
		pApp->tStats.bLastProbeOk = bOk && (iExitCode == 0);
		procCopyText(pApp->tStats.sLastTarget, sizeof(pApp->tStats.sLastTarget), pJob->sTarget, "");
		procCopyText(pApp->tStats.sLastStdout, sizeof(pApp->tStats.sLastStdout), sStdout, "");
		procCopyText(pApp->tStats.sLastStderr, sizeof(pApp->tStats.sLastStderr), sStderr, "");
		xrtMutexUnlock(pApp->hStatsMutex);

		xrtFree(pJob);
	}

	return 0u;
}

static int procChildProbeMain(void)
{
	char sTarget[64];
	size_t iLen;

	memset(sTarget, 0, sizeof(sTarget));
	if ( fgets(sTarget, sizeof(sTarget), stdin) == NULL ) {
		procCopyText(sTarget, sizeof(sTarget), "demo", "");
	}

	iLen = strlen(sTarget);
	if ( (iLen > 0u) && (sTarget[iLen - 1u] == '\n') ) {
		sTarget[iLen - 1u] = '\0';
	}

	if ( strcmp(sTarget, "fail") == 0 ) {
		fprintf(stderr, "probe target=%s status=failed\n", sTarget);
		return 2;
	}

	xrtSleep(120u);
	printf("probe target=%s status=ok\n", sTarget);
	fprintf(stderr, "latency_ms=120\n");
	return 0;
}
```


## 5. Why This Split Is Stable

### 5.1 The HTTP entry only accepts probe requests

`POST /api/probe` should not launch the external tool directly.

It should only:

- decide the target of this probe
- push a job into `xmpscqwait`

That keeps:

- the request path short
- the browser unblocked by the probe tool
- later throttling, deduplication, or batching in the worker layer

### 5.2 Process completion is unified through a future

The value of `xrtProcessWaitFuture()` here is not style. It is:

- turning "external tool completed" into a formal wait object
- giving timeout and failure one unified meaning
- keeping the model ready for continuation, `WhenAll`, or task-group composition later

### 5.3 stdout and stderr stay part of the runtime model

The latest probe result should flow into:

- `/api/dashboard`
- `/dashboard`
- `runtime/last-probe.json`
- `runtime/probe_console.log`

These should not be separate formats. They should be different consumers of one runtime state model.

### 5.4 The self-bootstrapping child probe is only a teaching shell

Launching the current program again is only there to keep the case:

- self-contained
- easy to copy and run immediately

In a real project, replace:

- `tCfg.sProgram = xCore.AppFile`
- `arrArgs = { "--probe-child" }`

with your real tool:

- `ffprobe`
- `curl`
- your own CLI
- any other direct-argv probe program


## 6. Common Mistakes

### Mistake 1: doing `Spawn + Wait` directly inside the HTTP handler

That drags the slow probe back into the request path.

### Mistake 2: calling only `Destroy()` on timeout without `KillTree()` first

`xrtProcessDestroy()` is not a kill operation. If the tool is still running, destroying the handle does not close the external process cleanly.

### Mistake 3: assuming `GetStdout()` / `GetStderr()` can be held forever

The current implementation and subprocess docs both require you to treat them as borrowed buffers tied to `xprocess`.

### Mistake 4: letting page, log, and snapshot each rebuild their own probe result

That quickly splits field names and state meaning.


## 7. Next Extensions

The most natural follow-up upgrades from this page are:

- replace `--probe-child` with a real external tool
- add `future + job status` so that "queued / running / finished / failed" are shown separately
- keep several recent probe results in a list or `avltree`
- turn stdout / stderr into files and connect the flow to [Subprocess + Async File Tooling Pipeline](subprocess-file-async-pipeline.en.md)

Suggested companion reading:

- [A Local Console Service that Combines Config, Logging, Tasks, Networking, and Templates](local-console-service.en.md)
- [Subprocess + Async File Tooling Pipeline](subprocess-file-async-pipeline.en.md)
- [Guide: Subprocess and Tool Execution](../guide/subprocess-intro.en.md)
