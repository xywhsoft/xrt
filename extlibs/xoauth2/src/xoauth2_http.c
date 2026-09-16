/* xoauth2 便捷传输：直连 xrt net engine + TLS + http1。
 * 模式取自 xacme_http（单次 POST、Connection: close、future 化 IO），
 * 按 oauth2 场景裁剪：无重放 nonce / 无重试；POST 带 form 头，GET 无实体头。
 * 支持 http://（明文，测试/内网 IdP）与 https://（系统或指定 CA 验证）。
 * 全部 API 经 xoauth2-xrt.h → <xrt.h> 聚合声明（single 单头或宿主 shim）。 */
#include "xoauth2_internal.h"

#define XOAUTH2_HTTP_FIELD_MAX 16u
#define XOAUTH2_HTTP_IO_CHUNK 16384u
#define XOAUTH2_HTTP_TIMEOUT_DEFAULT 15000000ull  /* 15s */
/* 响应体上限：token/JWKS/userinfo 响应远小于 1MB；
 * 防 malformed Content-Length / 无限 chunked 把内存吃光 */
#define XOAUTH2_HTTP_MAX_BODY (1024u * 1024u)

struct xoauth2httpxrt {
	xnetengine* pEngine;      /* 借用或自建 */
	bool        bEngineOwned;
	xnetresolver* pResolver;
	void*       pVerifier;    /* xtlsverifier*（ opaque 存放，Unit 释放） */
	uint64_t    uTimeoutUs;
};

/* ------------------------------------------------------------------ */
/* URL 解析（http(s)://host[:port]/path）                                */
/* ------------------------------------------------------------------ */
typedef struct xoauth2url {
	char   sHost[256];
	uint16_t iPort;
	char   sPath[1024];
	bool   bTls;
} xoauth2url;

static bool url_parse(const char* sUrl, xoauth2url* pOut)
{
	const char* sHost;
	size_t iHostLen;
	const char* sPort;

	memset(pOut, 0, sizeof(*pOut));
	if ( sUrl == NULL ) return false;
	if ( strncmp(sUrl, "https://", 8) == 0 ) {
		pOut->bTls = true;
		sHost = sUrl + 8;
	}
	else if ( strncmp(sUrl, "http://", 7) == 0 ) {
		pOut->bTls = false;
		sHost = sUrl + 7;
	}
	else {
		return false;
	}
	pOut->iPort = pOut->bTls ? 443u : 80u;
	const char* sSlash = strchr(sHost, '/');
	if ( sSlash != NULL ) {
		iHostLen = (size_t)(sSlash - sHost);
		if ( strlen(sSlash) >= sizeof(pOut->sPath) ) return false;
		strcpy(pOut->sPath, sSlash);
	}
	else {
		iHostLen = strlen(sHost);
		strcpy(pOut->sPath, "/");
	}
	if ( sHost[0] == '[' ) {
		/* IPv6 字面量：[addr] 或 [addr]:port；Host 头按 RFC 9110 保留括号 */
		const char* sClose = (const char*)memchr(sHost, ']', iHostLen);
		size_t iAddr;
		if ( sClose == NULL ) return false;
		iAddr = (size_t)(sClose - sHost) + 1;
		if ( iAddr + 1 >= sizeof(pOut->sHost) ) return false;
		memcpy(pOut->sHost, sHost, iAddr);
		pOut->sHost[iAddr] = 0;
		if ( iAddr < iHostLen ) {
			long v;
			if ( sHost[iAddr] != ':' ) return false;
			v = atol(sHost + iAddr + 1);
			if ( v <= 0 || v > 65535 ) return false;
			pOut->iPort = (uint16_t)v;
		}
		return true;
	}
	sPort = (const char*)memchr(sHost, ':', iHostLen);
	if ( sPort != NULL ) {
		long v = atol(sPort + 1);
		if ( v <= 0 || v > 65535 ) return false;
		pOut->iPort = (uint16_t)v;
		iHostLen = (size_t)(sPort - sHost);
	}
	if ( iHostLen == 0 || iHostLen >= sizeof(pOut->sHost) ||
	     memchr(sHost, '@', iHostLen) != NULL )
		return false;
	memcpy(pOut->sHost, sHost, iHostLen);
	return true;
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

	if ( pBorrowedEngine != NULL ) {
		pHttp->pEngine = (xnetengine*)pBorrowedEngine;
	}
	else {
		xnetengineconfig Engine;
		xrtNetEngineConfigInit(&Engine);
		pHttp->pEngine = xrtNetEngineCreate(&Engine);
		if ( pHttp->pEngine == NULL ) goto fail;
		if ( !xrtNetEngineStart(pHttp->pEngine) ) {
			/* start 失败：destroy 后再置空，避免泄漏未启动的 engine */
			xrtNetEngineDestroy(pHttp->pEngine);
			pHttp->pEngine = NULL;
			goto fail;
		}
		pHttp->bEngineOwned = true;
	}
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
	return true;

fail:
	xoauth2HttpXrtUnit(pHttp);
	xoauth2__error(XOAUTH2_ERROR_NETWORK, "http transport init failed");
	return false;
}

