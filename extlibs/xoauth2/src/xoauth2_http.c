/* xoauth2 便捷传输：直连 xrt net engine + TLS + http1。
 * 模式取自 xacme_http（单次 POST、Connection: close、future 化 IO），
 * 按 oauth2 场景裁剪：无重放 nonce / 无重试；POST 带 form 头，GET 无实体头。
 * 支持 http://（明文，测试/内网 IdP）与 https://（系统或指定 CA 验证）。
 * 全部 API 经 xoauth2-xrt.h → <xrt.h> 聚合声明（single 单头或宿主 shim）。 */
#include "xoauth2_internal.h"

#define XOAUTH2_HTTP_FIELD_MAX 100u
#define XOAUTH2_HTTP_IO_CHUNK 16384u
#define XOAUTH2_HTTP_TIMEOUT_DEFAULT 15000000ull  /* 15s */
#define XOAUTH2_HTTP_ROLLBACK_TIMEOUT_MIN 30000000ull
/* 响应体上限：token/JWKS/userinfo 响应远小于 1MB；
 * 防 malformed Content-Length / 无限 chunked 把内存吃光 */
#define XOAUTH2_HTTP_MAX_BODY (1024u * 1024u)

struct xoauth2httpxrt {
	xnetengine* pEngine;      /* 借用或自建 */
	bool        bEngineOwned;
	xnetresolver* pResolver;
	void*       pVerifier;    /* xtlsverifier*（ opaque 存放，Unit 释放） */
	uint64_t    uTimeoutUs;
	struct xoauth2httpxrt* pPendingNext; /* 只用于未交付的失败堆构造。 */
};

/* Only unpublished failed heap constructors share this resource retirement queue. */
static xatomic32 __xoauth2PendingLock = { 0u };
static xoauth2httpxrt* __xoauth2PendingHead;
static xoauth2httpxrt* __xoauth2PendingTail;
static size_t __xoauth2PendingCount;

static void http_pending_lock(void)
{
	uint32 expected = 0u;
	while ( !xrtAtomic32CompareExchange(&__xoauth2PendingLock, &expected, 1u,
		XMEMORY_ACQUIRE, XMEMORY_RELAXED) ) {
		expected = 0u;
		xrtThreadYield();
	}
}

static void http_pending_unlock(void)
{
	xrtAtomic32Store(&__xoauth2PendingLock, 0u, XMEMORY_RELEASE);
}

/* Init failure has already released resolver/verifier; consume the heap owner without allocating. */
static void http_defer_owner(xoauth2httpxrt* pHttp)
{
	http_pending_lock();
	pHttp->pPendingNext = __xoauth2PendingHead;
	__xoauth2PendingHead = pHttp;
	if ( __xoauth2PendingTail == NULL ) __xoauth2PendingTail = pHttp;
	__xoauth2PendingCount++;
	http_pending_unlock();
}

bool xoauth2HttpXrtCleanupPending(uint64_t uTimeoutUs, size_t* piPending)
{
	xerror* pPrevious = xrtErrorRef(xrtGetError());
	xerror* pFirst = NULL;
	xdeadline deadline = xrtDeadlineAfter(uTimeoutUs);
	size_t iPending;
	bool bError = false;
	for (;;) {
		xoauth2httpxrt *pList, *pListTail, *pWait = NULL, *pWaitTail = NULL;
		http_pending_lock();
		pList = __xoauth2PendingHead;
		pListTail = __xoauth2PendingTail;
		__xoauth2PendingHead = NULL;
		__xoauth2PendingTail = NULL;
		http_pending_unlock();
		/* Claimed owners remain counted, including while another caller drains them. */
		while ( pList != NULL ) {
			xoauth2httpxrt* pNext = pList->pPendingNext;
			xnetretireresult retired = xrtNetEngineTryDestroy(pList->pEngine);
			if ( retired == XNET_RETIRE_READY ) {
				xrtSecureZero(pList, sizeof(*pList));
				xrtFree(pList);
				http_pending_lock();
				__xoauth2PendingCount--;
				http_pending_unlock();
			} else {
				if ( retired == XNET_RETIRE_ERROR ) {
					bError = true;
					if ( pFirst == NULL ) pFirst = xrtErrorRef(xrtGetError());
				}
				pList->pPendingNext = pWait;
				pWait = pList;
				if ( pWaitTail == NULL ) pWaitTail = pList;
			}
			pList = pNext;
			if ( pList != NULL && (bError ||
				(uTimeoutUs != 0u && xrtDeadlineExpired(deadline))) ) {
				/* Preserve the unvisited tail after ERROR or budget exhaustion. */
				if ( pWaitTail != NULL ) pWaitTail->pPendingNext = pList;
				else pWait = pList;
				pWaitTail = pListTail;
				pList = NULL;
			}
		}
		http_pending_lock();
		if ( pWait != NULL ) {
			pWaitTail->pPendingNext = __xoauth2PendingHead;
			if ( __xoauth2PendingTail == NULL ) __xoauth2PendingTail = pWaitTail;
			__xoauth2PendingHead = pWait;
		}
		iPending = __xoauth2PendingCount;
		http_pending_unlock();
		if ( iPending == 0u || bError || uTimeoutUs == 0u ) break;
		if ( xrtDeadlineExpired(deadline) ) {
			xrtSetErrorInfo(XERR_TIMEOUT, "xrt.oauth2", XOAUTH2_ERROR_NETWORK,
				"http pending cleanup still has live objects");
			break;
		}
		xrtSleep(1u);
	}
	if ( piPending != NULL ) *piPending = iPending;
	if ( pPrevious != NULL ) {
		xrtErrorFree(pFirst);
		xrtSetErrorTake(pPrevious);
	} else if ( pFirst != NULL ) xrtSetErrorTake(pFirst);
	return iPending == 0u;
}

