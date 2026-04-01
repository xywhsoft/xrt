#ifndef XSPIDER_H
#define XSPIDER_H

#include "xspider_html.h"

/*
	XSpider
	一个基于 XRT 的教学型网站并发爬虫 demo

	XSpider
	A teaching-oriented concurrent website crawler demo built on XRT
*/

#define XSPIDER_URL_CAP			4096
#define XSPIDER_SCHEME_CAP		16
#define XSPIDER_HOST_CAP		256
#define XSPIDER_PATH_CAP		3072
#define XSPIDER_QUERY_CAP		2048
#define XSPIDER_OUTPUT_CAP		260
#define XSPIDER_USER_AGENT_CAP	128

typedef struct xspider xspider;

typedef struct {
	uint32 iDiscoveredUrlCount;
	uint32 iVisitedPageCount;
	uint32 iSavedPageCount;
	uint32 iErrorCount;
	uint32 iRedirectCount;
	uint32 iFilteredUrlCount;
	uint32 iActiveTaskCount;
	uint32 iQueuedTaskCount;
} xspider_stats;

typedef struct {
	int iMaxDepth;
	int iMaxPages;
	int iThreadCount;
	int iCoroutinePerThread;
	int iEngineWorkerCount;
	uint32 iRequestTimeoutMs;
	uint32 iPolitenessDelayMs;
	uint32 iIdleSleepMs;
	bool bVerifyPeer;
	bool bIncludeSubdomains;
	bool bVerbose;
	char sUserAgent[XSPIDER_USER_AGENT_CAP];
	char sOutputDir[XSPIDER_OUTPUT_CAP];
} xspider_config;

typedef struct {
	const char* sUrl;
	const char* sReferer;
	const xhttpresponse* pResponse;
	int iDepth;
	int iDiscoveredLinkCount;
} xspider_page;

typedef bool (*xspider_should_visit_fn)(const char* sUrl, const char* sReferer, int iDepth, ptr pUserData);
typedef void (*xspider_on_discover_fn)(const char* sUrl, const char* sReferer, int iDepth, ptr pUserData);
typedef void (*xspider_on_page_fn)(const xspider_page* pPage, ptr pUserData);
typedef void (*xspider_on_error_fn)(const char* sUrl, const char* sReferer, int iDepth, xnet_result iStatus, const char* sStage, const char* sErrorText, ptr pUserData);

typedef struct {
	xspider_should_visit_fn procShouldVisit;
	xspider_on_discover_fn procOnDiscover;
	xspider_on_page_fn procOnPage;
	xspider_on_error_fn procOnError;
} xspider_events;

void xspiderConfigInit(xspider_config* pCfg);
xspider* xspiderCreate(const xspider_config* pCfg, const xspider_events* pEvents, ptr pUserData);
void xspiderDestroy(xspider* pSpider);
bool xspiderAddSeed(xspider* pSpider, const char* sUrl);
bool xspiderRun(xspider* pSpider);
void xspiderStop(xspider* pSpider);
xspider_stats xspiderSnapshotStats(xspider* pSpider);
void xspiderNotifyPageSaved(xspider* pSpider);

#ifdef XSPIDER_IMPLEMENTATION

typedef struct {
	char* sUrl;
	char* sReferer;
	int iDepth;
} xspider_task;

typedef struct {
	xspider* pSpider;
	int iThreadId;
	xcosched* pSched;
} xspider_worker_ctx;

typedef struct {
	xspider* pSpider;
	xspider_worker_ctx* pWorker;
	int iCoroutineId;
} xspider_coro_ctx;

typedef struct {
	xspider* pSpider;
	xspider_task* pTask;
	int iLinkCount;
} xspider_extract_ctx;

struct xspider {
	xspider_config tConfig;
	xspider_events tEvents;
	ptr pUserData;
	xnetengine* pEngine;
	xmutex pLock;
	xdict tblSeen;
	xthread* arrThreads;
	xspider_worker_ctx* arrWorkers;
	xspider_coro_ctx* arrCoroutines;
	xspider_task** arrQueue;
	int iQueueCap;
	int iQueueHead;
	int iQueueTail;
	int iQueueCount;
	int iActiveCount;
	int iScheduledCount;
	bool bStopRequested;
	bool bCompleted;
	int64 iNextRequestAtMs;
	char sRootHost[XSPIDER_HOST_CAP];
	char sRootScheme[XSPIDER_SCHEME_CAP];
	uint16 iRootPort;
	xspider_stats tStats;
};


static bool procXSpiderStartsWithNoCase(const char* sText, const char* sNeedle)
{
	size_t i = 0;
	size_t iNeedleLen;

	if ( sText == NULL || sNeedle == NULL ) {
		return false;
	}

	iNeedleLen = strlen(sNeedle);
	for ( i = 0; i < iNeedleLen; ++i ) {
		char chText = sText[i];
		char chNeedle = sNeedle[i];

		if ( chText == '\0' ) {
			return false;
		}

		if ( chText >= 'A' && chText <= 'Z' ) {
			chText = (char)(chText - 'A' + 'a');
		}
		if ( chNeedle >= 'A' && chNeedle <= 'Z' ) {
			chNeedle = (char)(chNeedle - 'A' + 'a');
		}

		if ( chText != chNeedle ) {
			return false;
		}
	}

	return true;
}


