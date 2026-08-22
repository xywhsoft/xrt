#include "../internal/xrt_http.h"
#include "../internal/xrt_time.h"

#include <xrt/http_retry.h>



#if defined(XRT_FEATURE_HTTP_RETRY)

/* 无错误副作用地解析完整无符号十进制秒数。 */
static bool __xrtHttpRetrySeconds(
	xstrview Value,
	uint64* pSeconds,
	bool* pOverflow
)
{
	uint64 iValue = 0;
	size_t i;

	*pOverflow = false;
	if ( Value.Size == 0 ) {
		return false;
	}
	for ( i = 0; i < Value.Size; i++ ) {
		uint64 iDigit;
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( (iByte < (unsigned char)'0') ||
			(iByte > (unsigned char)'9') ) {
			return false;
		}
		iDigit = (uint64)(iByte - (unsigned char)'0');
		if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
			*pOverflow = true;
			return false;
		}
		iValue = (iValue * UINT64_C(10)) + iDigit;
	}
	*pSeconds = iValue;
	return true;
}



/* 把已加载的 Retry-After 描述符规范化为短暂的栈文本。 */
static bool __xrtHttpRetryAfterFormat(
	const xhttpretryafter* pRetry,
	char Text[30],
	size_t* pSize
)
{
	if ( pRetry->Kind == XHTTP_RETRY_AFTER_DELAY ) {
		uint64 iSeconds = pRetry->Seconds;
		size_t iSize = 0;

		do {
			Text[iSize++] = (char)('0' + (iSeconds % UINT64_C(10)));
			iSeconds /= UINT64_C(10);
		} while ( iSeconds != 0 );
		for ( size_t i = 0; i < (iSize / 2u); i++ ) {
			char iByte = Text[i];

			Text[i] = Text[iSize - i - 1u];
			Text[iSize - i - 1u] = iByte;
		}
		*pSize = iSize;
		return true;
	}
	if ( pRetry->Kind == XHTTP_RETRY_AFTER_DATE ) {
		size_t iSize = xrtTimeWriteHTTPDate(Text, 30u, pRetry->Date);

		if ( iSize == XRT_NPOS ) {
			return false;
		}
		*pSize = iSize;
		return true;
	}
	__xrtErrorSetValue();
	return false;
}