/* ------------------------------------------------------------------ */
/* URL 解析（http(s)://host[:port]/path）                                */
/* ------------------------------------------------------------------ */
typedef struct xoauth2url {
	char   sHost[256];
	uint16_t iPort;
	char   sPath[1024];
	bool   bTls;
	bool   bIpLiteral;
} xoauth2url;

static bool url_scheme_equal(const char* sUrl, const char* sPrefix)
{
	while ( *sPrefix != 0 ) {
		unsigned char c = (unsigned char)*sUrl++;
		if ( c >= 'A' && c <= 'Z' ) c += 'a' - 'A';
		if ( c != (unsigned char)*sPrefix++ ) return false;
	}
	return true;
}

static bool url_hex_valid(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
		(c >= 'a' && c <= 'f');
}

static bool url_port_parse(const char* sPort, size_t iSize, uint16_t* pPort)
{
	unsigned iPort = 0;
	if ( iSize == 0u ) return false;
	for ( size_t i = 0; i < iSize; i++ ) {
		unsigned iDigit;
		if ( sPort[i] < '0' || sPort[i] > '9' ) return false;
		iDigit = (unsigned)(sPort[i] - '0');
		if ( iPort > (65535u - iDigit) / 10u ) return false;
		iPort = iPort * 10u + iDigit;
	}
	if ( iPort == 0u ) return false;
	*pPort = (uint16_t)iPort;
	return true;
}

static bool url_target_char_valid(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') ||
		(c != 0u && strchr("-._~!$&'()*+,;=:@/?", (int)c) != NULL);
}

/* Validate the ASCII path/query/fragment grammar before discarding a fragment. */
static bool url_component_valid(const char* sText, size_t iSize)
{
	for ( size_t i = 0; i < iSize; i++ ) {
		unsigned char c = (unsigned char)sText[i];
		if ( c == '%' ) {
			if ( iSize - i < 3u || !url_hex_valid((unsigned char)sText[i + 1u]) ||
				!url_hex_valid((unsigned char)sText[i + 2u]) ) return false;
			i += 2u;
		} else if ( !url_target_char_valid(c) ) return false;
	}
	return true;
}

/* Legacy IPv4 components may mix decimal/octal and 0x hexadecimal forms. */
static bool url_numeric_host(const char* sHost, size_t iSize)
{
	size_t i = 0u;
	while ( i < iSize ) {
		size_t iStart;
		bool bHex = iSize - i >= 2u && sHost[i] == '0' &&
			(sHost[i + 1u] == 'x' || sHost[i + 1u] == 'X');
		if ( bHex ) i += 2u;
		iStart = i;
		while ( i < iSize && sHost[i] != '.' ) {
			unsigned char c = (unsigned char)sHost[i];
			if ( bHex ? !url_hex_valid(c) : (c < '0' || c > '9') ) return false;
			i++;
		}
		if ( i == iStart ) return false;
		if ( i < iSize ) i++; /* A root dot does not turn an address into a DNS identity. */
	}
	return true;
}