static bool procXSpiderContainsNoCase(const char* sText, const char* sNeedle)
{
	size_t i = 0;
	size_t iTextLen;
	size_t iNeedleLen;

	if ( sText == NULL || sNeedle == NULL ) {
		return false;
	}

	iTextLen = strlen(sText);
	iNeedleLen = strlen(sNeedle);
	if ( iNeedleLen == 0 || iTextLen < iNeedleLen ) {
		return false;
	}

	for ( i = 0; i + iNeedleLen <= iTextLen; ++i ) {
		if ( procXSpiderStartsWithNoCase(sText + i, sNeedle) ) {
			return true;
		}
	}

	return false;
}


static int64 procXSpiderNowMs(void)
{
	return (int64)(xrtTimer() * 1000.0);
}


static char* procXSpiderCopyN(const char* sText, size_t iLen)
{
	char* sCopy;

	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}

	memcpy(sCopy, sText, iLen);
	sCopy[iLen] = '\0';
	return sCopy;
}


static char* procXSpiderCopy(const char* sText)
{
	if ( sText == NULL ) {
		return NULL;
	}

	return xrtCopyStr((str)sText, strlen(sText) + 1u);
}


static void procXSpiderLowerAscii(char* sText)
{
	size_t i = 0;

	if ( sText == NULL ) {
		return;
	}

	for ( i = 0; sText[i] != '\0'; ++i ) {
		if ( sText[i] >= 'A' && sText[i] <= 'Z' ) {
			sText[i] = (char)(sText[i] - 'A' + 'a');
		}
	}
}


static void procXSpiderStripFragment(char* sUrl)
{
	char* sFrag;

	if ( sUrl == NULL ) {
		return;
	}

	sFrag = strchr(sUrl, '#');
	if ( sFrag != NULL ) {
		*sFrag = '\0';
	}
}


static bool procXSpiderShouldSkipScheme(const char* sUrl)
{
	if ( sUrl == NULL ) {
		return true;
	}

	return procXSpiderStartsWithNoCase(sUrl, "javascript:")
		|| procXSpiderStartsWithNoCase(sUrl, "mailto:")
		|| procXSpiderStartsWithNoCase(sUrl, "tel:")
		|| procXSpiderStartsWithNoCase(sUrl, "data:")
		|| procXSpiderStartsWithNoCase(sUrl, "file:");
}


static xspider_task* procXSpiderTaskCreate(const char* sUrl, const char* sReferer, int iDepth)
{
	xspider_task* pTask;

	pTask = (xspider_task*)xrtMalloc(sizeof(*pTask));
	if ( pTask == NULL ) {
		return NULL;
	}

	memset(pTask, 0, sizeof(*pTask));
	pTask->sUrl = procXSpiderCopy(sUrl);
	pTask->sReferer = procXSpiderCopy(sReferer);
	pTask->iDepth = iDepth;
	if ( pTask->sUrl == NULL ) {
		xrtFree(pTask->sReferer);
		xrtFree(pTask);
		return NULL;
	}

	return pTask;
}


static void procXSpiderTaskDestroy(xspider_task* pTask)
{
	if ( pTask == NULL ) {
		return;
	}

	xrtFree(pTask->sUrl);
	xrtFree(pTask->sReferer);
	xrtFree(pTask);
}


static bool procXSpiderQueuePush(xspider* pSpider, xspider_task* pTask)
{
	if ( pSpider->iQueueCount >= pSpider->iQueueCap ) {
		return false;
	}

	pSpider->arrQueue[pSpider->iQueueTail] = pTask;
	pSpider->iQueueTail = (pSpider->iQueueTail + 1) % pSpider->iQueueCap;
	pSpider->iQueueCount++;
	pSpider->tStats.iQueuedTaskCount = (uint32)pSpider->iQueueCount;
	return true;
}


static xspider_task* procXSpiderQueuePop(xspider* pSpider)
{
	xspider_task* pTask;

	if ( pSpider->iQueueCount <= 0 ) {
		return NULL;
	}

	pTask = pSpider->arrQueue[pSpider->iQueueHead];
	pSpider->arrQueue[pSpider->iQueueHead] = NULL;
	pSpider->iQueueHead = (pSpider->iQueueHead + 1) % pSpider->iQueueCap;
	pSpider->iQueueCount--;
	pSpider->tStats.iQueuedTaskCount = (uint32)pSpider->iQueueCount;
	return pTask;
}


static bool procXSpiderIsFinishedLocked(xspider* pSpider)
{
	if ( pSpider->bStopRequested ) {
		return true;
	}

	if ( pSpider->bCompleted ) {
		return true;
	}

	return pSpider->iQueueCount == 0 && pSpider->iActiveCount == 0;
}


