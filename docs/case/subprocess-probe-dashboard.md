# 把本地控制台服务升级成一个子进程探测面板

> 这个案例讲的不是“会不会启动一个子进程”，而是当一个本地服务需要把外部探测工具稳定接进自己的运行时主线时，怎样把 `config + logging + thread + queue + subprocess + future + xhttpd + template` 这些层真正串成一个可落地的面板程序。

[返回案例索引](README.md)

---

## 1. 场景

假设你已经有了一个本地控制台服务，但现在需求升级了：

- `POST /api/probe` 需要触发一次真实探测
- 探测不是进程内函数，而是一个外部 CLI 工具
- 探测的 stdout / stderr 都要保留下来
- 浏览器里的 dashboard 要看到最近一次探测结果
- 探测结果还要落成一个最新快照文件，方便别的脚本读

如果这时没有一条正式主线，代码很容易退化成：

- HTTP handler 里直接 `system()` 或 shell 拼命令
- 探测卡住时，整个请求也跟着卡住
- stdout / stderr 只有控制台打印，没有正式对象边界
- 页面、JSON、日志、快照文件各自再拼一套字段

这页要做的，就是把“本地控制台服务”继续升级成“子进程探测面板”。


## 2. 为什么这次要从上一页继续升级

上一页 [把配置、日志、任务、网络和模板串成一个本地控制台服务](local-console-service.md) 已经把 5 条基础线拉直了：

- 配置
- 日志
- 后台任务
- HTTP
- 模板

但它的后台任务还是“进程内刷新”。

这次的问题已经变成：

> 后台任务真正需要调用外部工具，而且这个“外部工具完成”本身也要进入正式等待模型。

所以这次新增的关键层只有两层：

- `subprocess`
- `future`

也就是：

- `queue + thread` 负责把探测任务搬到后台
- `subprocess` 负责把探测工具跑起来
- `xrtProcessWaitFuture()` 负责把“探测完成”接进统一等待语义


## 3. 这条链里每一层负责什么

| 层 | 这次承担的角色 | 真正解决的问题 |
|----|----------------|----------------|
| `path + file` | 配置、模板、日志、快照路径 | 让程序落到真实文件系统里 |
| `json + xvalue` | 统一配置与 dashboard 模型 | 避免页面、API、快照字段分叉 |
| `thread + xmpscqwait` | 探测任务后台串行消费 | HTTP 不直接阻塞在外部工具上 |
| `subprocess` | 拉起探测 CLI | direct argv、stdin/stdout/stderr、exit code |
| `xrtProcessWaitFuture()` | 等待子进程完成 | timeout、统一等待 |
| `xhttpd` | 本地 HTTP 入口 | 触发探测和查看当前状态 |
| `template` | dashboard HTML 输出 | 浏览器直接可读的页面 |

一句话记忆：

> HTTP 只负责任务入口，worker 负责进程外探测，`future` 负责等待完成，`xvalue` 负责把结果统一导出成 JSON、HTML 和快照文件。


## 4. 完整骨架

这份代码故意保持“无第三方外部依赖”的可验证形态：

- 父进程正常启动成本地面板
- worker 真正执行探测时，会把当前程序自己再拉起一次
- 被拉起的子进程走 `--probe-child` 模式，模拟一个真实探测工具

这样写的好处是：

- 案例主线完整
- 不依赖你机器里一定有某个外部命令
- 读者复制后更容易先跑通，再替换成自己的真实探测工具

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

static const char g_sDefaultConfigJson[] =
	"{\n"
	"\t\"service_name\": \"xrt-probe-console\",\n"
	"\t\"bind_ip\": \"127.0.0.1\",\n"
	"\t\"port\": 8086,\n"
	"\t\"queue_capacity\": 128,\n"
	"\t\"default_target\": \"demo\"\n"
	"}\n";

static const char g_sDefaultDashboardTemplate[] =
	"<!doctype html>\n"
	"<html>\n"
	"<head>\n"
	"\t<meta charset=\"utf-8\">\n"
	"\t<title>{$service_name}</title>\n"
	"</head>\n"
	"<body>\n"
	"\t<h1>{$service_name}</h1>\n"
	"\t<p>listen={$listen_addr}</p>\n"
	"\t<p>pending_jobs={$pending_jobs}</p>\n"
	"\t<p>probe_count={$probe_count}</p>\n"
	"\t<p>last_target={$last_probe_target}</p>\n"
	"\t<p>last_ok={$last_probe_ok}</p>\n"
	"\t<p>last_exit_code={$last_probe_exit_code}</p>\n"
	"\t<p>last_probe_at={$last_probe_at}</p>\n"
	"\t<p>last_log={$last_log_line}</p>\n"
	"\t<form method=\"post\" action=\"/api/probe\">\n"
	"\t\t<button type=\"submit\">probe now</button>\n"
	"\t</form>\n"
	"\t<ul>\n"
	"\t{#foreach:cards}\n"
	"\t\t<li><strong>{$title}</strong>: {$value_text}</li>\n"
	"\t{#end}\n"
	"\t</ul>\n"
	"\t<h2>stdout</h2>\n"
	"\t<pre>{$last_stdout}</pre>\n"
	"\t<h2>stderr</h2>\n"
	"\t<pre>{$last_stderr}</pre>\n"
	"</body>\n"
	"</html>\n";

typedef struct
{
	char sServiceName[64];
	char sBindIP[64];
	uint16 iPort;
	uint32 iQueueCapacity;
	char sDefaultTarget[64];
} AppConfig;