static bool url_parse(const char* sUrl, xoauth2url* pOut)
{
	const char *sHost, *sTail, *sPort;
	size_t iAuthorityLen, iHostLen, iTargetLen, iPrefix;

	memset(pOut, 0, sizeof(*pOut));
	if ( sUrl == NULL ) return false;
	if ( url_scheme_equal(sUrl, "https://") ) {
		pOut->bTls = true;
		sHost = sUrl + 8;
	}
	else if ( url_scheme_equal(sUrl, "http://") ) {
		pOut->bTls = false;
		sHost = sUrl + 7;
	}
	else {
		return false;
	}
	pOut->iPort = pOut->bTls ? 443u : 80u;
	/* Authority 在首个 /、? 或 # 结束；userinfo 会改变目标主机，拒绝。 */
	iAuthorityLen = strcspn(sHost, "/?#");
	if ( iAuthorityLen == 0u || memchr(sHost, '@', iAuthorityLen) != NULL )
		return false;
	sTail = sHost + iAuthorityLen;
	iPrefix = *sTail == '/' ? 0u : 1u;
	iTargetLen = strcspn(sTail, "#");
	if ( iTargetLen > sizeof(pOut->sPath) - 1u - iPrefix ) return false;
	if ( !url_component_valid(sTail, iTargetLen) ) return false;
	if ( sTail[iTargetLen] == '#' &&
		!url_component_valid(sTail + iTargetLen + 1u, strlen(sTail + iTargetLen + 1u)) ) return false;
	if ( iPrefix ) pOut->sPath[0] = '/';
	memcpy(pOut->sPath + iPrefix, sTail, iTargetLen);
	pOut->sPath[iPrefix + iTargetLen] = 0;
	if ( sHost[0] == '[' ) {
		/* IPv6 字面量：[addr] 或 [addr]:port；Host 头按 RFC 9110 保留括号 */
		const char* sClose = (const char*)memchr(sHost, ']', iAuthorityLen);
		size_t iAddr;
		xnetaddr Address;
		bool bNumeric;
		if ( sClose == NULL ) return false;
		iAddr = (size_t)(sClose - sHost) + 1;
		if ( iAddr <= 2u || iAddr >= sizeof(pOut->sHost) ) return false;
		for ( size_t i = 1u; i + 1u < iAddr; i++ ) {
			unsigned char c = (unsigned char)sHost[i];
			if ( !url_hex_valid(c) && c != ':' && c != '.' ) return false;
		}
		memcpy(pOut->sHost, sHost, iAddr);
		pOut->sHost[iAddr] = 0;
		if ( iAddr < iAuthorityLen ) {
			if ( sHost[iAddr] != ':' ) return false;
			if ( !url_port_parse(sHost + iAddr + 1u,
				iAuthorityLen - iAddr - 1u, &pOut->iPort) ) return false;
		}
		pOut->sHost[iAddr - 1u] = 0;
		bNumeric = xrtNetAddrParse(&Address, pOut->sHost + 1u,
			pOut->iPort) && Address.Family == XNET_FAMILY_IPV6;
		pOut->sHost[iAddr - 1u] = ']';
		if ( !bNumeric ) return false;
		pOut->bIpLiteral = true;
		return true;
	}
	sPort = (const char*)memchr(sHost, ':', iAuthorityLen);
	iHostLen = iAuthorityLen;
	if ( sPort != NULL ) {
		if ( !url_port_parse(sPort + 1u,
			iAuthorityLen - (size_t)(sPort - sHost) - 1u,
			&pOut->iPort) ) return false;
		iHostLen = (size_t)(sPort - sHost);
	}
	if ( iHostLen == 0 || iHostLen >= sizeof(pOut->sHost) ||
		sHost[0] == '.' ||
		(iHostLen > 1u && sHost[iHostLen - 1u] == '.' &&
		 sHost[iHostLen - 2u] == '.') ) return false;
	for ( size_t i = 0; i < iHostLen; i++ ) {
		unsigned char c = (unsigned char)sHost[i];
		if ( !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_') )
			return false;
	}
	memcpy(pOut->sHost, sHost, iHostLen);
	{
		bool bNumeric = true;
		for ( size_t i = 0; i < iHostLen && bNumeric; i++ )
			bNumeric = (sHost[i] >= '0' && sHost[i] <= '9') ||
				sHost[i] == '.';
		if ( bNumeric ) {
			xnetaddr Address;
			if ( !xrtNetAddrParse(&Address, pOut->sHost,
				pOut->iPort) || Address.Family != XNET_FAMILY_IPV4 )
				return false;
			pOut->bIpLiteral = true;
		}
		else if ( url_numeric_host(sHost, iHostLen) ) return false;
	}
	return true;
}

static void url_tls_names(const xoauth2url* pUrl,
	xtlsclientconfig* pTls, xtlsdialconfig* pDial)
{
	const char* sVerify = pUrl->sHost;
	size_t iVerifySize = strlen(sVerify);
	if ( pUrl->bIpLiteral && sVerify[0] == '[' ) {
		sVerify++;
		iVerifySize -= 2u;
	} else if ( !pUrl->bIpLiteral && iVerifySize > 0u &&
		sVerify[iVerifySize - 1u] == '.' ) {
		/* RFC 6066 的 SNI DNS 主机名不含末尾根点。 */
		iVerifySize--;
	}
	pTls->VerifyName = (xstrview){ sVerify, iVerifySize };
	if ( !pUrl->bIpLiteral )
		pTls->ServerName = pTls->VerifyName;
	else
		pTls->ServerName = (xstrview){ NULL, 0 };
	/* 即使 ServerName 为空，也不能让 Dial 从 IP 主机自动补成 SNI。 */
	pDial->ServerNameFromHost = false;
}

/* ------------------------------------------------------------------ */
/* 初始化 / 释放                                                         */
/* ------------------------------------------------------------------ */
bool xoauth2HttpXrtInit(xoauth2httpxrt* pHttp, void* pBorrowedEngine,
                        const char* sCaPem, uint64_t uTimeoutUs)
{
	if ( pHttp == NULL ) return false;
	memset(pHttp, 0, sizeof(*pHttp));
	pHttp->uTimeoutUs = (uTimeoutUs != 0) ? uTimeoutUs
	                                      : XOAUTH2_HTTP_TIMEOUT_DEFAULT;
	if ( pBorrowedEngine == NULL && !xoauth2HttpXrtCleanupPending(0u, NULL) ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK,
			"http pending cleanup must finish before creating a private engine");
		return false;
	}

	pHttp->pEngine = (xnetengine*)pBorrowedEngine;
	pHttp->pResolver = xrtNetResolverCreate(NULL);
	if ( pHttp->pResolver == NULL ) goto fail;

	{
		xtlsverifierconfig Verify;
		xx509store* pStore = NULL;
		xrtTlsVerifierConfigInit(&Verify);
		if ( sCaPem != NULL && sCaPem[0] != '\0' ) {
			size_t iAdded = 0;
			pStore = xrtX509StoreCreate();
			if ( pStore == NULL ||
			     !xrtX509StoreAddPem(pStore, sCaPem, strlen(sCaPem), &iAdded) ||
			     iAdded == 0 ) {
				xrtX509StoreFree(pStore);
				goto fail;
			}
			Verify.Store = pStore;
		}
		else {
			pStore = xrtX509StoreSystem();
			if ( pStore == NULL ) goto fail;
			Verify.Store = pStore;
		}
		pHttp->pVerifier = xrtTlsVerifierCreate(&Verify);
		xrtX509StoreFree(pStore);   /* 信任库已深复制进验证器 */
		if ( pHttp->pVerifier == NULL ) goto fail;
	}
	/* 配置完成后才启动；失败时有界退休，未完成的拥有者仍可重试。 */
	if ( pHttp->pEngine == NULL ) {
		xnetengineconfig Engine;
		xrtNetEngineConfigInit(&Engine);
		pHttp->pEngine = xrtNetEngineCreate(&Engine);
		if ( pHttp->pEngine == NULL ) goto fail;
		pHttp->bEngineOwned = true;
		if ( !xrtNetEngineStart(pHttp->pEngine) ) goto fail;
	}
	return true;

fail:
	{
		xerror* pCause = xrtErrorRef(xrtGetError());
		uint64_t uRequestTimeout = pHttp->uTimeoutUs;
		if ( pHttp->uTimeoutUs < XOAUTH2_HTTP_ROLLBACK_TIMEOUT_MIN )
			pHttp->uTimeoutUs = XOAUTH2_HTTP_ROLLBACK_TIMEOUT_MIN;
		(void)xoauth2HttpXrtCleanup(pHttp);
		pHttp->uTimeoutUs = uRequestTimeout;
		if ( pCause != NULL ) {
			xerror* pFailure = xrtErrorWrap(pCause, XERR_STATE, "xrt.oauth2",
				XOAUTH2_ERROR_NETWORK, "http transport init failed");
			if ( pFailure != NULL ) {
				xrtErrorFree(pCause);
				xrtSetErrorTake(pFailure);
			} else xrtSetErrorTake(pCause);
		} else xoauth2__error(XOAUTH2_ERROR_NETWORK, "http transport init failed");
	}
	return false;
}