static bool procXSpiderCanonicalizeUrl(const char* sBaseUrl, const char* sInputUrl, char* sOut, size_t iOutCap)
{
	char sTrimmed[XSPIDER_URL_CAP];
	char sResolved[XSPIDER_URL_CAP];
	char sScheme[XSPIDER_SCHEME_CAP];
	char sHost[XSPIDER_HOST_CAP];
	char sPath[XSPIDER_PATH_CAP];
	char sQuery[XSPIDER_QUERY_CAP];
	xrturlview tBase;
	xrturlview tUrl;
	size_t iInputLen = 0;
	size_t iOutLen = 0;
	uint16 iPort = 0;
	bool bHasQuery = false;
	bool bDefaultPort = false;
	const char* sWorkUrl = sInputUrl;

	if ( sOut == NULL || iOutCap == 0 || sInputUrl == NULL ) {
		return false;
	}

	memset(sOut, 0, iOutCap);
	memset(sTrimmed, 0, sizeof(sTrimmed));
	memset(sResolved, 0, sizeof(sResolved));
	memset(sScheme, 0, sizeof(sScheme));
	memset(sHost, 0, sizeof(sHost));
	memset(sPath, 0, sizeof(sPath));
	memset(sQuery, 0, sizeof(sQuery));

	while ( *sWorkUrl != '\0' && isspace((unsigned char)*sWorkUrl) ) {
		++sWorkUrl;
	}

	iInputLen = strlen(sWorkUrl);
	while ( iInputLen > 0 && isspace((unsigned char)sWorkUrl[iInputLen - 1u]) ) {
		--iInputLen;
	}

	if ( iInputLen == 0 || iInputLen >= sizeof(sTrimmed) ) {
		return false;
	}

	memcpy(sTrimmed, sWorkUrl, iInputLen);
	sTrimmed[iInputLen] = '\0';

	if ( procXSpiderShouldSkipScheme(sTrimmed) ) {
		return false;
	}

	if ( sBaseUrl != NULL ) {
		if ( !xrtUrlParseView(sBaseUrl, &tBase) ) {
			return false;
		}
		if ( !xrtUrlResolveTo(&tBase, sTrimmed, strlen(sTrimmed), sResolved, sizeof(sResolved), &iOutLen) ) {
			return false;
		}
	} else {
		if ( strlen(sTrimmed) >= sizeof(sResolved) ) {
			return false;
		}
		strcpy(sResolved, sTrimmed);
	}

	procXSpiderStripFragment(sResolved);
	if ( !xrtUrlParseView(sResolved, &tUrl) ) {
		return false;
	}

	if ( !xrtUrlViewMatchesScheme2(&tUrl, "http", "https") ) {
		return false;
	}

	if ( (tUrl.iFlags & XRT_URL_F_HAS_HOST) == 0 ) {
		return false;
	}

	if ( !xrtUrlViewCopySchemeTo(&tUrl, sScheme, sizeof(sScheme)) ) {
		return false;
	}
	if ( !xrtUrlViewCopyHostTo(&tUrl, sHost, sizeof(sHost)) ) {
		return false;
	}

	procXSpiderLowerAscii(sScheme);
	procXSpiderLowerAscii(sHost);

	if ( tUrl.iFlags & XRT_URL_F_HAS_PATH ) {
		size_t iPathLen = 0;
		if ( !xrtUrlNormalizePathTo(tUrl.tPath.sPtr, tUrl.tPath.iLen, sPath, sizeof(sPath), &iPathLen) ) {
			return false;
		}
		if ( iPathLen == 0 ) {
			strcpy(sPath, "/");
		}
	} else {
		strcpy(sPath, "/");
	}

	if ( tUrl.iFlags & XRT_URL_F_HAS_QUERY ) {
		if ( !xrtUrlViewCopyQueryTo(&tUrl, sQuery, sizeof(sQuery)) ) {
			return false;
		}
		bHasQuery = sQuery[0] != '\0';
	}

	iPort = tUrl.iPort;
	bDefaultPort = xrtUrlIsDefaultPort(&tUrl);
	if ( bDefaultPort ) {
		iPort = 0;
	}

	if ( iPort != 0 ) {
		snprintf(
			sOut,
			iOutCap,
			"%s://%s:%u%s%s%s",
			sScheme,
			sHost,
			(unsigned)iPort,
			sPath,
			bHasQuery ? "?" : "",
			bHasQuery ? sQuery : ""
		);
	} else {
		snprintf(
			sOut,
			iOutCap,
			"%s://%s%s%s%s",
			sScheme,
			sHost,
			sPath,
			bHasQuery ? "?" : "",
			bHasQuery ? sQuery : ""
		);
	}

	return sOut[0] != '\0';
}


