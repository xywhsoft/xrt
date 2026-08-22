#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_EXCHANGE)

/* 验证公开 Feed 的输入和输出不会覆盖 Exchange 或当前请求快照。 */
static bool __xrtHttp1ServerExchangeFeedValid(
	xhttp1serverexchange* pExchange,
	xbytesview Input,
	size_t* pAccepted
)
{
	size_t iZero = 0;

	if ( !__xrtRangeValid(pAccepted, sizeof(iZero)) ) {
		return false;
	}
	if ( pExchange == NULL ) {
		memcpy(pAccepted, &iZero, sizeof(iZero));
		return false;
	}
	if ( !__xrtRangeValid(Input.Data, Input.Size) ) {
		memcpy(pAccepted, &iZero, sizeof(iZero));
		return false;
	}
	if ( __xrtRangesOverlap(
		pAccepted, sizeof(iZero), Input.Data, Input.Size
	) || __xrtRangesOverlap(
		pAccepted, sizeof(iZero), pExchange, sizeof(*pExchange)
	) || __xrtRangesOverlap(
		Input.Data, Input.Size, pExchange, sizeof(*pExchange)
	) || ((pExchange->Request != NULL) &&
		(!__xrtHttpServerRequestOutputValid(
			pExchange->Request, pAccepted, sizeof(iZero)
		) || !__xrtHttpServerRequestOutputValid(
			pExchange->Request, Input.Data, Input.Size
		))) ) {
		return false;
	}
	memcpy(pAccepted, &iZero, sizeof(iZero));
	return true;
}



/* 返回从指定位置开始的输入视图，并避免对空指针执行算术。 */
static xbytesview __xrtHttp1ServerExchangeInputAt(
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



/* 返回已经固定的 Exchange 终态而不再接受输入。 */
static xhttp1serverfeedstatus __xrtHttp1ServerExchangeStable(
	xhttp1serverexchange* pExchange
)
{
	switch ( pExchange->State ) {
		case XRT_HTTP_SERVER_STATE_COMPLETE:
			return XHTTP1_SERVER_FEED_COMPLETE;

		case XRT_HTTP_SERVER_STATE_REJECTED:
			return XHTTP1_SERVER_FEED_REJECTED;

		case XRT_HTTP_SERVER_STATE_CLOSED:
			return XHTTP1_SERVER_FEED_CLOSED;

		case XRT_HTTP_SERVER_STATE_FAILED:
			if ( pExchange->Error != NULL ) {
				xrtSetError(pExchange->Error);
			}
			return XHTTP1_SERVER_FEED_ERROR;

		default:
			return XHTTP1_SERVER_FEED_MORE;
	}
}



/* 推进当前请求，并准确报告可以从连接缓冲移除的前缀。 */
XRT_API xhttp1serverfeedstatus xrtHttp1ServerExchangeFeed(
	xhttp1serverexchange* pExchange,
	xbytesview Input,
	bool bEnd,
	size_t* pAccepted
)
{
	xhttp1serverfeedstatus Status;
	size_t iOffset = 0;

	if ( !__xrtHttp1ServerExchangeFeedValid(
		pExchange, Input, pAccepted
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP1_SERVER_FEED_ERROR;
	}
	if ( pExchange->InCallback ) {
		(void)__xrtHttp1ServerExchangeFail(
			pExchange,
			XHTTP1_SERVER_ERROR_STATE,
			XERR_STATE,
			"feed-http1-server-exchange",
			"HTTP server Exchange cannot be fed from one of its callbacks"
		);
		return XHTTP1_SERVER_FEED_ERROR;
	}
	Status = __xrtHttp1ServerExchangeStable(pExchange);
	if ( Status != XHTTP1_SERVER_FEED_MORE ) {
		return Status;
	}
	if ( pExchange->Paused ) {
		return XHTTP1_SERVER_FEED_PAUSED;
	}
	if ( (pExchange->State == XRT_HTTP_SERVER_STATE_HEAD) &&
		(Input.Size == 0) && bEnd &&
		(pExchange->HeadBuffer.Size == 0) ) {
		pExchange->State = XRT_HTTP_SERVER_STATE_CLOSED;
		return XHTTP1_SERVER_FEED_CLOSED;
	}

	for ( ;; ) {
		xbytesview Current = __xrtHttp1ServerExchangeInputAt(
			Input, iOffset
		);
		size_t iAccepted = 0;
		uint32 iState = pExchange->State;

		if ( iState == XRT_HTTP_SERVER_STATE_HEAD ) {
			Status = __xrtHttp1ServerExchangeFeedHead(
				pExchange,
				Current,
				bEnd,
				&iAccepted
			);
		} else if ( iState == XRT_HTTP_SERVER_STATE_BODY ) {
			Status = __xrtHttp1ServerExchangeFeedBody(
				pExchange,
				Current,
				bEnd,
				&iAccepted
			);
		} else {
			Status = __xrtHttp1ServerExchangeStable(
				pExchange
			);
		}
		if ( iAccepted > Current.Size ) {
			(void)__xrtHttp1ServerExchangeFail(
				pExchange,
				XHTTP1_SERVER_ERROR_STATE,
				XERR_STATE,
				"feed-http1-server-exchange",
				"HTTP server Exchange accepted beyond its input"
			);
			return XHTTP1_SERVER_FEED_ERROR;
		}
		iOffset += iAccepted;
		memcpy(pAccepted, &iOffset, sizeof(iOffset));
		if ( Status != XHTTP1_SERVER_FEED_MORE ) {
			return Status;
		}
		if ( (iState == XRT_HTTP_SERVER_STATE_HEAD) &&
			(pExchange->State == XRT_HTTP_SERVER_STATE_BODY) &&
			(iOffset < Input.Size) ) {
			continue;
		}
		return XHTTP1_SERVER_FEED_MORE;
	}
}

#endif
