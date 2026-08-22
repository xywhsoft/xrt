#include "../internal/xrt_http_server.h"

#include <xrt/http_trailer.h>

#include <xrt/http_connection.h>
#include <xrt/http_expect.h>
#include <xrt/http_te.h>



#if defined(XRT_FEATURE_HTTP_SERVER_EXCHANGE)

/* 推进 CRLFCRLF 四字节边界匹配器。 */
static uint32 __xrtHttp1ServerDelimiterStep(
	uint32 iState,
	unsigned char iByte
)
{
	switch ( iState ) {
		case 0:
			return iByte == (unsigned char)'\r' ? 1u : 0u;

		case 1:
			if ( iByte == (unsigned char)'\n' ) {
				return 2u;
			}
			return iByte == (unsigned char)'\r' ? 1u : 0u;

		case 2:
			return iByte == (unsigned char)'\r' ? 3u : 0u;

		case 3:
			if ( iByte == (unsigned char)'\n' ) {
				return 4u;
			}
			return iByte == (unsigned char)'\r' ? 1u : 0u;

		default:
			return 4u;
	}
}



/* 扫描本段 Header 输入，并返回到完整边界为止的字节数。 */
static size_t __xrtHttp1ServerDelimiterScan(
	xhttp1serverexchange* pExchange,
	xbytesview Input,
	bool* pReady
)
{
	size_t i;

	*pReady = false;
	for ( i = 0; i < Input.Size; i++ ) {
		pExchange->Delimiter = __xrtHttp1ServerDelimiterStep(
			pExchange->Delimiter,
			Input.Data[i]
		);
		if ( pExchange->Delimiter == 4 ) {
			*pReady = true;
			return i + 1u;
		}
	}
	return Input.Size;
}



/* 按实际跨段 Header 长度扩大临时缓冲。 */
static bool __xrtHttp1ServerHeadAppend(
	xhttp1serverexchange* pExchange,
	xbytesview Data
)
{
	size_t iRequired;
	size_t iCapacity;
	bytes pBuffer;

	if ( Data.Size == 0 ) {
		return true;
	}
	if ( (pExchange->HeadBuffer.Size >
		  (SIZE_MAX - Data.Size)) ||
		((pExchange->HeadBuffer.Size + Data.Size) >
		 pExchange->Config.Head.MaxHead) ) {
		return __xrtHttp1ServerExchangeFail(
			pExchange,
			XHTTP1_SERVER_ERROR_HEAD,
			XERR_RANGE,
			"buffer-http1-server-head",
			"HTTP server request Header exceeds its limit"
		);
	}
	iRequired = pExchange->HeadBuffer.Size + Data.Size;
	if ( iRequired > pExchange->HeadBuffer.Capacity ) {
		iCapacity = pExchange->HeadBuffer.Capacity;
		if ( iCapacity == 0 ) {
			iCapacity = iRequired;
		}
		while ( iCapacity < iRequired ) {
			size_t iNext = iCapacity > (SIZE_MAX / 2u) ?
				iRequired : (iCapacity * 2u);

			if ( iNext <= iCapacity ) {
				iCapacity = iRequired;
				break;
			}
			iCapacity = iNext;
		}
		if ( iCapacity > pExchange->Config.Head.MaxHead ) {
			iCapacity = pExchange->Config.Head.MaxHead;
		}
		pBuffer = (bytes)xrtRealloc(
			pExchange->HeadBuffer.Data,
			iCapacity
		);
		if ( pBuffer == NULL ) {
			return __xrtHttp1ServerExchangeFailCause(
				pExchange,
				XHTTP1_SERVER_ERROR_HEAD,
				XERR_MEMORY,
				"buffer-http1-server-head",
				"HTTP server request Header buffer allocation failed",
				xrtGetError()
			);
		}
		pExchange->HeadBuffer.Data = pBuffer;
		pExchange->HeadBuffer.Capacity = iCapacity;
	}
	memcpy(
		pExchange->HeadBuffer.Data +
			pExchange->HeadBuffer.Size,
		Data.Data,
		Data.Size
	);
	pExchange->HeadBuffer.Size = iRequired;
	return true;
}



