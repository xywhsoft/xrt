#include "../internal/xrt_http_client_runtime.h"

#include <xrt/http_encoding.h>



#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)

typedef struct xrt_http_decompress_output {
	xhttpcall* Call;
	size_t Inflater;
} xrt_http_decompress_output;



/* 初始化高层客户端自动内容解码策略。 */
XRT_API void xrtHttpDecompressConfigInit(
	xhttpdecompressconfig* pConfig
)
{
	xhttpdecompressconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-decompress-config",
			"HTTP decompression config range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.MaxBody = XHTTP_DECOMPRESS_BODY_DEFAULT;
	Config.MaxCodings =
		XHTTP_DECOMPRESS_CODINGS_DEFAULT;
	Config.Enabled = true;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 设置一次表示解码失败并保留当前错误作为后续原因。 */
static bool __xrtHttpDecompressError(
	xhttpcall* pCall,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	pCall->DecompressFailed = true;
	__xrtHttpClientSetError(
		Kind,
		XHTTP_CLIENT_ERROR_DECOMPRESSION,
		sOperation,
		sMessage,
		pCause
	);
	return false;
}



/* 把协议层内置编码映射为客户端 Inflate 格式。 */
static xinflateformat __xrtHttpDecompressFormat(
	xhttpcoding Coding
)
{
	return Coding == XHTTP_CODING_GZIP ?
		XINFLATE_GZIP : XINFLATE_DEFLATE;
}



/* 释放一个尚未发布到 Call 的局部解码器数组。 */
static void __xrtHttpDecompressArrayFree(
	xinflate** pInflaters,
	size_t iCount
)
{
	size_t i;

	if ( pInflaters == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; i++ ) {
		xrtInflateDestroy(pInflaters[i]);
	}
	xrtFree(pInflaters);
}



/* 复制全部同名 Content-Encoding 字段并按标准字段合并顺序连接。 */
static str __xrtHttpDecompressEncodingCopy(
	const xhttpresponse* pResponse,
	size_t iSize
)
{
	const xhttpheaders* pHeaders =
		xrtHttpResponseHeaders(pResponse);
	const xhttpfield* pFields =
		xrtHttpHeadersData(pHeaders);
	size_t iFieldCount =
		xrtHttpHeadersCount(pHeaders);
	str sEncoding;
	size_t iWritten;

	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sEncoding = (str)xrtMalloc(iSize + 1u);
	if ( sEncoding == NULL ) {
		return NULL;
	}
	if ( !xrtHttpContentEncodingWrite(
		pFields,
		iFieldCount,
		sEncoding,
		iSize,
		&iWritten
	) || (iWritten != iSize) ) {
		xrtFree(sEncoding);
		return NULL;
	}
	sEncoding[iWritten] = '\0';
	return sEncoding;
}



/*
	解析完整 Content-Encoding 列表。
	零表示没有可解码表示，一表示全部受支持，负值表示语法或限额失败。
*/
static int __xrtHttpDecompressPlan(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xinflateformat Formats[XHTTP_DECOMPRESS_CODINGS_MAX],
	size_t* pInflaterCount,
	size_t* pEncodingSize
)
{
	const xhttpheaders* pHeaders =
		xrtHttpResponseHeaders(pResponse);
	const xhttpfield* pFields =
		xrtHttpHeadersData(pHeaders);
	size_t iFieldCount =
		xrtHttpHeadersCount(pHeaders);
	xhttpcontentencodingplan Plan;
	xhttpcontentencodingcursor Cursor;
	xhttpcontentencodingitem Item;
	xhttpnext Next;
	size_t iInflaterCount = 0;

	if ( !xrtHttpContentEncodingPlan(
		pFields, iFieldCount, &Plan
	) ) {
		xerror* pCause = xrtErrorRef(xrtGetError());
		xerrkind Kind = xrtErrorIs(
			pCause, XERR_RANGE
		) != NULL ? XERR_RANGE : XERR_PROTOCOL;

		(void)__xrtHttpDecompressError(
			pCall,
			Kind,
			"parse-http-content-encoding",
			"HTTP Content-Encoding list is invalid",
			pCause
		);
		xrtErrorFree(pCause);
		return -1;
	}
	if ( (Plan.FieldCount == 0) ||
		(Plan.UnknownCount != 0) ) {
		return 0;
	}
	if ( Plan.CodingCount >
		pCall->DecompressMaxCodings ) {
		(void)__xrtHttpDecompressError(
			pCall,
			XERR_RANGE,
			"parse-http-content-encoding",
			"HTTP Content-Encoding nesting exceeds its limit",
			NULL
		);
		return -1;
	}
	if ( Plan.DecoderCount == 0 ) {
		return 0;
	}
	xrtHttpContentEncodingCursorInit(&Cursor);
	while ( (Next = xrtHttpContentEncodingNext(
		pFields,
		iFieldCount,
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( (Item.Coding == XHTTP_CODING_GZIP) ||
			(Item.Coding == XHTTP_CODING_DEFLATE) ) {
			Formats[iInflaterCount] =
				__xrtHttpDecompressFormat(
					Item.Coding
				);
			iInflaterCount++;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		(void)__xrtHttpDecompressError(
			pCall,
			XERR_INTERNAL,
			"parse-http-content-encoding",
			"HTTP Content-Encoding plan changed while iterating",
			xrtGetError()
		);
		return -1;
	}
	*pInflaterCount = iInflaterCount;
	*pEncodingSize = Plan.JoinedSize;
	return 1;
}



/* 为最终响应创建逆序执行的表示解码链。 */
static bool __xrtHttpDecompressSetup(
	xhttpcall* pCall,
	xhttpresponse* pResponse
)
{
	xinflateformat Formats[
		XHTTP_DECOMPRESS_CODINGS_MAX
	];
	xinflateconfig Config;
	xinflate** pInflaters;
	str sOriginal;
	size_t iCodingCount = 0;
	size_t iEncodingSize = 0;
	size_t i;
	int iPlan;

	if ( !xrtHttpResponseContentAllowed(
		xrtHttpRequestMethod(pCall->Request),
		xrtHttpResponseStatus(pResponse)
	) ) {
		return true;
	}
	iPlan = __xrtHttpDecompressPlan(
		pCall,
		pResponse,
		Formats,
		&iCodingCount,
		&iEncodingSize
	);
	if ( iPlan <= 0 ) {
		return iPlan == 0;
	}
	pInflaters = (xinflate**)xrtCalloc(
		iCodingCount,
		sizeof(*pInflaters)
	);
	if ( pInflaters == NULL ) {
		return __xrtHttpDecompressError(
			pCall,
			XERR_MEMORY,
			"create-http-content-decoders",
			"HTTP content decoder array allocation failed",
			xrtGetError()
		);
	}
	xrtInflateConfigInit(&Config);
	Config.OutputLimit = pCall->DecompressLimit;
	for ( i = 0; i < iCodingCount; i++ ) {
		Config.Format =
			Formats[iCodingCount - i - 1u];
		pInflaters[i] = xrtInflateCreate(&Config);
		if ( pInflaters[i] == NULL ) {
			xerror* pCause = xrtErrorRef(xrtGetError());

			__xrtHttpDecompressArrayFree(
				pInflaters,
				iCodingCount
			);
			(void)__xrtHttpDecompressError(
				pCall,
				xrtErrorIs(pCause, XERR_MEMORY) != NULL ?
					XERR_MEMORY : XERR_INTERNAL,
				"create-http-content-decoders",
				"HTTP content decoder creation failed",
				pCause
			);
			xrtErrorFree(pCause);
			return false;
		}
	}
	sOriginal = __xrtHttpDecompressEncodingCopy(
		pResponse,
		iEncodingSize
	);
	if ( sOriginal == NULL ) {
		xerror* pCause = xrtErrorRef(xrtGetError());

		__xrtHttpDecompressArrayFree(
			pInflaters,
			iCodingCount
		);
		(void)__xrtHttpDecompressError(
			pCall,
			xrtErrorIs(pCause, XERR_RANGE) != NULL ?
				XERR_RANGE : XERR_MEMORY,
			"copy-http-content-encoding",
			"HTTP Content-Encoding copy failed",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	pCall->Inflaters = pInflaters;
	pCall->InflaterCount = iCodingCount;
	pCall->DecompressResponse = pResponse;
	pCall->DecompressActive = true;
	__xrtHttpResponseSetDecoded(
		pResponse,
		sOriginal,
		iEncodingSize
	);
	return true;
}



/* 转发信息响应，不为其建立内容解码状态。 */
static bool __xrtHttpDecompressInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->DecompressNext.Informational == NULL ) {
		return true;
	}
	return pCall->DecompressNext.Informational(
		pResponse,
		pCall->DecompressNext.Data
	);
}



/* 在用户看到最终 Header 前完成编码计划和元数据改写。 */
static bool __xrtHttpDecompressHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( !__xrtHttpDecompressSetup(
		pCall,
		(xhttpresponse*)pResponse
	) ) {
		return false;
	}
	if ( pCall->DecompressNext.Headers == NULL ) {
		return true;
	}
	return pCall->DecompressNext.Headers(
		pResponse,
		pCall->DecompressNext.Data
	);
}



/* 把最内层解码输出交给下一个事件消费者。 */
static bool __xrtHttpDecompressDeliver(
	xhttpcall* pCall,
	xbytesview Data
)
{
	bool bAccepted;

	/*
		Exchange 会在最外层回调返回后累计线路片段。
		发布明文前恢复已接受的解码长度，使流式回调看到一致的响应语义。
	*/
	__xrtHttpResponseSetBodyBytes(
		pCall->DecompressResponse,
		pCall->DecompressBodyBytes
	);
	if ( pCall->DecompressNext.Body == NULL ) {
		bAccepted = __xrtHttpResponseBufferDeliveredBody(
			pCall->DecompressResponse,
			Data
		);
	} else {
		bAccepted = pCall->DecompressNext.Body(
			pCall->DecompressResponse,
			Data,
			pCall->DecompressNext.Data
		);
	}
	if ( !bAccepted ) {
		if ( pCall->DecompressNext.Body == NULL ) {
			pCall->DecompressFailed = true;
		} else {
			pCall->DecompressForwardFailed = true;
		}
		return false;
	}
	if ( pCall->DecompressBodyBytes >
		(UINT64_MAX - (uint64)Data.Size) ) {
		__xrtErrorSetSizeOverflow();
		pCall->DecompressFailed = true;
		return false;
	}
	pCall->DecompressBodyBytes += (uint64)Data.Size;
	return true;
}



/* 把一层输出递归送入下一层，最后交给 HTTP 事件链。 */
static bool __xrtHttpDecompressOutput(
	xbytesview Data,
	ptr pData
)
{
	xrt_http_decompress_output* pOutput =
		(xrt_http_decompress_output*)pData;
	xhttpcall* pCall = pOutput->Call;
	size_t iNext = pOutput->Inflater + 1u;

	if ( iNext < pCall->InflaterCount ) {
		xrt_http_decompress_output Next;

		Next.Call = pCall;
		Next.Inflater = iNext;
		return xrtInflateWrite(
			pCall->Inflaters[iNext],
			Data,
			false,
			__xrtHttpDecompressOutput,
			&Next
		);
	}
	return __xrtHttpDecompressDeliver(pCall, Data);
}



/* 解码一段线路表示正文，未知编码路径直接转发原始字节。 */
static bool __xrtHttpDecompressBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	xrt_http_decompress_output Output;

	if ( !pCall->DecompressActive ) {
		if ( pCall->DecompressNext.Body == NULL ) {
			return __xrtHttpResponseBufferDeliveredBody(
				(xhttpresponse*)pResponse,
				Data
			);
		}
		return pCall->DecompressNext.Body(
			pResponse,
			Data,
			pCall->DecompressNext.Data
		);
	}
	Output.Call = pCall;
	Output.Inflater = 0;
	if ( !xrtInflateWrite(
		pCall->Inflaters[0],
		Data,
		false,
		__xrtHttpDecompressOutput,
		&Output
	) ) {
		pCall->DecompressFailed = true;
		return false;
	}
	return true;
}



/* 冻结本次调用的启用模式、限额和自动协商 Header。 */
bool __xrtHttpDecompressInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	xhttpdecompressmode Mode = pOptions->Decompress;

	if ( (Mode < XHTTP_DECOMPRESS_DEFAULT) ||
		(Mode > XHTTP_DECOMPRESS_RAW) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_DECOMPRESSION,
			"configure-http-decompression",
			"HTTP call decompression mode is invalid",
			NULL
		);
		return false;
	}
	pCall->DecompressEnabled =
		Mode == XHTTP_DECOMPRESS_AUTO ||
		((Mode == XHTTP_DECOMPRESS_DEFAULT) &&
		 pCall->Client->Config.Decompress.Enabled);
	pCall->DecompressLimit =
		pCall->Client->Config.Decompress.MaxBody;
	pCall->DecompressMaxCodings =
		pCall->Client->Config.Decompress.MaxCodings;
	if ( !pCall->DecompressEnabled ||
		(xrtHttpRequestHeader(
			pCall->Request,
			XRT_STR_LITERAL("Range")
		) != NULL) ||
		(xrtHttpRequestHeader(
			pCall->Request,
			XRT_STR_LITERAL("Accept-Encoding")
		) != NULL) ) {
		return true;
	}
	if ( !xrtHttpRequestAddHeader(
		pCall->Request,
		XRT_STR_LITERAL("Accept-Encoding"),
		XRT_STR_LITERAL("gzip, deflate")
	) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpClientSetError(
			xrtErrorIs(pCause, XERR_MEMORY) != NULL ?
				XERR_MEMORY : XERR_VALUE,
			XHTTP_CLIENT_ERROR_DECOMPRESSION,
			"prepare-http-accept-encoding",
			"HTTP automatic Accept-Encoding could not be added",
			pCause
		);
		return false;
	}
	return true;
}