xoauth2httpxrt* xoauth2HttpXrtCreate(void* pBorrowedEngine,
                                     const char* sCaPem, uint64_t uTimeoutUs)
{
	xoauth2httpxrt* pHttp = (xoauth2httpxrt*)xrtMalloc(sizeof(xoauth2httpxrt));
	if ( pHttp == NULL ) return NULL;
	if ( !xoauth2HttpXrtInit(pHttp, pBorrowedEngine, sCaPem, uTimeoutUs) ) {
		xrtFree(pHttp);
		return NULL;
	}
	return pHttp;
}

void xoauth2HttpXrtDestroy(xoauth2httpxrt* pHttp)
{
	if ( pHttp == NULL ) return;
	xoauth2HttpXrtUnit(pHttp);
	xrtFree(pHttp);
}

void xoauth2HttpXrtUnit(xoauth2httpxrt* pHttp)
{
	if ( pHttp == NULL ) return;
	if ( pHttp->pResolver != NULL ) {
		xrtNetResolverDestroy(pHttp->pResolver);
		pHttp->pResolver = NULL;
	}
	if ( pHttp->bEngineOwned && pHttp->pEngine != NULL ) {
		xrtNetEngineStop(pHttp->pEngine);
		xrtNetEngineDestroy(pHttp->pEngine);
	}
	pHttp->pEngine = NULL;
	pHttp->bEngineOwned = false;
	if ( pHttp->pVerifier != NULL ) {
		xrtTlsVerifierRelease((xtlsverifier*)pHttp->pVerifier);
		pHttp->pVerifier = NULL;
	}
}

/* ------------------------------------------------------------------ */
/* 流封装（future 化 IO，同 xacme）                                      */
/* ------------------------------------------------------------------ */
typedef struct xoauth2stream {
	xtlsstream* pTls;
	xnetstream* pTcp;
} xoauth2stream;

static bool future_wait(xfuture* pFuture, uint64_t uUs)
{
	xwaitresult eWait = xrtFutureWaitFor(pFuture, uUs);
	xrtFutureDestroy(pFuture);
	return eWait == XWAIT_OK;
}

static bool stream_send_all(xoauth2stream* pStream, const void* pData,
                            size_t iSize, uint64_t uUs)
{
	size_t iOffset = 0;
	while ( iOffset < iSize ) {
		size_t iChunk = iSize - iOffset;
		if ( iChunk > XOAUTH2_HTTP_IO_CHUNK ) iChunk = XOAUTH2_HTTP_IO_CHUNK;
		if ( pStream->pTls != NULL ) {
			xfuture* pF = xrtTlsStreamSendAsync(
				pStream->pTls, (const uint8*)pData + iOffset, iChunk);
			if ( pF == NULL || !future_wait(pF, uUs) ) return false;
		}
		else {
			xnetresult eResult;
			xfuture* pF;
			while ( (eResult = xrtNetStreamSend(
				pStream->pTcp, (const uint8*)pData + iOffset, iChunk))
				== XNET_RESULT_AGAIN ) {
				pF = xrtNetStreamWaitAsync(
					pStream->pTcp, XNET_STREAM_WAIT_WRITE);
				if ( pF == NULL || !future_wait(pF, uUs) ) return false;
			}
			if ( eResult != XNET_RESULT_OK ) return false;
		}
		iOffset += iChunk;
	}
	return true;
}

/* 返回 1=有数据 0=流结束 -1=失败/超时 */
static int stream_recv(xoauth2stream* pStream, uint8* pBuffer,
                       size_t iCapacity, size_t* pRead, uint64_t uUs)
{
	for ( ;; ) {
		xfuture* pFuture;
		xnetbytes* pBytes;
		if ( pStream->pTls != NULL )
			pFuture = xrtTlsStreamRecvAsync(pStream->pTls, iCapacity);
		else
			pFuture = xrtNetStreamRecvAsync(pStream->pTcp, iCapacity);
		if ( pFuture == NULL ) return -1;
		if ( xrtFutureWaitFor(pFuture, uUs) != XWAIT_OK ) {
			xrtFutureDestroy(pFuture);
			return -1;
		}
		pBytes = (xnetbytes*)xrtFutureValue(pFuture);
		if ( pBytes == NULL || xrtNetBytesView(pBytes).Size == 0 ) {
			bool bEnd;
			xrtFutureDestroy(pFuture);
			if ( pStream->pTls != NULL ) {
				xtlsstreamstate eState = xrtTlsStreamState(pStream->pTls);
				bEnd = (eState != XTLS_STREAM_OPEN) &&
				       (eState != XTLS_STREAM_CONNECTING) &&
				       (eState != XTLS_STREAM_HANDSHAKE);
			}
			else {
				xnetstreamstate eState = xrtNetStreamState(pStream->pTcp);
				bEnd = (eState != XNET_STREAM_OPEN);
			}
			if ( bEnd ) return 0;
			continue;   /* 0 字节但流仍开：再试 */
		}
		{
			xbytesview View = xrtNetBytesView(pBytes);
			memcpy(pBuffer, View.Data, View.Size);
			*pRead = View.Size;
		}
		xrtFutureDestroy(pFuture);
		return 1;
	}
}