static bool procXSpiderIsSameSite(xspider* pSpider, const char* sCanonicalUrl)
{
	xrturlview tUrl;
	char sHost[XSPIDER_HOST_CAP];
	size_t iHostLen;
	size_t iRootLen;

	if ( !xrtUrlParseView(sCanonicalUrl, &tUrl) ) {
		return false;
	}
	if ( !xrtUrlViewCopyHostTo(&tUrl, sHost, sizeof(sHost)) ) {
		return false;
	}

	procXSpiderLowerAscii(sHost);
	if ( strcmp(sHost, pSpider->sRootHost) == 0 ) {
		return true;
	}

	if ( !pSpider->tConfig.bIncludeSubdomains ) {
		return false;
	}

	iHostLen = strlen(sHost);
	iRootLen = strlen(pSpider->sRootHost);
	if ( iHostLen <= iRootLen ) {
		return false;
	}

	if ( strcmp(sHost + (iHostLen - iRootLen), pSpider->sRootHost) != 0 ) {
		return false;
	}

	return sHost[iHostLen - iRootLen - 1u] == '.';
}


static void procXSpiderReportError(
	xspider* pSpider,
	const char* sUrl,
	const char* sReferer,
	int iDepth,
	xnet_result iStatus,
	const char* sStage,
	const char* sErrorText
)
{
	xrtMutexLock(pSpider->pLock);
	pSpider->tStats.iErrorCount++;
	xrtMutexUnlock(pSpider->pLock);

	if ( pSpider->tEvents.procOnError ) {
		pSpider->tEvents.procOnError(sUrl, sReferer, iDepth, iStatus, sStage, sErrorText, pSpider->pUserData);
	}
}


static bool procXSpiderScheduleUrl(xspider* pSpider, const char* sUrl, const char* sReferer, int iDepth)
{
	char sCanonical[XSPIDER_URL_CAP];
	xspider_task* pTask;
	uint8* pMark;
	size_t iKeyLen;
	bool bNew = false;

	if ( pSpider == NULL || sUrl == NULL ) {
		return false;
	}

	if ( iDepth > pSpider->tConfig.iMaxDepth ) {
		xrtMutexLock(pSpider->pLock);
		pSpider->tStats.iFilteredUrlCount++;
		xrtMutexUnlock(pSpider->pLock);
		return false;
	}

	if ( !procXSpiderCanonicalizeUrl(sReferer, sUrl, sCanonical, sizeof(sCanonical)) ) {
		xrtMutexLock(pSpider->pLock);
		pSpider->tStats.iFilteredUrlCount++;
		xrtMutexUnlock(pSpider->pLock);
		return false;
	}

	if ( pSpider->sRootHost[0] != '\0' && !procXSpiderIsSameSite(pSpider, sCanonical) ) {
		xrtMutexLock(pSpider->pLock);
		pSpider->tStats.iFilteredUrlCount++;
		xrtMutexUnlock(pSpider->pLock);
		return false;
	}

	if ( pSpider->tEvents.procShouldVisit && !pSpider->tEvents.procShouldVisit(sCanonical, sReferer, iDepth, pSpider->pUserData) ) {
		xrtMutexLock(pSpider->pLock);
		pSpider->tStats.iFilteredUrlCount++;
		xrtMutexUnlock(pSpider->pLock);
		return false;
	}

	pTask = procXSpiderTaskCreate(sCanonical, sReferer, iDepth);
	if ( pTask == NULL ) {
		return false;
	}

	iKeyLen = strlen(sCanonical);

	xrtMutexLock(pSpider->pLock);
	if ( pSpider->bStopRequested || pSpider->iScheduledCount >= pSpider->tConfig.iMaxPages ) {
		xrtMutexUnlock(pSpider->pLock);
		procXSpiderTaskDestroy(pTask);
		return false;
	}
	if ( xrtDictExists(pSpider->tblSeen, (ptr)sCanonical, (uint32)iKeyLen) ) {
		xrtMutexUnlock(pSpider->pLock);
		procXSpiderTaskDestroy(pTask);
		return false;
	}
	pMark = (uint8*)xrtDictSet(pSpider->tblSeen, (ptr)sCanonical, (uint32)iKeyLen, &bNew);
	if ( pMark == NULL || !bNew ) {
		xrtMutexUnlock(pSpider->pLock);
		procXSpiderTaskDestroy(pTask);
		return false;
	}
	*pMark = 1u;
	if ( !procXSpiderQueuePush(pSpider, pTask) ) {
		xrtDictRemove(pSpider->tblSeen, (ptr)sCanonical, (uint32)iKeyLen);
		xrtMutexUnlock(pSpider->pLock);
		procXSpiderTaskDestroy(pTask);
		return false;
	}

	pSpider->iScheduledCount++;
	pSpider->tStats.iDiscoveredUrlCount++;
	xrtMutexUnlock(pSpider->pLock);

	if ( pSpider->tEvents.procOnDiscover ) {
		pSpider->tEvents.procOnDiscover(pTask->sUrl, pTask->sReferer, pTask->iDepth, pSpider->pUserData);
	}

	return true;
}


