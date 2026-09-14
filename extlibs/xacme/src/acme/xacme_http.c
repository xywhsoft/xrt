#include "../internal/xacme_http.h"

#if defined(XACME_FEATURE_ACME_HTTP)

#include <xrt/buffer.h>
#include <xrt/http1.h>
#include <xrt/net.h>
#include <xrt/tcp.h>
#include <xrt/thread.h>
#include <xrt/tls_client.h>
#include <xrt/tls_stream.h>
#include <xrt/tls_verify.h>
#include <xrt/x509.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XACME_HTTP_FIELD_MAX 32u
#define XACME_HTTP_IO_CHUNK 16384u

static void xacmeHttpError(
	xerrkind Kind, xacmehttperror Code, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.http", (int32)Code, sMessage);
}

/* ------------------------------------------------------------------ */
/* 初始化                                                              */
/* ------------------------------------------------------------------ */

bool xacmeHttpInit(
	xacmehttp* pHttp, struct xnetengine* pBorrowedEngine,
	cstr sCaPem, uint64 uTimeoutUs)
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
	pHttp->uTimeoutUs = (uTimeoutUs != 0u) ?
		uTimeoutUs : UINT64_C(30000000);

	if(pBorrowedEngine != NULL)
	{
		pHttp->pEngine = pBorrowedEngine;
	}
	else
	{
		xnetengineconfig Engine;
		xrtNetEngineConfigInit(&Engine);
		pHttp->pEngine = xrtNetEngineCreate(&Engine);
		if((pHttp->pEngine == NULL) ||
			!xrtNetEngineStart(pHttp->pEngine))
		{
			pHttp->pEngine = NULL;
			goto Failure;
		}
		pHttp->bEngineOwned = true;
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
	return true;

Failure:
	xacmeHttpError(
		XERR_STATE, XACME_HTTP_ERROR_CONNECT,
		"acme http init failed");
	xacmeHttpUnit(pHttp);
	return false;
}

void xacmeHttpUnit(xacmehttp* pHttp)
{
	if(pHttp == NULL)
	{
		return;
	}
	if(pHttp->pResolver != NULL)
	{
		(void)xrtNetResolverDestroy(pHttp->pResolver);
		pHttp->pResolver = NULL;
	}
	if(pHttp->bEngineOwned && (pHttp->pEngine != NULL))
	{
		(void)xrtNetEngineStop(pHttp->pEngine);
		(void)xrtNetEngineDestroy(pHttp->pEngine);
	}
	pHttp->pEngine = NULL;
	pHttp->bEngineOwned = false;
	if(pHttp->pVerifier != NULL)
	{
		xrtTlsVerifierRelease(pHttp->pVerifier);
		pHttp->pVerifier = NULL;
	}
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
} xacmeurl;

static bool xacmeUrlParse(cstr sUrl, xacmeurl* pOut)
{
	const char* sHost;
	size_t iHostLen;
	const char* sPort;
	const char* sSlash;

	memset(pOut, 0, sizeof(*pOut));
	if(sUrl == NULL)
	{
		return false;
	}
	if(strncmp(sUrl, "https://", 8u) == 0)
	{
		pOut->bTls = true;
		sHost = sUrl + 8;
	}
	else if(strncmp(sUrl, "http://", 7u) == 0)
	{
		pOut->bTls = false;
		sHost = sUrl + 7;
	}
	else
	{
		return false;
	}
	pOut->iPort = pOut->bTls ? 443u : 80u;
	sSlash = strchr(sHost, '/');
	if(sSlash != NULL)
	{
		iHostLen = (size_t)(sSlash - sHost);
		if((strlen(sSlash) >= sizeof(pOut->sPath)))
		{
			return false;
		}
		strcpy(pOut->sPath, sSlash);
	}
	else
	{
		iHostLen = strlen(sHost);
		strcpy(pOut->sPath, "/");
	}
	sPort = memchr(sHost, ':', iHostLen);
	if(sPort != NULL)
	{
		long v = atol(sPort + 1);
		if((v <= 0) || (v > 65535))
		{
			return false;
		}
		pOut->iPort = (uint16)v;
		iHostLen = (size_t)(sPort - sHost);
	}
	if((iHostLen == 0) || (iHostLen >= sizeof(pOut->sHost)) ||
		(memchr(sHost, '@', iHostLen) != NULL))
	{
		return false;
	}
	memcpy(pOut->sHost, sHost, iHostLen);
	return true;
}

/* ------------------------------------------------------------------ */
/* 流封装：任意线程安全的 future 化 IO                                   */
/* ------------------------------------------------------------------ */

typedef struct xacmestream {
	xtlsstream* pTls;
	xnetstream* pTcp;
} xacmestream;

static bool xacmeFutureWait(xfuture* pFuture, uint64 uUs)
{
	xwaitresult eWait = xrtFutureWaitFor(pFuture, uUs);
	xrtFutureDestroy(pFuture);
	return eWait == XWAIT_OK;
}