xoauth2httpxrt* xoauth2HttpXrtCreate(void* pBorrowedEngine,
                                     const char* sCaPem, uint64_t uTimeoutUs)
{
	xoauth2httpxrt* pHttp = (xoauth2httpxrt*)xrtMalloc(sizeof(xoauth2httpxrt));
	if ( pHttp == NULL ) return NULL;
	if ( !xoauth2HttpXrtInit(pHttp, pBorrowedEngine, sCaPem, uTimeoutUs) ) {
		if ( pHttp->bEngineOwned && pHttp->pEngine != NULL ) http_defer_owner(pHttp);
		else xrtFree(pHttp);
		return NULL;
	}
	return pHttp;
}

void xoauth2HttpXrtDestroy(xoauth2httpxrt* pHttp)
{
	if ( pHttp == NULL ) return;
	if ( xoauth2HttpXrtCleanup(pHttp) ) xrtFree(pHttp);
}

void xoauth2HttpXrtUnit(xoauth2httpxrt* pHttp)
{
	(void)xoauth2HttpXrtCleanup(pHttp);
}

bool xoauth2HttpXrtCleanup(xoauth2httpxrt* pHttp)
{
	bool bReady = true;
	xerror* pPrevious;
	if ( pHttp == NULL ) return true;
	pPrevious = xrtErrorRef(xrtGetError());
	if ( pHttp->pResolver != NULL ) {
		xrtNetResolverDestroy(pHttp->pResolver);
		pHttp->pResolver = NULL;
	}
	if ( pHttp->bEngineOwned && pHttp->pEngine != NULL ) {
		xdeadline Deadline = xrtDeadlineAfter(pHttp->uTimeoutUs);
		for (;;) {
			xnetretireresult Result = xrtNetEngineTryDestroy(pHttp->pEngine);
			if ( Result == XNET_RETIRE_READY ) {
				pHttp->pEngine = NULL;
				pHttp->bEngineOwned = false;
				break;
			}
			if ( Result == XNET_RETIRE_ERROR ) { bReady = false; break; }
			if ( xrtDeadlineExpired(Deadline) ) {
				xoauth2__error(XOAUTH2_ERROR_NETWORK,
					"http transport engine still has live objects during cleanup");
				bReady = false;
				break;
			}
			/* Close/Abort 异步释放内部引用；只在 READY 时消费创建者拥有权。 */
			xrtSleep(1u);
		}
	} else {
		pHttp->pEngine = NULL;
		pHttp->bEngineOwned = false;
	}
	if ( pHttp->pVerifier != NULL ) {
		xrtTlsVerifierRelease((xtlsverifier*)pHttp->pVerifier);
		pHttp->pVerifier = NULL;
	}
	if ( pPrevious != NULL ) xrtSetErrorTake(pPrevious);
	return bReady;
}