static xspider_task* procXSpiderAcquireTask(xspider* pSpider)
{
	xspider_task* pTask = NULL;

	xrtMutexLock(pSpider->pLock);
	if ( !procXSpiderIsFinishedLocked(pSpider) ) {
		pTask = procXSpiderQueuePop(pSpider);
		if ( pTask != NULL ) {
			pSpider->iActiveCount++;
			pSpider->tStats.iActiveTaskCount = (uint32)pSpider->iActiveCount;
		}
	}
	xrtMutexUnlock(pSpider->pLock);

	return pTask;
}


static void procXSpiderCompleteTask(xspider* pSpider)
{
	xrtMutexLock(pSpider->pLock);
	if ( pSpider->iActiveCount > 0 ) {
		pSpider->iActiveCount--;
	}
	pSpider->tStats.iActiveTaskCount = (uint32)pSpider->iActiveCount;
	if ( pSpider->iQueueCount == 0 && pSpider->iActiveCount == 0 ) {
		pSpider->bCompleted = true;
	}
	xrtMutexUnlock(pSpider->pLock);
}


static bool procXSpiderWaitPoliteness(xspider* pSpider)
{
	uint32 iDelay = pSpider->tConfig.iPolitenessDelayMs;

	if ( iDelay == 0 ) {
		return true;
	}

	for ( ;; ) {
		int64 iNowMs;
		int64 iWaitMs;

		xrtMutexLock(pSpider->pLock);
		if ( pSpider->bStopRequested ) {
			xrtMutexUnlock(pSpider->pLock);
			return false;
		}

		iNowMs = procXSpiderNowMs();
		if ( iNowMs >= pSpider->iNextRequestAtMs ) {
			pSpider->iNextRequestAtMs = iNowMs + (int64)iDelay;
			xrtMutexUnlock(pSpider->pLock);
			return true;
		}

		iWaitMs = pSpider->iNextRequestAtMs - iNowMs;
		xrtMutexUnlock(pSpider->pLock);

		if ( iWaitMs <= 0 ) {
			continue;
		}

		if ( iWaitMs > 200 ) {
			iWaitMs = 200;
		}

		xrtCoSleep((uint32)iWaitMs);
	}
}


static bool procXSpiderIsHtmlResponse(const xhttpresponse* pResp)
{
	const char* sType;

	if ( pResp == NULL ) {
		return false;
	}

	sType = xrtHttpResponseHeader(pResp, "Content-Type");
	if ( sType == NULL ) {
		return false;
	}

	return procXSpiderContainsNoCase(sType, "text/html")
		|| procXSpiderContainsNoCase(sType, "application/xhtml+xml");
}


static bool procXSpiderExtractSink(const char* sLink, size_t iLen, ptr pUserData)
{
	xspider_extract_ctx* pCtx = (xspider_extract_ctx*)pUserData;
	char* sTemp;
	bool bQueued;

	if ( pCtx == NULL || sLink == NULL || iLen == 0 ) {
		return false;
	}

	sTemp = procXSpiderCopyN(sLink, iLen);
	if ( sTemp == NULL ) {
		return false;
	}

	bQueued = procXSpiderScheduleUrl(
		pCtx->pSpider,
		sTemp,
		pCtx->pTask->sUrl,
		pCtx->pTask->iDepth + 1
	);
	if ( bQueued ) {
		pCtx->iLinkCount++;
	}

	xrtFree(sTemp);
	return bQueued;
}


static void procXSpiderFollowRedirect(xspider* pSpider, xspider_task* pTask, const xhttpresponse* pResp)
{
	const char* sLocation;

	if ( pResp == NULL ) {
		return;
	}

	if ( pResp->iStatusCode != 301u
		&& pResp->iStatusCode != 302u
		&& pResp->iStatusCode != 303u
		&& pResp->iStatusCode != 307u
		&& pResp->iStatusCode != 308u ) {
		return;
	}

	sLocation = xrtHttpResponseHeader(pResp, "Location");
	if ( sLocation == NULL || sLocation[0] == '\0' ) {
		return;
	}

	if ( procXSpiderScheduleUrl(pSpider, sLocation, pTask->sUrl, pTask->iDepth) ) {
		xrtMutexLock(pSpider->pLock);
		pSpider->tStats.iRedirectCount++;
		xrtMutexUnlock(pSpider->pLock);
	}
}


