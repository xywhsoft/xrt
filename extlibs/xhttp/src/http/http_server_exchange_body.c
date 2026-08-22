#include "../internal/xrt_http_server.h"

#include <xrt/http_trailer.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER_EXCHANGE)

/* 返回从指定位置开始的输入视图，并避免对空指针执行算术。 */
static xbytesview __xrtHttp1ServerBodyInputAt(
	xbytesview Input,
	size_t iOffset
)
{
	xbytesview Result = { NULL, Input.Size - iOffset };

	if ( Result.Size != 0 ) {
		Result.Data = Input.Data + iOffset;
	}
	return Result;
}



/* 为解析器按实际 Trailer 数量建立临时字段描述符。 */
static bool __xrtHttp1ServerBodyTrailersGrow(
	xhttp1serverexchange* pExchange,
	size_t iRequired
)
{
	xhttpfield* pTrailers;

	if ( iRequired <= pExchange->ParseTrailerCapacity ) {
		return true;
	}
	if ( iRequired > (SIZE_MAX / sizeof(xhttpfield)) ) {
		__xrtErrorSetSizeOverflow();
		return __xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_TRAILER_STORAGE,
			XERR_RANGE,
			"allocate-http1-server-trailers",
			"HTTP server Trailer field count overflowed",
			xrtGetError()
		);
	}
	pTrailers = (xhttpfield*)xrtRealloc(
		pExchange->ParseTrailers,
		iRequired * sizeof(xhttpfield)
	);
	if ( pTrailers == NULL ) {
		return __xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_TRAILER_STORAGE,
			XERR_MEMORY,
			"allocate-http1-server-trailers",
			"HTTP server Trailer descriptors could not be allocated",
			xrtGetError()
		);
	}
	pExchange->ParseTrailers = pTrailers;
	pExchange->ParseTrailerCapacity = iRequired;
	return xrtHttp1BodyTrailers(
		&pExchange->Body,
		pExchange->ParseTrailers,
		pExchange->ParseTrailerCapacity
	);
}



/* 保存完整 Trailer 集合并结束当前请求。 */
static xhttp1serverfeedstatus __xrtHttp1ServerBodyFinish(
	xhttp1serverexchange* pExchange
)
{
	if ( !xrtHttpTrailerSectionValid(
		pExchange->Body.Trailers,
		pExchange->Body.TrailerCount
	) ) {
		(void)__xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_TRAILER,
			XERR_PROTOCOL,
			"validate-http1-server-trailers",
			"HTTP server request contains an invalid Trailer section",
			xrtGetError()
		);
		return XHTTP1_SERVER_FEED_ERROR;
	}
	if ( !__xrtHttpServerRequestSetTrailers(
		pExchange->Request,
		pExchange->Body.Trailers,
		pExchange->Body.TrailerCount
	) ) {
		(void)__xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_TRAILER_STORAGE,
			XERR_MEMORY,
			"copy-http1-server-trailers",
			"HTTP server request Trailers could not be stored",
			xrtGetError()
		);
		return XHTTP1_SERVER_FEED_ERROR;
	}
	return __xrtHttp1ServerExchangeFinish(pExchange);
}



/* 缓冲或同步交付一个已经解除 HTTP 分帧的数据片段。 */
static bool __xrtHttp1ServerBodyDeliver(
	xhttp1serverexchange* pExchange,
	xbytesview Data
)
{
	uint32 iFlags = xrtHttpServerRequestFlags(
		pExchange->Request
	);

	pExchange->BodyStarted = true;
	if ( (iFlags & XHTTP_SERVER_REQUEST_STREAMED) == 0 ) {
		if ( __xrtHttpServerRequestAppendBody(
			pExchange->Request, Data
		) ) {
			return true;
		}
		return __xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_BODY_STORAGE,
			XERR_MEMORY,
			"buffer-http1-server-body",
			"HTTP server request body could not be stored",
			xrtGetError()
		);
	}

	if ( ((iFlags & XHTTP_SERVER_REQUEST_DISCARDED) == 0) &&
		(pExchange->Events.Body != NULL) ) {
		bool bAccepted;

		xrtClearError();
		pExchange->InCallback = true;
		bAccepted = pExchange->Events.Body(
			pExchange,
			pExchange->Request,
			Data,
			pExchange->Events.Data
		);
		pExchange->InCallback = false;
		if ( !bAccepted ) {
			return __xrtHttp1ServerExchangeFailCause(
				pExchange,
				XHTTP1_SERVER_ERROR_BODY_CALLBACK,
				XERR_CANCELLED,
				"deliver-http1-server-body",
				"HTTP server body callback failed",
				xrtGetError()
			);
		}
	}
	if ( pExchange->State == XRT_HTTP_SERVER_STATE_FAILED ) {
		return false;
	}
	if ( !__xrtHttpServerRequestDeliverBody(
		pExchange->Request, (uint64)Data.Size
	) ) {
		return __xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_BODY_STORAGE,
			XERR_RANGE,
			"count-http1-server-body",
			"HTTP server request body byte count overflowed",
			xrtGetError()
		);
	}
	return true;
}



