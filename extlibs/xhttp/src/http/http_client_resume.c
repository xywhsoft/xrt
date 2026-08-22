#include "../internal/xrt_http_client_runtime.h"

#include <xrt/tls_client.h>
#include <xrt/tls_resume.h>



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)

/* 每张票据用同一块内存保存恢复对象、HTTP 路由和 host 深拷贝。 */
struct __xrt_http_client_resume {
	__xrt_http_client_resume* Previous;
	__xrt_http_client_resume* Next;
	xtlsresume* Resume;
	xstrview Host;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxy* Proxy;
	#endif
	uint16 Port;
};



/* 设置恢复缓存公共 API 错误。 */
static void __xrtHttpResumeError(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtHttpClientSetError(
		Kind,
		Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 判断缓存项与当前 TLS 路由完全一致。 */
static bool __xrtHttpResumeMatch(
	const __xrt_http_client_resume* pEntry,
	const __xrt_http_resume_route* pRoute
)
{
	return (pEntry->Port == pRoute->Port) &&
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			(pEntry->Proxy == pRoute->Proxy) &&
		#endif
		xrtHttpHostEqual(
			pEntry->Host,
			pRoute->Host
		);
}



/* 从双向 LRU 摘除一张票据。 */
static void __xrtHttpResumeDetach(
	xhttpclient* pClient,
	__xrt_http_client_resume* pEntry
)
{
	if ( pEntry->Previous != NULL ) {
		pEntry->Previous->Next = pEntry->Next;
	} else {
		pClient->ResumeHead = pEntry->Next;
	}
	if ( pEntry->Next != NULL ) {
		pEntry->Next->Previous = pEntry->Previous;
	} else {
		pClient->ResumeTail = pEntry->Previous;
	}
	pEntry->Previous = NULL;
	pEntry->Next = NULL;
	pClient->ResumeCount--;
}



/* 释放缓存项持有的代理与票据引用。 */
static void __xrtHttpResumeFree(
	__xrt_http_client_resume* pEntry
)
{
	if ( pEntry == NULL ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xrtNetProxyRelease(pEntry->Proxy);
	#endif
	xrtTlsResumeRelease(pEntry->Resume);
	xrtFree(pEntry);
}



/* 淘汰指定项并累计原因。 */
static void __xrtHttpResumeRemove(
	xhttpclient* pClient,
	__xrt_http_client_resume* pEntry,
	bool bExpired
)
{
	__xrtHttpResumeDetach(pClient, pEntry);
	if ( bExpired ) {
		pClient->ResumeExpired++;
	} else {
		pClient->ResumeEvicted++;
	}
	__xrtHttpResumeFree(pEntry);
}



/* 在持锁状态清理已经失效的墙钟票据。 */
static void __xrtHttpResumePrune(
	xhttpclient* pClient,
	xtime iNow
)
{
	__xrt_http_client_resume* pEntry =
		pClient->ResumeHead;

	while ( pEntry != NULL ) {
		__xrt_http_client_resume* pNext =
			pEntry->Next;

		if ( !xrtTlsResumeValidAt(
			pEntry->Resume,
			iNow
		) ) {
			__xrtHttpResumeRemove(
				pClient,
				pEntry,
				true
			);
		}
		pEntry = pNext;
	}
}



/*
	把一张新票据加入有界 LRU。
	缓存键必须使用 HTTP 实际验证 host；IP 地址不会出现在 TLS SNI 中。
*/
void __xrtHttpResumeStore(
	const __xrt_http_resume_route* pRoute,
	xtlsresume* pResume
)
{
	xhttpclient* pClient = pRoute->Client;
	__xrt_http_client_resume* pEntry;
	__xrt_http_client_resume* pOldest = NULL;
	bytes pHost;
	size_t iOrigin = 0;

	if ( (pClient->Config.Resume.MaxEntries == 0) ||
		(pClient->Config.Resume.MaxEntriesPerOrigin == 0) ) {
		xrtTlsResumeRelease(pResume);
		return;
	}
	if ( !xrtTlsResumeValidAt(pResume, xrtNow()) ) {
		xrtClearError();
		xrtTlsResumeRelease(pResume);
		return;
	}
	if ( (pRoute->Host.Data == NULL) ||
		(pRoute->Host.Size == 0) ||
		(pRoute->Host.Size >
		 (SIZE_MAX - sizeof(*pEntry))) ) {
		xrtTlsResumeRelease(pResume);
		return;
	}
	pEntry = (__xrt_http_client_resume*)xrtCalloc(
		1,
		sizeof(*pEntry) + pRoute->Host.Size
	);
	if ( pEntry == NULL ) {
		xrtClearError();
		(void)xrtMutexLock(&pClient->ResumeLock);
		pClient->ResumeDropped++;
		(void)xrtMutexUnlock(&pClient->ResumeLock);
		xrtTlsResumeRelease(pResume);
		return;
	}
	pEntry->Resume = pResume;
	pHost = (bytes)(pEntry + 1);
	memcpy(pHost, pRoute->Host.Data, pRoute->Host.Size);
	pEntry->Host.Data = (cstr)pHost;
	pEntry->Host.Size = pRoute->Host.Size;
	pEntry->Port = pRoute->Port;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( pRoute->Proxy != NULL ) {
			pEntry->Proxy = xrtNetProxyRetain(
				pRoute->Proxy
			);
			if ( pEntry->Proxy == NULL ) {
				xrtClearError();
				(void)xrtMutexLock(
					&pClient->ResumeLock
				);
				pClient->ResumeDropped++;
				(void)xrtMutexUnlock(
					&pClient->ResumeLock
				);
				__xrtHttpResumeFree(pEntry);
				return;
			}
		}
	#endif

	(void)xrtMutexLock(&pClient->ResumeLock);
	__xrtHttpResumePrune(pClient, xrtNow());
	for (
		__xrt_http_client_resume* pCurrent =
			pClient->ResumeHead;
		pCurrent != NULL;
		pCurrent = pCurrent->Next
	) {
		if ( __xrtHttpResumeMatch(pCurrent, pRoute) ) {
			if ( pOldest == NULL ) {
				pOldest = pCurrent;
			}
			iOrigin++;
		}
	}
	if ( (iOrigin >=
		pClient->Config.Resume.MaxEntriesPerOrigin) &&
		(pOldest != NULL) ) {
		__xrtHttpResumeRemove(
			pClient,
			pOldest,
			false
		);
	}
	while ( pClient->ResumeCount >=
		pClient->Config.Resume.MaxEntries ) {
		__xrtHttpResumeRemove(
			pClient,
			pClient->ResumeHead,
			false
		);
	}
	pEntry->Previous = pClient->ResumeTail;
	if ( pClient->ResumeTail != NULL ) {
		pClient->ResumeTail->Next = pEntry;
	} else {
		pClient->ResumeHead = pEntry;
	}
	pClient->ResumeTail = pEntry;
	pClient->ResumeCount++;
	pClient->ResumeStored++;
	(void)xrtMutexUnlock(&pClient->ResumeLock);
}



/* 初始化 HTTP Client 的恢复缓存策略。 */
XRT_API void xrtHttpResumeConfigInit(
	xhttpresumeconfig* pConfig
)
{
	const xhttpresumeconfig Config = {
		XHTTP_RESUME_ENTRIES_DEFAULT,
		XHTTP_RESUME_ORIGIN_DEFAULT
	};

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpResumeError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-resume-config",
			"HTTP resume config storage is invalid"
		);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 建立 Client 独享的恢复缓存锁。 */
bool __xrtHttpResumeInit(xhttpclient* pClient)
{
	if ( !xrtMutexInit(&pClient->ResumeLock) ) {
		return false;
	}
	pClient->ResumeReady = true;
	return true;
}



/* 最后一个 Client 引用释放全部未使用票据。 */
void __xrtHttpResumeUnit(xhttpclient* pClient)
{
	__xrt_http_client_resume* pEntry;

	if ( !pClient->ResumeReady ) {
		return;
	}
	pClient->ResumeReady = false;
	pEntry = pClient->ResumeHead;
	pClient->ResumeHead = NULL;
	pClient->ResumeTail = NULL;
	pClient->ResumeCount = 0;
	while ( pEntry != NULL ) {
		__xrt_http_client_resume* pNext =
			pEntry->Next;

		__xrtHttpResumeFree(pEntry);
		pEntry = pNext;
	}
	(void)xrtMutexUnit(&pClient->ResumeLock);
}



/* 取出最新的同路由有效票据，票据不会再次进入缓存。 */
xtlsresume* __xrtHttpResumeTake(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_resume_route Route;
	__xrt_http_client_resume* pEntry;
	xtlsresume* pResume = NULL;

	memset(&Route, 0, sizeof(Route));
	Route.Client = pClient;
	Route.Host.Data = pCall->Host;
	Route.Host.Size = strlen(pCall->Host);
	Route.Port = pCall->Port;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		Route.Proxy = pCall->Proxy;
	#endif
	if ( (pClient->Config.Resume.MaxEntries == 0) ||
		(pClient->Config.Resume.MaxEntriesPerOrigin == 0) ) {
		return NULL;
	}
	(void)xrtMutexLock(&pClient->ResumeLock);
	__xrtHttpResumePrune(pClient, xrtNow());
	pEntry = pClient->ResumeTail;
	while ( pEntry != NULL ) {
		if ( __xrtHttpResumeMatch(pEntry, &Route) ) {
			__xrtHttpResumeDetach(
				pClient,
				pEntry
			);
			pResume = pEntry->Resume;
			pEntry->Resume = NULL;
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
				xrtNetProxyRelease(pEntry->Proxy);
			#endif
			xrtFree(pEntry);
			pClient->ResumeHits++;
			break;
		}
		pEntry = pEntry->Previous;
	}
	if ( pResume == NULL ) {
		pClient->ResumeMisses++;
	}
	(void)xrtMutexUnlock(&pClient->ResumeLock);
	return pResume;
}



/* 把会话在 HTTP 终态前收到的新票据移入 Client 缓存。 */
void __xrtHttpResumeCollect(
	xhttpcall* pCall,
	xtlsstream* pStream
)
{
	__xrt_http_resume_route Route;

	memset(&Route, 0, sizeof(Route));
	Route.Client = pCall->Client;
	Route.Host.Data = pCall->Host;
	Route.Host.Size = strlen(pCall->Host);
	Route.Port = pCall->Port;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		Route.Proxy = pCall->Proxy;
	#endif
	__xrtHttpResumeCollectRoute(&Route, pStream);
}



/* 从给定 HTTP 路由的 TLS 会话接管全部待取票据。 */
void __xrtHttpResumeCollectRoute(
	const __xrt_http_resume_route* pRoute,
	xtlsstream* pStream
)
{
	xtlssession* pSession;

	if ( (pRoute == NULL) || (pStream == NULL) ) {
		return;
	}
	pSession = xrtTlsStreamSession(pStream);
	if ( pSession == NULL ) {
		xrtClearError();
		return;
	}
	while ( xrtTlsClientResumeCount(pSession) != 0 ) {
		xtlsresume* pResume =
			xrtTlsClientTakeResume(pSession);

		if ( pResume == NULL ) {
			xrtClearError();
			break;
		}
		__xrtHttpResumeStore(pRoute, pResume);
	}
}



/* 清空所有尚未使用的恢复票据。 */
XRT_API size_t xrtHttpClientResumeClear(
	xhttpclient* pClient
)
{
	__xrt_http_client_resume* pEntry;
	size_t iCleared;

	if ( pClient == NULL ) {
		__xrtHttpResumeError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"clear-http-client-resume",
			"HTTP client is null"
		);
		return 0;
	}
	(void)xrtMutexLock(&pClient->ResumeLock);
	pEntry = pClient->ResumeHead;
	iCleared = pClient->ResumeCount;
	pClient->ResumeHead = NULL;
	pClient->ResumeTail = NULL;
	pClient->ResumeCount = 0;
	(void)xrtMutexUnlock(&pClient->ResumeLock);
	while ( pEntry != NULL ) {
		__xrt_http_client_resume* pNext =
			pEntry->Next;

		__xrtHttpResumeFree(pEntry);
		pEntry = pNext;
	}
	return iCleared;
}



/* 读取恢复缓存和累计计数的一致快照。 */
XRT_API bool xrtHttpClientResumeStats(
	const xhttpclient* pClient,
	xhttpresumestats* pStats
)
{
	xhttpclient* pMutable = (xhttpclient*)pClient;
	xhttpresumestats Stats;

	if ( (pClient == NULL) ||
		!__xrtRangeValid(pStats, sizeof(Stats)) ) {
		__xrtHttpResumeError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-client-resume",
			"HTTP client and complete resume stats storage are required"
		);
		return false;
	}
	memset(&Stats, 0, sizeof(Stats));
	(void)xrtMutexLock(&pMutable->ResumeLock);
	__xrtHttpResumePrune(pMutable, xrtNow());
	Stats.Entries = pClient->ResumeCount;
	Stats.Hits = pClient->ResumeHits;
	Stats.Misses = pClient->ResumeMisses;
	Stats.Stored = pClient->ResumeStored;
	Stats.Evicted = pClient->ResumeEvicted;
	Stats.Expired = pClient->ResumeExpired;
	Stats.Dropped = pClient->ResumeDropped;
	(void)xrtMutexUnlock(&pMutable->ResumeLock);
	memcpy(pStats, &Stats, sizeof(Stats));
	return true;
}

#endif
