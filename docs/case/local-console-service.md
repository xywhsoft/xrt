# 把配置、日志、任务、网络和模板串成一个本地控制台服务

> 这个案例要解决的不是“某个模块会不会用”，而是更贴近真实工程的问题：当一个本地服务既要读配置、落日志、跑后台任务，又要同时提供 JSON API 和 HTML 页面时，怎样把这些能力接成一条稳定主线，而不是到处散落。

[返回案例索引](README.md)

---

## 1. 场景

假设你要做一个本地控制台服务，它有 5 个最基础、也最容易互相打架的需求：

- 启动时要从磁盘读取配置
- 运行中要把关键事件写到日志文件
- 后台要持续消费“刷新状态”这类任务
- 对外要暴露 HTTP 接口，给脚本和浏览器访问
- 浏览器打开时，既要能拿 JSON，也要能直接看到 HTML 页面

如果这 5 件事各写各的，通常会很快出现 3 类坏味道：

- 配置和运行状态没有统一数据模型，字段名开始分叉
- 后台任务和 HTTP 处理共享状态时，锁和生命周期变得混乱
- 模板、JSON、日志、快照文件各自拼字符串，后面谁都不敢改

这个案例的目标，就是把这 5 条线拉成一条清晰主线。


## 2. 这次为什么选这组模块

这次用到的模块分工是：

- `path + file`
	负责配置、模板、日志、运行时快照这些真实文件路径
- `json + xvalue`
	负责配置解析和统一的服务内部模型
- `thread + xmpscqwait`
	负责后台刷新任务，而不是把慢工作塞进 HTTP handler
- `xhttpd`
	负责本地 HTTP 服务入口
- `template`
	负责把同一份 `xvalue` 模型渲染成 HTML 页面

这套拆法最关键的地方不在于“模块多”，而在于每一层只负责自己该负责的事：

- HTTP handler 不直接做慢刷新
- worker 线程不直接拼 HTML
- 配置文件不直接决定运行时对象结构
- JSON 和 HTML 都从同一份运行时模型导出


## 3. 最小可落地目录

