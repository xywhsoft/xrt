#include <xrt/detail/wait.h>
#include "../internal/xacme_http.h"

#if defined(XACME_FEATURE_ACME_HTTP)

#include <xrt/buffer.h>
#include <xrt/atomic.h>
#include <xrt/http1.h>
#include <xrt/net.h>
#include <xrt/tcp.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <xrt/tls_client.h>
#include <xrt/tls_stream.h>
#include <xrt/tls_verify.h>
#include <xrt/x509.h>
#include <xrt/memory.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XACME_HTTP_FIELD_MAX 100u
#define XACME_HTTP_IO_CHUNK 16384u

/* 这里只共享异常回滚的资源拥有者，不共享请求或账户状态。 */
static xatomic32 __xacmePendingLock = { 0u };
static xacmehttp* __xacmePendingHead;
static xacmehttp* __xacmePendingTail;
static size_t __xacmePendingCount;

static void xacmePendingLock(void)
{
	uint32 Expected = 0u;
	while(!xrtAtomic32CompareExchange(&__xacmePendingLock, &Expected, 1u,
		XMEMORY_ACQUIRE, XMEMORY_RELAXED))
	{
		Expected = 0u;
		xrtThreadYield();
	}
}

static void xacmePendingUnlock(void)
{
	xrtAtomic32Store(&__xacmePendingLock, 0u, XMEMORY_RELEASE);
}

/*
	传输级重试：IO/超时类失败最多 3 次尝试（500ms/1s 退避）。
	GET/HEAD 只在未收到响应时重试；其他方法仅在发送前失败时重试。
	HTTP 层语义（状态码、badNonce）由上层处理，不受影响。
	收到响应前的连接故障仍可能发生于服务端处理 POST 之后；
	写入端点的重放安全性须由调用方及 provider 的幂等/去重语义保证。
	全部尝试失败时保留首个根因。
*/
#define XACME_HTTP_RETRY_MAX 3
#define XACME_HTTP_MAX_RESPONSE_BODY (4u * 1024u * 1024u)

static bool xacmeHttpErrorRetryable(void)
{
	xerrkind Kind = xrtErrorKind(xrtGetError());
	return (Kind == XERR_IO) || (Kind == XERR_TIMEOUT);
}

static bool xacmeHttpMethodReadOnly(cstr sMethod)
{
	return (sMethod != NULL) &&
		((strcmp(sMethod, "GET") == 0) || (strcmp(sMethod, "HEAD") == 0));
}

static void xacmeHttpMarkWriteUncertain(xacmehttp* pHttp)
{
	const xerror* pCause = xrtGetError();
	pHttp->bWriteUncertain = true;
	xerror* pUncertain = xrtErrorWrap(
		pCause, xrtErrorKind(pCause), "xrt.acme.http",
		(int32)XACME_HTTP_ERROR_UNCERTAIN,
		"http write outcome unknown after request started");
	if(pUncertain != NULL)
	{
		xrtSetErrorTake(pUncertain);
	}
}

static bool xacmeHttpExchangeOnceImpl(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount, xacmehttpresponse* pResponse,
	bool* pbRequestStarted, bool* pbResponseStarted);

static bool xacmeHttpExchangeRetry(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount, xacmehttpresponse* pResponse)
{
	uint32 uAttempt;
	xerror* pFirst = NULL;
	bool bReadOnlyMethod = xacmeHttpMethodReadOnly(sMethod);
	for(uAttempt = 1u; uAttempt <= XACME_HTTP_RETRY_MAX; uAttempt++)
	{
		bool bRequestStarted = false;
		bool bResponseStarted = false;
		bool bOk = xacmeHttpExchangeOnceImpl(
			pHttp, sMethod, sUrl, sContentType, sBody, pExtraHeaders,
			iExtraCount, pResponse, &bRequestStarted, &bResponseStarted);
		if(bOk)
		{
			xrtErrorFree(pFirst);
			return true;
		}
		if(bResponseStarted || (bRequestStarted && !bReadOnlyMethod) ||
			!xacmeHttpErrorRetryable())
		{
			if(bRequestStarted && !bReadOnlyMethod)
			{
				xacmeHttpMarkWriteUncertain(pHttp);
			}
			xrtErrorFree(pFirst);
			return false;
		}
		if(pFirst == NULL)
		{
			pFirst = xrtErrorRef(xrtGetError());
		}
		if(uAttempt == XACME_HTTP_RETRY_MAX)
		{
			if(pFirst != NULL)
			{
				xrtSetErrorTake(pFirst);
			}
			return false;
		}
		if(getenv("XACME_DEBUG"))
		{
			printf("[http-retry] attempt=%u url=%.80s\n",
				(unsigned)uAttempt, sUrl);
		}
		xrtSleep((uAttempt == 1u) ? 500u : 1000u);
	}
	return false;
}

static void xacmeHttpError(
	xerrkind Kind, xacmehttperror Code, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.http", (int32)Code, sMessage);
}

void xacmeHttpDeferOwner(xacmehttp* pHttp, size_t iOwnerSize)
{
	/* 全部调用者拥有首字段为 Http 的堆对象，已拆除 HTTP 外的子资源。 */
	xrtSecureZero((uint8*)pHttp + sizeof(*pHttp), iOwnerSize - sizeof(*pHttp));
	pHttp->iPendingOwnerSize = iOwnerSize;
	xacmePendingLock();
	pHttp->pPendingNext = __xacmePendingHead;
	__xacmePendingHead = pHttp;
	if(__xacmePendingTail == NULL) __xacmePendingTail = pHttp;
	__xacmePendingCount++;
	xacmePendingUnlock();
}