/* 解析 Header，并按实际字段数量创建一次临时描述符数组。 */
static bool __xrtHttp1ServerHeadParse(
	xhttp1serverexchange* pExchange,
	xbytesview HeadInput,
	xhttp1head* pHead
)
{
	xhttp1status Status;
	size_t iRequired;

	xrtHttp1HeadInit(pHead, NULL, 0);
	Status = xrtHttp1RequestParse(
		HeadInput,
		pHead,
		&pExchange->Config.Head,
		NULL
	);
	if ( Status == XHTTP1_FIELDS ) {
		iRequired = pHead->FieldCount;
		if ( iRequired > (SIZE_MAX / sizeof(xhttpfield)) ) {
			__xrtErrorSetSizeOverflow();
			return __xrtHttp1ServerExchangeFailCause(
				pExchange,
				XHTTP1_SERVER_ERROR_HEAD,
				XERR_RANGE,
				"parse-http1-server-head",
				"HTTP server request field count overflowed",
				xrtGetError()
			);
		}
		pExchange->ParseFields = (xhttpfield*)xrtMalloc(
			iRequired * sizeof(xhttpfield)
		);
		if ( pExchange->ParseFields == NULL ) {
			return __xrtHttp1ServerExchangeFailCause(
				pExchange,
				XHTTP1_SERVER_ERROR_HEAD,
				XERR_MEMORY,
				"parse-http1-server-head",
				"HTTP server request field descriptors could not be allocated",
				xrtGetError()
			);
		}
		pExchange->ParseFieldCapacity = iRequired;
		xrtHttp1HeadInit(
			pHead,
			pExchange->ParseFields,
			pExchange->ParseFieldCapacity
		);
		Status = xrtHttp1RequestParse(
			HeadInput,
			pHead,
			&pExchange->Config.Head,
			NULL
		);
	}
	if ( Status != XHTTP1_READY ) {
		return __xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_HEAD,
			XERR_PROTOCOL,
			"parse-http1-server-head",
			"HTTP server request Header is invalid",
			xrtGetError()
		);
	}
	return true;
}



/* 执行 HTTP/1.1 必需 Host 与 HTTP/1.0 可选单 Host 规则。 */
static bool __xrtHttp1ServerHostValid(
	xhttp1serverexchange* pExchange,
	const xhttp1head* pHead
)
{
	const xhttpfield* pHost;
	size_t iCount = xrtHttpFieldCount(
		pHead->Fields,
		pHead->FieldCount,
		XRT_STR_LITERAL("Host")
	);

	if ( ((pHead->Version == XHTTP_VERSION_1_1) &&
		 (iCount != 1)) ||
		((pHead->Version == XHTTP_VERSION_1_0) &&
		 (iCount > 1)) ) {
		return __xrtHttp1ServerExchangeFail(
			pExchange,
			XHTTP1_SERVER_ERROR_HOST,
			XERR_PROTOCOL,
			"validate-http1-server-host",
			"HTTP server request has a missing or repeated Host field"
		);
	}
	if ( iCount == 0 ) {
		return true;
	}
	pHost = xrtHttpFieldGet(
		pHead->Fields,
		pHead->FieldCount,
		XRT_STR_LITERAL("Host")
	);
	if ( (pHost == NULL) || !xrtHttpHostValid(pHost->Value) ) {
		return __xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_HOST,
			XERR_PROTOCOL,
			"validate-http1-server-host",
			"HTTP server Host field is not one valid authority",
			xrtGetError()
		);
	}
	return true;
}



/* 验证方法与四种 request-target 形式的组合。 */
static bool __xrtHttp1ServerTargetValid(
	xhttp1serverexchange* pExchange,
	const xhttp1head* pHead
)
{
	xhttptarget Target;

	if ( xrtHttpTargetParse(
		pHead->Method,
		pHead->Target,
		&Target
	) ) {
		return true;
	}
	return __xrtHttp1ServerExchangeFailCause(
		pExchange,
		XHTTP1_SERVER_ERROR_TARGET,
		XERR_PROTOCOL,
		"validate-http1-server-target",
		"HTTP server request-target form does not match its method",
		xrtGetError()
	);
}



