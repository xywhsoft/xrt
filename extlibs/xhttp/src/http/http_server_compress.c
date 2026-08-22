#include "../internal/xrt_http_server.h"

#include <xrt/http_compress.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER_COMPRESS)

/* 从服务端请求合并全部 Accept-Encoding 后调用无网络压缩层。 */
XRT_API xhttpreplycompressstatus xrtHttpServerReplyCompress(
	const xhttpserverrequest* pRequest,
	const xhttpreply* pReply,
	const xhttpreplycompressconfig* pConfig,
	xhttpreply** ppOutput
)
{
	xhttpacceptencoding Accept;
	xhttpreply* pOutput = NULL;
	size_t i;

	if ( !__xrtRangeValid(ppOutput, sizeof(*ppOutput)) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress-server-reply",
			"HTTP Server Reply compression output is invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	if ( __xrtRangesOverlap(
			ppOutput, sizeof(*ppOutput),
			pRequest, sizeof(*pRequest)
		) ||
		__xrtRangesOverlap(
			ppOutput, sizeof(*ppOutput),
			pReply, sizeof(*pReply)
		) ||
		((pConfig != NULL) &&
		 __xrtRangesOverlap(
			ppOutput, sizeof(*ppOutput),
			pConfig, sizeof(*pConfig)
		 )) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress-server-reply",
			"HTTP Server Reply compression output overlaps input"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	memcpy(ppOutput, &pOutput, sizeof(pOutput));
	if ( (pRequest == NULL) || (pReply == NULL) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress-server-reply",
			"HTTP Server Reply compression arguments are invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	xrtHttpAcceptEncodingInit(&Accept);
	for ( i = 0;
		  i < xrtHttpServerRequestHeaderCount(pRequest);
		  i++ ) {
		const xhttpfield* pField =
			xrtHttpServerRequestHeaderAt(pRequest, i);

		if ( (pField != NULL) &&
			xrtHttpFieldNameEqual(
				pField->Name,
				XRT_STR_LITERAL("Accept-Encoding")
			) &&
			!xrtHttpAcceptEncodingAdd(
				&Accept, pField->Value
			) ) {
			return XHTTP_REPLY_COMPRESS_ERROR;
		}
	}
	return xrtHttpReplyCompress(
		&Accept,
		xrtHttpServerRequestMethod(pRequest),
		pReply,
		pConfig,
		ppOutput
	);
}

#endif