bool xrtAcmeCleanupPending(int64 uTimeoutMs, size_t* piPending)
{
	xerror* pPrevious = xrtErrorRef(xrtGetError());
	xerror* pFirst = NULL;
	double Deadline = __xrtWaitAfter(uTimeoutMs);
	size_t iPending;
	bool bError = false;
	for(;;)
	{
		xacmehttp *pList, *pListTail, *pWait = NULL, *pWaitTail = NULL;
		xacmePendingLock();
		pList = __xacmePendingHead;
		pListTail = __xacmePendingTail;
		__xacmePendingHead = NULL;
		__xacmePendingTail = NULL;
		xacmePendingUnlock();
		/* 局部链表取得独占拥有权；计数仍包括这些正在处理的外壳。 */
		while(pList != NULL)
		{
			xacmehttp* pNext = pList->pPendingNext;
			xnetretireresult Result = xrtNetEngineTryDestroy(pList->pEngine);
			if(Result == XNET_RETIRE_READY)
			{
				size_t iSize = pList->iPendingOwnerSize;
				xrtSecureZero(pList, iSize);
				xrtFree(pList);
				xacmePendingLock();
				__xacmePendingCount--;
				xacmePendingUnlock();
			}
			else
			{
				if(Result == XNET_RETIRE_ERROR)
				{
					bError = true;
					if(pFirst == NULL) pFirst = xrtErrorRef(xrtGetError());
				}
				pList->pPendingNext = pWait;
				pWait = pList;
				if(pWaitTail == NULL) pWaitTail = pList;
			}
			pList = pNext;
			if(pList != NULL && (bError ||
				(uTimeoutMs != 0u && __xrtWaitExpired(Deadline))))
			{
				/* 预算耗尽或首个 ERROR 后，未处理的尾段也仍是本次的拥有者。 */
				if(pWaitTail != NULL) pWaitTail->pPendingNext = pList;
				else pWait = pList;
				pWaitTail = pListTail;
				pList = NULL;
			}
		}
		xacmePendingLock();
		if(pWait != NULL)
		{
			pWaitTail->pPendingNext = __xacmePendingHead;
			if(__xacmePendingTail == NULL) __xacmePendingTail = pWaitTail;
			__xacmePendingHead = pWait;
		}
		iPending = __xacmePendingCount;
		xacmePendingUnlock();
		if(iPending == 0u || bError || uTimeoutMs == 0u) break;
		if(__xrtWaitExpired(Deadline))
		{
			xacmeHttpError(XERR_TIMEOUT, XACME_HTTP_ERROR_TIMEOUT,
				"acme pending cleanup still has live objects");
			break;
		}
		xrtSleep(1u);
	}
	if(piPending != NULL) *piPending = iPending;
	if(pPrevious != NULL)
	{
		xrtErrorFree(pFirst);
		xrtSetErrorTake(pPrevious);
	}
	else if(pFirst != NULL) xrtSetErrorTake(pFirst);
	return iPending == 0u;
}

/* ------------------------------------------------------------------ */
/* 初始化                                                              */
/* ------------------------------------------------------------------ */

bool xacmeHttpInit(
	xacmehttp* pHttp, struct xnetengine* pBorrowedEngine,
	cstr sCaPem, int64 uTimeoutMs)
{
	xtlsverifierconfig Verify;
	xx509store* pStore = NULL;

	if(pHttp == NULL)
	{
		xacmeHttpError(
			XERR_ARGUMENT, XACME_HTTP_ERROR_ARGUMENT,
			"acme http init requires http");
		return false;
	}
	memset(pHttp, 0, sizeof(*pHttp));
	pHttp->uTimeoutMs = (uTimeoutMs != 0u) ?
		uTimeoutMs : INT64_C(30000);

	pHttp->pEngine = pBorrowedEngine;
	if(pBorrowedEngine == NULL)
	{
		size_t iPending = 0u;
		/* 异常尚未退休时暂停新增私有引擎，避免反复失败无限增加线程和内存。 */
		if(!xrtAcmeCleanupPending(0, &iPending))
		{
			xacmeHttpError(XERR_STATE, XACME_HTTP_ERROR_CONNECT,
				"acme pending cleanup must finish before creating a private engine");
			return false;
		}
	}

	pHttp->pResolver = xrtNetResolverCreate(NULL);
	if(pHttp->pResolver == NULL)
	{
		goto Failure;
	}

	xrtTlsVerifierConfigInit(&Verify);
	if((sCaPem != NULL) && (sCaPem[0] != '\0'))
	{
		size_t iAdded = 0u;
		pStore = xrtX509StoreCreate();
		if((pStore == NULL) ||
			!xrtX509StoreAddPem(
				pStore, sCaPem, strlen(sCaPem), &iAdded) || (iAdded == 0u))
		{
			xrtX509StoreFree(pStore);
			goto Failure;
		}
		Verify.Store = pStore;
	}
	else
	{
		pStore = xrtX509StoreSystem();
		if(pStore == NULL)
		{
			goto Failure;
		}
		Verify.Store = pStore;
	}
	pHttp->pVerifier = xrtTlsVerifierCreate(&Verify);
	/* 信任库已深复制进验证器。 */
	if(pStore != NULL)
	{
		xrtX509StoreFree(pStore);
	}
	if(pHttp->pVerifier == NULL)
	{
		goto Failure;
	}
	/* 配置分配均完成后才启动私有引擎；初始化失败不依赖请求的极短预算。 */
	if(pHttp->pEngine == NULL)
	{
		xnetengineconfig Engine;
		xrtNetEngineConfigInit(&Engine);
		pHttp->pEngine = xrtNetEngineCreate(&Engine);
		if(pHttp->pEngine == NULL) goto Failure;
		pHttp->bEngineOwned = true;
		if(!xrtNetEngineStart(pHttp->pEngine))
		{
			xerror* pStartError = xrtErrorRef(xrtGetError());
			/* 尚未发布，也没有连接对象；同步归还启动失败的引擎。 */
			if(xrtNetEngineDestroy(pHttp->pEngine))
			{
				pHttp->pEngine = NULL;
				pHttp->bEngineOwned = false;
			}
			if(pStartError != NULL) xrtSetErrorTake(pStartError);
			goto Failure;
		}
	}
	return true;

Failure:
	{
		xerror* pCause = xrtErrorRef(xrtGetError());
		if(pCause != NULL)
		{
			xerror* pWrapped = xrtErrorWrap(pCause, XERR_STATE, "xrt.acme.http",
				XACME_HTTP_ERROR_CONNECT, "acme http init failed");
			if(pWrapped != NULL) xrtSetErrorTake(pWrapped);
			else xrtSetError(pCause); /* OOM 时仍保留底层首因。 */
			xrtErrorFree(pCause);
		}
		else xacmeHttpError(XERR_STATE, XACME_HTTP_ERROR_CONNECT, "acme http init failed");
	}
	xacmeHttpUnit(pHttp);
	return false;
}

