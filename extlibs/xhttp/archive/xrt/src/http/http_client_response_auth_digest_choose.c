#include "../internal/xrt_http_client.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE)

/* 校验选择输出，并建立失败原子的空结果。 */
static bool __xrtHttpResponseDigestChoosePrepare(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
)
{
	xhttpdigestchallenge EmptyChallenge = { 0 };
	xhttpdigestchoice EmptyChoice = { 0 };
	size_t iZero = 0;

	if ( ((pPolicy != NULL) && !__xrtRangeValid(
			pPolicy, sizeof(*pPolicy)
		)) || !__xrtHttpResponseOutputValid(
			pResponse, pSize, sizeof(*pSize)
		) || !__xrtHttpResponseOutputValid(
			pResponse, pChallenge, sizeof(*pChallenge)
		) || !__xrtHttpResponseOutputValid(
			pResponse, pChoice, sizeof(*pChoice)
		) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !__xrtHttpResponseOutputValid(
			pResponse, pOutput, iCapacity
		)) || __xrtRangesOverlap(
			pSize, sizeof(*pSize), pChallenge, sizeof(*pChallenge)
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize), pChoice, sizeof(*pChoice)
		) || __xrtRangesOverlap(
			pChallenge, sizeof(*pChallenge),
			pChoice, sizeof(*pChoice)
		) || ((pPolicy != NULL) && (__xrtRangesOverlap(
			pPolicy, sizeof(*pPolicy), pSize, sizeof(*pSize)
		) || __xrtRangesOverlap(
			pPolicy, sizeof(*pPolicy),
			pChallenge, sizeof(*pChallenge)
		) || __xrtRangesOverlap(
			pPolicy, sizeof(*pPolicy), pChoice, sizeof(*pChoice)
		) || ((pOutput != NULL) && __xrtRangesOverlap(
			pPolicy, sizeof(*pPolicy), pOutput, iCapacity
		)))) || ((pOutput != NULL) && (__xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(*pSize)
		) || __xrtRangesOverlap(
			pOutput, iCapacity,
			pChallenge, sizeof(*pChallenge)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pChoice, sizeof(*pChoice)
		))) ) {
		__xrtHttpResponseSetError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_ARGUMENT,
			"choose-http-response-digest-challenge",
			"HTTP response Digest selection output is invalid",
			NULL
		);
		return false;
	}
	memcpy(pSize, &iZero, sizeof(iZero));
	memcpy(pChallenge, &EmptyChallenge, sizeof(EmptyChallenge));
	memcpy(pChoice, &EmptyChoice, sizeof(EmptyChoice));
	return true;
}



/* 恢复通用解析结果覆盖的完整 challenge 连续视图。 */
static bool __xrtHttpResponseDigestChallengeValue(
	const xhttpauth* pAuth,
	xstrview* pValue
)
{
	uintptr_t iStart = (uintptr_t)pAuth->Scheme.Data;
	uintptr_t iEnd;

	if ( pAuth->Kind == XHTTP_AUTH_NONE ) {
		if ( pAuth->Scheme.Size > (UINTPTR_MAX - iStart) ) {
			return false;
		}
		iEnd = iStart + pAuth->Scheme.Size;
	} else {
		iEnd = (uintptr_t)pAuth->Data.Data;
		if ( pAuth->Data.Size > (UINTPTR_MAX - iEnd) ) {
			return false;
		}
		iEnd += pAuth->Data.Size;
	}
	if ( iEnd < iStart ) {
		return false;
	}
	*pValue = (xstrview){
		pAuth->Scheme.Data,
		(size_t)(iEnd - iStart)
	};
	return true;
}



/* 包装协议层解析或策略错误到稳定的客户端响应错误域。 */
static xhttpnext __xrtHttpResponseDigestChooseError(cstr sMessage)
{
	__xrtHttpResponseWrapError(
		XERR_VALUE,
		XHTTP_RESPONSE_ERROR_AUTH,
		"choose-http-response-digest-challenge",
		sMessage
	);
	return XHTTP_NEXT_ERROR;
}



/* 按字段线路顺序选择首个本地支持的 Digest challenge。 */
static xhttpnext __xrtHttpResponseDigestChallengeChoose(
	const xhttpresponse* pResponse,
	bool bProxy,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
)
{
	xhttpauthcursor Cursor;
	xhttpauth Auth;
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	xstrview Value;
	size_t iRequired;
	xhttpnext Next;

	if ( !__xrtHttpResponseDigestChoosePrepare(
		pResponse,
		pPolicy,
		pOutput,
		iCapacity,
		pSize,
		pChallenge,
		pChoice
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	xrtHttpAuthCursorInit(&Cursor);
	for ( ;; ) {
		xhttpdigestchoosecheck Check;

		Next = bProxy ? xrtHttpResponseProxyChallengeNext(
			pResponse, &Cursor, &Auth
		) : xrtHttpResponseChallengeNext(
			pResponse, &Cursor, &Auth
		);
		if ( Next != XHTTP_NEXT_ITEM ) {
			return Next;
		}
		if ( !xrtHttpTokenEqual(
			Auth.Scheme, XRT_STR_LITERAL("Digest")
		) ) {
			continue;
		}
		if ( !__xrtHttpResponseDigestChallengeValue(
			&Auth, &Value
		) ) {
			__xrtHttpResponseSetError(
				XERR_STATE,
				XHTTP_RESPONSE_ERROR_AUTH,
				"choose-http-response-digest-challenge",
				"HTTP response Digest challenge range is invalid",
				NULL
			);
			return XHTTP_NEXT_ERROR;
		}
		if ( !xrtHttpDigestChallengeRead(
			Value, NULL, 0, &iRequired, &Challenge
		) ) {
			return __xrtHttpResponseDigestChooseError(
				"HTTP response Digest challenge is invalid"
			);
		}
		Check = xrtHttpDigestChallengeChoose(
			&Challenge, pPolicy, &Choice
		);
		if ( Check == XHTTP_DIGEST_CHOOSE_ERROR ) {
			return __xrtHttpResponseDigestChooseError(
				"HTTP response Digest selection policy is invalid"
			);
		}
		if ( Check == XHTTP_DIGEST_CHOOSE_REJECTED ) {
			continue;
		}
		memcpy(pSize, &iRequired, sizeof(iRequired));
		if ( pOutput != NULL ) {
			if ( !xrtHttpDigestChallengeRead(
				Value,
				pOutput,
				iCapacity,
				&iRequired,
				&Challenge
			) ) {
				memcpy(pSize, &iRequired, sizeof(iRequired));
				return __xrtHttpResponseDigestChooseError(
					"HTTP response Digest challenge cannot be decoded"
				);
			}
		}
		memcpy(pChallenge, &Challenge, sizeof(Challenge));
		memcpy(pChoice, &Choice, sizeof(Choice));
		return XHTTP_NEXT_ITEM;
	}
}



/* 选择源站首个可用 Digest challenge。 */
XRT_API xhttpnext xrtHttpResponseDigestChallengeChoose(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
)
{
	return __xrtHttpResponseDigestChallengeChoose(
		pResponse,
		false,
		pPolicy,
		pOutput,
		iCapacity,
		pSize,
		pChallenge,
		pChoice
	);
}



/* 选择代理首个可用 Digest challenge。 */
XRT_API xhttpnext xrtHttpResponseProxyDigestChallengeChoose(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
)
{
	return __xrtHttpResponseDigestChallengeChoose(
		pResponse,
		true,
		pPolicy,
		pOutput,
		iCapacity,
		pSize,
		pChallenge,
		pChoice
	);
}

#endif