/* 严格解析相对秒数或三种兼容 HTTP 日期。 */
XRT_API bool xrtHttpRetryAfterParse(
	xstrview Value,
	xhttpretryafter* pRetry
)
{
	xhttpretryafter Retry = { 0 };
	bool bOverflow;

	if ( !__xrtRangeValid(pRetry, sizeof(Retry)) ||
		!__xrtHttpViewValid(Value) ||
		__xrtRangesOverlap(
			pRetry, sizeof(Retry), Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pRetry, &Retry, sizeof(Retry));
	Value = xrtHttpOwsTrim(Value);
	if ( __xrtHttpRetrySeconds(
		Value, &Retry.Seconds, &bOverflow
	) ) {
		Retry.Kind = XHTTP_RETRY_AFTER_DELAY;
		memcpy(pRetry, &Retry, sizeof(Retry));
		return true;
	}
	if ( bOverflow ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( __xrtTimeParseHTTPDateValue(Value, &Retry.Date) ) {
		Retry.Kind = XHTTP_RETRY_AFTER_DATE;
		memcpy(pRetry, &Retry, sizeof(Retry));
		return true;
	}
	__xrtErrorSetValue();
	return false;
}



/* 把线路秒数或绝对墙钟时间安全转换为微秒延迟。 */
XRT_API bool xrtHttpRetryAfterDelay(
	const xhttpretryafter* pRetry,
	xtime iNow,
	uint64* pDelay
)
{
	xhttpretryafter Retry;
	uint64 iDelay;

	if ( !__xrtRangeValid(pRetry, sizeof(Retry)) ||
		!__xrtRangeValid(pDelay, sizeof(iDelay)) ||
		__xrtRangesOverlap(
			pRetry, sizeof(Retry), pDelay, sizeof(iDelay)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Retry, pRetry, sizeof(Retry));
	if ( Retry.Kind == XHTTP_RETRY_AFTER_DELAY ) {
		if ( Retry.Seconds >
			(UINT64_MAX / (uint64)XRT_TIME_SECOND) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iDelay = Retry.Seconds * (uint64)XRT_TIME_SECOND;
	} else if ( Retry.Kind == XHTTP_RETRY_AFTER_DATE ) {
		iDelay = Retry.Date > iNow ?
			((uint64)Retry.Date - (uint64)iNow) : UINT64_C(0);
	} else {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pDelay, &iDelay, sizeof(iDelay));
	return true;
}



/* 读取单值 Retry-After，并保持缺失与错误结果可区分。 */
XRT_API xhttpnext xrtHttpRetryAfterFields(
	const xhttpfield* pFields,
	size_t iCount,
	xtime iNow,
	uint64* pDelay
)
{
	const xhttpfield* pField;
	xhttpretryafter Retry;
	xhttpnext Next;
	uint64 iDelay;
	const uint64 iZero = 0;

	if ( !__xrtRangeValid(pDelay, sizeof(iDelay)) ||
		!__xrtHttpFieldArrayValid(pFields, iCount) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pDelay, sizeof(iDelay)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pDelay, &iZero, sizeof(iZero));
	Next = xrtHttpFieldGetUnique(
		pFields,
		iCount,
		XRT_STR_LITERAL("Retry-After"),
		&pField
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	if ( !xrtHttpRetryAfterParse(pField->Value, &Retry) ||
		!xrtHttpRetryAfterDelay(&Retry, iNow, &iDelay) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pDelay, &iDelay, sizeof(iDelay));
	return XHTTP_NEXT_ITEM;
}



/* 规范写出相对秒数或 IMF-fixdate，并保持短缓冲失败原子性。 */
XRT_API bool xrtHttpRetryAfterWrite(
	const xhttpretryafter* pRetry,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpretryafter Retry;
	char Text[30];
	size_t iRequired;

	if ( !__xrtRangeValid(pRetry, sizeof(Retry)) ||
		!__xrtRangeValid(pOutput, iCapacity) ||
		!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		__xrtRangesOverlap(
			pRetry, sizeof(Retry), pSize, sizeof(iRequired)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(iRequired)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Retry, pRetry, sizeof(Retry));
	if ( !__xrtHttpRetryAfterFormat(&Retry, Text, &iRequired) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtRangesOverlap(
		pRetry, sizeof(Retry), pOutput, iRequired
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pOutput, Text, iRequired);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 构建零结尾 Retry-After 字段值。 */
XRT_API str xrtHttpRetryAfterBuild(
	const xhttpretryafter* pRetry,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( (pSize != NULL) &&
		(!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		 !__xrtRangeValid(pRetry, sizeof(*pRetry)) ||
		 __xrtRangesOverlap(
			pRetry, sizeof(*pRetry), pSize, sizeof(iRequired)
		 )) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpRetryAfterWrite(
		pRetry, NULL, 0, &iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpRetryAfterWrite(
		pRetry, sOutput, iRequired, &iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	return sOutput;
}



/* 使用保守集合识别常见的临时服务或路由失败。 */
XRT_API bool xrtHttpRetryStatusDefault(uint16 iStatus)
{
	switch ( iStatus ) {
		case XHTTP_STATUS_REQUEST_TIMEOUT:
		case XHTTP_STATUS_MISDIRECTED_REQUEST:
		case XHTTP_STATUS_TOO_EARLY:
		case XHTTP_STATUS_TOO_MANY_REQUESTS:
		case XHTTP_STATUS_INTERNAL_SERVER_ERROR:
		case XHTTP_STATUS_BAD_GATEWAY:
		case XHTTP_STATUS_SERVICE_UNAVAILABLE:
		case XHTTP_STATUS_GATEWAY_TIMEOUT:
			return true;

		default:
			return false;
	}
}



/* 以最多 64 次有效倍增得到不回绕的封顶指数退避。 */
XRT_API bool xrtHttpRetryBackoff(
	uint64 iBase,
	uint64 iMaximum,
	uint32 iRetry,
	uint64* pDelay
)
{
	uint64 iDelay = iBase;
	uint32 i;

	if ( !__xrtRangeValid(pDelay, sizeof(iDelay)) ||
		(iMaximum == 0) ||
		(iBase > iMaximum) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	/* 零基数的全部指数项都为零，避免按外部序号空转。 */
	if ( iBase == 0 ) {
		memcpy(pDelay, &iDelay, sizeof(iDelay));
		return true;
	}
	for ( i = 0; (i < iRetry) && (iDelay < iMaximum); i++ ) {
		if ( iDelay > (iMaximum / UINT64_C(2)) ) {
			iDelay = iMaximum;
		} else {
			iDelay *= UINT64_C(2);
		}
	}
	memcpy(pDelay, &iDelay, sizeof(iDelay));
	return true;
}

#endif