bool xacmeHttpUnit(xacmehttp* pHttp)
{
	bool bReady = true;
	xerror* pPrevious;
	if(pHttp == NULL)
	{
		return true;
	}
	pPrevious = xrtErrorRef(xrtGetError());
	if(pHttp->pResolver != NULL)
	{
		(void)xrtNetResolverDestroy(pHttp->pResolver);
		pHttp->pResolver = NULL;
	}
	if(pHttp->bEngineOwned && (pHttp->pEngine != NULL))
	{
		double Deadline = __xrtWaitAfter(pHttp->uTimeoutMs);
		for(;;)
		{
			xnetretireresult Result = xrtNetEngineTryDestroy(pHttp->pEngine);
			if(Result == XNET_RETIRE_READY)
			{
				pHttp->pEngine = NULL;
				pHttp->bEngineOwned = false;
				break;
			}
			if(Result == XNET_RETIRE_ERROR) { bReady = false; break; }
			if(__xrtWaitExpired(Deadline))
			{
				xacmeHttpError(XERR_TIMEOUT, XACME_HTTP_ERROR_TIMEOUT,
					"acme http engine still has live objects during cleanup");
				bReady = false;
				break;
			}
			/* Close/Abort 是异步命令；等待内部引用退休，不丢弃创建者拥有权。 */
			xrtSleep(1u);
		}
	}
	else
	{
		pHttp->pEngine = NULL;
		pHttp->bEngineOwned = false;
	}
	if(pHttp->pVerifier != NULL)
	{
		xrtTlsVerifierRelease(pHttp->pVerifier);
		pHttp->pVerifier = NULL;
	}
	if(pPrevious != NULL) xrtSetErrorTake(pPrevious);
	return bReady;
}

void xacmeHttpResponseUnit(xacmehttpresponse* pResponse)
{
	if(pResponse == NULL)
	{
		return;
	}
	xrtFree(pResponse->sLocation);
	xrtFree(pResponse->sReplayNonce);
	xrtFree(pResponse->sRetryAfter);
	xrtFree(pResponse->sLink);
	xrtFree(pResponse->sContentType);
	xrtFree(pResponse->sBody);
	memset(pResponse, 0, sizeof(*pResponse));
}

/* ------------------------------------------------------------------ */
/* URL 解析（https://host[:port]/path，ACME 全集）                      */
/* ------------------------------------------------------------------ */

typedef struct xacmeurl {
	char sHost[256];
	uint16 iPort;
	char sPath[1024];
	bool bTls;
	bool bIpLiteral;
} xacmeurl;

static bool xacmeUrlScheme(cstr sUrl, cstr sPrefix)
{
	while(*sPrefix != 0)
	{
		unsigned char c = (unsigned char)*sUrl++;
		if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
		if(c != (unsigned char)*sPrefix++) return false;
	}
	return true;
}

static bool xacmeUrlPort(cstr sPort, size_t iSize, uint16* pPort)
{
	unsigned iPort = 0u;
	size_t i;
	if(iSize == 0u) return false;
	for(i = 0u; i < iSize; i++)
	{
		unsigned iDigit;
		if(sPort[i] < '0' || sPort[i] > '9') return false;
		iDigit = (unsigned)(sPort[i] - '0');
		if(iPort > (65535u - iDigit) / 10u) return false;
		iPort = iPort * 10u + iDigit;
	}
	if(iPort == 0u) return false;
	*pPort = (uint16)iPort;
	return true;
}

static bool xacmeUrlHex(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
		(c >= 'a' && c <= 'f');
}

static bool xacmeUrlTargetChar(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') ||
		(c != 0u && strchr("-._~!$&'()*+,;=:@/?", (int)c) != NULL);
}

/* Path/query/fragment use the same ASCII grammar here; validate before discarding a fragment. */
static bool xacmeUrlComponent(cstr sText, size_t iSize)
{
	size_t i;
	for(i = 0u; i < iSize; i++)
	{
		unsigned char c = (unsigned char)sText[i];
		if(c == '%')
		{
			if(iSize - i < 3u || !xacmeUrlHex((unsigned char)sText[i + 1u]) ||
				!xacmeUrlHex((unsigned char)sText[i + 2u])) return false;
			i += 2u;
		}
		else if(!xacmeUrlTargetChar(c)) return false;
	}
	return true;
}

/* Legacy IPv4 components may mix decimal/octal and 0x hexadecimal forms. */
static bool xacmeUrlNumericHost(cstr sHost, size_t iSize)
{
	size_t i = 0u;
	while(i < iSize)
	{
		size_t iStart = i;
		bool bHex = iSize - i >= 2u && sHost[i] == '0' &&
			(sHost[i + 1u] == 'x' || sHost[i + 1u] == 'X');
		if(bHex) i += 2u;
		iStart = i;
		while(i < iSize && sHost[i] != '.')
		{
			unsigned char c = (unsigned char)sHost[i];
			if(bHex ? !xacmeUrlHex(c) : (c < '0' || c > '9')) return false;
			i++;
		}
		if(i == iStart) return false;
		if(i < iSize) i++; /* A root dot does not turn an address into a DNS identity. */
	}
	return true;
}