这个案例约定程序启动后会维护下面几类文件：

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/last-refresh.json
```

其中：

- `config/console.json` 是启动配置
- `web/dashboard.html` 是启动时读取并预编译的模板
- `runtime/console.log` 是追加日志
- `runtime/last-refresh.json` 是后台任务每次刷新后写出的最新快照


## 4. 完整骨架

下面这份代码故意做成“自举型”：

- 第一次启动时，如果配置和模板不存在，就自动写出默认文件
- 启动后创建 worker 队列和线程
- `POST /api/refresh` 只负责投递任务，不直接做慢工作
- `GET /api/dashboard` 返回当前状态 JSON
- `GET /dashboard` 用同一份状态模型渲染 HTML

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

static const char g_sDefaultConfigJson[] =
	"{\n"
	"\t\"service_name\": \"xrt-local-console\",\n"
	"\t\"bind_ip\": \"127.0.0.1\",\n"
	"\t\"port\": 8085,\n"
	"\t\"queue_capacity\": 128\n"
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
	"\t<p>refresh_count={$refresh_count}</p>\n"
	"\t<p>last_reason={$last_refresh_reason}</p>\n"
	"\t<p>last_refresh_at={$last_refresh_at}</p>\n"
	"\t<p>last_log={$last_log_line}</p>\n"
	"\t<form method=\"post\" action=\"/api/refresh\">\n"
	"\t\t<button type=\"submit\">refresh now</button>\n"
	"\t</form>\n"
	"\t<ul>\n"
	"\t{#foreach:cards}\n"
	"\t\t<li><strong>{$title}</strong>: {$value_text}</li>\n"
	"\t{#end}\n"
	"\t</ul>\n"
	"</body>\n"
	"</html>\n";

typedef struct
{
	char sServiceName[64];
	char sBindIP[64];
	uint16 iPort;
	uint32 iQueueCapacity;
} AppConfig;

typedef struct
{
	int32 iNextJobId;
	int32 iPendingJobs;
	int32 iRefreshCount;
	xtime tLastRefreshAt;
	char sLastReason[64];
	char sLastLogLine[160];
} AppStats;

typedef struct
{
	int32 iJobId;
	char sReason[64];
} RefreshJob;

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
		sUse = sFallback ? sFallback : "";
	}
	if ( iCap == 0u ) {
		return;
	}
	snprintf(sDst, iCap, "%s", sUse);
}


static void procInitDefaultConfig(AppConfig* pCfg)
{
	memset(pCfg, 0, sizeof(*pCfg));
	procCopyText(pCfg->sServiceName, sizeof(pCfg->sServiceName), "xrt-local-console", "");
	procCopyText(pCfg->sBindIP, sizeof(pCfg->sBindIP), "127.0.0.1", "");
	pCfg->iPort = 8085u;
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
	pApp->sConfigPath = xrtPathJoin(2u, "config", "console.json");
	pApp->sTemplatePath = xrtPathJoin(2u, "web", "dashboard.html");
	pApp->sLogPath = xrtPathJoin(2u, "runtime", "console.log");
	pApp->sSnapshotPath = xrtPathJoin(2u, "runtime", "last-refresh.json");

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
		sLevel ? sLevel : "INFO",
		sText ? sText : ""
	);
	if ( iLen > 0 ) {
		(void)xrtFileAppend(pApp->sLogPath, sLine, (size_t)iLen, XRT_CP_UTF8);
		procUpdateLastLogLine(pApp, sText ? sText : "");
	}
}


static xvalue procBuildDashboardModel(AppState* pApp)
{
	AppStats tStats;
	xvalue vModel = NULL;
	xvalue vCards = NULL;
	xvalue vCard = NULL;
	char sListenAddr[96];
	char sRefreshAt[64];
	char sValueText[64];

	memset(&tStats, 0, sizeof(tStats));

	xrtMutexLock(pApp->hStatsMutex);
	tStats = pApp->tStats;
	xrtMutexUnlock(pApp->hStatsMutex);

	snprintf(sListenAddr, sizeof(sListenAddr), "%s:%u", pApp->tCfg.sBindIP, (unsigned)pApp->tCfg.iPort);
	snprintf(sRefreshAt, sizeof(sRefreshAt), "%lld", (long long)tStats.tLastRefreshAt);

	vModel = xvoCreateTable();
	if ( vModel == NULL ) {
		return NULL;
	}

	xvoTableSetBool(vModel, "ok", 0u, TRUE);
	xvoTableSetText(vModel, "service_name", 0u, pApp->tCfg.sServiceName, 0u, FALSE);
	xvoTableSetText(vModel, "listen_addr", 0u, sListenAddr, 0u, TRUE);
	xvoTableSetInt(vModel, "pending_jobs", 0u, tStats.iPendingJobs);
	xvoTableSetInt(vModel, "refresh_count", 0u, tStats.iRefreshCount);
	xvoTableSetText(vModel, "last_refresh_reason", 0u, tStats.sLastReason, 0u, FALSE);
	xvoTableSetText(vModel, "last_refresh_at", 0u, sRefreshAt, 0u, TRUE);
	xvoTableSetText(vModel, "last_log_line", 0u, tStats.sLastLogLine, 0u, FALSE);

	vCards = xvoCreateArray();
	if ( vCards != NULL ) {
		vCard = xvoCreateTable();
		if ( vCard != NULL ) {
			xvoTableSetText(vCard, "title", 0u, "queue_capacity", 0u, FALSE);
			snprintf(sValueText, sizeof(sValueText), "%u", (unsigned)pApp->tCfg.iQueueCapacity);
			xvoTableSetText(vCard, "value_text", 0u, sValueText, 0u, TRUE);
			(void)xvoArrayAppendValue(vCards, vCard, TRUE);
		}

		vCard = xvoCreateTable();
		if ( vCard != NULL ) {
			xvoTableSetText(vCard, "title", 0u, "refresh_count", 0u, FALSE);
			snprintf(sValueText, sizeof(sValueText), "%d", (int)tStats.iRefreshCount);
			xvoTableSetText(vCard, "value_text", 0u, sValueText, 0u, TRUE);
			(void)xvoArrayAppendValue(vCards, vCard, TRUE);
		}

		vCard = xvoCreateTable();
		if ( vCard != NULL ) {
			xvoTableSetText(vCard, "title", 0u, "pending_jobs", 0u, FALSE);
			snprintf(sValueText, sizeof(sValueText), "%d", (int)tStats.iPendingJobs);
			xvoTableSetText(vCard, "value_text", 0u, sValueText, 0u, TRUE);
			(void)xvoArrayAppendValue(vCards, vCard, TRUE);
		}

		(void)xvoTableSetValue(vModel, "cards", 0u, vCards, TRUE);
	}

	return vModel;
}


static bool procWriteJsonResponse(xhttpdresponse* pResp, uint32 iStatusCode, xvalue vPayload)
{
	str sJson = NULL;
	size_t iJsonSize = 0u;
	bool bOk = false;

	sJson = xrtStringifyJSON(vPayload, FALSE, &iJsonSize);
	if ( sJson == NULL ) {
		return false;
	}

	xrtHttpdResponseSetStatus(pResp, iStatusCode, "OK");
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
		if ( procWriteJsonResponse(pResp, iStatusCode, vErr) ) {
			xvoUnref(vErr);
			return;
		}
		xvoUnref(vErr);
	}

	xrtHttpdResponseSetStatus(pResp, iStatusCode, "Error");
	(void)xrtHttpdResponseSetBodyCopy(pResp, "{\"ok\":false}", sizeof("{\"ok\":false}") - 1u, "application/json; charset=utf-8");
}


static bool procWriteSnapshotFile(AppState* pApp, int32 iJobId, const char* sReason)
{
	xvalue vSnapshot = NULL;
	str sJson = NULL;
	size_t iJsonSize = 0u;
	bool bOk = false;
	char sJobId[32];
	char sNow[64];
	char sReasonText[64];

	vSnapshot = procBuildDashboardModel(pApp);
	if ( vSnapshot == NULL ) {
		return false;
	}

	snprintf(sJobId, sizeof(sJobId), "%d", (int)iJobId);
	snprintf(sNow, sizeof(sNow), "%lld", (long long)xrtNow());
	procCopyText(sReasonText, sizeof(sReasonText), sReason, "");
	xvoTableSetText(vSnapshot, "job_id", 0u, sJobId, 0u, TRUE);
	xvoTableSetText(vSnapshot, "refresh_reason", 0u, sReasonText, 0u, TRUE);
	xvoTableSetText(vSnapshot, "snapshot_at", 0u, sNow, 0u, TRUE);

	sJson = xrtStringifyJSON(vSnapshot, TRUE, &iJsonSize);
	if ( sJson != NULL ) {
		bOk = (xrtFileWriteAll(pApp->sSnapshotPath, sJson, iJsonSize, XRT_CP_UTF8) >= 0);
		xrtFree(sJson);
	}

	xvoUnref(vSnapshot);
	return bOk;
}


static void procProcessRefreshJob(AppState* pApp, RefreshJob* pJob)
{
	char sLog[160];

	snprintf(sLog, sizeof(sLog), "start refresh job #%d (%s)", (int)pJob->iJobId, pJob->sReason);
	procWriteLog(pApp, "INFO", sLog);

	xrtSleep(150u);

	xrtMutexLock(pApp->hStatsMutex);
	if ( pApp->tStats.iPendingJobs > 0 ) {
		pApp->tStats.iPendingJobs--;
	}
	pApp->tStats.iRefreshCount++;
	pApp->tStats.tLastRefreshAt = xrtNow();
	procCopyText(pApp->tStats.sLastReason, sizeof(pApp->tStats.sLastReason), pJob->sReason, "manual");
	xrtMutexUnlock(pApp->hStatsMutex);

	(void)procWriteSnapshotFile(pApp, pJob->iJobId, pJob->sReason);

	snprintf(sLog, sizeof(sLog), "finish refresh job #%d", (int)pJob->iJobId);
	procWriteLog(pApp, "INFO", sLog);
}


static uint32 procRefreshWorker(ptr pArg)
{
	AppState* pApp = (AppState*)pArg;
	RefreshJob* pJob = NULL;
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

		procProcessRefreshJob(pApp, pJob);
		xrtFree(pJob);
	}

	return 0u;
}


static bool procQueueRefresh(AppState* pApp, const char* sReason, int32* pRetJobId)
{
	RefreshJob* pJob = NULL;
	xqueue_result iRet;

	pJob = (RefreshJob*)xrtMalloc(sizeof(*pJob));
	if ( pJob == NULL ) {
		return false;
	}
	memset(pJob, 0, sizeof(*pJob));

	xrtMutexLock(pApp->hStatsMutex);
	pApp->tStats.iNextJobId++;
	pJob->iJobId = pApp->tStats.iNextJobId;
	pApp->tStats.iPendingJobs++;
	xrtMutexUnlock(pApp->hStatsMutex);

	procCopyText(pJob->sReason, sizeof(pJob->sReason), sReason, "manual");
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

	bOk = procWriteJsonResponse(pResp, 200u, vModel);
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


static bool procHandleRefreshPost(AppState* pApp, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	xvalue vResp = NULL;
	const char* sReason = NULL;
	int32 iJobId = 0;
	bool bOk = false;

	if ( strcmp(pReq->sMethod, "POST") != 0 ) {
		(void)xrtHttpdResponseSetHeader(pResp, "Allow", "POST");
		procWriteSimpleJsonError(pResp, 405u, "method_not_allowed");
		return true;
	}

	sReason = xrtHttpdRequestHeader(pReq, "X-Refresh-Reason");
	if ( !procQueueRefresh(pApp, sReason, &iJobId) ) {
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

	xrtMutexLock(pApp->hStatsMutex);
	xvoTableSetInt(vResp, "pending_jobs", 0u, pApp->tStats.iPendingJobs);
	xrtMutexUnlock(pApp->hStatsMutex);

	bOk = procWriteJsonResponse(pResp, 202u, vResp);
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

	if ( strcmp(pReq->sPath, "/api/refresh") == 0 ) {
		return procHandleRefreshPost(pApp, pReq, pResp);
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

	pApp->hWorker = xrtThreadCreate(procRefreshWorker, pApp, 0u);
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


int main(void)
{
	AppState tApp;

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

	procWriteLog(&tApp, "INFO", "service started");
	(void)procQueueRefresh(&tApp, "startup", NULL);

	printf("dashboard  : http://%s:%u/dashboard\n", tApp.tCfg.sBindIP, (unsigned)xrtHttpdBoundPort(tApp.pHttpd));
	printf("dashboard  : http://%s:%u/api/dashboard\n", tApp.tCfg.sBindIP, (unsigned)xrtHttpdBoundPort(tApp.pHttpd));
	printf("refresh    : curl -X POST http://%s:%u/api/refresh -H \"X-Refresh-Reason: manual\"\n", tApp.tCfg.sBindIP, (unsigned)xrtHttpdBoundPort(tApp.pHttpd));
	printf("press Enter to stop...\n");
	(void)getchar();

cleanup:
	procStopRuntime(&tApp);
	procFreePaths(&tApp);
	return 0;
}
```