static void procXSpiderVisitTask(xspider* pSpider, xspider_task* pTask)
{
	xhttprequest tReq;
	xnetfuture* pFuture = NULL;
	xhttpresponse* pResp = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	const char* sFutureError = NULL;
	xspider_extract_ctx tExtractCtx;
	xspider_page tPage;

	if ( !procXSpiderWaitPoliteness(pSpider) ) {
		procXSpiderReportError(pSpider, pTask->sUrl, pTask->sReferer, pTask->iDepth, XRT_NET_CANCELLED, "politeness", "crawler politeness wait was interrupted");
		return;
	}

	xrtHttpRequestInit(&tReq);
	xrtHttpRequestSetMethod(&tReq, "GET");
	xrtHttpRequestSetURL(&tReq, pTask->sUrl);
	xrtHttpRequestSetTimeout(&tReq, pSpider->tConfig.iRequestTimeoutMs);
	xrtHttpRequestSetVerifyPeer(&tReq, pSpider->tConfig.bVerifyPeer);
	xrtHttpRequestSetHeader(&tReq, "User-Agent", pSpider->tConfig.sUserAgent);
	xrtHttpRequestSetHeader(&tReq, "Accept", "text/html,application/xhtml+xml;q=0.9,*/*;q=0.1");

	pFuture = xrtHttpExecuteAsync(pSpider->pEngine, &tReq);
	xrtHttpRequestUnit(&tReq);
	if ( pFuture == NULL ) {
		procXSpiderReportError(pSpider, pTask->sUrl, pTask->sReferer, pTask->iDepth, XRT_NET_ERROR, "http_execute", "xrtHttpExecuteAsync returned null");
		return;
	}

	iStatus = xrtNetFutureWaitCoTimeout(pFuture, pSpider->tConfig.iRequestTimeoutMs);
	if ( iStatus == XRT_NET_OK ) {
		pResp = (xhttpresponse*)xrtNetFutureValue(pFuture);
		if ( pResp == NULL ) {
			sFutureError = "HTTP future resolved without a response object.";
		}
	} else {
		sFutureError = xFutureError((xfuture*)pFuture);
	}

	if ( iStatus != XRT_NET_OK || pResp == NULL ) {
		procXSpiderReportError(pSpider, pTask->sUrl, pTask->sReferer, pTask->iDepth, iStatus, "http_wait", sFutureError);
		xrtNetFutureDestroy(pFuture);
		pFuture = NULL;
		return;
	}

	xrtNetFutureDestroy(pFuture);
	pFuture = NULL;

	xrtMutexLock(pSpider->pLock);
	pSpider->tStats.iVisitedPageCount++;
	xrtMutexUnlock(pSpider->pLock);

	procXSpiderFollowRedirect(pSpider, pTask, pResp);

	memset(&tExtractCtx, 0, sizeof(tExtractCtx));
	tExtractCtx.pSpider = pSpider;
	tExtractCtx.pTask = pTask;

	if ( procXSpiderIsHtmlResponse(pResp) && pResp->pBody != NULL && pResp->iBodyLen > 0 ) {
		(void)xspiderHtmlExtractLinks(
			pResp->pBody,
			pResp->iBodyLen,
			procXSpiderExtractSink,
			&tExtractCtx,
			&tExtractCtx.iLinkCount
		);
	}

	memset(&tPage, 0, sizeof(tPage));
	tPage.sUrl = pTask->sUrl;
	tPage.sReferer = pTask->sReferer;
	tPage.pResponse = pResp;
	tPage.iDepth = pTask->iDepth;
	tPage.iDiscoveredLinkCount = tExtractCtx.iLinkCount;

	if ( pSpider->tEvents.procOnPage ) {
		pSpider->tEvents.procOnPage(&tPage, pSpider->pUserData);
	}

	xrtHttpResponseDestroy(pResp);
}


static void procXSpiderWorkerCoroutine(ptr pParam)
{
	xspider_coro_ctx* pCtx = (xspider_coro_ctx*)pParam;
	xspider* pSpider = pCtx->pSpider;

	(void)pCtx;

	for ( ;; ) {
		xspider_task* pTask;
		bool bStopNow = false;

		xrtMutexLock(pSpider->pLock);
		bStopNow = procXSpiderIsFinishedLocked(pSpider);
		xrtMutexUnlock(pSpider->pLock);
		if ( bStopNow ) {
			return;
		}

		pTask = procXSpiderAcquireTask(pSpider);
		if ( pTask == NULL ) {
			xrtCoSleep(pSpider->tConfig.iIdleSleepMs);
			continue;
		}

		procXSpiderVisitTask(pSpider, pTask);
		procXSpiderCompleteTask(pSpider);
		procXSpiderTaskDestroy(pTask);
	}
}


static uint32 procXSpiderWorkerThread(ptr pParam)
{
	xspider_worker_ctx* pWorker = (xspider_worker_ctx*)pParam;
	xspider* pSpider = pWorker->pSpider;
	int i;

	pWorker->pSched = xrtCoSchedCreate();
	if ( pWorker->pSched == NULL ) {
		return 1u;
	}

	for ( i = 0; i < pSpider->tConfig.iCoroutinePerThread; ++i ) {
		xspider_coro_ctx* pCoroCtx = &pSpider->arrCoroutines[
			pWorker->iThreadId * pSpider->tConfig.iCoroutinePerThread + i
		];

		memset(pCoroCtx, 0, sizeof(*pCoroCtx));
		pCoroCtx->pSpider = pSpider;
		pCoroCtx->pWorker = pWorker;
		pCoroCtx->iCoroutineId = i;

		if ( xrtCoSchedSpawn(pWorker->pSched, procXSpiderWorkerCoroutine, pCoroCtx, XRT_CO_STACK_DEFAULT) == NULL ) {
			xrtCoSchedDestroy(pWorker->pSched);
			pWorker->pSched = NULL;
			return 2u;
		}
	}

	xrtCoSchedRun(pWorker->pSched);
	xrtCoSchedDestroy(pWorker->pSched);
	pWorker->pSched = NULL;
	return 0u;
}