static bool xacmeUrlParse(cstr sUrl, xacmeurl* pOut)
{
	const char *sHost, *sTail, *sPort;
	size_t iAuthority, iHost, iTarget, iPrefix, i;
	bool bNumeric = true;

	memset(pOut, 0, sizeof(*pOut));
	if(sUrl == NULL) return false;
	if(xacmeUrlScheme(sUrl, "https://"))
	{
		pOut->bTls = true;
		sHost = sUrl + 8;
	}
	else if(xacmeUrlScheme(sUrl, "http://"))
	{
		sHost = sUrl + 7;
	}
	else return false;
	pOut->iPort = pOut->bTls ? 443u : 80u;
	/* Authority 在 /、? 或 # 前结束；完整检查 userinfo，不能被端口截断掩盖。 */
	iAuthority = strcspn(sHost, "/?#");
	if(iAuthority == 0u || memchr(sHost, '@', iAuthority) != NULL) return false;
	sTail = sHost + iAuthority;
	iPrefix = *sTail == '/' ? 0u : 1u;
	iTarget = strcspn(sTail, "#");
	if(iTarget > sizeof(pOut->sPath) - 1u - iPrefix) return false;
	if(!xacmeUrlComponent(sTail, iTarget)) return false;
	if(sTail[iTarget] == '#' &&
		!xacmeUrlComponent(sTail + iTarget + 1u, strlen(sTail + iTarget + 1u))) return false;
	if(iPrefix != 0u) pOut->sPath[0] = '/';
	memcpy(pOut->sPath + iPrefix, sTail, iTarget);
	pOut->sPath[iPrefix + iTarget] = 0;
	if(sHost[0] == '[')
	{
		const char* sClose = (const char*)memchr(sHost, ']', iAuthority);
		xnetaddr Address;
		bool bValid;
		if(sClose == NULL) return false;
		iHost = (size_t)(sClose - sHost) + 1u;
		if(iHost <= 2u || iHost >= sizeof(pOut->sHost)) return false;
		for(i = 1u; i + 1u < iHost; i++)
		{
			unsigned char c = (unsigned char)sHost[i];
			if(!xacmeUrlHex(c) && c != ':' && c != '.') return false;
		}
		memcpy(pOut->sHost, sHost, iHost);
		pOut->sHost[iHost] = 0;
		if(iHost < iAuthority)
		{
			if(sHost[iHost] != ':' || !xacmeUrlPort(sHost + iHost + 1u,
				iAuthority - iHost - 1u, &pOut->iPort)) return false;
		}
		/* Host 保留方括号；数值解析与证书校验不带括号，不支持 scope/IPvFuture。 */
		pOut->sHost[iHost - 1u] = 0;
		bValid = xrtNetAddrParse(&Address, pOut->sHost + 1u, pOut->iPort) &&
			Address.Family == XNET_FAMILY_IPV6;
		pOut->sHost[iHost - 1u] = ']';
		if(!bValid) return false;
		pOut->bIpLiteral = true;
		return true;
	}
	sPort = (const char*)memchr(sHost, ':', iAuthority);
	iHost = iAuthority;
	if(sPort != NULL)
	{
		if(!xacmeUrlPort(sPort + 1u, iAuthority - (size_t)(sPort - sHost) - 1u,
			&pOut->iPort)) return false;
		iHost = (size_t)(sPort - sHost);
	}
	if(iHost == 0u || iHost >= sizeof(pOut->sHost) || sHost[0] == '.' ||
		(iHost > 1u && sHost[iHost - 1u] == '.' && sHost[iHost - 2u] == '.')) return false;
	for(i = 0u; i < iHost; i++)
	{
		unsigned char c = (unsigned char)sHost[i];
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_')) return false;
		bNumeric = bNumeric && ((c >= '0' && c <= '9') || c == '.');
	}
	memcpy(pOut->sHost, sHost, iHost);
	if(bNumeric)
	{
		xnetaddr Address;
		/* 拒绝缩写、前导零和纯整数，避免宿主解析器把 DNS 名转成旧式 IP。 */
		if(!xrtNetAddrParse(&Address, pOut->sHost, pOut->iPort) ||
			Address.Family != XNET_FAMILY_IPV4) return false;
		pOut->bIpLiteral = true;
	}
	else if(xacmeUrlNumericHost(sHost, iHost)) return false;
	return true;
}

static void xacmeUrlTlsNames(const xacmeurl* pUrl,
	xtlsclientconfig* pTls, xtlsdialconfig* pDial)
{
	const char* sName = pUrl->sHost;
	size_t iSize = strlen(sName);
	if(pUrl->bIpLiteral && sName[0] == '[') { sName++; iSize -= 2u; }
	else if(!pUrl->bIpLiteral && iSize > 0u && sName[iSize - 1u] == '.') iSize--;
	pTls->VerifyName = (xstrview){ sName, iSize };
	pTls->ServerName = pUrl->bIpLiteral ? (xstrview){ NULL, 0u } : pTls->VerifyName;
	/* 空 SNI 不能由 Dial 从 IP 主机自动补回。 */
	pDial->ServerNameFromHost = false;
}

/* ------------------------------------------------------------------ */
/* 流封装：任意线程安全的 future 化 IO                                   */
/* ------------------------------------------------------------------ */

typedef struct xacmestream {
	xtlsstream* pTls;
	xnetstream* pTcp;
} xacmestream;

static bool xacmeFutureResolved(xfuture* pFuture, uint64 uUs)
{
	return uUs != 0u &&
		xrtFutureWaitFor(pFuture, uUs) == XWAIT_OK &&
		xrtFutureState(pFuture) == XFUTURE_RESOLVED;
}

static bool xacmeFutureWait(xfuture* pFuture, uint64 uUs)
{
	bool bResolved = xacmeFutureResolved(pFuture, uUs);
	xrtFutureDestroy(pFuture);
	return bResolved;
}

/* 发送全部字节；TLS 用 SendAsync（任意线程），明文用复制语义 Send。 */
static bool xacmeStreamSendAll(
	xacmestream* pStream, const void* pData, size_t iSize, uint64 uUs)
{
	size_t iOffset = 0u;
	double Deadline = __xrtWaitAfter(uUs);
	while(iOffset < iSize)
	{
		size_t iChunk = iSize - iOffset;
		if(__xrtWaitExpired(Deadline))
		{
			return false;
		}
		if(iChunk > XACME_HTTP_IO_CHUNK)
		{
			iChunk = XACME_HTTP_IO_CHUNK;
		}
		if(pStream->pTls != NULL)
		{
			xfuture* pFuture = xrtTlsStreamSendAsync(
				pStream->pTls, (const uint8*)pData + iOffset, iChunk);
			if((pFuture == NULL) ||
				!xacmeFutureWait(pFuture, __xrtWaitRemaining(Deadline)))
			{
				return false;
			}
		}
		else
		{
			xnetresult eResult;
			xfuture* pFuture;
			while((eResult = xrtNetStreamSend(
				pStream->pTcp, (const uint8*)pData + iOffset, iChunk))
				== XNET_RESULT_AGAIN)
			{
				if(__xrtWaitExpired(Deadline))
				{
					return false;
				}
				pFuture = xrtNetStreamWaitAsync(
					pStream->pTcp, XNET_STREAM_WAIT_WRITE);
				if((pFuture == NULL) ||
					!xacmeFutureWait(pFuture, __xrtWaitRemaining(Deadline)))
				{
					return false;
				}
			}
			if(eResult != XNET_RESULT_OK)
			{
				return false;
			}
		}
		iOffset += iChunk;
	}
	return true;
}

