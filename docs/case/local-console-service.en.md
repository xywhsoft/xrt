# XRT Case Study: A Local Console Service with Config, Logging, Tasks, Networking, and Templates

> This case is not about whether one module works in isolation. It targets a more realistic engineering problem: when one local service needs to load config, write logs, run background work, and expose both JSON and HTML, how do you turn those pieces into one stable mainline instead of a scattered set of helpers?

[中文](local-console-service.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you are building a local console service with five basic requirements that tend to interfere with each other:

- it must load config from disk at startup
- it must append important events to a log file while running
- it must keep consuming background refresh jobs
- it must expose an HTTP surface for scripts and browsers
- when a browser opens the service, it should be able to get either JSON or an HTML page

If these five lines are implemented independently, three failure patterns appear very quickly:

- config and runtime state drift into different data models with different field names
- background tasks and HTTP handlers share state with unclear locking and lifetime rules
- templates, JSON, logs, and snapshots all start building strings separately

The goal of this case is to pull those five lines into one clear mainline.


## 2. Why This Set of Modules

This case uses the following split:

- `path + file`
	handle config files, templates, logs, and runtime snapshots as real filesystem paths
- `json + xvalue`
	handle config parsing and the unified in-memory service model
- `thread + xmpscqwait`
	handle background refresh work instead of blocking the HTTP handler with slow work
- `xhttpd`
	handle the local HTTP server entry
- `template`
	render the same `xvalue` model into an HTML page

The important part is not that there are more modules. The important part is that each layer keeps a narrow job:

- the HTTP handler does not perform the refresh work directly
- the worker thread does not assemble HTML
- the config file does not directly define runtime objects
- JSON and HTML are both exported from the same runtime model


## 3. Smallest Practical Layout

This case assumes the program will maintain these files after startup:

```text
config/console.json
web/dashboard.html
runtime/console.log
runtime/last-refresh.json
```

Where:

- `config/console.json` is the startup config
- `web/dashboard.html` is the template read and precompiled at startup
- `runtime/console.log` is the append-only log
- `runtime/last-refresh.json` is the latest snapshot written by the background worker


## 4. Full Skeleton

The code below is intentionally self-bootstrapping:

- on first startup, it writes a default config file and a default template if they do not exist
- after startup, it creates one worker queue and one worker thread
- `POST /api/refresh` only enqueues work and does not perform the refresh inline
- `GET /api/dashboard` returns the current state as JSON
- `GET /dashboard` renders HTML from the same state model

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


## 5. What the Default Config and Template Look Like

On first startup, the program writes these two files.

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


## 6. Why This Skeleton Is Stable

What matters here is not only that it works. What matters is that the boundaries stay stable.

### 6.1 Config enters the runtime only during startup

This example intentionally does not implement live config hot-reload.

The reason is simple:

- startup config and runtime state are not the same layer
- writing a stable load, validation, and defaulting path is more important than jumping to hot reload

### 6.2 `POST /api/refresh` does not perform the refresh inline

This endpoint only does two things:

- build a task object
- push it into `xmpscqwait`

The actual refresh work runs in the worker thread, so:

- the HTTP handler stays short
- the browser is not blocked by the refresh
- later extensions such as throttling, retry, or batching have a clear insertion point

### 6.3 JSON, HTML, and snapshot files all grow from the same model

This case has three output paths:

- JSON at `/api/dashboard`
- HTML at `/dashboard`
- a snapshot file at `runtime/last-refresh.json`

They all start from the same `procBuildDashboardModel()`.

That means:

- field names stay aligned
- the page and the API do not slowly drift apart
- the worker-generated snapshot stays in the same structure as the online outputs

### 6.4 Templates are precompiled at startup, not parsed inside requests

This point is critical.

Reading and parsing the template on every request looks flexible, but it introduces:

- extra I/O on the request path
- template errors that only surface once the service is already handling traffic
- unstable page latency

The more stable mainline is:

- load the template at startup
- run `xteParseEx()` at startup
- only call `xteMake()` on the request path


## 7. Run Steps

If you place the skeleton above into your own `main.c`, you can verify it like this.

### Windows

```powershell
tcc .\main.c -o .\console.exe -lws2_32 -lshell32 -liphlpapi
.\console.exe
```

Or:

```powershell
gcc .\main.c -O2 -o .\console.exe -lws2_32 -lshell32 -liphlpapi
.\console.exe
```

### Linux

```bash
tcc ./main.c -o ./console
./console
```

Or:

```bash
gcc ./main.c -O2 -o ./console
./console
```

After startup, you can verify it directly:

```bash
curl http://127.0.0.1:8085/api/dashboard
curl -X POST http://127.0.0.1:8085/api/refresh -H "X-Refresh-Reason: manual"
```

And open in the browser:

```text
http://127.0.0.1:8085/dashboard
```


## 8. Common Mistakes in This Case

### 8.1 Putting the refresh work directly inside `OnRequest`

That looks simple at first, but it quickly turns into:

- blocked requests
- handlers that keep growing
- a later rewrite once you need queueing or throttling

### 8.2 Reading and compiling the template on every request

That pulls file I/O and template errors directly into the request path.

### 8.3 Letting logs, JSON, and HTML each invent their own fields

That runs in the short term and becomes expensive in the long term.

### 8.4 Assuming `xvoArrayAppendTable()` returns the child table object

In the current public header, `xvoArrayAppendTable()` is still:

```c
#define xvoArrayAppendTable(pArr) xvoArrayAppendValue(pArr, xvoCreateTable(), TRUE)
```

That means:

- it returns `bool`
- it does not return the newly created child table handle

If you need to fill a child table before appending, the safe pattern is exactly what this case uses:

1. call `xvoCreateTable()` first
2. then call `xvoArrayAppendValue(..., vCard, TRUE)`
3. keep using your own `vCard` handle


## 9. Natural Next Extensions

The four most natural upgrades from this skeleton are:

- add `future + job status` to `POST /api/refresh` so that "accepted" and "finished" are separate states
- add `subprocess` to the worker so the refresh path becomes a real external probe chain
- add more template partials and split the page into multiple templates
- move snapshot writing to `file_async` so the refresh path keeps removing synchronous I/O

If you want the module foundations that lead into this page, continue with:

- [Configuration System with `xvalue + json`](json-config-system.en.md)
- [Queue + Future Multi-Producer Worker](queue-worker-future.en.md)
- [Full HTTP + JSON + Template Service Chain](http-json-template-chain.en.md)
- [Rendering HTML with Template](template-render-html.en.md)