/* ------------------------------------------------------------------ */
/* 流封装（future 化 IO，同 xacme）                                      */
/* ------------------------------------------------------------------ */
typedef struct xoauth2stream {
	xtlsstream* pTls;
	xnetstream* pTcp;
} xoauth2stream;

static bool future_resolved_for(xfuture* pFuture, uint64 uRemaining)
{
	return uRemaining != 0u &&
		xrtFutureWaitFor(pFuture, uRemaining) == XWAIT_OK &&
		xrtFutureState(pFuture) == XFUTURE_RESOLVED;
}

/* Wait only publishes the terminal state, not the producer's thread error.
 * Retain that diagnostic before releasing the observer or requesting cancel. */
static void future_finish(xfuture* pFuture, bool bResolved)
{
	xerror* pPrevious = NULL;
	if ( !bResolved ) {
		const xerror* pError = xrtFutureError(pFuture);
		if ( pError != NULL ) xrtSetError(pError);
		pPrevious = xrtErrorRef(xrtGetError());
		if ( xrtFutureState(pFuture) == XFUTURE_PENDING )
			(void)xrtFutureCancel(pFuture);
	}
	xrtFutureDestroy(pFuture);
	if ( pPrevious != NULL ) xrtSetErrorTake(pPrevious);
}

static void transport_error(const char* sMessage)
{
	/* An exhausted allocator is not a retryable network failure. */
	if ( xrtErrorKind(xrtGetError()) != XERR_MEMORY )
		xoauth2__error(XOAUTH2_ERROR_NETWORK, sMessage);
}

static bool future_wait(xfuture* pFuture, xdeadline Deadline)
{
	uint64 uRemaining = xrtDeadlineRemaining(Deadline);
	bool bResolved = future_resolved_for(pFuture, uRemaining);
	future_finish(pFuture, bResolved);
	return bResolved;
}

static bool stream_send_all(xoauth2stream* pStream, const void* pData,
                            size_t iSize, uint64_t uUs)
{
	size_t iOffset = 0;
	xdeadline Deadline = xrtDeadlineAfter(uUs);
	while ( iOffset < iSize ) {
		size_t iChunk = iSize - iOffset;
		if ( xrtDeadlineExpired(Deadline) ) return false;
		if ( iChunk > XOAUTH2_HTTP_IO_CHUNK ) iChunk = XOAUTH2_HTTP_IO_CHUNK;
		if ( pStream->pTls != NULL ) {
			xfuture* pF = xrtTlsStreamSendAsync(
				pStream->pTls, (const uint8*)pData + iOffset, iChunk);
			if ( pF == NULL || !future_wait(pF, Deadline) ) return false;
		}
		else {
			xnetresult eResult;
			xfuture* pF;
			while ( (eResult = xrtNetStreamSend(
				pStream->pTcp, (const uint8*)pData + iOffset, iChunk))
				== XNET_RESULT_AGAIN ) {
				if ( xrtDeadlineExpired(Deadline) ) return false;
				pF = xrtNetStreamWaitAsync(
					pStream->pTcp, XNET_STREAM_WAIT_WRITE);
				if ( pF == NULL || !future_wait(pF, Deadline) ) return false;
			}
			if ( eResult != XNET_RESULT_OK ) {
				const xerror* pError = xrtNetStreamError(pStream->pTcp);
				if ( pError != NULL ) xrtSetError(pError);
				return false;
			}
		}
		iOffset += iChunk;
	}
	return true;
}