/* 验证全部 Expect 值并返回是否请求 100 Continue。 */
static bool __xrtHttp1ServerExpect(
	xhttp1serverexchange* pExchange,
	const xhttp1head* pHead,
	bool* pContinue
)
{
	xhttpexpectresult Result;

	*pContinue = false;
	Result = xrtHttpExpectFields(
		pHead->Fields, pHead->FieldCount
	);
	if ( Result == XHTTP_EXPECT_NONE ) {
		return true;
	}
	if ( Result == XHTTP_EXPECT_ERROR ) {
		return __xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_EXPECTATION,
			XERR_PROTOCOL,
			"validate-http1-server-expect",
			"HTTP server request contains an invalid Expect field",
			xrtGetError()
		);
	}
	if ( Result == XHTTP_EXPECT_UNSUPPORTED ) {
		return __xrtHttp1ServerExchangeFail(
			pExchange,
			XHTTP1_SERVER_ERROR_EXPECTATION,
			XERR_PROTOCOL,
			"validate-http1-server-expect",
			"HTTP server request uses an unsupported expectation"
		);
	}
	if ( pHead->Version == XHTTP_VERSION_1_1 ) {
		*pContinue = true;
	}
	return true;
}



/* 验证 TE，并只对正确逐跳声明的 HTTP/1.1 请求发布 Trailer 能力。 */
static bool __xrtHttp1ServerTe(
	xhttp1serverexchange* pExchange,
	const xhttp1head* pHead,
	bool* pAcceptsTrailers
)
{
	xhttpteinfo Info;
	xhttpnext Next;

	*pAcceptsTrailers = false;
	if ( !xrtHttpTeParse(
		pHead->Fields, pHead->FieldCount, &Info
	) ) {
		return __xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_TE,
			XERR_PROTOCOL,
			"validate-http1-server-te",
			"HTTP server request contains an invalid TE field",
			xrtGetError()
		);
	}
	if ( (pHead->Version != XHTTP_VERSION_1_1) ||
		((Info.Flags & XHTTP_TE_ACCEPTS_TRAILERS) == 0) ) {
		return true;
	}
	Next = xrtHttpConnectionFind(
		pHead->Fields,
		pHead->FieldCount,
		XRT_STR_LITERAL("TE")
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return __xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_TE,
			XERR_PROTOCOL,
			"validate-http1-server-te",
			"HTTP server Connection field is invalid",
			xrtGetError()
		);
	}
	*pAcceptsTrailers = Next == XHTTP_NEXT_ITEM;
	return true;
}



/* 在发布请求 Header 前完整验证 Trailer 声明。 */
static bool __xrtHttp1ServerTrailer(
	xhttp1serverexchange* pExchange,
	const xhttp1head* pHead
)
{
	size_t iTrailerNames;

	if ( xrtHttpTrailerCount(
		pHead->Fields, pHead->FieldCount, &iTrailerNames
	) ) {
		return true;
	}
	return __xrtHttp1ServerExchangeFailCause(
		pExchange,
		XHTTP1_SERVER_ERROR_TRAILER,
		XERR_PROTOCOL,
		"validate-http1-server-trailer",
		"HTTP server request contains an invalid Trailer declaration",
		xrtGetError()
	);
}