typedef struct
{
	int32 iNextJobId;
	int32 iPendingJobs;
	int32 iProbeCount;
	int32 iLastExitCode;
	bool bLastProbeOk;
	xtime tLastProbeAt;
	char sLastTarget[64];
	char sLastStdout[256];
	char sLastStderr[256];
	char sLastLogLine[160];
} AppStats;

typedef struct
{
	int32 iJobId;
	char sTarget[64];
} ProbeJob;

typedef struct
{
	AppConfig tCfg;
	AppStats tStats;
	str sConfigPath;
	str sTemplatePath;
	str sLogPath;
	str sSnapshotPath;
	xmutex hStatsMutex;
	xmpscqwait hQueue;
	xthread hWorker;
	xteengine hTemplateEngine;
	xtetemplate hDashboardTemplate;
	xnetengine* pNetEngine;
	xhttpdserver* pHttpd;
} AppState;


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


static void procCopyBytesText(char* sDst, size_t iCap, const void* pData, size_t iSize, const char* sFallback)
{
	size_t iCopy;

	if ( (sDst == NULL) || (iCap == 0u) ) {
		return;
	}

	if ( (pData == NULL) || (iSize == 0u) ) {
		procCopyText(sDst, iCap, sFallback, "");
		return;
	}

	iCopy = iSize;
	if ( iCopy >= iCap ) {
		iCopy = iCap - 1u;
	}

	memcpy(sDst, pData, iCopy);
	sDst[iCopy] = '\0';
}


static void procInitDefaultConfig(AppConfig* pCfg)
{
	memset(pCfg, 0, sizeof(*pCfg));
	procCopyText(pCfg->sServiceName, sizeof(pCfg->sServiceName), "xrt-probe-console", "");
	procCopyText(pCfg->sBindIP, sizeof(pCfg->sBindIP), "127.0.0.1", "");
	procCopyText(pCfg->sDefaultTarget, sizeof(pCfg->sDefaultTarget), "demo", "");
	pCfg->iPort = 8086u;
	pCfg->iQueueCapacity = 128u;
}


static bool procEnsureParentDir(str sPath)
{
	str sDir = NULL;
	bool bOk = false;

	sDir = xrtPathGetDir(sPath, 0u);
	if ( sDir == NULL ) {
		return false;
	}

	bOk = xrtDirCreateAll(sDir);
	xrtFree(sDir);
	return bOk;
}


static bool procBuildPaths(AppState* pApp)
{
	pApp->sConfigPath = xrtPathJoin(2u, "config", "probe_console.json");
	pApp->sTemplatePath = xrtPathJoin(2u, "web", "probe_dashboard.html");
	pApp->sLogPath = xrtPathJoin(2u, "runtime", "probe_console.log");
	pApp->sSnapshotPath = xrtPathJoin(2u, "runtime", "last-probe.json");

	return
		(pApp->sConfigPath != NULL) &&
		(pApp->sTemplatePath != NULL) &&
		(pApp->sLogPath != NULL) &&
		(pApp->sSnapshotPath != NULL);
}


static bool procEnsureBootstrapFiles(AppState* pApp)
{
	if ( !procEnsureParentDir(pApp->sConfigPath) ) {
		return false;
	}
	if ( !procEnsureParentDir(pApp->sTemplatePath) ) {
		return false;
	}
	if ( !procEnsureParentDir(pApp->sLogPath) ) {
		return false;
	}
	if ( !procEnsureParentDir(pApp->sSnapshotPath) ) {
		return false;
	}

	if ( !xrtFileExists(pApp->sConfigPath) ) {
		if ( xrtFileWriteAll(pApp->sConfigPath, (str)g_sDefaultConfigJson, sizeof(g_sDefaultConfigJson) - 1u, XRT_CP_UTF8) < 0 ) {
			return false;
		}
	}

	if ( !xrtFileExists(pApp->sTemplatePath) ) {
		if ( xrtFileWriteAll(pApp->sTemplatePath, (str)g_sDefaultDashboardTemplate, sizeof(g_sDefaultDashboardTemplate) - 1u, XRT_CP_UTF8) < 0 ) {
			return false;
		}
	}

	return true;
}


static bool procLoadConfig(AppState* pApp)
{
	str sJson = NULL;
	size_t iJsonSize = 0u;
	xvalue vCfg = NULL;
	const char* sText = NULL;
	int64 iNum = 0;

	procInitDefaultConfig(&pApp->tCfg);

	sJson = xrtFileReadAll(pApp->sConfigPath, XRT_CP_UTF8, &iJsonSize);
	if ( sJson == NULL ) {
		return false;
	}

	vCfg = xrtParseJSON(sJson, iJsonSize);
	xrtFree(sJson);
	if ( xvoIsNull(vCfg) ) {
		return false;
	}

	sText = xvoTableGetText(vCfg, "service_name", 0u);
	if ( (sText != NULL) && (*sText != '\0') ) {
		procCopyText(pApp->tCfg.sServiceName, sizeof(pApp->tCfg.sServiceName), sText, NULL);
	}

	sText = xvoTableGetText(vCfg, "bind_ip", 0u);
	if ( (sText != NULL) && (*sText != '\0') ) {
		procCopyText(pApp->tCfg.sBindIP, sizeof(pApp->tCfg.sBindIP), sText, NULL);
	}

	sText = xvoTableGetText(vCfg, "default_target", 0u);
	if ( (sText != NULL) && (*sText != '\0') ) {
		procCopyText(pApp->tCfg.sDefaultTarget, sizeof(pApp->tCfg.sDefaultTarget), sText, NULL);
	}

	iNum = xvoTableGetInt(vCfg, "port", 0u);
	if ( (iNum > 0) && (iNum < 65536) ) {
		pApp->tCfg.iPort = (uint16)iNum;
	}

	iNum = xvoTableGetInt(vCfg, "queue_capacity", 0u);
	if ( iNum >= 8 ) {
		pApp->tCfg.iQueueCapacity = (uint32)iNum;
	}

	xvoUnref(vCfg);
	return true;
}


