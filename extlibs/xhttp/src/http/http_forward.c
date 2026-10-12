#include "../internal/xrt_http.h"

#include <xrt/http_forward.h>



#if defined(XHTTP_FEATURE_HTTP_FORWARD)

/* 判断字段名是否属于 RFC 9110 固定逐跳集合。 */
XHTTP_API bool xrtHttpHopFieldKnown(xstrview Name)
{
	if ( !xrtHttpTokenValid(Name) ) {
		return false;
	}
	return xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Connection")
	) || xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Keep-Alive")
	) || xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Proxy-Connection")
	) || xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("TE")
	) || xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Transfer-Encoding")
	) || xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Upgrade")
	);
}



/* 判断字段是固定逐跳字段或被 Connection 提名。 */
XHTTP_API xhttpnext xrtHttpHopField(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
)
{
	xhttpnext Next;

	if ( !xrtHttpTokenValid(Name) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	Next = xrtHttpConnectionFind(
		pFields, iCount, Name
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return Next;
	}
	return (xrtHttpHopFieldKnown(Name) ||
		(Next == XHTTP_NEXT_ITEM)) ?
		XHTTP_NEXT_ITEM : XHTTP_NEXT_END;
}



/* 严格解析十进制 Max-Forwards。 */
XHTTP_API bool xrtHttpMaxForwardsParse(
	xstrview Value,
	uint64* pForwards
)
{
	uint64 iValue = 0;
	size_t i;

	if ( !xrtMemRangeValid(pForwards, sizeof(iValue)) ||
		!__xhttpViewValid(Value) ||
		xrtMemRangesOverlap(
			pForwards, sizeof(iValue), Value.Data, Value.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pForwards, &iValue, sizeof(iValue));
	if ( Value.Size == 0 ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	for ( i = 0; i < Value.Size; i++ ) {
		uint8 iByte = (uint8)Value.Data[i];
		uint64 iDigit;

		if ( (iByte < (uint8)'0') ||
			(iByte > (uint8)'9') ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		iDigit = (uint64)(iByte - (uint8)'0');
		if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iValue = (iValue * UINT64_C(10)) + iDigit;
	}
	memcpy(pForwards, &iValue, sizeof(iValue));
	return true;
}



/* 解析并更新 Max-Forwards 转发计数。 */
XHTTP_API xhttpforwardstatus xrtHttpMaxForwardsUpdate(
	xstrview Value,
	uint64 iMaximum,
	uint64* pNext
)
{
	uint64 iValue;
	uint64 iNext = 0;

	if ( !xrtMemRangeValid(pNext, sizeof(iNext)) ||
		!__xhttpViewValid(Value) ||
		xrtMemRangesOverlap(
			pNext, sizeof(iNext), Value.Data, Value.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_FORWARD_ERROR;
	}
	memcpy(pNext, &iNext, sizeof(iNext));
	if ( !xrtHttpMaxForwardsParse(Value, &iValue) ) {
		return XHTTP_FORWARD_ERROR;
	}
	if ( iValue == 0 ) {
		return XHTTP_FORWARD_FINAL;
	}
	iNext = iValue - UINT64_C(1);
	if ( iNext > iMaximum ) {
		iNext = iMaximum;
	}
	memcpy(pNext, &iNext, sizeof(iNext));
	return XHTTP_FORWARD_NEXT;
}



/* 规范写出十进制 Max-Forwards。 */
XHTTP_API bool xrtHttpMaxForwardsWrite(
	uint64 iForwards,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iRequired = __xhttpUInt64Size(iForwards);

	if ( !xrtMemRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !xrtMemRangeValid(pOutput, iCapacity)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (pOutput != NULL) && xrtMemRangesOverlap(
		pOutput, iRequired, pSize, sizeof(iRequired)
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < iRequired ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	(void)__xhttpUInt64Write((char*)pOutput, iForwards);
	return true;
}

#endif