/* 创建拥有请求并发布 Headers 正文策略。 */
static xhttp1serverfeedstatus __xrtHttp1ServerHeadReady(
	xhttp1serverexchange* pExchange,
	xbytesview HeadInput
)
{
	xhttp1bodylimits BodyLimits;
	xhttp1bodyplan Plan;
	xhttp1head Head;
	xhttpserverbodypolicy Policy = XHTTP_SERVER_BODY_BUFFER;
	uint32 iFlags = XHTTP_SERVER_REQUEST_NONE;
	bool bAcceptsTrailers;
	bool bContinue;

	if ( !__xrtHttp1ServerHeadParse(
		pExchange, HeadInput, &Head
	) || !__xrtHttp1ServerTargetValid(
		pExchange, &Head
	) || !__xrtHttp1ServerHostValid(
		pExchange, &Head
	) || !__xrtHttp1ServerExpect(
		pExchange, &Head, &bContinue
	) || !__xrtHttp1ServerTe(
		pExchange, &Head, &bAcceptsTrailers
	) || !__xrtHttp1ServerTrailer(
		pExchange, &Head
	) ) {
		return XHTTP1_SERVER_FEED_ERROR;
	}
	if ( ((Head.Flags & (uint32)XHTTP1_TRANSFER_OTHER) != 0) &&
		!pExchange->Config.AllowRawTransferCodings ) {
		(void)__xrtHttp1ServerExchangeFail(
			pExchange,
			XHTTP1_SERVER_ERROR_FRAMING,
			XERR_UNSUPPORTED,
			"frame-http1-server-request",
			"HTTP server request uses an unsupported Transfer-Encoding"
		);
		return XHTTP1_SERVER_FEED_ERROR;
	}
	if ( !xrtHttp1RequestBodyPlan(&Head, &Plan) ) {
		(void)__xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_FRAMING,
			XERR_PROTOCOL,
			"frame-http1-server-request",
			"HTTP server request body framing is invalid",
			xrtGetError()
		);
		return XHTTP1_SERVER_FEED_ERROR;
	}
	if ( (Head.Flags & (uint32)XHTTP1_KEEP_ALIVE) != 0 ) {
		iFlags |= XHTTP_SERVER_REQUEST_KEEP_ALIVE;
	}
	if ( (Head.Flags & (uint32)XHTTP1_UPGRADE) != 0 ) {
		iFlags |= XHTTP_SERVER_REQUEST_UPGRADE;
	}
	if ( bAcceptsTrailers ) {
		iFlags |= XHTTP_SERVER_REQUEST_ACCEPTS_TRAILERS;
	}
	if ( bContinue &&
		((Plan.Mode == XHTTP1_BODY_CHUNKED) ||
		 ((Plan.Mode == XHTTP1_BODY_FIXED) &&
		  (Plan.Length != 0))) ) {
		iFlags |= XHTTP_SERVER_REQUEST_EXPECT_CONTINUE;
	}
	pExchange->Request = __xrtHttpServerRequestCreate(
		&Head, &Plan, iFlags
	);
	if ( pExchange->Request == NULL ) {
		(void)__xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_HEAD,
			XERR_MEMORY,
			"copy-http1-server-request",
			"HTTP server request metadata could not be stored",
			xrtGetError()
		);
		return XHTTP1_SERVER_FEED_ERROR;
	}
	xrtFree(pExchange->ParseFields);
	pExchange->ParseFields = NULL;
	pExchange->ParseFieldCapacity = 0;

	BodyLimits = pExchange->Config.Body;
	BodyLimits.MaxBody = UINT64_MAX;
	if ( !xrtHttp1BodyInit(
		&pExchange->Body,
		&Plan,
		NULL,
		0,
		&BodyLimits
	) ) {
		(void)__xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_FRAMING,
			XERR_PROTOCOL,
			"init-http1-server-body",
			"HTTP server request body reader could not be initialized",
			xrtGetError()
		);
		return XHTTP1_SERVER_FEED_ERROR;
	}
	pExchange->BodyLimit = pExchange->Config.Body.MaxBody;
	pExchange->State = XRT_HTTP_SERVER_STATE_BODY;
	if ( pExchange->Events.Headers != NULL ) {
		xrtClearError();
		pExchange->InCallback = true;
		Policy = pExchange->Events.Headers(
			pExchange,
			pExchange->Request,
			pExchange->Events.Data
		);
		pExchange->InCallback = false;
	}
	if ( pExchange->State == XRT_HTTP_SERVER_STATE_FAILED ) {
		return XHTTP1_SERVER_FEED_ERROR;
	}
	if ( Policy == XHTTP_SERVER_BODY_REJECT ) {
		pExchange->Paused = false;
		pExchange->State = XRT_HTTP_SERVER_STATE_REJECTED;
		return XHTTP1_SERVER_FEED_REJECTED;
	}
	if ( (Policy != XHTTP_SERVER_BODY_BUFFER) &&
		(Policy != XHTTP_SERVER_BODY_STREAM) &&
		(Policy != XHTTP_SERVER_BODY_DISCARD) ) {
		(void)__xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_HEADERS_CALLBACK,
			XERR_VALUE,
			"select-http1-server-body",
			"HTTP server Headers callback returned an invalid body policy",
			xrtGetError()
		);
		return XHTTP1_SERVER_FEED_ERROR;
	}
	if ( !xrtHttp1BodyDone(&pExchange->Body) &&
		((Policy == XHTTP_SERVER_BODY_STREAM) ||
		 (Policy == XHTTP_SERVER_BODY_DISCARD)) ) {
		__xrtHttpServerRequestSetFlags(
			pExchange->Request,
			XHTTP_SERVER_REQUEST_STREAMED
		);
	}
	if ( !xrtHttp1BodyDone(&pExchange->Body) &&
		(Policy == XHTTP_SERVER_BODY_DISCARD) ) {
		__xrtHttpServerRequestSetFlags(
			pExchange->Request,
			XHTTP_SERVER_REQUEST_DISCARDED
		);
	}
	if ( !xrtHttp1ServerExchangeSetBodyLimit(
		pExchange, pExchange->BodyLimit
	) ) {
		return XHTTP1_SERVER_FEED_ERROR;
	}
	if ( xrtHttp1BodyDone(&pExchange->Body) ) {
		return __xrtHttp1ServerExchangeFinish(pExchange);
	}
	return pExchange->Paused ?
		XHTTP1_SERVER_FEED_PAUSED :
		XHTTP1_SERVER_FEED_MORE;
}