static void procUpdateLastLogLine(AppState* pApp, const char* sText)
{
	xrtMutexLock(pApp->hStatsMutex);
	procCopyText(pApp->tStats.sLastLogLine, sizeof(pApp->tStats.sLastLogLine), sText, "");
	xrtMutexUnlock(pApp->hStatsMutex);
}


static void procWriteLog(AppState* pApp, const char* sLevel, const char* sText)
{
	char sLine[320];
	int iLen = 0;

	iLen = snprintf(
		sLine,
		sizeof(sLine),
		"[%lld] [%s] %s\n",
		(long long)xrtNow(),
		(sLevel != NULL) ? sLevel : "INFO",
		(sText != NULL) ? sText : ""
	);
	if ( iLen > 0 ) {
		(void)xrtFileAppend(pApp->sLogPath, sLine, (size_t)iLen, XRT_CP_UTF8);
		procUpdateLastLogLine(pApp, sText);
	}
}


static void procRecordProbeResult(
	AppState* pApp,
	const char* sTarget,
	int32 iExitCode,
	bool bOk,
	const void* pStdout,
	size_t iStdoutSize,
	const void* pStderr,
	size_t iStderrSize
)
{
	xrtMutexLock(pApp->hStatsMutex);
	if ( pApp->tStats.iPendingJobs > 0 ) {
		pApp->tStats.iPendingJobs--;
	}
	pApp->tStats.iProbeCount++;
	pApp->tStats.iLastExitCode = iExitCode;
	pApp->tStats.bLastProbeOk = bOk;
	pApp->tStats.tLastProbeAt = xrtNow();
	procCopyText(pApp->tStats.sLastTarget, sizeof(pApp->tStats.sLastTarget), sTarget, "");
	procCopyBytesText(pApp->tStats.sLastStdout, sizeof(pApp->tStats.sLastStdout), pStdout, iStdoutSize, "");
	procCopyBytesText(pApp->tStats.sLastStderr, sizeof(pApp->tStats.sLastStderr), pStderr, iStderrSize, "");
	xrtMutexUnlock(pApp->hStatsMutex);
}


static xvalue procBuildDashboardModel(AppState* pApp)
{
	AppStats tStats;
	xvalue vModel = NULL;
	xvalue vCards = NULL;
	xvalue vCard = NULL;
	char sListenAddr[96];
	char sProbeAt[64];
	char sExitCode[32];
	char sValueText[64];

	memset(&tStats, 0, sizeof(tStats));

	xrtMutexLock(pApp->hStatsMutex);
	tStats = pApp->tStats;
	xrtMutexUnlock(pApp->hStatsMutex);

	snprintf(sListenAddr, sizeof(sListenAddr), "%s:%u", pApp->tCfg.sBindIP, (unsigned)pApp->tCfg.iPort);
	snprintf(sProbeAt, sizeof(sProbeAt), "%lld", (long long)tStats.tLastProbeAt);
	snprintf(sExitCode, sizeof(sExitCode), "%d", (int)tStats.iLastExitCode);

	vModel = xvoCreateTable();
	if ( vModel == NULL ) {
		return NULL;
	}

	xvoTableSetBool(vModel, "ok", 0u, TRUE);
	xvoTableSetText(vModel, "service_name", 0u, pApp->tCfg.sServiceName, 0u, FALSE);
	xvoTableSetText(vModel, "listen_addr", 0u, sListenAddr, 0u, TRUE);
	xvoTableSetInt(vModel, "pending_jobs", 0u, tStats.iPendingJobs);
	xvoTableSetInt(vModel, "probe_count", 0u, tStats.iProbeCount);
	xvoTableSetText(vModel, "last_probe_target", 0u, tStats.sLastTarget, 0u, FALSE);
	xvoTableSetBool(vModel, "last_probe_ok", 0u, tStats.bLastProbeOk);
	xvoTableSetInt(vModel, "last_probe_exit_code", 0u, tStats.iLastExitCode);
	xvoTableSetText(vModel, "last_probe_at", 0u, sProbeAt, 0u, TRUE);
	xvoTableSetText(vModel, "last_stdout", 0u, tStats.sLastStdout, 0u, FALSE);
	xvoTableSetText(vModel, "last_stderr", 0u, tStats.sLastStderr, 0u, FALSE);
	xvoTableSetText(vModel, "last_log_line", 0u, tStats.sLastLogLine, 0u, FALSE);

	vCards = xvoCreateArray();
	if ( vCards != NULL ) {
		vCard = xvoCreateTable();
		if ( vCard != NULL ) {
			xvoTableSetText(vCard, "title", 0u, (str)"queue_capacity", 0u, FALSE);
			snprintf(sValueText, sizeof(sValueText), "%u", (unsigned)pApp->tCfg.iQueueCapacity);
			xvoTableSetText(vCard, "value_text", 0u, sValueText, 0u, TRUE);
			(void)xvoArrayAppendValue(vCards, vCard, TRUE);
		}

		vCard = xvoCreateTable();
		if ( vCard != NULL ) {
			xvoTableSetText(vCard, "title", 0u, (str)"probe_count", 0u, FALSE);
			snprintf(sValueText, sizeof(sValueText), "%d", (int)tStats.iProbeCount);
			xvoTableSetText(vCard, "value_text", 0u, sValueText, 0u, TRUE);
			(void)xvoArrayAppendValue(vCards, vCard, TRUE);
		}

		vCard = xvoCreateTable();
		if ( vCard != NULL ) {
			xvoTableSetText(vCard, "title", 0u, (str)"last_exit_code", 0u, FALSE);
			xvoTableSetText(vCard, "value_text", 0u, sExitCode, 0u, TRUE);
			(void)xvoArrayAppendValue(vCards, vCard, TRUE);
		}

		(void)xvoTableSetValue(vModel, "cards", 0u, vCards, TRUE);
	}

	return vModel;
}


