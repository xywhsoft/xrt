#include "../internal/xrt_http.h"

#include <xrt/http_priority.h>



#if defined(XRT_FEATURE_HTTP_PRIORITY_WRITE)

/* 判断待写出描述符只包含有效的显式参数。 */
static bool __xrtHttpPriorityWriteValid(
	const xhttppriority* pPriority
)
{
	if ( (pPriority->Flags &
		~(XHTTP_PRIORITY_HAS_URGENCY |
		  XHTTP_PRIORITY_HAS_INCREMENTAL)) != 0 ) {
		return false;
	}
	if ( ((pPriority->Flags &
		XHTTP_PRIORITY_HAS_URGENCY) != 0) &&
		(pPriority->Urgency > XHTTP_PRIORITY_URGENCY_MAX) ) {
		return false;
	}
	if ( ((pPriority->Flags &
		XHTTP_PRIORITY_HAS_INCREMENTAL) != 0) &&
		(pPriority->Incremental > 1u) ) {
		return false;
	}
	return true;
}



/* 建立只有一个裸值的 Dictionary Item。 */
static void __xrtHttpPriorityWriteEntry(
	xhttpstructureddictionaryentry* pEntry,
	xstrview Key,
	xhttpstructuredtype Type,
	int64 iNumber
)
{
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->Key = Key;
	pEntry->Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	pEntry->Member.Item.Bare.Type = Type;
	pEntry->Member.Item.Bare.Number = iNumber;
}



/* 规范写出显式 Priority 参数。 */
XRT_API bool xrtHttpPriorityWrite(
	const xhttppriority* pPriority,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpstructureddictionaryentry arrEntries[2];
	xhttppriority Priority;
	size_t iCount = 0;

	if ( !__xrtRangeValid(pPriority, sizeof(Priority)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Priority, pPriority, sizeof(Priority));
	if ( !__xrtHttpPriorityWriteValid(&Priority) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Priority.Flags & XHTTP_PRIORITY_HAS_URGENCY) != 0 ) {
		__xrtHttpPriorityWriteEntry(
			&arrEntries[iCount++], XRT_STR_LITERAL("u"),
			XHTTP_STRUCTURED_INTEGER, Priority.Urgency
		);
	}
	if ( (Priority.Flags &
		XHTTP_PRIORITY_HAS_INCREMENTAL) != 0 ) {
		__xrtHttpPriorityWriteEntry(
			&arrEntries[iCount++], XRT_STR_LITERAL("i"),
			XHTTP_STRUCTURED_BOOLEAN, Priority.Incremental
		);
	}
	return xrtHttpStructuredDictionaryWrite(
		arrEntries, iCount, pOutput, iCapacity, pSize
	);
}

#endif