/* 推进正文、按需扩充 Trailer 描述符并保留精确输入前缀。 */
xhttp1serverfeedstatus __xrtHttp1ServerExchangeFeedBody(
	xhttp1serverexchange* pExchange,
	xbytesview Input,
	bool bEnd,
	size_t* pAccepted
)
{
	size_t iOffset = 0;

	*pAccepted = 0;
	if ( pExchange->Paused ) {
		return XHTTP1_SERVER_FEED_PAUSED;
	}
	for ( ;; ) {
		xhttp1errorinfo Error;
		xhttp1bodystatus Status;
		xbytesview Current = __xrtHttp1ServerBodyInputAt(
			Input, iOffset
		);
		xbytesview Data;
		size_t iConsumed = 0;

		Status = xrtHttp1BodyRead(
			&pExchange->Body,
			Current,
			bEnd,
			&iConsumed,
			&Data,
			&Error
		);
		if ( iConsumed > Current.Size ) {
			(void)__xrtHttp1ServerExchangeFail(
				pExchange,
				XHTTP1_SERVER_ERROR_STATE,
				XERR_STATE,
				"read-http1-server-body",
				"HTTP server body reader consumed beyond its input"
			);
			return XHTTP1_SERVER_FEED_ERROR;
		}
		iOffset += iConsumed;
		*pAccepted = iOffset;
		if ( !__xrtHttp1ServerExchangeWireAdd(
			pExchange, iConsumed
		) ) {
			return XHTTP1_SERVER_FEED_ERROR;
		}

		if ( Status == XHTTP1_BODY_DATA ) {
			if ( !__xrtHttp1ServerBodyDeliver(
				pExchange, Data
			) ) {
				return XHTTP1_SERVER_FEED_ERROR;
			}
			if ( pExchange->Paused ) {
				return XHTTP1_SERVER_FEED_PAUSED;
			}
			if ( xrtHttp1BodyDone(&pExchange->Body) ) {
				return __xrtHttp1ServerBodyFinish(
					pExchange
				);
			}
			continue;
		}
		if ( Status == XHTTP1_BODY_FIELDS ) {
			if ( !__xrtHttp1ServerBodyTrailersGrow(
				pExchange,
				pExchange->Body.TrailerCount
			) ) {
				return XHTTP1_SERVER_FEED_ERROR;
			}
			continue;
		}
		if ( Status == XHTTP1_BODY_DONE ) {
			return __xrtHttp1ServerBodyFinish(pExchange);
		}
		if ( Status == XHTTP1_BODY_ERROR ) {
			(void)__xrtHttp1ServerExchangeFailCause(
				pExchange,
				(Error.Code == XHTTP1_ERROR_BODY_TOO_LARGE) ?
					XHTTP1_SERVER_ERROR_BODY_LIMIT :
					XHTTP1_SERVER_ERROR_FRAMING,
				(Error.Code == XHTTP1_ERROR_BODY_TOO_LARGE) ?
					XERR_RANGE : XERR_PROTOCOL,
				"read-http1-server-body",
				"HTTP server request body framing is invalid",
				xrtGetError()
			);
			return XHTTP1_SERVER_FEED_ERROR;
		}
		return XHTTP1_SERVER_FEED_MORE;
	}
}

#endif