/* 发送全部字节；TLS 用 SendAsync（任意线程），明文用复制语义 Send。 */
static bool xacmeStreamSendAll(
	xacmestream* pStream, const void* pData, size_t iSize, uint64 uUs)
{
	size_t iOffset = 0u;
	while(iOffset < iSize)
	{
		size_t iChunk = iSize - iOffset;
		if(iChunk > XACME_HTTP_IO_CHUNK)
		{
			iChunk = XACME_HTTP_IO_CHUNK;
		}
		if(pStream->pTls != NULL)
		{
			xfuture* pFuture = xrtTlsStreamSendAsync(
				pStream->pTls, (const uint8*)pData + iOffset, iChunk);
			if((pFuture == NULL) || !xacmeFutureWait(pFuture, uUs))
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
				pFuture = xrtNetStreamWaitAsync(
					pStream->pTcp, XNET_STREAM_WAIT_WRITE);
				if((pFuture == NULL) || !xacmeFutureWait(pFuture, uUs))
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

/* 返回 1=读到数据，0=流结束，-1=错误/超时；数据复制到调用方缓冲。 */
static int xacmeStreamRecv(
	xacmestream* pStream, uint8* pBuffer, size_t iCapacity,
	size_t* pRead, uint64 uUs)
{
	xfuture* pFuture;
	xnetbytes* pBytes;

	if(pStream->pTls != NULL)
	{
		pFuture = xrtTlsStreamRecvAsync(pStream->pTls, iCapacity);
	}
	else
	{
		pFuture = xrtNetStreamRecvAsync(pStream->pTcp, iCapacity);
	}
	if(pFuture == NULL)
	{
		return -1;
	}
	if(xrtFutureWaitFor(pFuture, uUs) != XWAIT_OK)
	{
		xrtFutureDestroy(pFuture);
		return -1;
	}
	pBytes = (xnetbytes*)xrtFutureValue(pFuture);
	if((pBytes == NULL) || (xrtNetBytesView(pBytes).Size == 0u))
	{
		xrtFutureDestroy(pFuture);
		/* 0 字节不等于 EOF：仅在流确已离开 OPEN 态时判定结束。 */
		{
			bool bEnd;
			if(pStream->pTls != NULL)
			{
				xtlsstreamstate eState = xrtTlsStreamState(pStream->pTls);
				bEnd = (eState != XTLS_STREAM_OPEN) &&
					(eState != XTLS_STREAM_CONNECTING) &&
					(eState != XTLS_STREAM_HANDSHAKE);
			}
			else
			{
				bEnd = false; /* 明文流状态另查，暂按等待 CLOSE 判定。 */
				xfuture* pClose = xrtNetStreamWaitAsync(
					pStream->pTcp, XNET_STREAM_WAIT_READ);
				if((pClose != NULL) && xacmeFutureWait(pClose, uUs))
				{
					bEnd = xrtNetStreamState(pStream->pTcp) !=
						XNET_STREAM_OPEN;
				}
			}
			if(bEnd)
			{
				return 0;
			}
			/* 短暂让步后再试一轮（数据尚在路上）。 */
			xrtSleep(5u);
			return xacmeStreamRecv(
				pStream, pBuffer, iCapacity, pRead, uUs);
		}
	}
	if(xrtNetBytesView(pBytes).Size > iCapacity)
	{
		xrtFutureDestroy(pFuture);
		return -1;
	}
	memcpy(pBuffer, xrtNetBytesView(pBytes).Data, xrtNetBytesView(pBytes).Size);
	*pRead = xrtNetBytesView(pBytes).Size;
	xrtFutureDestroy(pFuture);
	return 1;
}

static void xacmeStreamClose(xacmestream* pStream)
{
	if(pStream->pTls != NULL)
	{
		(void)xrtTlsStreamClose(pStream->pTls);
		xrtTlsStreamDestroy(pStream->pTls);
		pStream->pTls = NULL;
	}
	if(pStream->pTcp != NULL)
	{
		(void)xrtNetStreamClose(pStream->pTcp);
		xrtNetStreamDestroy(pStream->pTcp);
		pStream->pTcp = NULL;
	}
}

/* ------------------------------------------------------------------ */
/* 交换                                                               */
/* ------------------------------------------------------------------ */

static str xacmeHeaderTake(const xhttp1head* pHead, cstr sName)
{
	{
		const xhttpfield* pField = xrtHttpFieldGet(
			pHead->Fields, pHead->FieldCount,
			(xstrview){ sName, strlen(sName) });
		str sValue;
		if((pField == NULL) || (pField->Value.Data == NULL))
		{
			return NULL;
		}
		sValue = (str)xrtMalloc(pField->Value.Size + 1u);
		if(sValue == NULL)
		{
			return NULL;
		}
		memcpy(sValue, pField->Value.Data, pField->Value.Size);
		sValue[pField->Value.Size] = '\0';
		return sValue;
	}
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
	bool bOk = false;
	bool bHeadDone = false;
	bool bStreamEnd = false;

	if((pHttp == NULL) || (sMethod == NULL) || (sUrl == NULL) ||
		(pResponse == NULL))
	{
		xacmeHttpError(
			XERR_ARGUMENT, XACME_HTTP_ERROR_ARGUMENT,
			"acme http exchange requires http, method, url and response");
		return false;
	}
	memset(pResponse, 0, sizeof(*pResponse));
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
		for(i = 0;
			(i < iExtraCount) && (iField < XACME_HTTP_FIELD_MAX);
			i++)
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
	if((sContentType != NULL) && (sContentType[0] != '\0'))
	{
		Fields[iField++] = (xhttpfield){
			(xstrview){ "Content-Type", 12u },
			(xstrview){ sContentType, strlen(sContentType) } };
	}
	if((sBody.Data != NULL) || (strcmp(sMethod, "POST") == 0))
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
		Tls.ServerName = (xstrview){ Url.sHost, strlen(Url.sHost) };
		Tls.VerifyName = Tls.ServerName;
		Tls.Verifier = pHttp->pVerifier;
		xrtTlsDialConfigInit(&Dial);
		Dial.Timeout = pHttp->uTimeoutUs;
		pFuture = xrtTlsDialAsync(
			pHttp->pEngine, pHttp->pResolver, Url.sHost, Url.iPort,
			&Tls, &Dial, NULL, NULL);
	}
	else
	{
		xnetdialconfig Dial;
		xrtNetDialConfigInit(&Dial);
		Dial.Timeout = pHttp->uTimeoutUs;
		pFuture = xrtNetDialAsync(
			pHttp->pEngine, pHttp->pResolver, Url.sHost, Url.iPort,
			&Dial, NULL, NULL);
	}
	if(pFuture == NULL)
	{
		/* 底层拨号错误已在线程错误里；仅补充域信息。 */
		goto Done;
	}
	if(xrtFutureWaitFor(pFuture, pHttp->uTimeoutUs) != XWAIT_OK ||
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
	if(!xacmeStreamSendAll(
		&Stream, Request.Data, Request.Size, pHttp->uTimeoutUs))
	{
		xacmeHttpError(
			XERR_IO, XACME_HTTP_ERROR_SEND,
			"acme http send failed");
		goto Done;
	}

	/* ---- 接收头 ---- */
	xrtHttp1LimitsInit(&Limits);
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
		if(eStatus == XHTTP1_ERROR)
		{
			xacmeHttpError(
				XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
				"acme http response head invalid");
			goto Done;
		}
		{
			int iGot = xacmeStreamRecv(
				&Stream, Chunk, sizeof(Chunk), &iUsed, pHttp->uTimeoutUs);
			if(iGot < 0)
			{
				xacmeHttpError(
					XERR_TIMEOUT, XACME_HTTP_ERROR_PROTOCOL,
					"acme http response head timeout");
				goto Done;
			}
			if(iGot == 0)
			{
				xacmeHttpError(
					XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
					"acme http response head truncated");
				goto Done;
			}
			if(!xrtBufferAppend(&Received, (xbytesview){ Chunk, iUsed }))
			{
				goto Done;
			}
		}
	}

	/* ---- 响应字段 ---- */
	pResponse->iStatus = Head.Status;
	pResponse->sLocation = xacmeHeaderTake(&Head, "Location");
	pResponse->sReplayNonce = xacmeHeaderTake(&Head, "Replay-Nonce");
	pResponse->sRetryAfter = xacmeHeaderTake(&Head, "Retry-After");
	pResponse->sLink = xacmeHeaderTake(&Head, "Link");
	pResponse->sContentType = xacmeHeaderTake(&Head, "Content-Type");

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
	if(!xrtHttp1BodyInit(&Body, &Plan, NULL, 0u, &BodyLimits))
	{
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
		if(eBody == XHTTP1_BODY_ERROR)
		{
			xacmeHttpError(
				XERR_PROTOCOL, XACME_HTTP_ERROR_PROTOCOL,
				"acme http response body invalid");
			goto Done;
		}
		if(eBody == XHTTP1_BODY_DATA)
		{
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
			int iResult = xacmeStreamRecv(
				&Stream, Chunk, sizeof(Chunk), &iGot, pHttp->uTimeoutUs);
			if(iResult < 0)
			{
				xacmeHttpError(
					XERR_TIMEOUT, XACME_HTTP_ERROR_PROTOCOL,
					"acme http response body timeout");
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
	xacmeStreamClose(&Stream);
	xrtBufferUnit(&Request);
	xrtBufferUnit(&Received);
	xrtBufferUnit(&BodyBuffer);
	return bOk;
}

#endif