/* 返回 1=读到数据，0=流结束，-1=超时，-2=I/O 失败。 */
static int xacmeStreamRecv(
	xacmestream* pStream, uint8* pBuffer, size_t iCapacity,
	size_t* pRead, double Deadline)
{
	xfuture* pFuture;
	xnetbytes* pBytes;
	xwaitresult eWait;
	xbytesview View;
	int64 uRemaining = __xrtWaitRemaining(Deadline);
	if(uRemaining == 0u) return -1;
	if(pStream->pTls != NULL)
		pFuture = xrtTlsStreamRecvAsync(pStream->pTls, iCapacity);
	else
		pFuture = xrtNetStreamRecvAsync(pStream->pTcp, iCapacity);
	if(pFuture == NULL) return -2;
	eWait = xrtFutureWaitFor(pFuture, uRemaining);
	if(eWait == XWAIT_OK && xrtFutureState(pFuture) == XFUTURE_CLOSED)
	{
		xrtFutureDestroy(pFuture);
		/* CLOSED 也可来自终止路径，必须确认正常读端结束。 */
		if(pStream->pTls != NULL)
		{
			bool bEnd;
			pFuture = xrtTlsStreamWaitAsync(pStream->pTls, XTLS_STREAM_WAIT_END);
			if(pFuture == NULL) return -2;
			eWait = xrtFutureWaitFor(pFuture, __xrtWaitRemaining(Deadline));
			bEnd = eWait == XWAIT_OK && xrtFutureState(pFuture) == XFUTURE_RESOLVED;
			xrtFutureDestroy(pFuture);
			return bEnd ? 0 : (eWait == XWAIT_TIMEOUT ? -1 : -2);
		}
		else
		{
			xnetstreamstats Stats;
			return xrtNetStreamStats(pStream->pTcp, &Stats) &&
				Stats.ReadEnded && xrtNetStreamError(pStream->pTcp) == NULL ? 0 : -2;
		}
	}
	if(eWait != XWAIT_OK || xrtFutureState(pFuture) != XFUTURE_RESOLVED)
	{
		xrtFutureDestroy(pFuture);
		return eWait == XWAIT_TIMEOUT ? -1 : -2;
	}
	pBytes = (xnetbytes*)xrtFutureValue(pFuture);
	if(pBytes == NULL)
	{
		xrtFutureDestroy(pFuture);
		return -2;
	}
	View = xrtNetBytesView(pBytes);
	if(View.Size == 0u || View.Size > iCapacity)
	{
		xrtFutureDestroy(pFuture);
		return -2;
	}
	memcpy(pBuffer, View.Data, View.Size);
	*pRead = View.Size;
	xrtFutureDestroy(pFuture);
	return 1;
}

static void xacmeStreamClose(xacmestream* pStream, bool bAbort)
{
	if(pStream->pTls != NULL)
	{
		if(bAbort) (void)xrtTlsStreamAbort(pStream->pTls);
		else (void)xrtTlsStreamClose(pStream->pTls);
		xrtTlsStreamDestroy(pStream->pTls);
		pStream->pTls = NULL;
	}
	if(pStream->pTcp != NULL)
	{
		if(bAbort) (void)xrtNetStreamAbort(pStream->pTcp);
		else (void)xrtNetStreamClose(pStream->pTcp);
		xrtNetStreamDestroy(pStream->pTcp);
		pStream->pTcp = NULL;
	}
}

/* ------------------------------------------------------------------ */
/* 交换                                                               */
/* ------------------------------------------------------------------ */

static bool xacmeHeaderTake(const xhttp1head* pHead, cstr sName, str* psValue)
{
	{
		const xhttpfield* pField = xrtHttpFieldGet(
			pHead->Fields, pHead->FieldCount,
			(xstrview){ sName, strlen(sName) });
		str sValue;
		*psValue = NULL;
		if((pField == NULL) || (pField->Value.Data == NULL))
		{
			return true;
		}
		sValue = (str)xrtMalloc(pField->Value.Size + 1u);
		if(sValue == NULL)
		{
			return false;
		}
		memcpy(sValue, pField->Value.Data, pField->Value.Size);
		sValue[pField->Value.Size] = '\0';
		*psValue = sValue;
		return true;
	}
}

/* Link is a list field: preserve every field line in wire order (RFC 9110
 * section 5.3). Other copied fields retain their individual semantics. */
static bool xacmeHeaderTakeLinks(const xhttp1head* pHead, str* psValue)
{
	size_t i, iSize = 1u, iOffset = 0u, iCount = 0u;
	str sValue;
	*psValue = NULL;
	for(i = 0u; i < pHead->FieldCount; i++)
	{
		const xhttpfield* pField = &pHead->Fields[i];
		if(!xrtHttpFieldNameEqual(pField->Name, XRT_STR_LITERAL("Link")) ||
			(pField->Value.Data == NULL)) continue;
		if(iCount != 0u)
		{
			if(iSize > SIZE_MAX - 2u) goto TooLarge;
			iSize += 2u;
		}
		if(pField->Value.Size > SIZE_MAX - iSize) goto TooLarge;
		iSize += pField->Value.Size;
		iCount++;
	}
	if(iCount == 0u) return true;
	sValue = (str)xrtMalloc(iSize);
	if(sValue == NULL) return false;
	for(i = 0u; i < pHead->FieldCount; i++)
	{
		const xhttpfield* pField = &pHead->Fields[i];
		if(!xrtHttpFieldNameEqual(pField->Name, XRT_STR_LITERAL("Link")) ||
			(pField->Value.Data == NULL)) continue;
		memcpy(sValue + iOffset, pField->Value.Data, pField->Value.Size);
		iOffset += pField->Value.Size;
		if(--iCount != 0u)
		{
			memcpy(sValue + iOffset, ", ", 2u);
			iOffset += 2u;
		}
	}
	sValue[iOffset] = '\0';
	*psValue = sValue;
	return true;

TooLarge:
	xacmeHttpError(XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
		"acme http Link response header too large");
	return false;
}

