#include "../internal/xrt_compress.h"
#include "../internal/xrt_http.h"
#include "../internal/xrt_http_body.h"

#include <xrt/http_body_decode.h>



#if defined(XRT_FEATURE_HTTP_BODY_DECODE)

/* 把公共内容编码映射为对应 Inflate 包装。 */
static xinflateformat __xrtHttpBodyDecodeFormat(
	xhttpcoding Coding
)
{
	return Coding == XHTTP_CODING_GZIP ?
		XINFLATE_GZIP : XINFLATE_DEFLATE;
}



/* 为原样或未知编码结果发布来源的独立引用。 */
static xhttpbodydecoderesult __xrtHttpBodyDecodeRaw(
	xhttpbody* pSource,
	xhttpbodydecoderesult Result,
	xhttpbody** ppOutput
)
{
	xhttpbody* pBody = xrtHttpBodyRef(pSource);

	if ( pBody == NULL ) {
		return XHTTP_BODY_DECODE_ERROR;
	}
	*ppOutput = pBody;
	return Result;
}



/* 初始化通用内容解码配置。 */
XRT_API void xrtHttpBodyDecodeConfigInit(
	xhttpbodydecodeconfig* pConfig
)
{
	xhttpbodydecodeconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtHttpBodyInflateConfigInit(&Config.Inflate);
	Config.MaxCodings =
		XHTTP_CONTENT_CODINGS_DEFAULT;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 按协议计划逆序组合内置流式解码层。 */
XRT_API xhttpbodydecoderesult xrtHttpBodyDecodeFields(
	xhttpbody* pSource,
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpbodydecodeconfig* pConfig,
	xhttpbody** ppOutput
)
{
	xhttpbodydecodeconfig Config;
	xhttpcontentencodingplan Plan;
	xhttpcontentencodingcursor Cursor;
	xhttpcontentencodingitem Item;
	xhttpcoding Codings[XHTTP_CONTENT_CODINGS_MAX];
	xhttpbody* pBody;
	xhttpnext Next;
	size_t iCoding = 0;
	size_t i;

	if ( (pSource == NULL) ||
		!__xrtRangeValid(ppOutput, sizeof(*ppOutput)) ||
		!__xrtHttpFieldArrayValid(
			pFields, iCount
		) ||
		__xrtRangesOverlap(
			pSource, sizeof(*pSource),
			ppOutput, sizeof(*ppOutput)
		) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			ppOutput, sizeof(*ppOutput)
		) ||
		((pConfig != NULL) &&
		 __xrtRangesOverlap(
			pConfig, sizeof(*pConfig),
			ppOutput, sizeof(*ppOutput)
		 ))
	) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_BODY_DECODE_ERROR;
	}
	*ppOutput = NULL;
	if ( (pConfig != NULL) &&
		!__xrtRangeValid(
			pConfig, sizeof(*pConfig)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_BODY_DECODE_ERROR;
	}
	xrtHttpBodyDecodeConfigInit(&Config);
	if ( pConfig != NULL ) {
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( (Config.MaxCodings == 0) ||
		(Config.MaxCodings >
		 XHTTP_CONTENT_CODINGS_MAX) ||
		(Config.Inflate.ReadSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_BODY_DECODE_ERROR;
	}
	Config.Inflate.Inflate.Format =
		XINFLATE_DEFLATE;
	if ( !__xrtInflateConfigValid(
		&Config.Inflate.Inflate,
		"decode-http-body"
	) ) {
		return XHTTP_BODY_DECODE_ERROR;
	}
	if ( !xrtHttpContentEncodingPlan(
		pFields, iCount, &Plan
	) ) {
		return XHTTP_BODY_DECODE_ERROR;
	}
	if ( Plan.UnknownCount != 0 ) {
		return __xrtHttpBodyDecodeRaw(
			pSource,
			XHTTP_BODY_DECODE_UNSUPPORTED,
			ppOutput
		);
	}
	if ( Plan.CodingCount > Config.MaxCodings ) {
		__xrtErrorSetRange();
		return XHTTP_BODY_DECODE_ERROR;
	}
	if ( Plan.DecoderCount == 0 ) {
		return __xrtHttpBodyDecodeRaw(
			pSource,
			XHTTP_BODY_DECODE_UNCHANGED,
			ppOutput
		);
	}
	xrtHttpContentEncodingCursorInit(&Cursor);
	while ( (Next = xrtHttpContentEncodingNext(
		pFields, iCount, &Cursor, &Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( (Item.Coding == XHTTP_CODING_GZIP) ||
			(Item.Coding == XHTTP_CODING_DEFLATE) ) {
			Codings[iCoding] = Item.Coding;
			iCoding++;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return XHTTP_BODY_DECODE_ERROR;
	}
	if ( iCoding != Plan.DecoderCount ) {
		__xrtErrorSetInternal();
		return XHTTP_BODY_DECODE_ERROR;
	}
	pBody = xrtHttpBodyRef(pSource);
	if ( pBody == NULL ) {
		return XHTTP_BODY_DECODE_ERROR;
	}
	for ( i = iCoding; i != 0; i-- ) {
		xhttpbody* pNext;

		Config.Inflate.Inflate.Format =
			__xrtHttpBodyDecodeFormat(
				Codings[i - 1u]
			);
		pNext = xrtHttpBodyInflate(
			pBody, &Config.Inflate
		);
		xrtHttpBodyDestroy(pBody);
		if ( pNext == NULL ) {
			return XHTTP_BODY_DECODE_ERROR;
		}
		pBody = pNext;
	}
	*ppOutput = pBody;
	return XHTTP_BODY_DECODE_APPLIED;
}



/* 把单字段值适配到完整字段计划入口。 */
XRT_API xhttpbodydecoderesult xrtHttpBodyDecode(
	xhttpbody* pSource,
	xstrview ContentEncoding,
	const xhttpbodydecodeconfig* pConfig,
	xhttpbody** ppOutput
)
{
	xhttpfield Field;

	Field.Name = XRT_STR_LITERAL("Content-Encoding");
	Field.Value = ContentEncoding;
	return xrtHttpBodyDecodeFields(
		pSource, &Field, 1, pConfig, ppOutput
	);
}

#endif