/* 返回 1=有数据，0=流结束，-1=超时，-2=I/O 失败。 */
static int stream_recv(xoauth2stream* pStream, uint8* pBuffer,
                       size_t iCapacity, size_t* pRead, xdeadline Deadline)
{
	xfuture* pFuture;
	xnetbytes* pBytes;
	xwaitresult eWait;
	uint64 uRemaining = xrtDeadlineRemaining(Deadline);
	if ( uRemaining == 0u ) return -1;
	if ( pStream->pTls != NULL )
		pFuture = xrtTlsStreamRecvAsync(pStream->pTls, iCapacity);
	else
		pFuture = xrtNetStreamRecvAsync(pStream->pTcp, iCapacity);
	if ( pFuture == NULL ) return -2;
	eWait = xrtFutureWaitFor(pFuture, uRemaining);
	if ( eWait == XWAIT_OK && xrtFutureState(pFuture) == XFUTURE_CLOSED ) {
		xrtFutureDestroy(pFuture);
		/* Recv 的 CLOSED 可能是 EOF，也可能是终止路径；确认正常读端结束。 */
		if ( pStream->pTls != NULL ) {
			bool bEnd;
			pFuture = xrtTlsStreamWaitAsync(pStream->pTls, XTLS_STREAM_WAIT_END);
			if ( pFuture == NULL ) return -2;
			eWait = xrtFutureWaitFor(pFuture, xrtDeadlineRemaining(Deadline));
			bEnd = eWait == XWAIT_OK &&
				xrtFutureState(pFuture) == XFUTURE_RESOLVED;
			future_finish(pFuture, bEnd);
			return bEnd ? 0 : (eWait == XWAIT_TIMEOUT ? -1 : -2);
		} else {
			xnetstreamstats Stats;
			const xerror* pError;
			if ( !xrtNetStreamStats(pStream->pTcp, &Stats) ) return -2;
			pError = xrtNetStreamError(pStream->pTcp);
			if ( pError != NULL ) xrtSetError(pError);
			return Stats.ReadEnded && pError == NULL ? 0 : -2;
		}
	}
	if ( eWait != XWAIT_OK ||
	     xrtFutureState(pFuture) != XFUTURE_RESOLVED ) {
		future_finish(pFuture, false);
		return eWait == XWAIT_TIMEOUT ? -1 : -2;
	}
	pBytes = (xnetbytes*)xrtFutureValue(pFuture);
	if ( pBytes == NULL || xrtNetBytesView(pBytes).Size == 0 ) {
		xrtFutureDestroy(pFuture);
		return -2;  /* Recv 成功必须交付非空数据；EOF 由 CLOSED 表示。 */
	}
	{
		xbytesview View = xrtNetBytesView(pBytes);
		if ( View.Size > iCapacity ) {
			xrtFutureDestroy(pFuture);
			return -2;
		}
		memcpy(pBuffer, View.Data, View.Size);
		*pRead = View.Size;
	}
	xrtFutureDestroy(pFuture);
	return 1;
}

static void stream_close(xoauth2stream* pStream, bool bAbort)
{
	if ( pStream->pTls != NULL ) {
		if ( bAbort ) (void)xrtTlsStreamAbort(pStream->pTls);
		else (void)xrtTlsStreamClose(pStream->pTls);
		xrtTlsStreamDestroy(pStream->pTls);
		pStream->pTls = NULL;
	}
	if ( pStream->pTcp != NULL ) {
		if ( bAbort ) (void)xrtNetStreamAbort(pStream->pTcp);
		else (void)xrtNetStreamClose(pStream->pTcp);
		xrtNetStreamDestroy(pStream->pTcp);
		pStream->pTcp = NULL;
	}
}