static bool xacmeHttpHeaderOwned(cstr sName)
{
	static const xhttpfield Owned[] = {
		{ { "Host", 4u }, { NULL, 0u } },
		{ { "User-Agent", 10u }, { NULL, 0u } },
		{ { "Accept", 6u }, { NULL, 0u } },
		{ { "Connection", 10u }, { NULL, 0u } },
		{ { "Content-Type", 12u }, { NULL, 0u } },
		{ { "Content-Length", 14u }, { NULL, 0u } },
		{ { "Transfer-Encoding", 17u }, { NULL, 0u } }
	};
	return xrtHttpFieldGet(Owned, sizeof(Owned) / sizeof(Owned[0]),
		(xstrview){ sName, strlen(sName) }) != NULL;
}

bool xacmeHttpExchange(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, xacmehttpresponse* pResponse)
{
	return xacmeHttpExchangeV(
		pHttp, sMethod, sUrl, sContentType, sBody, NULL, 0u, pResponse);
}

bool xacmeHttpExchangeV(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount, xacmehttpresponse* pResponse)
{
	if(pHttp != NULL) pHttp->bWriteUncertain = false;
	return xacmeHttpExchangeRetry(
		pHttp, sMethod, sUrl, sContentType, sBody, pExtraHeaders,
		iExtraCount, pResponse);
}

bool xacmeHttpExchangeOnceV(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount, xacmehttpresponse* pResponse)
{
	bool bRequestStarted = false;
	if(pHttp != NULL) pHttp->bWriteUncertain = false;
	bool bOk = xacmeHttpExchangeOnceImpl(
		pHttp, sMethod, sUrl, sContentType, sBody, pExtraHeaders,
		iExtraCount, pResponse, &bRequestStarted, NULL);
	if(!bOk && bRequestStarted && !xacmeHttpMethodReadOnly(sMethod))
	{
		xacmeHttpMarkWriteUncertain(pHttp);
	}
	return bOk;
}