void xspiderConfigInit(xspider_config* pCfg)
{
	if ( pCfg == NULL ) {
		return;
	}

	memset(pCfg, 0, sizeof(*pCfg));
	pCfg->iMaxDepth = 4;
	pCfg->iMaxPages = 256;
	pCfg->iThreadCount = 2;
	pCfg->iCoroutinePerThread = 8;
	pCfg->iEngineWorkerCount = 4;
	pCfg->iRequestTimeoutMs = 10000u;
	pCfg->iPolitenessDelayMs = 200u;
	pCfg->iIdleSleepMs = 20u;
	pCfg->bVerifyPeer = true;
	pCfg->bIncludeSubdomains = false;
	pCfg->bVerbose = true;
	strcpy(pCfg->sUserAgent, "XSpider/0.1 (+https://gitee.com/xywhsoft/xrt)");
	strcpy(pCfg->sOutputDir, "./output");
}


xspider* xspiderCreate(const xspider_config* pCfg, const xspider_events* pEvents, ptr pUserData)
{
	xspider* pSpider;

	if ( pCfg == NULL ) {
		return NULL;
	}

	pSpider = (xspider*)xrtMalloc(sizeof(*pSpider));
	if ( pSpider == NULL ) {
		return NULL;
	}

	memset(pSpider, 0, sizeof(*pSpider));
	memcpy(&pSpider->tConfig, pCfg, sizeof(*pCfg));
	if ( pEvents != NULL ) {
		memcpy(&pSpider->tEvents, pEvents, sizeof(*pEvents));
	}
	pSpider->pUserData = pUserData;
	pSpider->pLock = xrtMutexCreate();
	pSpider->tblSeen = xrtDictCreate(sizeof(uint8), XRT_OBJMODE_SHARED);
	pSpider->iQueueCap = pSpider->tConfig.iMaxPages + 32;
	if ( pSpider->iQueueCap < 64 ) {
		pSpider->iQueueCap = 64;
	}
	pSpider->arrQueue = (xspider_task**)xrtMalloc(sizeof(xspider_task*) * (size_t)pSpider->iQueueCap);
	if ( pSpider->pLock == NULL || pSpider->tblSeen == NULL || pSpider->arrQueue == NULL ) {
		xspiderDestroy(pSpider);
		return NULL;
	}

	memset(pSpider->arrQueue, 0, sizeof(xspider_task*) * (size_t)pSpider->iQueueCap);
	return pSpider;
}


void xspiderDestroy(xspider* pSpider)
{
	int i;

	if ( pSpider == NULL ) {
		return;
	}

	if ( pSpider->pEngine != NULL ) {
		xrtHttpCloseIdleConnections(pSpider->pEngine);
		xrtNetEngineStop(pSpider->pEngine);
		xrtNetEngineDestroy(pSpider->pEngine);
		pSpider->pEngine = NULL;
	}

	if ( pSpider->arrThreads != NULL ) {
		for ( i = 0; i < pSpider->tConfig.iThreadCount; ++i ) {
			if ( pSpider->arrThreads[i] != NULL ) {
				xrtThreadDestroy(pSpider->arrThreads[i]);
			}
		}
		xrtFree(pSpider->arrThreads);
	}

	if ( pSpider->arrQueue != NULL ) {
		for ( i = 0; i < pSpider->iQueueCap; ++i ) {
			if ( pSpider->arrQueue[i] != NULL ) {
				procXSpiderTaskDestroy(pSpider->arrQueue[i]);
			}
		}
		xrtFree(pSpider->arrQueue);
	}

	if ( pSpider->arrWorkers != NULL ) {
		xrtFree(pSpider->arrWorkers);
	}
	if ( pSpider->arrCoroutines != NULL ) {
		xrtFree(pSpider->arrCoroutines);
	}

	if ( pSpider->tblSeen != NULL ) {
		xrtDictDestroy(pSpider->tblSeen);
	}
	if ( pSpider->pLock != NULL ) {
		xrtMutexDestroy(pSpider->pLock);
	}

	xrtFree(pSpider);
}