static bool procWriteJsonResponse(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason, xvalue vPayload)
{
	str sJson = NULL;
	size_t iJsonSize = 0u;
	bool bOk = false;

	sJson = xrtStringifyJSON(vPayload, FALSE, &iJsonSize);
	if ( sJson == NULL ) {
		return false;
	}

	xrtHttpdResponseSetStatus(pResp, iStatusCode, sReason);
	(void)xrtHttpdResponseSetHeader(pResp, "Cache-Control", "no-store");
	bOk = xrtHttpdResponseSetBodyCopy(pResp, sJson, iJsonSize, "application/json; charset=utf-8");
	xrtFree(sJson);
	return bOk;
}


static bool procWriteHtmlResponse(xhttpdresponse* pResp, xtetemplate hTemplate, xvalue vPayload)
{
	char* sHtml = NULL;
	size_t iHtmlSize = 0u;
	bool bOk = false;

	sHtml = xteMake(hTemplate, vPayload, NULL, NULL, &iHtmlSize);
	if ( sHtml == NULL ) {
		return false;
	}

	xrtHttpdResponseSetStatus(pResp, 200u, "OK");
	(void)xrtHttpdResponseSetHeader(pResp, "Cache-Control", "no-store");
	bOk = xrtHttpdResponseSetBodyCopy(pResp, sHtml, iHtmlSize, "text/html; charset=utf-8");
	xrtFree(sHtml);
	return bOk;
}


static void procWriteSimpleJsonError(xhttpdresponse* pResp, uint32 iStatusCode, const char* sError)
{
	xvalue vErr = xvoCreateTable();
	char sErrorText[64];

	procCopyText(sErrorText, sizeof(sErrorText), sError, "internal");

	if ( vErr != NULL ) {
		xvoTableSetBool(vErr, "ok", 0u, FALSE);
		xvoTableSetText(vErr, "error", 0u, sErrorText, 0u, TRUE);
		if ( procWriteJsonResponse(pResp, iStatusCode, "Error", vErr) ) {
			xvoUnref(vErr);
			return;
		}
		xvoUnref(vErr);
	}

	xrtHttpdResponseSetStatus(pResp, iStatusCode, "Error");
	(void)xrtHttpdResponseSetBodyCopy(pResp, "{\"ok\":false}", sizeof("{\"ok\":false}") - 1u, "application/json; charset=utf-8");
}