static bool xacmeHttpExchangeOnceImpl(
	xacmehttp* pHttp, cstr sMethod, cstr sUrl, cstr sContentType,
	xstrview sBody, const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount, xacmehttpresponse* pResponse,
	bool* pbRequestStarted, bool* pbResponseStarted)
{
	xacmeurl Url;
	xacmestream Stream = { NULL, NULL };
	xbuffer Request;
	xbuffer Received;
	xbuffer BodyBuffer;
	xhttpfield Fields[XACME_HTTP_FIELD_MAX];
	xhttp1head Head;
	xhttp1limits Limits;
	xhttp1errorinfo ProtocolError;
	xhttp1bodyplan Plan;
	xhttp1body Body;
	xhttp1bodylimits BodyLimits;
	xhttp1bodystatus eBody;
	char sHostHeader[280];
	char sLength[24];
	char sPortText[8];
	uint8 Chunk[XACME_HTTP_IO_CHUNK];
	size_t iRequestSize = 0u;
	size_t iField = 0u;
	size_t iUsed = 0u;
	size_t iConsumed = 0u;
	xfuture* pFuture = NULL;
	double ResponseDeadline;
	bool bOk = false;
	bool bHeadDone = false;
	bool bStreamEnd = false;
	bool bContentType = (sContentType != NULL && sContentType[0] != '\0');
	bool bContentLength;
	size_t iReserved;
	if(pbRequestStarted != NULL)
	{
		*pbRequestStarted = false;
	}
	if(pbResponseStarted != NULL)
	{
		*pbResponseStarted = false;
	}

	if((pHttp == NULL) || (sMethod == NULL) || (sUrl == NULL) ||
		(pResponse == NULL))
	{
		xacmeHttpError(
			XERR_ARGUMENT, XACME_HTTP_ERROR_ARGUMENT,
			"acme http exchange requires http, method, url and response");
		return false;
	}
	memset(pResponse, 0, sizeof(*pResponse));
	bContentLength = (sBody.Data != NULL || strcmp(sMethod, "POST") == 0);
	if(pHttp->pEngine == NULL || pHttp->pResolver == NULL || pHttp->pVerifier == NULL)
	{
		xacmeHttpError(XERR_ARGUMENT, XACME_HTTP_ERROR_ARGUMENT,
			"acme http transport is not initialized or has been cleaned up");
		return false;
	}
	iReserved = 4u + (size_t)bContentType + (size_t)bContentLength;
	if((sBody.Data == NULL && sBody.Size != 0u) ||
		(iExtraCount != 0u && pExtraHeaders == NULL) ||
		iExtraCount > XACME_HTTP_FIELD_MAX - iReserved)
	{
		xacmeHttpError(XERR_ARGUMENT, XACME_HTTP_ERROR_ARGUMENT,
			"acme http body or extra header count invalid");
		return false;
	}
	{
		size_t i;
		for(i = 0u; i < iExtraCount; i++)
		{
			if(pExtraHeaders[i].sName == NULL || pExtraHeaders[i].sValue == NULL ||
				xacmeHttpHeaderOwned(pExtraHeaders[i].sName))
			{
				xacmeHttpError(XERR_ARGUMENT, XACME_HTTP_ERROR_ARGUMENT,
					"acme http extra header invalid or managed by transport");
				return false;
			}
		}
	}
	if(!xacmeUrlParse(sUrl, &Url))
	{
		xacmeHttpError(
			XERR_ARGUMENT, XACME_HTTP_ERROR_URL,
			"acme http exchange url is not http(s) absolute");
		return false;
	}
	if(((Url.bTls ? 443u : 80u) != Url.iPort))
	{
		snprintf(sPortText, sizeof(sPortText), ":%u", (unsigned)Url.iPort);
	}
	else
	{
		sPortText[0] = '\0';
	}
	snprintf(
		sHostHeader, sizeof(sHostHeader), "%s%s", Url.sHost, sPortText);

	/* ---- 请求组装 ---- */
	xrtBufferInit(&Request);
	xrtBufferInit(&Received);
	xrtBufferInit(&BodyBuffer);
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Host"),
		(xstrview){ sHostHeader, strlen(sHostHeader) } };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("User-Agent"), XRT_STR_LITERAL("xacme") };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Accept"), XRT_STR_LITERAL("*/*") };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Connection"), XRT_STR_LITERAL("close") };
	{
		size_t i;
		for(i = 0; i < iExtraCount; i++)
		{
			Fields[iField++] = (xhttpfield){
				(xstrview){
					pExtraHeaders[i].sName,
					strlen(pExtraHeaders[i].sName) },
				(xstrview){
					pExtraHeaders[i].sValue,
					strlen(pExtraHeaders[i].sValue) } };
		}
	}
	if(bContentType)
	{
		Fields[iField++] = (xhttpfield){
			(xstrview){ "Content-Type", 12u },
			(xstrview){ sContentType, strlen(sContentType) } };
	}
	if(bContentLength)
	{
		snprintf(sLength, sizeof(sLength), "%llu",
			(unsigned long long)((sBody.Data == NULL) ? 0u : sBody.Size));
		Fields[iField++] = (xhttpfield){
			XRT_STR_LITERAL("Content-Length"),
			(xstrview){ sLength, strlen(sLength) } };
	}
	if(!xrtHttp1RequestWrite(
		(xstrview){ sMethod, strlen(sMethod) },
		(xstrview){ Url.sPath, strlen(Url.sPath) },
		XHTTP_VERSION_1_1, Fields, iField, NULL, 0u, &iRequestSize))
	{
		goto Done;
	}
	if(!xrtBufferResize(&Request, iRequestSize) ||
		!xrtHttp1RequestWrite(
			(xstrview){ sMethod, strlen(sMethod) },
			(xstrview){ Url.sPath, strlen(Url.sPath) },
			XHTTP_VERSION_1_1, Fields, iField,
			Request.Data, iRequestSize, &iRequestSize))
	{
		goto Done;
	}
	if((sBody.Data != NULL) && (sBody.Size > 0u) &&
		!xrtBufferAppend(
			&Request, (xbytesview){ (const uint8*)sBody.Data, sBody.Size }))
	{
		goto Done;
	}

	/* ---- 连接 ---- */
	if(Url.bTls)
	{
		xtlsclientconfig Tls;
		xtlsdialconfig Dial;
		xrtTlsClientConfigInit(&Tls);
		Tls.Verifier = pHttp->pVerifier;
		xrtTlsDialConfigInit(&Dial);
		xacmeUrlTlsNames(&Url, &Tls, &Dial);
		Dial.Timeout = pHttp->uTimeoutMs;
		pFuture = xrtTlsDialAsync(
			pHttp->pEngine, pHttp->pResolver, Url.sHost, Url.iPort,
			&Tls, &Dial, NULL, NULL);
	}
	else
	{
		xnetdialconfig Dial;
		xrtNetDialConfigInit(&Dial);
		Dial.Timeout = pHttp->uTimeoutMs;
		pFuture = xrtNetDialAsync(
			pHttp->pEngine, pHttp->pResolver, Url.sHost, Url.iPort,
			&Dial, NULL, NULL);
	}
	if(pFuture == NULL)
	{
		/* 底层拨号错误已在线程错误里；仅补充域信息。 */
		goto Done;
	}
	if(xrtFutureWaitFor(pFuture, pHttp->uTimeoutMs) != XWAIT_OK ||
		xrtFutureState(pFuture) != XFUTURE_RESOLVED)
	{
		const xerror* pFutureError = xrtFutureError(pFuture);
		if(pFutureError != NULL)
		{
			/* 包装底层根因，保留完整因链。 */
			xerror* pWrap = xrtErrorWrap(
				pFutureError, XERR_IO, "xrt.acme.http",
				(int32)XACME_HTTP_ERROR_CONNECT,
				"acme http connect failed");
			if(pWrap != NULL)
			{
				xrtSetErrorTake(pWrap);
				goto Done;
			}
		}
		{
			xacmeHttpError(
				XERR_TIMEOUT, XACME_HTTP_ERROR_CONNECT,
				"acme http connect timeout");
		}
		goto Done;
	}
	if(Url.bTls)
	{
		Stream.pTls = xrtTlsStreamRef((xtlsstream*)xrtFutureValue(pFuture));
		if(Stream.pTls == NULL)
		{
			goto ConnectFail;
		}
	}
	else
	{
		Stream.pTcp = xrtNetStreamRef((xnetstream*)xrtFutureValue(pFuture));
		if(Stream.pTcp == NULL)
		{
			goto ConnectFail;
		}
	}
	xrtFutureDestroy(pFuture);
	pFuture = NULL;

	/* ---- 发送 ---- */
	if(pbRequestStarted != NULL)
	{
		*pbRequestStarted = true;
	}
	if(!xacmeStreamSendAll(
		&Stream, Request.Data, Request.Size, pHttp->uTimeoutMs))
	{
		xacmeHttpError(
			XERR_IO, XACME_HTTP_ERROR_SEND,
			"acme http send failed");
		goto Done;
	}
	ResponseDeadline = __xrtWaitAfter(pHttp->uTimeoutMs);

	/* ---- 接收头 ---- */
	xrtHttp1LimitsInit(&Limits);
	Limits.MaxFields = XACME_HTTP_FIELD_MAX;
	memset(&Head, 0, sizeof(Head));
	Head.Fields = Fields;
	Head.FieldCapacity = XACME_HTTP_FIELD_MAX;
	while(!bHeadDone)
	{
		xhttp1status eStatus = xrtHttp1ResponseParse(
			xrtBufferView(&Received), &Head, &Limits, &ProtocolError);
		if(eStatus == XHTTP1_READY)
		{
			bHeadDone = true;
			break;
		}
		if(eStatus != XHTTP1_MORE)
		{
			xacmeHttpError(
				XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
				"acme http response head invalid");
			goto Done;
		}
		{
			int iGot = xacmeStreamRecv(
				&Stream, Chunk, sizeof(Chunk), &iUsed,
				ResponseDeadline);
			if(iGot < 0)
			{
				xacmeHttpError(
					(iGot == -1) ? XERR_TIMEOUT : XERR_IO,
					XACME_HTTP_ERROR_PROTOCOL,
					(iGot == -1) ? "acme http response head timeout" :
					"acme http response head read failed");
				goto Done;
			}
			if(iGot == 0)
			{
				/* 连接在收到任何响应字节前关闭 = 传输层故障
				   （可重试）；已收到部分头才算协议截断。 */
				xacmeHttpError(
					(Received.Size == 0u) ? XERR_IO : XERR_PROTOCOL,
					XACME_HTTP_ERROR_PROTOCOL,
					(Received.Size == 0u) ?
						"acme http connection closed before response" :
						"acme http response head truncated");
				goto Done;
			}
			if(pbResponseStarted != NULL)
			{
				*pbResponseStarted = true;
			}
			if(!xrtBufferAppend(&Received, (xbytesview){ Chunk, iUsed }))
			{
				goto Done;
			}
		}
	}

	/* ---- 响应字段 ---- */
	pResponse->iStatus = Head.Status;
	if(!xacmeHeaderTake(&Head, "Location", &pResponse->sLocation) ||
		!xacmeHeaderTake(&Head, "Replay-Nonce", &pResponse->sReplayNonce) ||
		!xacmeHeaderTake(&Head, "Retry-After", &pResponse->sRetryAfter) ||
		!xacmeHeaderTakeLinks(&Head, &pResponse->sLink) ||
		!xacmeHeaderTake(&Head, "Content-Type", &pResponse->sContentType))
	{
		goto Done;
	}

	/* ---- 接收体 ---- */
	if(!xrtHttp1ResponseBodyPlan(
		&Head, (xstrview){ sMethod, strlen(sMethod) }, &Plan))
	{
		xacmeHttpError(
			XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
			"acme http response body plan failed");
		goto Done;
	}
	xrtHttp1BodyLimitsInit(&BodyLimits);
	BodyLimits.MaxBody = XACME_HTTP_MAX_RESPONSE_BODY;
	BodyLimits.MaxTrailers = XACME_HTTP_FIELD_MAX;
	/* Header 已复制，Body Plan 不再借用字段，可复用存储解析 trailer。 */
	if(!xrtHttp1BodyInit(&Body, &Plan, Fields, XACME_HTTP_FIELD_MAX, &BodyLimits))
	{
		xacmeHttpError(
			XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
			"acme http response body invalid");
		goto Done;
	}
	iUsed = Head.Bytes;
	for(;;)
	{
		xbytesview Data;
		eBody = xrtHttp1BodyRead(
			&Body,
			(xbytesview){
				(uint8*)Received.Data + iUsed, Received.Size - iUsed },
			bStreamEnd, &iConsumed, &Data, &ProtocolError);
		iUsed += iConsumed;
		if(eBody == XHTTP1_BODY_ERROR || eBody == XHTTP1_BODY_FIELDS)
		{
			xacmeHttpError(
				XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
				"acme http response body invalid");
			goto Done;
		}
		if(eBody == XHTTP1_BODY_DATA)
		{
			/* DNS 与 ACME 消费零结尾文本，不允许按 NUL 截断有效前缀。 */
			if(Data.Size != 0u && memchr(Data.Data, 0, Data.Size) != NULL)
			{
				xacmeHttpError(
					XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
					"acme http response body invalid");
				goto Done;
			}
			if(!xrtBufferAppend(&BodyBuffer, Data))
			{
				goto Done;
			}
			continue;
		}
		if(eBody == XHTTP1_BODY_DONE)
		{
			break;
		}
		if(bStreamEnd)
		{
			xacmeHttpError(
				XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
				"acme http response body truncated");
			goto Done;
		}
		{
			size_t iGot = 0u;
			/* Data 已复制；只保留未完成 trailer 等尚未消费的字节。 */
			if(iUsed != 0u)
			{
				if(!xrtBufferRemove(&Received, 0u, iUsed)) goto Done;
				iUsed = 0u;
			}
			int iResult = xacmeStreamRecv(
				&Stream, Chunk, sizeof(Chunk), &iGot,
				ResponseDeadline);
			if(iResult < 0)
			{
				xacmeHttpError(
					(iResult == -1) ? XERR_TIMEOUT : XERR_IO,
					XACME_HTTP_ERROR_PROTOCOL,
					(iResult == -1) ? "acme http response body timeout" :
					"acme http response body read failed");
				goto Done;
			}
			if(iResult == 0)
			{
				bStreamEnd = true;
				continue;
			}
			if(!xrtBufferAppend(&Received, (xbytesview){ Chunk, iGot }))
			{
				goto Done;
			}
		}
	}
	/* 体数据已独立收拢在 BodyBuffer；拷出为零结尾文本。 */
	pResponse->sBody = (str)xrtMalloc(BodyBuffer.Size + 1u);
	if(pResponse->sBody == NULL)
	{
		goto Done;
	}
	if(BodyBuffer.Size > 0u)
	{
		memcpy(pResponse->sBody, BodyBuffer.Data, BodyBuffer.Size);
	}
	pResponse->sBody[BodyBuffer.Size] = '\0';
	pResponse->iBodySize = BodyBuffer.Size;
	if(getenv("XACME_DEBUG"))
	{
		printf("[ex-dbg] %s %s -> status=%u bodyLen=%zu ct=%s\n",
			sMethod, sUrl, (unsigned)pResponse->iStatus, pResponse->iBodySize,
			(pResponse->sContentType != NULL) ? pResponse->sContentType : "-");
	}
	bOk = true;
	goto Done;

ConnectFail:
	xacmeHttpError(
		XERR_IO, XACME_HTTP_ERROR_CONNECT,
		"acme http connect failed");
Done:
	if(pFuture != NULL)
	{
		xrtFutureDestroy(pFuture);
	}
	xacmeStreamClose(&Stream, !bOk);
	xrtBufferUnit(&Request);
	xrtBufferUnit(&Received);
	xrtBufferUnit(&BodyBuffer);
	if(!bOk)
	{
		/* 响应头可能已分配；失败时不把部分响应留给调用方。 */
		xacmeHttpResponseUnit(pResponse);
	}
	return bOk;
}

#endif