/* 清除当前一跳的解码对象，不改变冻结的调用策略。 */
void __xrtHttpDecompressReset(xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		return;
	}
	__xrtHttpDecompressArrayFree(
		pCall->Inflaters,
		pCall->InflaterCount
	);
	pCall->Inflaters = NULL;
	pCall->InflaterCount = 0;
	pCall->DecompressResponse = NULL;
	pCall->DecompressBodyBytes = 0;
	pCall->DecompressActive = false;
	pCall->DecompressFailed = false;
	pCall->DecompressForwardFailed = false;
}



/* 构造只在启用时存在的内容解码事件装饰器。 */
const xhttp1exchangeevents* __xrtHttpDecompressEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
)
{
	if ( !pCall->DecompressEnabled ) {
		return pNext;
	}
	pCall->DecompressNext = *pNext;
	memset(
		&pCall->DecompressEvents,
		0,
		sizeof(pCall->DecompressEvents)
	);
	pCall->DecompressEvents.Informational =
		__xrtHttpDecompressInformational;
	pCall->DecompressEvents.Headers =
		__xrtHttpDecompressHeaders;
	pCall->DecompressEvents.Body =
		__xrtHttpDecompressBody;
	pCall->DecompressEvents.Data = pCall;
	return &pCall->DecompressEvents;
}