## 5. 默认配置和模板长什么样

上面这份程序第一次启动时，会自动写出下面两份文件。

### `config/console.json`

```json
{
	"service_name": "xrt-local-console",
	"bind_ip": "127.0.0.1",
	"port": 8085,
	"queue_capacity": 128
}
```

### `web/dashboard.html`

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
	<p>refresh_count={$refresh_count}</p>
	<p>last_reason={$last_refresh_reason}</p>
	<p>last_refresh_at={$last_refresh_at}</p>
	<p>last_log={$last_log_line}</p>
	<form method="post" action="/api/refresh">
		<button type="submit">refresh now</button>
	</form>
	<ul>
	{#foreach:cards}
		<li><strong>{$title}</strong>: {$value_text}</li>
	{#end}
	</ul>
</body>
</html>
```


## 6. 为什么这份骨架是稳的

这里真正重要的不是“功能全”，而是边界稳定。

### 6.1 配置只在启动阶段进入运行时

这个例子故意没有做“配置文件热改自动生效”。

原因是：

- 启动配置和运行时状态不是同一层
- 先把加载、校验、默认值补齐这条线写稳，比后面上热更新更重要

### 6.2 `POST /api/refresh` 不直接做刷新

这个接口只做两件事：

- 构造任务对象
- 压进 `xmpscqwait`

真正的刷新工作放在线程里做，所以：

- HTTP handler 很短
- 浏览器不会被慢刷新拖住
- 后续要加限流、重试、批量合并时，扩展点也清楚

### 6.3 JSON、HTML、快照文件都从同一份模型长出来

这个案例里有 3 个出口：

- `/api/dashboard` 的 JSON
- `/dashboard` 的 HTML
- `runtime/last-refresh.json` 的落盘快照

它们都建立在同一个 `procBuildDashboardModel()` 上。

这意味着：

- 字段名天然一致
- 页面和 API 不会慢慢分叉
- 后台任务写出的快照也和在线接口保持同一套结构

### 6.4 模板在启动时预编译，不在请求里现解析

这点非常关键。

模板文件每次请求再读、再 parse，看起来“更灵活”，实际上会带来：

- 请求路径上的额外 I/O
- 模板错误延迟到线上请求时才暴露
- 页面性能抖动

更稳的主线是：

- 启动时读取模板
- 启动时 `xteParseEx()`
- 请求里只做 `xteMake()`


## 7. 运行步骤

如果你把上面的骨架放到自己的 `main.c` 里，可以按下面方式验证：

### Windows

```powershell
tcc .\main.c -o .\console.exe -lws2_32 -lshell32 -liphlpapi
.\console.exe
```

或：

```powershell
gcc .\main.c -O2 -o .\console.exe -lws2_32 -lshell32 -liphlpapi
.\console.exe
```

### Linux

```bash
tcc ./main.c -o ./console
./console
```

或：

```bash
gcc ./main.c -O2 -o ./console
./console
```

启动后可以直接验证：

```bash
curl http://127.0.0.1:8085/api/dashboard
curl -X POST http://127.0.0.1:8085/api/refresh -H "X-Refresh-Reason: manual"
```

浏览器直接打开：

```text
http://127.0.0.1:8085/dashboard
```


## 8. 这个案例里最容易写错的地方

### 8.1 把后台刷新塞进 `OnRequest`

这样一开始看起来最省事，但很快就会遇到：

- 请求阻塞
- handler 变得越来越长
- 后面想加排队和限流时只能重拆

### 8.2 把模板文件放到请求里现读现编译

这会把文件 I/O 和模板错误都拖进请求路径。

### 8.3 让日志、JSON、HTML 各自拼一套字段

短期能跑，长期最难维护。

### 8.4 误以为 `xvoArrayAppendTable()` 会返回新表对象

当前公开头文件里，`xvoArrayAppendTable()` 的本质仍然是：

```c
#define xvoArrayAppendTable(pArr) xvoArrayAppendValue(pArr, xvoCreateTable(), TRUE)
```

也就是：

- 它返回的是 `bool`
- 不是新建出来的子表句柄

如果你需要先填子表再 append，就应该像这个案例一样：

1. 先 `xvoCreateTable()`
2. 再 `xvoArrayAppendValue(..., vCard, TRUE)`
3. 然后继续使用你手上的 `vCard`


## 9. 下一步可以怎么扩展

这份骨架往下最自然的 4 个升级方向是：

- 给 `POST /api/refresh` 增加 `future + job status`，把“已接收”和“已完成”拆开
- 给 worker 增加 `subprocess`，把刷新任务升级成真正的外部探测链
- 给 dashboard 增加更多模板 partial，把页面拆成多模板组合
- 把快照落盘改成 `file_async`，让刷新链路进一步去同步 I/O

如果你想把这一页前面的模块基础先补完整，建议继续读：

- [用 xvalue + json 构造配置系统](json-config-system.md)
- [用 Queue + Future 写一个多生产者 Worker](queue-worker-future.md)
- [一个完整的 HTTP + JSON + Template 服务链路](http-json-template-chain.md)
- [用 Template 渲染一个 HTML 页面](template-render-html.md)
