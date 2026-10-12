#include "../internal/xrt_http.h"

#include <xrt/http_retry.h>
#include <xrt/time.h>



#if defined(XHTTP_FEATURE_HTTP_RETRY)

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
	xrtSetErrorKind(XERR_VALUE);
	return false;
}



/* 严格解析相对秒数或三种兼容 HTTP 日期。 */
XHTTP_API bool xrtHttpRetryAfterParse(
	xstrview Value,
	xhttpretryafter* pRetry
)
{
	xhttpretryafter Retry = { 0 };
	bool bOverflow;

	if ( !xrtMemRangeValid(pRetry, sizeof(Retry)) ||
		!__xhttpViewValid(Value) ||
		xrtMemRangesOverlap(
			pRetry, sizeof(Retry), Value.Data, Value.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
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
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( xrtTimeTryParseHTTPDate(Value, &Retry.Date) ) {
		Retry.Kind = XHTTP_RETRY_AFTER_DATE;
		memcpy(pRetry, &Retry, sizeof(Retry));
		return true;
	}
	xrtSetErrorKind(XERR_VALUE);
	return false;
}



/* 把线路秒数或绝对墙钟时间安全转换为毫秒延迟。 */
XHTTP_API bool xrtHttpRetryAfterDelay(
	const xhttpretryafter* pRetry,
	xtime iNow,
	uint64* pDelay
)
{
	xhttpretryafter Retry;
	uint64 iDelay;

	if ( !xrtMemRangeValid(pRetry, sizeof(Retry)) ||
		!xrtMemRangeValid(pDelay, sizeof(iDelay)) ||
		xrtMemRangesOverlap(
			pRetry, sizeof(Retry), pDelay, sizeof(iDelay)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(&Retry, pRetry, sizeof(Retry));
	if ( Retry.Kind == XHTTP_RETRY_AFTER_DELAY ) {
		if ( Retry.Seconds >
			(UINT64_MAX / (uint64)XRT_TIME_SECOND) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iDelay = Retry.Seconds * (uint64)XRT_TIME_SECOND;
	} else if ( Retry.Kind == XHTTP_RETRY_AFTER_DATE ) {
		iDelay = Retry.Date > iNow ?
			((uint64)Retry.Date - (uint64)iNow) : UINT64_C(0);
	} else {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	memcpy(pDelay, &iDelay, sizeof(iDelay));
	return true;
}



/* 读取单值 Retry-After，并保持缺失与错误结果可区分。 */
XHTTP_API xhttpnext xrtHttpRetryAfterFields(
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

	if ( !xrtMemRangeValid(pDelay, sizeof(iDelay)) ||
		!__xhttpFieldArrayValid(pFields, iCount) ||
		__xhttpFieldArrayOverlap(
			pFields, iCount, pDelay, sizeof(iDelay)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
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
XHTTP_API bool xrtHttpRetryAfterWrite(
	const xhttpretryafter* pRetry,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpretryafter Retry;
	char Text[30];
	size_t iRequired;

	if ( !xrtMemRangeValid(pRetry, sizeof(Retry)) ||
		!xrtMemRangeValid(pOutput, iCapacity) ||
		!xrtMemRangeValid(pSize, sizeof(iRequired)) ||
		xrtMemRangesOverlap(
			pRetry, sizeof(Retry), pSize, sizeof(iRequired)
		) || xrtMemRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(iRequired)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
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
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( xrtMemRangesOverlap(
		pRetry, sizeof(Retry), pOutput, iRequired
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pOutput, Text, iRequired);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 构建零结尾 Retry-After 字段值。 */
XHTTP_API str xrtHttpRetryAfterBuild(
	const xhttpretryafter* pRetry,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( (pSize != NULL) &&
		(!xrtMemRangeValid(pSize, sizeof(iRequired)) ||
		 !xrtMemRangeValid(pRetry, sizeof(*pRetry)) ||
		 xrtMemRangesOverlap(
			pRetry, sizeof(*pRetry), pSize, sizeof(iRequired)
		 )) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( !xrtHttpRetryAfterWrite(
		pRetry, NULL, 0, &iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		xrtSetErrorKind(XERR_RANGE);
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



#endif
