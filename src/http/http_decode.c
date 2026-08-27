#include "../internal/xrt_http.h"

#include <xrt/compress.h>
#include <xrt/http_decode.h>



#if defined(XRT_FEATURE_HTTP_DECODE)

#define XRT_HTTP_DECODE_MAGIC UINT32_C(0x58484443)



/* 解码器保留最多十六层 Inflate 指针，复位时不释放滑动窗口。 */
struct xhttpdecode {
	xhttpdecodeconfig Config;
	xinflate* Levels[XHTTP_CONTENT_CODINGS_MAX];
	uint64 InputSize;
	uint64 OutputSize;
	uint32 Magic;
	uint32 AllocatedCount;
	uint32 ActiveCount;
	xhttpdecodemode Mode;
	bool Writing;
	bool Done;
	bool Failed;
};



/* 同步解码链回调使用栈上下文，不产生逐片分配。 */
typedef struct xrt_http_decode_output {
	xhttpdecode* Decode;
	uint32 Level;
	xhttpdecodeoutputproc Output;
	ptr Data;
} xrt_http_decode_output;



/* 建立 HTTP Decode 域的结构化错误。 */
static void __xrtHttpDecodeError(
	xerrkind Kind,
	xhttpdecodeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtErrorSetDetail(
		Kind,
		"xrt.http.decode",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 验证公开解码器仍处于活动对象范围。 */
static bool __xrtHttpDecodeValid(
	const xhttpdecode* pDecode,
	cstr sOperation
)
{
	if ( __xrtRangeValid(pDecode, sizeof(*pDecode)) &&
		(pDecode->Magic == XRT_HTTP_DECODE_MAGIC) ) {
		return true;
	}
	__xrtHttpDecodeError(
		XERR_ARGUMENT,
		XHTTP_DECODE_ERROR_ARGUMENT,
		sOperation,
		"HTTP decoder range is invalid"
	);
	return false;
}



/* 复制并验证可能位于未对齐存储中的配置。 */
static bool __xrtHttpDecodeConfigResolve(
	const xhttpdecodeconfig* pInput,
	xhttpdecodeconfig* pConfig,
	cstr sOperation
)
{
	if ( pInput == NULL ) {
		xrtHttpDecodeConfigInit(pConfig);
	} else {
		if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
			__xrtHttpDecodeError(
				XERR_ARGUMENT,
				XHTTP_DECODE_ERROR_ARGUMENT,
				sOperation,
				"HTTP decode config range is invalid"
			);
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( (pConfig->GzipHeaderLimit < 10u) ||
		(pConfig->MaxCodings == 0) ||
		(pConfig->MaxCodings > XHTTP_CONTENT_CODINGS_MAX) ||
		((pConfig->Flags & ~XHTTP_DECODE_ALLOW_RAW) != 0) ) {
		__xrtHttpDecodeError(
			XERR_VALUE,
			XHTTP_DECODE_ERROR_CONFIG,
			sOperation,
			"HTTP decode config is invalid"
		);
		return false;
	}
	return true;
}



/* 把最终明文计入硬限额并同步交给调用方。 */
static bool __xrtHttpDecodeEmit(
	xrt_http_decode_output* pContext,
	xbytesview Data
)
{
	xhttpdecode* pDecode = pContext->Decode;

	if ( Data.Size == 0 ) {
		return true;
	}
	if ( (pDecode->OutputSize > pDecode->Config.OutputLimit) ||
		((uint64)Data.Size >
		 (pDecode->Config.OutputLimit - pDecode->OutputSize)) ) {
		__xrtHttpDecodeError(
			XERR_RANGE,
			XHTTP_DECODE_ERROR_LIMIT,
			"write-http-decode",
			"HTTP decoded body exceeds its configured limit"
		);
		return false;
	}
	if ( pContext->Output != NULL ) {
		xrtClearError();
		if ( !pContext->Output(Data, pContext->Data) ) {
			if ( xrtGetError() == NULL ) {
				__xrtHttpDecodeError(
					XERR_CANCELLED,
					XHTTP_DECODE_ERROR_OUTPUT,
					"write-http-decode-output",
					"HTTP decode output callback stopped decoding"
				);
			}
			return false;
		}
	}
	pDecode->OutputSize += (uint64)Data.Size;
	return true;
}



/* 把当前 Inflate 输出直接送入下一层，最后一层才发布给调用方。 */
static bool __xrtHttpDecodeOutput(xbytesview Data, ptr pData)
{
	xrt_http_decode_output* pContext =
		(xrt_http_decode_output*)pData;
	xrt_http_decode_output Next;
	uint32 iNext = pContext->Level + 1u;

	if ( iNext >= pContext->Decode->ActiveCount ) {
		return __xrtHttpDecodeEmit(pContext, Data);
	}
	Next = *pContext;
	Next.Level = iNext;
	return xrtInflateWrite(
		pContext->Decode->Levels[iNext],
		Data,
		false,
		__xrtHttpDecodeOutput,
		&Next
	);
}



/* 解析编码列表并按反向解码顺序配置可复用的 Inflate 层。 */
static bool __xrtHttpDecodeConfigure(
	xhttpdecode* pDecode,
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpdecodeconfig* pInput,
	cstr sOperation
)
{
	xhttpcontentencodingcursor Cursor;
	xhttpcontentencodingitem Item;
	xhttpcontentencodingplan Plan;
	xhttpdecodeconfig Config;
	xhttpcoding Codings[XHTTP_CONTENT_CODINGS_MAX];
	xinflateconfig Inflate;
	xhttpnext Next;
	uint32 iCodingCount = 0;
	uint32 iDecoderCount = 0;
	uint32 i;
	bool bUnknown = false;

	if ( !__xrtHttpDecodeConfigResolve(
		pInput, &Config, sOperation
	) || !xrtHttpContentEncodingPlan(
		pFields, iCount, &Plan
	) ) {
		return false;
	}
	if ( Plan.CodingCount > (size_t)Config.MaxCodings ) {
		__xrtHttpDecodeError(
			XERR_RANGE,
			XHTTP_DECODE_ERROR_LIMIT,
			sOperation,
			"Content-Encoding has too many coding layers"
		);
		return false;
	}
	xrtHttpContentEncodingCursorInit(&Cursor);
	while ( (Next = xrtHttpContentEncodingNext(
		pFields, iCount, &Cursor, &Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( iCodingCount >= Config.MaxCodings ) {
			__xrtHttpDecodeError(
				XERR_RANGE,
				XHTTP_DECODE_ERROR_LIMIT,
				sOperation,
				"Content-Encoding has too many coding layers"
			);
			return false;
		}
		Codings[iCodingCount++] = Item.Coding;
		if ( Item.Coding == XHTTP_CODING_NONE ) {
			bUnknown = true;
		} else if ( Item.Coding != XHTTP_CODING_IDENTITY ) {
			iDecoderCount++;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtHttpDecodeError(
			XERR_PROTOCOL,
			XHTTP_DECODE_ERROR_CONTENT_ENCODING,
			sOperation,
			"Content-Encoding syntax is invalid"
		);
		return false;
	}
	if ( bUnknown ) {
		if ( (Config.Flags & XHTTP_DECODE_ALLOW_RAW) == 0 ) {
			__xrtHttpDecodeError(
				XERR_UNSUPPORTED,
				XHTTP_DECODE_ERROR_UNSUPPORTED,
				sOperation,
				"Content-Encoding contains an unsupported coding"
			);
			return false;
		}
		pDecode->Config = Config;
		pDecode->ActiveCount = 0;
		pDecode->Mode = XHTTP_DECODE_RAW;
		pDecode->InputSize = 0;
		pDecode->OutputSize = 0;
		pDecode->Done = false;
		pDecode->Failed = false;
		return true;
	}
	if ( iDecoderCount == 0 ) {
		pDecode->Config = Config;
		pDecode->ActiveCount = 0;
		pDecode->Mode = XHTTP_DECODE_IDENTITY;
		pDecode->InputSize = 0;
		pDecode->OutputSize = 0;
		pDecode->Done = false;
		pDecode->Failed = false;
		return true;
	}

	xrtInflateConfigInit(&Inflate);
	Inflate.OutputLimit = Config.OutputLimit;
	Inflate.GzipHeaderLimit = Config.GzipHeaderLimit;
	iDecoderCount = 0;
	for ( i = iCodingCount; i != 0; i-- ) {
		xhttpcoding Coding = Codings[i - 1u];

		if ( Coding == XHTTP_CODING_IDENTITY ) {
			continue;
		}
		Inflate.Format = (Coding == XHTTP_CODING_GZIP) ?
			XINFLATE_GZIP : XINFLATE_DEFLATE;
		if ( iDecoderCount < pDecode->AllocatedCount ) {
			if ( !xrtInflateReset(
				pDecode->Levels[iDecoderCount], &Inflate
			) ) {
				pDecode->Failed = true;
				return false;
			}
		} else {
			pDecode->Levels[iDecoderCount] =
				xrtInflateCreate(&Inflate);
			if ( pDecode->Levels[iDecoderCount] == NULL ) {
				pDecode->Failed = true;
				return false;
			}
			pDecode->AllocatedCount++;
		}
		iDecoderCount++;
	}
	pDecode->Config = Config;
	pDecode->ActiveCount = iDecoderCount;
	pDecode->Mode = XHTTP_DECODE_CONTENT;
	pDecode->InputSize = 0;
	pDecode->OutputSize = 0;
	pDecode->Done = false;
	pDecode->Failed = false;
	return true;
}



/* 初始化 HTTP 正文自动解码配置。 */
XRT_API void xrtHttpDecodeConfigInit(xhttpdecodeconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtHttpDecodeError(
			XERR_ARGUMENT,
			XHTTP_DECODE_ERROR_ARGUMENT,
			"initialize-http-decode-config",
			"HTTP decode config is null"
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->OutputLimit = XHTTP_DECODE_OUTPUT_UNLIMITED;
	pConfig->GzipHeaderLimit = XINFLATE_GZIP_HEADER_DEFAULT;
	pConfig->MaxCodings = XHTTP_CONTENT_CODINGS_DEFAULT;
}



/* 初始化有明确明文预算的安全默认配置。 */
XRT_API void xrtHttpDecodeConfigInitSafe(xhttpdecodeconfig* pConfig)
{
	xrtHttpDecodeConfigInit(pConfig);
	if ( pConfig != NULL ) {
		pConfig->OutputLimit = XHTTP_DECODE_OUTPUT_SAFE_DEFAULT;
	}
}



/* 创建可跨消息复用的 HTTP 正文解码器。 */
XRT_API xhttpdecode* xrtHttpDecodeCreate(
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpdecodeconfig* pConfig
)
{
	xhttpdecode* pDecode = (xhttpdecode*)xrtMalloc(sizeof(*pDecode));

	if ( pDecode == NULL ) {
		return NULL;
	}
	memset(pDecode, 0, sizeof(*pDecode));
	pDecode->Magic = XRT_HTTP_DECODE_MAGIC;
	if ( !__xrtHttpDecodeConfigure(
		pDecode,
		pFields,
		iCount,
		pConfig,
		"create-http-decode"
	) ) {
		xrtHttpDecodeDestroy(pDecode);
		return NULL;
	}
	return pDecode;
}



/* 复位消息状态并复用底层 Inflate 窗口。 */
XRT_API bool xrtHttpDecodeReset(
	xhttpdecode* pDecode,
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpdecodeconfig* pConfig
)
{
	if ( !__xrtHttpDecodeValid(
		pDecode, "reset-http-decode"
	) ) {
		return false;
	}
	if ( pDecode->Writing ) {
		__xrtHttpDecodeError(
			XERR_STATE,
			XHTTP_DECODE_ERROR_STATE,
			"reset-http-decode",
			"HTTP decoder cannot reset from its output callback"
		);
		return false;
	}
	return __xrtHttpDecodeConfigure(
		pDecode,
		pFields,
		iCount,
		pConfig,
		"reset-http-decode"
	);
}



/* 消费一段线路正文并按逆序同步推进全部内容编码层。 */
XRT_API bool xrtHttpDecodeWrite(
	xhttpdecode* pDecode,
	xbytesview Input,
	bool bFinal,
	xhttpdecodeoutputproc pOutput,
	ptr pData
)
{
	xrt_http_decode_output Context;
	bool bSuccess = true;
	uint32 i;

	if ( !__xrtHttpDecodeValid(
		pDecode, "write-http-decode"
	) ) {
		return false;
	}
	if ( !__xrtRangeValid(Input.Data, Input.Size) ) {
		__xrtHttpDecodeError(
			XERR_ARGUMENT,
			XHTTP_DECODE_ERROR_ARGUMENT,
			"write-http-decode",
			"HTTP decode input range is invalid"
		);
		pDecode->Failed = true;
		return false;
	}
	if ( pDecode->Writing || pDecode->Done || pDecode->Failed ) {
		__xrtHttpDecodeError(
			XERR_STATE,
			XHTTP_DECODE_ERROR_STATE,
			"write-http-decode",
			"HTTP decoder is busy or already terminal"
		);
		return false;
	}
	if ( (uint64)Input.Size > (UINT64_MAX - pDecode->InputSize) ) {
		__xrtHttpDecodeError(
			XERR_RANGE,
			XHTTP_DECODE_ERROR_LIMIT,
			"write-http-decode",
			"HTTP encoded body size overflows uint64"
		);
		pDecode->Failed = true;
		return false;
	}
	Context.Decode = pDecode;
	Context.Level = 0;
	Context.Output = pOutput;
	Context.Data = pData;
	pDecode->Writing = true;
	if ( pDecode->ActiveCount == 0 ) {
		bSuccess = __xrtHttpDecodeEmit(&Context, Input);
	} else {
		bSuccess = xrtInflateWrite(
			pDecode->Levels[0],
			Input,
			bFinal,
			__xrtHttpDecodeOutput,
			&Context
		);
		if ( bSuccess && bFinal ) {
			for ( i = 1; i < pDecode->ActiveCount; i++ ) {
				if ( xrtInflateDone(pDecode->Levels[i]) ) {
					continue;
				}
				Context.Level = i;
				if ( !xrtInflateWrite(
					pDecode->Levels[i],
					(xbytesview){ NULL, 0 },
					true,
					__xrtHttpDecodeOutput,
					&Context
				) ) {
					bSuccess = false;
					break;
				}
			}
		}
	}
	pDecode->Writing = false;
	if ( !bSuccess ) {
		pDecode->Failed = true;
		return false;
	}
	pDecode->InputSize += (uint64)Input.Size;
	if ( bFinal ) {
		pDecode->Done = true;
	}
	return true;
}



/* 返回当前表示采用的交付模式。 */
XRT_API xhttpdecodemode xrtHttpDecodeMode(
	const xhttpdecode* pDecode
)
{
	if ( !__xrtHttpDecodeValid(
		pDecode, "query-http-decode-mode"
	) ) {
		return XHTTP_DECODE_IDENTITY;
	}
	return pDecode->Mode;
}



/* 查询当前消息是否完整完成。 */
XRT_API bool xrtHttpDecodeDone(const xhttpdecode* pDecode)
{
	if ( !__xrtHttpDecodeValid(
		pDecode, "query-http-decode-done"
	) ) {
		return false;
	}
	return pDecode->Done && !pDecode->Failed;
}



/* 返回成功提交的线路正文长度。 */
XRT_API uint64 xrtHttpDecodeInputSize(const xhttpdecode* pDecode)
{
	if ( !__xrtHttpDecodeValid(
		pDecode, "query-http-decode-input"
	) ) {
		return 0;
	}
	return pDecode->InputSize;
}



/* 返回成功交付的正文长度。 */
XRT_API uint64 xrtHttpDecodeOutputSize(const xhttpdecode* pDecode)
{
	if ( !__xrtHttpDecodeValid(
		pDecode, "query-http-decode-output"
	) ) {
		return 0;
	}
	return pDecode->OutputSize;
}



/* 销毁全部 Inflate 层和解码器本身。 */
XRT_API void xrtHttpDecodeDestroy(xhttpdecode* pDecode)
{
	uint32 i;

	if ( pDecode == NULL ) {
		return;
	}
	if ( !__xrtHttpDecodeValid(
		pDecode, "destroy-http-decode"
	) ) {
		return;
	}
	if ( pDecode->Writing ) {
		__xrtHttpDecodeError(
			XERR_STATE,
			XHTTP_DECODE_ERROR_STATE,
			"destroy-http-decode",
			"HTTP decoder cannot be destroyed from its output callback"
		);
		return;
	}
	for ( i = 0; i < pDecode->AllocatedCount; i++ ) {
		xrtInflateDestroy(pDecode->Levels[i]);
	}
	memset(pDecode, 0, sizeof(*pDecode));
	xrtFree(pDecode);
}

#endif