/* 依次结束外层到内层解码器并发布最终正文长度。 */
bool __xrtHttpDecompressFinish(
	xhttpcall* pCall,
	xhttpresponse* pResponse
)
{
	size_t i;

	if ( !pCall->DecompressActive ) {
		return true;
	}
	if ( (pResponse == NULL) ||
		(pCall->DecompressResponse != pResponse) ||
		(pCall->InflaterCount == 0) ) {
		return __xrtHttpDecompressError(
			pCall,
			XERR_INTERNAL,
			"finish-http-content-decoders",
			"HTTP content decoder state is inconsistent",
			NULL
		);
	}
	for ( i = 0; i < pCall->InflaterCount; i++ ) {
		xrt_http_decompress_output Output;

		if ( xrtInflateDone(pCall->Inflaters[i]) ) {
			continue;
		}
		Output.Call = pCall;
		Output.Inflater = i;
		if ( !xrtInflateWrite(
			pCall->Inflaters[i],
			(xbytesview){ NULL, 0 },
			true,
			__xrtHttpDecompressOutput,
			&Output
		) ) {
			pCall->DecompressFailed = true;
			return false;
		}
	}
	__xrtHttpResponseSetBodyBytes(
		pResponse,
		pCall->DecompressBodyBytes
	);
	pCall->DecompressActive = false;
	return true;
}



/* 把真正的解码失败提升为高层客户端终态。 */
bool __xrtHttpDecompressFail(
	xhttpcall* pCall,
	const xerror* pCause
)
{
	xerrkind Kind = XERR_PROTOCOL;

	if ( (pCall == NULL) ||
		!pCall->DecompressFailed ||
		pCall->DecompressForwardFailed ) {
		return false;
	}
	if ( xrtErrorIs(pCause, XERR_MEMORY) != NULL ) {
		Kind = XERR_MEMORY;
	} else if ( xrtErrorIs(pCause, XERR_RANGE) != NULL ) {
		Kind = XERR_RANGE;
	}
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		XHTTP_CLIENT_ERROR_DECOMPRESSION,
		Kind,
		"decode-http-response",
		"HTTP response content decoding failed",
		pCause
	);
	return true;
}

#endif