/* 推进请求 Header 并只接受到 Header 终点。 */
xhttp1serverfeedstatus __xrtHttp1ServerExchangeFeedHead(
	xhttp1serverexchange* pExchange,
	xbytesview Input,
	bool bEnd,
	size_t* pAccepted
)
{
	xbytesview HeadInput;
	size_t iTake;
	size_t iTotal;
	bool bReady;

	*pAccepted = 0;
	iTake = __xrtHttp1ServerDelimiterScan(
		pExchange, Input, &bReady
	);
	if ( pExchange->HeadBuffer.Size >
		(SIZE_MAX - iTake) ) {
		(void)__xrtHttp1ServerExchangeFail(
			pExchange,
			XHTTP1_SERVER_ERROR_HEAD,
			XERR_RANGE,
			"scan-http1-server-head",
			"HTTP server request Header size overflowed"
		);
		return XHTTP1_SERVER_FEED_ERROR;
	}
	iTotal = pExchange->HeadBuffer.Size + iTake;
	if ( iTotal > pExchange->Config.Head.MaxHead ) {
		(void)__xrtHttp1ServerExchangeFail(
			pExchange,
			XHTTP1_SERVER_ERROR_HEAD,
			XERR_RANGE,
			"scan-http1-server-head",
			"HTTP server request Header exceeds its limit"
		);
		return XHTTP1_SERVER_FEED_ERROR;
	}
	if ( !bReady ) {
		if ( !__xrtHttp1ServerHeadAppend(
			pExchange,
			Input
		) ) {
			return XHTTP1_SERVER_FEED_ERROR;
		}
		*pAccepted = Input.Size;
		if ( !__xrtHttp1ServerExchangeWireAdd(
			pExchange, Input.Size
		) ) {
			return XHTTP1_SERVER_FEED_ERROR;
		}
		if ( bEnd ) {
			(void)__xrtHttp1ServerExchangeFail(
				pExchange,
				XHTTP1_SERVER_ERROR_UNEXPECTED_EOF,
				XERR_PROTOCOL,
				"read-http1-server-head",
				"HTTP server request Header ended before its final empty line"
			);
			return XHTTP1_SERVER_FEED_ERROR;
		}
		if ( iTotal == pExchange->Config.Head.MaxHead ) {
			(void)__xrtHttp1ServerExchangeFail(
				pExchange,
				XHTTP1_SERVER_ERROR_HEAD,
				XERR_RANGE,
				"read-http1-server-head",
				"HTTP server request Header has no delimiter within its limit"
			);
			return XHTTP1_SERVER_FEED_ERROR;
		}
		return XHTTP1_SERVER_FEED_MORE;
	}

	if ( pExchange->HeadBuffer.Size != 0 ) {
		if ( !__xrtHttp1ServerHeadAppend(
			pExchange,
			(xbytesview){ Input.Data, iTake }
		) ) {
			return XHTTP1_SERVER_FEED_ERROR;
		}
		HeadInput = (xbytesview){
			pExchange->HeadBuffer.Data,
			pExchange->HeadBuffer.Size
		};
	} else {
		HeadInput = (xbytesview){ Input.Data, iTake };
	}
	*pAccepted = iTake;
	if ( !__xrtHttp1ServerExchangeWireAdd(
		pExchange, iTake
	) ) {
		return XHTTP1_SERVER_FEED_ERROR;
	}
	{
		xhttp1serverfeedstatus Status =
			__xrtHttp1ServerHeadReady(
				pExchange, HeadInput
			);

		xrtFree(pExchange->HeadBuffer.Data);
		memset(
			&pExchange->HeadBuffer,
			0,
			sizeof(pExchange->HeadBuffer)
		);
		pExchange->Delimiter = 0;
		return Status;
	}
}

#endif