static bool procWriteSnapshotFile(AppState* pApp, int32 iJobId)
{
	xvalue vSnapshot = NULL;
	str sJson = NULL;
	size_t iJsonSize = 0u;
	bool bOk = false;
	char sJobId[32];

	vSnapshot = procBuildDashboardModel(pApp);
	if ( vSnapshot == NULL ) {
		return false;
	}

	snprintf(sJobId, sizeof(sJobId), "%d", (int)iJobId);
	xvoTableSetText(vSnapshot, "job_id", 0u, sJobId, 0u, TRUE);

	sJson = xrtStringifyJSON(vSnapshot, TRUE, &iJsonSize);
	if ( sJson != NULL ) {
		bOk = (xrtFileWriteAll(pApp->sSnapshotPath, sJson, iJsonSize, XRT_CP_UTF8) >= 0);
		xrtFree(sJson);
	}

	xvoUnref(vSnapshot);
	return bOk;
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
		sError = xFutureError(pFuture);
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


static bool procRunProbeProcess(
	AppState* pApp,
	const char* sTarget,
	int32 iJobId,
	int32* pRetExitCode,
	char* sStdoutText,
	size_t iStdoutCap,
	char* sStderrText,
	size_t iStderrCap
)
{
	xprocessconfig tConfig;
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

	xrtProcessConfigInit(&tConfig);
	tConfig.iFlags =
		XPROC_F_PIPE_STDIN |
		XPROC_F_PIPE_STDOUT |
		XPROC_F_PIPE_STDERR |
		XPROC_F_KILL_TREE;
	tConfig.sProgram = xCore.AppFile;
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 1u;
	tConfig.iReadChunkSize = 4096u;
	tConfig.iMaxCaptureBytes = 64u * 1024u;

	pProcess = xrtProcessSpawn(&tConfig);
	if ( pProcess == NULL ) {
		procCopyText(sStderrText, iStderrCap, "spawn child failed", "");
		goto cleanup;
	}

	if ( xrtProcessWriteText(pProcess, (str)sInput, 0u) <= 0 ) {
		procCopyText(sStderrText, iStderrCap, "write child stdin failed", "");
		goto cleanup;
	}

	if ( !xrtProcessCloseStdin(pProcess) ) {
		procCopyText(sStderrText, iStderrCap, "close child stdin failed", "");
		goto cleanup;
	}

	pWaitFuture = xrtProcessWaitFuture(pProcess);
	if ( !procWaitFutureResolved("wait probe process", pWaitFuture, 5000) ) {
		if ( xrtProcessIsRunning(pProcess) ) {
			(void)xrtProcessKillTree(pProcess);
			(void)xrtProcessWait(pProcess);
		}
		procCopyText(sStderrText, iStderrCap, xFutureError(pWaitFuture), "wait probe process failed");
		goto cleanup;
	}

	if ( xFutureValue(pWaitFuture) != pProcess ) {
		procCopyText(sStderrText, iStderrCap, "probe future returned unexpected value", "");
		goto cleanup;
	}

	if ( pRetExitCode != NULL ) {
		*pRetExitCode = xrtProcessExitCode(pProcess);
	}

	pStdout = xrtProcessGetStdout(pProcess, &iStdoutSize);
	pStderr = xrtProcessGetStderr(pProcess, &iStderrSize);
	procCopyBytesText(sStdoutText, iStdoutCap, pStdout, iStdoutSize, "");
	procCopyBytesText(sStderrText, iStderrCap, pStderr, iStderrSize, "");
	bOk = true;

cleanup:
	if ( pWaitFuture != NULL ) {
		xFutureRelease(pWaitFuture);
		pWaitFuture = NULL;
	}
	if ( pProcess != NULL ) {
		xrtProcessDestroy(pProcess);
		pProcess = NULL;
	}
	if ( !bOk && (pRetExitCode != NULL) ) {
		*pRetExitCode = -1;
	}
	if ( !bOk ) {
		(void)procWriteSnapshotFile(pApp, iJobId);
	}
	return bOk;
}


static void procProcessProbeJob(AppState* pApp, ProbeJob* pJob)
{
	char sLog[192];
	char sStdout[256];
	char sStderr[256];
	int32 iExitCode = -1;
	bool bOk;

	memset(sStdout, 0, sizeof(sStdout));
	memset(sStderr, 0, sizeof(sStderr));

	snprintf(sLog, sizeof(sLog), "start probe job #%d target=%s", (int)pJob->iJobId, pJob->sTarget);
	procWriteLog(pApp, "INFO", sLog);

	bOk = procRunProbeProcess(
		pApp,
		pJob->sTarget,
		pJob->iJobId,
		&iExitCode,
		sStdout,
		sizeof(sStdout),
		sStderr,
		sizeof(sStderr)
	);

	if ( bOk ) {
		procRecordProbeResult(pApp, pJob->sTarget, iExitCode, (iExitCode == 0), sStdout, strlen(sStdout), sStderr, strlen(sStderr));
		(void)procWriteSnapshotFile(pApp, pJob->iJobId);
		snprintf(sLog, sizeof(sLog), "finish probe job #%d exit=%d", (int)pJob->iJobId, (int)iExitCode);
		procWriteLog(pApp, "INFO", sLog);
		return;
	}

	procRecordProbeResult(pApp, pJob->sTarget, iExitCode, false, sStdout, strlen(sStdout), sStderr, strlen(sStderr));
	(void)procWriteSnapshotFile(pApp, pJob->iJobId);
	snprintf(sLog, sizeof(sLog), "probe job #%d failed before normal completion", (int)pJob->iJobId);
	procWriteLog(pApp, "ERROR", sLog);
}


static uint32 procProbeWorker(ptr pArg)
{
	AppState* pApp = (AppState*)pArg;
	ProbeJob* pJob = NULL;
	xqueue_result iRet;

	for ( ;; ) {
		pJob = NULL;
		iRet = xrtMPSCQWaitPop(pApp->hQueue, (ptr*)&pJob);
		if ( iRet == XQUEUE_CLOSED ) {
			break;
		}
		if ( (iRet != XQUEUE_OK) || (pJob == NULL) ) {
			continue;
		}

		procProcessProbeJob(pApp, pJob);
		xrtFree(pJob);
	}

	return 0u;
}


static bool procQueueProbe(AppState* pApp, const char* sTarget, int32* pRetJobId)
{
	ProbeJob* pJob = NULL;
	xqueue_result iRet;

	pJob = (ProbeJob*)xrtMalloc(sizeof(*pJob));
	if ( pJob == NULL ) {
		return false;
	}
	memset(pJob, 0, sizeof(*pJob));

	xrtMutexLock(pApp->hStatsMutex);
	pApp->tStats.iNextJobId++;
	pJob->iJobId = pApp->tStats.iNextJobId;
	pApp->tStats.iPendingJobs++;
	xrtMutexUnlock(pApp->hStatsMutex);

	procCopyText(pJob->sTarget, sizeof(pJob->sTarget), sTarget, pApp->tCfg.sDefaultTarget);
	iRet = xrtMPSCQWaitTryPush(pApp->hQueue, pJob);
	if ( iRet != XQUEUE_OK ) {
		xrtMutexLock(pApp->hStatsMutex);
		if ( pApp->tStats.iPendingJobs > 0 ) {
			pApp->tStats.iPendingJobs--;
		}
		xrtMutexUnlock(pApp->hStatsMutex);
		xrtFree(pJob);
		return false;
	}

	if ( pRetJobId != NULL ) {
		*pRetJobId = pJob->iJobId;
	}
	return true;
}


static bool procHandleDashboardJson(AppState* pApp, xhttpdresponse* pResp)
{
	xvalue vModel = procBuildDashboardModel(pApp);
	bool bOk = false;

	if ( vModel == NULL ) {
		return false;
	}

	bOk = procWriteJsonResponse(pResp, 200u, "OK", vModel);
	xvoUnref(vModel);
	return bOk;
}


static bool procHandleDashboardHtml(AppState* pApp, xhttpdresponse* pResp)
{
	xvalue vModel = procBuildDashboardModel(pApp);
	bool bOk = false;

	if ( vModel == NULL ) {
		return false;
	}

	bOk = procWriteHtmlResponse(pResp, pApp->hDashboardTemplate, vModel);
	xvoUnref(vModel);
	return bOk;
}


static bool procHandleProbePost(AppState* pApp, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	xvalue vResp = NULL;
	const char* sTarget = NULL;
	int32 iJobId = 0;
	bool bOk = false;

	if ( strcmp(pReq->sMethod, "POST") != 0 ) {
		(void)xrtHttpdResponseSetHeader(pResp, "Allow", "POST");
		procWriteSimpleJsonError(pResp, 405u, "method_not_allowed");
		return true;
	}

	sTarget = xrtHttpdRequestHeader(pReq, "X-Probe-Target");
	if ( !procQueueProbe(pApp, sTarget, &iJobId) ) {
		procWriteSimpleJsonError(pResp, 503u, "queue_busy");
		return true;
	}

	vResp = xvoCreateTable();
	if ( vResp == NULL ) {
		procWriteSimpleJsonError(pResp, 500u, "oom");
		return true;
	}

	xvoTableSetBool(vResp, "ok", 0u, TRUE);
	xvoTableSetBool(vResp, "accepted", 0u, TRUE);
	xvoTableSetInt(vResp, "job_id", 0u, iJobId);
	xvoTableSetText(vResp, "target", 0u, (sTarget != NULL) ? (str)sTarget : (str)pApp->tCfg.sDefaultTarget, 0u, FALSE);

	xrtMutexLock(pApp->hStatsMutex);
	xvoTableSetInt(vResp, "pending_jobs", 0u, pApp->tStats.iPendingJobs);
	xrtMutexUnlock(pApp->hStatsMutex);

	bOk = procWriteJsonResponse(pResp, 202u, "Accepted", vResp);
	xvoUnref(vResp);
	if ( !bOk ) {
		procWriteSimpleJsonError(pResp, 500u, "write_response_failed");
	}
	return true;
}


static bool procOnRequest(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	AppState* pApp = (AppState*)pOwner;

	(void)pServer;
	(void)pConn;

	if ( strcmp(pReq->sPath, "/api/dashboard") == 0 ) {
		if ( strcmp(pReq->sMethod, "GET") != 0 ) {
			(void)xrtHttpdResponseSetHeader(pResp, "Allow", "GET");
			procWriteSimpleJsonError(pResp, 405u, "method_not_allowed");
			return true;
		}
		if ( !procHandleDashboardJson(pApp, pResp) ) {
			procWriteSimpleJsonError(pResp, 500u, "dashboard_json_failed");
		}
		return true;
	}

	if ( strcmp(pReq->sPath, "/dashboard") == 0 ) {
		if ( strcmp(pReq->sMethod, "GET") != 0 ) {
			procWriteSimpleJsonError(pResp, 405u, "method_not_allowed");
			return true;
		}
		if ( !procHandleDashboardHtml(pApp, pResp) ) {
			procWriteSimpleJsonError(pResp, 500u, "dashboard_html_failed");
		}
		return true;
	}

	if ( strcmp(pReq->sPath, "/api/probe") == 0 ) {
		return procHandleProbePost(pApp, pReq, pResp);
	}

	return false;
}


static bool procLoadTemplate(AppState* pApp)
{
	str sTemplate = NULL;
	size_t iTemplateSize = 0u;
	XTE_Error tError = {0};

	sTemplate = xrtFileReadAll(pApp->sTemplatePath, XRT_CP_UTF8, &iTemplateSize);
	if ( sTemplate == NULL ) {
		return false;
	}

	pApp->hTemplateEngine = xteCreateEngine();
	if ( pApp->hTemplateEngine == NULL ) {
		xrtFree(sTemplate);
		return false;
	}

	pApp->hDashboardTemplate = xteParseEx(pApp->hTemplateEngine, sTemplate, iTemplateSize, NULL, &tError);
	xrtFree(sTemplate);
	return (pApp->hDashboardTemplate != NULL);
}


static bool procStartRuntime(AppState* pApp)
{
	xqueue_config tQueueCfg = {0};
	xnetengineconfig tNetCfg;
	xhttpdconfig tHttpdCfg;
	xhttpdevents tEvents = {0};

	pApp->hStatsMutex = xrtMutexCreate();
	if ( pApp->hStatsMutex == NULL ) {
		return false;
	}

	tQueueCfg.iCapacity = pApp->tCfg.iQueueCapacity;
	pApp->hQueue = xrtMPSCQWaitCreate(&tQueueCfg);
	if ( pApp->hQueue == NULL ) {
		return false;
	}

	if ( !procLoadTemplate(pApp) ) {
		return false;
	}

	pApp->hWorker = xrtThreadCreate(procProbeWorker, pApp, 0u);
	if ( pApp->hWorker == NULL ) {
		return false;
	}

	xrtNetEngineConfigInit(&tNetCfg);
	tNetCfg.iWorkerCount = 1u;

	pApp->pNetEngine = xrtNetEngineCreate(&tNetCfg);
	if ( pApp->pNetEngine == NULL ) {
		return false;
	}
	if ( xrtNetEngineStart(pApp->pNetEngine) != XRT_NET_OK ) {
		return false;
	}

	xrtHttpdConfigInit(&tHttpdCfg);
	if ( xrtNetAddrParse(&tHttpdCfg.tBindAddr, pApp->tCfg.sBindIP, pApp->tCfg.iPort) != XRT_NET_OK ) {
		return false;
	}

	tEvents.OnRequest = procOnRequest;

	pApp->pHttpd = xrtHttpdCreate(pApp->pNetEngine, &tHttpdCfg, &tEvents, pApp);
	if ( pApp->pHttpd == NULL ) {
		return false;
	}
	if ( xrtHttpdStart(pApp->pHttpd) != XRT_NET_OK ) {
		return false;
	}

	return true;
}


static void procStopRuntime(AppState* pApp)
{
	if ( pApp->pHttpd != NULL ) {
		xrtHttpdStop(pApp->pHttpd);
		xrtHttpdDestroy(pApp->pHttpd);
		pApp->pHttpd = NULL;
	}

	if ( pApp->pNetEngine != NULL ) {
		xrtNetEngineStop(pApp->pNetEngine);
		xrtNetEngineDestroy(pApp->pNetEngine);
		pApp->pNetEngine = NULL;
	}

	if ( pApp->hQueue != NULL ) {
		xrtMPSCQWaitClose(pApp->hQueue);
	}
	if ( pApp->hWorker != NULL ) {
		xrtThreadWait(pApp->hWorker);
		xrtThreadDestroy(pApp->hWorker);
		pApp->hWorker = NULL;
	}
	if ( pApp->hQueue != NULL ) {
		xrtMPSCQWaitDestroy(pApp->hQueue);
		pApp->hQueue = NULL;
	}

	if ( pApp->hDashboardTemplate != NULL ) {
		xteDestroyTemplate(pApp->hDashboardTemplate);
		pApp->hDashboardTemplate = NULL;
	}
	if ( pApp->hTemplateEngine != NULL ) {
		xteDestroyEngine(pApp->hTemplateEngine);
		pApp->hTemplateEngine = NULL;
	}

	if ( pApp->hStatsMutex != NULL ) {
		xrtMutexDestroy(pApp->hStatsMutex);
		pApp->hStatsMutex = NULL;
	}
}


static void procFreePaths(AppState* pApp)
{
	if ( pApp->sConfigPath != NULL ) {
		xrtFree(pApp->sConfigPath);
		pApp->sConfigPath = NULL;
	}
	if ( pApp->sTemplatePath != NULL ) {
		xrtFree(pApp->sTemplatePath);
		pApp->sTemplatePath = NULL;
	}
	if ( pApp->sLogPath != NULL ) {
		xrtFree(pApp->sLogPath);
		pApp->sLogPath = NULL;
	}
	if ( pApp->sSnapshotPath != NULL ) {
		xrtFree(pApp->sSnapshotPath);
		pApp->sSnapshotPath = NULL;
	}
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


int main(int argc, char** argv)
{
	AppState tApp;

	if ( (argc > 1) && (strcmp(argv[1], "--probe-child") == 0) ) {
		xrtInit();
		return procChildProbeMain();
	}

	memset(&tApp, 0, sizeof(tApp));
	xrtInit();

	if ( !procBuildPaths(&tApp) ) {
		fprintf(stderr, "build paths failed\n");
		goto cleanup;
	}
	if ( !procEnsureBootstrapFiles(&tApp) ) {
		fprintf(stderr, "bootstrap files failed\n");
		goto cleanup;
	}
	if ( !procLoadConfig(&tApp) ) {
		fprintf(stderr, "load config failed: %s\n", tApp.sConfigPath);
		goto cleanup;
	}
	if ( !procStartRuntime(&tApp) ) {
		fprintf(stderr, "start runtime failed\n");
		goto cleanup;
	}

	procWriteLog(&tApp, "INFO", "probe dashboard started");
	(void)procQueueProbe(&tApp, tApp.tCfg.sDefaultTarget, NULL);

	printf("dashboard  : http://%s:%u/dashboard\n", tApp.tCfg.sBindIP, (unsigned)xrtHttpdBoundPort(tApp.pHttpd));
	printf("dashboard  : http://%s:%u/api/dashboard\n", tApp.tCfg.sBindIP, (unsigned)xrtHttpdBoundPort(tApp.pHttpd));
	printf("probe      : curl -X POST http://%s:%u/api/probe -H \"X-Probe-Target: demo\"\n", tApp.tCfg.sBindIP, (unsigned)xrtHttpdBoundPort(tApp.pHttpd));
	printf("fail probe : curl -X POST http://%s:%u/api/probe -H \"X-Probe-Target: fail\"\n", tApp.tCfg.sBindIP, (unsigned)xrtHttpdBoundPort(tApp.pHttpd));
	printf("press Enter to stop...\n");
	(void)getchar();

cleanup:
	procStopRuntime(&tApp);
	procFreePaths(&tApp);
	xrtUnit();
	return 0;
}
```


## 5. 默认配置和模板

第一次启动时，这个案例会自动写出下面两份文件。

### `config/probe_console.json`

```json
{
	"service_name": "xrt-probe-console",
	"bind_ip": "127.0.0.1",
	"port": 8086,
	"queue_capacity": 128,
	"default_target": "demo"
}
```

### `web/probe_dashboard.html`

```html
<!doctype html>
<html>
<head>
	<meta charset="utf-8">
	<title>{$service_name}</title>
</head>
<body>
	<h1>{$service_name}</h1>
	<p>listen={$listen_addr}</p>
	<p>pending_jobs={$pending_jobs}</p>
	<p>probe_count={$probe_count}</p>
	<p>last_target={$last_probe_target}</p>
	<p>last_ok={$last_probe_ok}</p>
	<p>last_exit_code={$last_probe_exit_code}</p>
	<p>last_probe_at={$last_probe_at}</p>
	<p>last_log={$last_log_line}</p>
	<form method="post" action="/api/probe">
		<button type="submit">probe now</button>
	</form>
	<ul>
	{#foreach:cards}
		<li><strong>{$title}</strong>: {$value_text}</li>
	{#end}
	</ul>
	<h2>stdout</h2>
	<pre>{$last_stdout}</pre>
	<h2>stderr</h2>
	<pre>{$last_stderr}</pre>
</body>
</html>
```


## 6. 为什么这样拆是稳的

### 6.1 HTTP 入口只负责“接受探测请求”

`POST /api/probe` 不直接拉外部工具。

它只做两件事：

- 决定这次探测目标
- 把任务压进 `xmpscqwait`

这样做的收益很直接：

- 请求路径短
- 浏览器不会跟着探测工具一起阻塞
- 后面要加限流、去重、批量合并时，改 worker 即可

### 6.2 子进程完成点用 future 收口，而不是靠轮询猜

当前主线里，`xrtProcessWaitFuture()` 的意义不是“更酷”，而是：

- 让“外部工具完成”成为正式等待对象
- 让 timeout 和失败状态有统一语义
- 后面如果想接 continuation、`WhenAll` 或 task group，模型也不用再换

### 6.3 stdout / stderr 仍然是运行时模型的一部分

这个案例里最近一次探测结果会同时进入 4 个出口：

- `/api/dashboard`
- `/dashboard`
- `runtime/last-probe.json`
- `runtime/probe_console.log`

关键点在于：

- 它们不是 4 套各写各的格式
- 而是同一份运行时状态被不同出口消费

### 6.4 自举型 child probe 只是教学外壳，不是设计限制

这里让父进程把自己再拉起一次，只是为了：

- 让案例自包含
- 复制后就能直接跑

你真正落地时，可以把：

- `tConfig.sProgram = xCore.AppFile`
- `arrArgs = { "--probe-child" }`

替换成你自己的：

- `ffprobe`
- `curl`
- 自研 CLI
- 任何 direct argv 形式的探测工具


## 7. 运行步骤

### Windows

```powershell
tcc .\main.c -o .\probe_console.exe -lws2_32 -lshell32 -liphlpapi
.\probe_console.exe
```

或：

```powershell
gcc .\main.c -O2 -o .\probe_console.exe -lws2_32 -lshell32 -liphlpapi
.\probe_console.exe
```

### Linux

```bash
tcc ./main.c -o ./probe_console
./probe_console
```

或：

```bash
gcc ./main.c -O2 -o ./probe_console
./probe_console
```

启动后可以这样验证：

```bash
curl http://127.0.0.1:8086/api/dashboard
curl -X POST http://127.0.0.1:8086/api/probe -H "X-Probe-Target: demo"
curl -X POST http://127.0.0.1:8086/api/probe -H "X-Probe-Target: fail"
```

浏览器页面：

```text
http://127.0.0.1:8086/dashboard
```


## 8. 这个案例里最容易犯的错误

### 8.1 在 HTTP handler 里直接 `Spawn + Wait`

这会把慢探测重新拖回请求线程。

### 8.2 超时后只 `Destroy()`，不先 `KillTree()`

`xrtProcessDestroy()` 不是 kill。

如果工具还没退出，只 destroy 句柄并不能帮你收口外部子进程。

### 8.3 误以为 `GetStdout()` / `GetStderr()` 返回值可以长期持有

当前实现和文档主线都要求按“借用缓冲区”理解：

- 生命周期受 `xprocess` 约束
- 如果要跨越更长边界，就先复制

### 8.4 让页面、日志和快照各自再拼一份探测结果

这会让字段名和状态语义很快分叉。


## 9. 下一步最自然的升级

这页继续往下扩展，最顺的是这 4 个方向：

- 用真实外部工具替换 `--probe-child`
- 给探测任务加 `future + job status` 页面，让“排队中 / 运行中 / 完成 / 失败”分开显示
- 把最近多次探测结果落到列表或 `avltree`，而不是只保留最后一次
- 把 stdout / stderr 文件化，并接上 [用 Subprocess + File Async 写一个工具链流水线](subprocess-file-async-pipeline.md) 那条异步文件主线

建议和这页一起读：

- [把配置、日志、任务、网络和模板串成一个本地控制台服务](local-console-service.md)
- [用 Subprocess + File Async 写一个工具链流水线](subprocess-file-async-pipeline.md)
- [XRT 子进程与工具执行入门](../guide/subprocess-intro.md)