bool xspiderAddSeed(xspider* pSpider, const char* sUrl)
{
	char sCanonical[XSPIDER_URL_CAP];
	xrturlview tUrl;

	if ( pSpider == NULL || sUrl == NULL ) {
		return false;
	}

	if ( !procXSpiderCanonicalizeUrl(NULL, sUrl, sCanonical, sizeof(sCanonical)) ) {
		return false;
	}
	if ( !xrtUrlParseView(sCanonical, &tUrl) ) {
		return false;
	}
	if ( !xrtUrlViewCopyHostTo(&tUrl, pSpider->sRootHost, sizeof(pSpider->sRootHost)) ) {
		return false;
	}
	if ( !xrtUrlViewCopySchemeTo(&tUrl, pSpider->sRootScheme, sizeof(pSpider->sRootScheme)) ) {
		return false;
	}

	procXSpiderLowerAscii(pSpider->sRootHost);
	procXSpiderLowerAscii(pSpider->sRootScheme);
	pSpider->iRootPort = xrtUrlIsDefaultPort(&tUrl) ? 0u : tUrl.iPort;

	return procXSpiderScheduleUrl(pSpider, sCanonical, NULL, 0);
}


bool xspiderRun(xspider* pSpider)
{
	xnetengineconfig tEngineCfg;
	bool bOk = true;
	int i;

	if ( pSpider == NULL ) {
		return false;
	}
	if ( pSpider->sRootHost[0] == '\0' ) {
		return false;
	}

	pSpider->arrThreads = (xthread*)xrtMalloc(sizeof(xthread) * (size_t)pSpider->tConfig.iThreadCount);
	pSpider->arrWorkers = (xspider_worker_ctx*)xrtMalloc(sizeof(xspider_worker_ctx) * (size_t)pSpider->tConfig.iThreadCount);
	pSpider->arrCoroutines = (xspider_coro_ctx*)xrtMalloc(
		sizeof(xspider_coro_ctx) * (size_t)(pSpider->tConfig.iThreadCount * pSpider->tConfig.iCoroutinePerThread)
	);
	if ( pSpider->arrThreads == NULL || pSpider->arrWorkers == NULL || pSpider->arrCoroutines == NULL ) {
		return false;
	}

	memset(pSpider->arrThreads, 0, sizeof(xthread) * (size_t)pSpider->tConfig.iThreadCount);
	memset(pSpider->arrWorkers, 0, sizeof(xspider_worker_ctx) * (size_t)pSpider->tConfig.iThreadCount);
	memset(
		pSpider->arrCoroutines,
		0,
		sizeof(xspider_coro_ctx) * (size_t)(pSpider->tConfig.iThreadCount * pSpider->tConfig.iCoroutinePerThread)
	);

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = (uint32)pSpider->tConfig.iEngineWorkerCount;
	if ( tEngineCfg.iWorkerCount == 0 ) {
		tEngineCfg.iWorkerCount = 1u;
	}

	pSpider->pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( pSpider->pEngine == NULL ) {
		return false;
	}
	if ( xrtNetEngineStart(pSpider->pEngine) != XRT_NET_OK ) {
		return false;
	}

	for ( i = 0; i < pSpider->tConfig.iThreadCount; ++i ) {
		xspider_worker_ctx* pWorker = &pSpider->arrWorkers[i];

		memset(pWorker, 0, sizeof(*pWorker));
		pWorker->pSpider = pSpider;
		pWorker->iThreadId = i;

		pSpider->arrThreads[i] = xrtThreadCreate(procXSpiderWorkerThread, pWorker, 0);
		if ( pSpider->arrThreads[i] == NULL ) {
			xspiderStop(pSpider);
			bOk = false;
			break;
		}
	}

	for ( i = 0; i < pSpider->tConfig.iThreadCount; ++i ) {
		if ( pSpider->arrThreads[i] != NULL ) {
			xrtThreadWait(pSpider->arrThreads[i]);
		}
	}

	xrtHttpCloseIdleConnections(pSpider->pEngine);
	xrtNetEngineStop(pSpider->pEngine);
	xrtNetEngineDestroy(pSpider->pEngine);
	pSpider->pEngine = NULL;
	return bOk;
}


void xspiderStop(xspider* pSpider)
{
	if ( pSpider == NULL ) {
		return;
	}

	xrtMutexLock(pSpider->pLock);
	pSpider->bStopRequested = true;
	xrtMutexUnlock(pSpider->pLock);
}


xspider_stats xspiderSnapshotStats(xspider* pSpider)
{
	xspider_stats tStats;

	memset(&tStats, 0, sizeof(tStats));
	if ( pSpider == NULL ) {
		return tStats;
	}

	xrtMutexLock(pSpider->pLock);
	memcpy(&tStats, &pSpider->tStats, sizeof(tStats));
	xrtMutexUnlock(pSpider->pLock);
	return tStats;
}


void xspiderNotifyPageSaved(xspider* pSpider)
{
	if ( pSpider == NULL ) {
		return;
	}

	xrtMutexLock(pSpider->pLock);
	pSpider->tStats.iSavedPageCount++;
	xrtMutexUnlock(pSpider->pLock);
}

#endif /* XSPIDER_IMPLEMENTATION */

#endif /* XSPIDER_H */