static void stream_close(xoauth2stream* pStream)
{
	if ( pStream->pTls != NULL ) {
		xrtTlsStreamClose(pStream->pTls);
		pStream->pTls = NULL;
	}
	if ( pStream->pTcp != NULL ) {
		xrtNetStreamClose(pStream->pTcp);
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
	bool bOk = false, bHeadDone = false, bStreamEnd = false;
	size_t iField = 0;

	if ( pHttp == NULL || sUrl == NULL || psResponseBody == NULL ||
	     piStatus == NULL || sMethod == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "http transport: null argument");
		return false;
	}
	*psResponseBody = NULL;
	*piStatus = 0;
	if ( !url_parse(sUrl, &Url) ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"http transport url is not http(s) absolute");
		return false;
	}
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
		Tls.ServerName = (xstrview){ Url.sHost, strlen(Url.sHost) };
		Tls.VerifyName = Tls.ServerName;
		Tls.Verifier = (xtlsverifier*)pHttp->pVerifier;
		xrtTlsDialConfigInit(&Dial);
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
	if ( pFuture == NULL ) goto done;
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
		xoauth2__error(XOAUTH2_ERROR_NETWORK, "http transport send failed");
		goto done;
	}

	/* ---- 接收头 ---- */
	xrtHttp1LimitsInit(&Limits);
	memset(&Head, 0, sizeof(Head));
	Head.Fields = Fields;
	Head.FieldCapacity = XOAUTH2_HTTP_FIELD_MAX;
	while ( !bHeadDone ) {
		xhttp1status eStatus = xrtHttp1ResponseParse(
			xrtBufferView(&Received), &Head, &Limits, &ProtocolError);
		if ( eStatus == XHTTP1_READY ) { bHeadDone = true; break; }
		if ( eStatus == XHTTP1_ERROR ) {
			xoauth2__error(XOAUTH2_ERROR_NETWORK,
				"http transport response head invalid");
			goto done;
		}
		{
			int iGot = stream_recv(&Stream, Chunk, sizeof(Chunk),
				&iUsed, pHttp->uTimeoutUs);
			if ( iGot < 0 ) {
				xoauth2__error(XOAUTH2_ERROR_NETWORK,
					"http transport response head timeout");
				goto done;
			}
			if ( iGot == 0 ) {
				xoauth2__error(XOAUTH2_ERROR_NETWORK,
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
	if ( !xrtHttp1BodyInit(&Body, &Plan, NULL, 0, &BodyLimits) )
		goto done;
	iUsed = Head.Bytes;
	for ( ;; ) {
		xbytesview Data;
		eBody = xrtHttp1BodyRead(&Body,
			(xbytesview){ (uint8*)Received.Data + iUsed, Received.Size - iUsed },
			bStreamEnd, &iConsumed, &Data, &ProtocolError);
		iUsed += iConsumed;
		if ( eBody == XHTTP1_BODY_ERROR ) {
			xoauth2__error(XOAUTH2_ERROR_NETWORK,
				"http transport response body invalid");
			goto done;
		}
		if ( eBody == XHTTP1_BODY_DATA ) {
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
			int iResult = stream_recv(&Stream, Chunk, sizeof(Chunk),
				&iGot, pHttp->uTimeoutUs);
			if ( iResult < 0 ) {
				xoauth2__error(XOAUTH2_ERROR_NETWORK,
					"http transport response body timeout");
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
	xoauth2__error(XOAUTH2_ERROR_NETWORK, "http transport connect failed");
done:
	if ( pFuture != NULL ) xrtFutureDestroy(pFuture);
	stream_close(&Stream);
	xrtBufferUnit(&Request);
	xrtBufferUnit(&Received);
	xrtBufferUnit(&BodyBuffer);
	if ( !bOk && *psResponseBody != NULL ) {
		xrtFree(*psResponseBody);
		*psResponseBody = NULL;
	}
	return bOk;
}