/* ------------------------------------------------------------------ */
/* 传输回调                                                             */
/* ------------------------------------------------------------------ */
bool xoauth2HttpXrt(const char* sMethod, const char* sUrl, const char* sBody,
                    const char* sAuthHeader,
                    char** psResponseBody, int* piStatus, void* pContext)
{
	xoauth2httpxrt* pHttp = (xoauth2httpxrt*)pContext;
	xoauth2url Url;
	xoauth2stream Stream = { NULL, NULL };
	xbuffer Request, Received, BodyBuffer;
	xhttpfield Fields[XOAUTH2_HTTP_FIELD_MAX];
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
	uint8 Chunk[XOAUTH2_HTTP_IO_CHUNK];
	size_t iRequestSize = 0, iUsed = 0, iConsumed = 0;
	size_t iBodyLen = sBody ? strlen(sBody) : 0;
	xfuture* pFuture = NULL;
	xerror* pFailure = NULL;
	bool bOk = false, bHeadDone = false, bStreamEnd = false;
	size_t iField = 0;
	xdeadline ResponseDeadline;

	if ( pHttp == NULL || sUrl == NULL || psResponseBody == NULL ||
	     piStatus == NULL || sMethod == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "http transport: null argument");
		return false;
	}
	*psResponseBody = NULL;
	*piStatus = 0;
	if ( pHttp->pEngine == NULL || pHttp->pResolver == NULL ||
	     pHttp->pVerifier == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"http transport is not initialized or has been cleaned up");
		return false;
	}
	if ( !url_parse(sUrl, &Url) ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"http transport URL is invalid or unsupported");
		return false;
	}
	xrtClearError();
	if ( (Url.bTls ? 443u : 80u) != Url.iPort )
		snprintf(sPortText, sizeof(sPortText), ":%u", (unsigned)Url.iPort);
	else
		sPortText[0] = '\0';
	snprintf(sHostHeader, sizeof(sHostHeader), "%s%s", Url.sHost, sPortText);

	/* ---- 请求组装 ---- */
	xrtBufferInit(&Request);
	xrtBufferInit(&Received);
	xrtBufferInit(&BodyBuffer);
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Host"), (xstrview){ sHostHeader, strlen(sHostHeader) } };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("User-Agent"), XRT_STR_LITERAL("xoauth2") };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Accept"), XRT_STR_LITERAL("application/json") };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Connection"), XRT_STR_LITERAL("close") };
	if ( sAuthHeader != NULL && sAuthHeader[0] != '\0' ) {
		Fields[iField++] = (xhttpfield){
			XRT_STR_LITERAL("Authorization"),
			(xstrview){ sAuthHeader, strlen(sAuthHeader) } };
	}
	if ( sBody != NULL ) {
		Fields[iField++] = (xhttpfield){
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("application/x-www-form-urlencoded") };
		snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iBodyLen);
		Fields[iField++] = (xhttpfield){
			XRT_STR_LITERAL("Content-Length"),
			(xstrview){ sLength, strlen(sLength) } };
	}

	if ( !xrtHttp1RequestWrite(
		(xstrview){ sMethod, strlen(sMethod) }, (xstrview){ Url.sPath, strlen(Url.sPath) },
		XHTTP_VERSION_1_1, Fields, iField, NULL, 0, &iRequestSize) )
		goto done;
	if ( !xrtBufferResize(&Request, iRequestSize) ||
	     !xrtHttp1RequestWrite(
		(xstrview){ sMethod, strlen(sMethod) }, (xstrview){ Url.sPath, strlen(Url.sPath) },
		XHTTP_VERSION_1_1, Fields, iField,
		Request.Data, iRequestSize, &iRequestSize) )
		goto done;
	if ( iBodyLen > 0 &&
	     !xrtBufferAppend(&Request, (xbytesview){ (const uint8*)sBody, iBodyLen }) )
		goto done;

	/* ---- 连接 ---- */
	if ( Url.bTls ) {
		xtlsclientconfig Tls;
		xtlsdialconfig Dial;
		xrtTlsClientConfigInit(&Tls);
		Tls.Verifier = (xtlsverifier*)pHttp->pVerifier;
		xrtTlsDialConfigInit(&Dial);
		url_tls_names(&Url, &Tls, &Dial);
		Dial.Timeout = pHttp->uTimeoutUs;
		pFuture = xrtTlsDialAsync(pHttp->pEngine, pHttp->pResolver,
			Url.sHost, Url.iPort, &Tls, &Dial, NULL, NULL);
	}
	else {
		xnetdialconfig Dial;
		xrtNetDialConfigInit(&Dial);
		Dial.Timeout = pHttp->uTimeoutUs;
		pFuture = xrtNetDialAsync(pHttp->pEngine, pHttp->pResolver,
			Url.sHost, Url.iPort, &Dial, NULL, NULL);
	}
	if ( pFuture == NULL ) goto connect_fail;
	if ( xrtFutureWaitFor(pFuture, pHttp->uTimeoutUs) != XWAIT_OK ||
	     xrtFutureState(pFuture) != XFUTURE_RESOLVED )
		goto connect_fail;
	if ( Url.bTls ) {
		Stream.pTls = xrtTlsStreamRef((xtlsstream*)xrtFutureValue(pFuture));
		if ( Stream.pTls == NULL ) goto connect_fail;
	}
	else {
		Stream.pTcp = xrtNetStreamRef((xnetstream*)xrtFutureValue(pFuture));
		if ( Stream.pTcp == NULL ) goto connect_fail;
	}
	xrtFutureDestroy(pFuture);
	pFuture = NULL;

	/* ---- 发送 ---- */
	if ( !stream_send_all(&Stream, Request.Data, Request.Size, pHttp->uTimeoutUs) ) {
		transport_error("http transport send failed");
		goto done;
	}
	ResponseDeadline = xrtDeadlineAfter(pHttp->uTimeoutUs);

	/* ---- 接收头 ---- */
	xrtHttp1LimitsInit(&Limits);
	Limits.MaxFields = XOAUTH2_HTTP_FIELD_MAX;
	memset(&Head, 0, sizeof(Head));
	Head.Fields = Fields;
	Head.FieldCapacity = XOAUTH2_HTTP_FIELD_MAX;
	while ( !bHeadDone ) {
		xhttp1status eStatus = xrtHttp1ResponseParse(
			xrtBufferView(&Received), &Head, &Limits, &ProtocolError);
		if ( eStatus == XHTTP1_READY ) { bHeadDone = true; break; }
		if ( eStatus != XHTTP1_MORE ) {
			xoauth2__error(XOAUTH2_ERROR_NETWORK,
				"http transport response head invalid");
			goto done;
		}
		{
			int iGot = stream_recv(&Stream, Chunk, sizeof(Chunk),
				&iUsed, ResponseDeadline);
			if ( iGot < 0 ) {
				transport_error(
					iGot == -1 ? "http transport response head timeout" :
					"http transport response head read failed");
				goto done;
			}
			if ( iGot == 0 ) {
				transport_error(
					"http transport response head truncated");
				goto done;
			}
			if ( !xrtBufferAppend(&Received, (xbytesview){ Chunk, iUsed }) )
				goto done;
		}
	}
	*piStatus = (int)Head.Status;

	/* ---- 接收体 ---- */
	if ( !xrtHttp1ResponseBodyPlan(
		&Head, (xstrview){ sMethod, strlen(sMethod) }, &Plan) ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK,
			"http transport body plan failed");
		goto done;
	}
	xrtHttp1BodyLimitsInit(&BodyLimits);
	BodyLimits.MaxBody = XOAUTH2_HTTP_MAX_BODY;   /* 默认 UINT64_MAX，收紧 */
	BodyLimits.MaxTrailers = XOAUTH2_HTTP_FIELD_MAX;
	/* Body Plan 已保存分帧事实，Header 字段不再使用，存储可复用于 trailer。 */
	if ( !xrtHttp1BodyInit(&Body, &Plan, Fields, XOAUTH2_HTTP_FIELD_MAX, &BodyLimits) ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK,
			"http transport response body invalid");
		goto done;
	}
	iUsed = Head.Bytes;
	for ( ;; ) {
		xbytesview Data;
		eBody = xrtHttp1BodyRead(&Body,
			(xbytesview){ (uint8*)Received.Data + iUsed, Received.Size - iUsed },
			bStreamEnd, &iConsumed, &Data, &ProtocolError);
		iUsed += iConsumed;
		if ( eBody == XHTTP1_BODY_ERROR || eBody == XHTTP1_BODY_FIELDS ) {
			xoauth2__error(XOAUTH2_ERROR_NETWORK,
				"http transport response body invalid");
			goto done;
		}
		if ( eBody == XHTTP1_BODY_DATA ) {
			/* 回调交付 C 字符串，不能把含 NUL 的线格式正文悄然截成有效前缀。 */
			if ( Data.Size != 0 && memchr(Data.Data, 0, Data.Size) != NULL ) {
				xoauth2__error(XOAUTH2_ERROR_NETWORK,
				"http transport response body invalid");
				goto done;
			}
			if ( !xrtBufferAppend(&BodyBuffer, Data) ) goto done;
			continue;
		}
		if ( eBody == XHTTP1_BODY_DONE ) break;
		if ( bStreamEnd ) {
			xoauth2__error(XOAUTH2_ERROR_NETWORK,
				"http transport response body truncated");
			goto done;
		}
		{
			size_t iGot = 0;
			/* 借用正文已复制、Header 不再使用，仅保留尚未消费的 trailer 等字节。 */
			if ( iUsed != 0 ) {
				if ( !xrtBufferRemove(&Received, 0, iUsed) ) goto done;
				iUsed = 0;
			}
			int iResult = stream_recv(&Stream, Chunk, sizeof(Chunk),
				&iGot, ResponseDeadline);
			if ( iResult < 0 ) {
				transport_error(
						iResult == -1 ? "http transport response body timeout" :
						"http transport response body read failed");
				goto done;
			}
			if ( iResult == 0 ) { bStreamEnd = true; continue; }
			if ( !xrtBufferAppend(&Received, (xbytesview){ Chunk, iGot }) )
				goto done;
		}
	}

	*psResponseBody = (char*)xrtMalloc(BodyBuffer.Size + 1);
	if ( *psResponseBody == NULL ) goto done;
	if ( BodyBuffer.Size > 0 )
		memcpy(*psResponseBody, BodyBuffer.Data, BodyBuffer.Size);
	(*psResponseBody)[BodyBuffer.Size] = '\0';
	bOk = true;
	goto done;

connect_fail:
	if ( pFuture != NULL ) {
		const xerror* pError = xrtFutureError(pFuture);
		if ( pError != NULL ) xrtSetError(pError);
	}
	transport_error("http transport connect failed");
done:
	if ( !bOk ) pFailure = xrtErrorRef(xrtGetError());
	if ( pFuture != NULL ) future_finish(pFuture, bOk);
	stream_close(&Stream, !bOk);
	xrtBufferUnit(&Request);
	xrtBufferUnit(&Received);
	xrtBufferUnit(&BodyBuffer);
	if ( !bOk ) {
		if ( *psResponseBody != NULL ) {
			xrtFree(*psResponseBody);
			*psResponseBody = NULL;
		}
		*piStatus = 0;
	}
	if ( pFailure != NULL ) xrtSetErrorTake(pFailure);
	return bOk;
}
