#include "../internal/xrt_http.h"

#include <xrt/http_priority.h>



#if defined(XRT_FEATURE_HTTP_PRIORITY)

/* 建立未显式指定参数的协议默认值。 */
static void __xrtHttpPriorityDefault(xhttppriority* pPriority)
{
	memset(pPriority, 0, sizeof(*pPriority));
	pPriority->Urgency = XHTTP_PRIORITY_URGENCY_DEFAULT;
}



/* 判断优先级描述符是否只包含有效的显式参数。 */
static bool __xrtHttpPriorityValid(const xhttppriority* pPriority)
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



/* 判断 Structured Fields key 是否是指定的单字节参数。 */
static bool __xrtHttpPriorityKey(
	xstrview Key,
	char iExpected
)
{
	return (Key.Size == 1u) &&
		(Key.Data[0] == iExpected);
}



/* 应用一个线路成员；重复 key 的最后一次出现决定最终结果。 */
static void __xrtHttpPriorityMemberApply(
	xhttppriority* pPriority,
	const xhttpstructureddictionarymember* pMember
)
{
	if ( __xrtHttpPriorityKey(pMember->Key, 'u') ) {
		pPriority->Urgency = XHTTP_PRIORITY_URGENCY_DEFAULT;
		pPriority->Flags &= (uint8)~XHTTP_PRIORITY_HAS_URGENCY;
		if ( (pMember->Member.Kind ==
			XHTTP_STRUCTURED_MEMBER_ITEM) &&
			(pMember->Member.Bare.Type ==
			 XHTTP_STRUCTURED_INTEGER) &&
			(pMember->Member.Bare.Number >= 0) &&
			(pMember->Member.Bare.Number <=
			 XHTTP_PRIORITY_URGENCY_MAX) ) {
			pPriority->Urgency =
				(uint8)pMember->Member.Bare.Number;
			pPriority->Flags |= XHTTP_PRIORITY_HAS_URGENCY;
		}
		return;
	}
	if ( __xrtHttpPriorityKey(pMember->Key, 'i') ) {
		pPriority->Incremental = 0;
		pPriority->Flags &=
			(uint8)~XHTTP_PRIORITY_HAS_INCREMENTAL;
		if ( (pMember->Member.Kind ==
			XHTTP_STRUCTURED_MEMBER_ITEM) &&
			(pMember->Member.Bare.Type ==
			 XHTTP_STRUCTURED_BOOLEAN) ) {
			pPriority->Incremental =
				(uint8)pMember->Member.Bare.Number;
			pPriority->Flags |=
				XHTTP_PRIORITY_HAS_INCREMENTAL;
		}
	}
}



/* 初始化 Priority 默认值。 */
XRT_API void xrtHttpPriorityInit(xhttppriority* pPriority)
{
	xhttppriority Priority;

	if ( !__xrtRangeValid(pPriority, sizeof(Priority)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	__xrtHttpPriorityDefault(&Priority);
	memcpy(pPriority, &Priority, sizeof(Priority));
}



/* 解析一个 Priority 字段值。 */
XRT_API bool xrtHttpPriorityValueParse(
	xstrview Value,
	xhttppriority* pPriority
)
{
	xhttpstructureddictionarymember Member;
	xhttppriority Priority;
	xhttpnext Next;
	size_t iOffset = 0;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pPriority, sizeof(Priority)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size,
			pPriority, sizeof(Priority)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtHttpPriorityDefault(&Priority);
	for ( ;; ) {
		Next = xrtHttpStructuredDictionaryNext(
			Value, &iOffset, &Member
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		__xrtHttpPriorityMemberApply(&Priority, &Member);
	}
	memcpy(pPriority, &Priority, sizeof(Priority));
	return true;
}



/* 解析全部重复 Priority 字段行。 */
XRT_API bool xrtHttpPriorityParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttppriority* pPriority
)
{
	xhttpstructureddictionarymember Member;
	xhttpstructuredfieldcursor Cursor;
	xhttppriority Priority;
	xhttpnext Next;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pPriority, sizeof(Priority)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pPriority, sizeof(Priority)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtHttpPriorityDefault(&Priority);
	xrtHttpStructuredFieldCursorInit(&Cursor);
	for ( ;; ) {
		Next = xrtHttpStructuredDictionaryFieldNext(
			pFields, iCount, XRT_STR_LITERAL("Priority"),
			&Cursor, &Member
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		__xrtHttpPriorityMemberApply(&Priority, &Member);
	}
	memcpy(pPriority, &Priority, sizeof(Priority));
	return true;
}



/* 以显式参数为边界覆盖两个 Priority 描述符。 */
XRT_API bool xrtHttpPriorityOverlay(
	const xhttppriority* pBase,
	const xhttppriority* pUpdate,
	xhttppriority* pPriority
)
{
	xhttppriority Base;
	xhttppriority Update;
	xhttppriority Priority;

	if ( !__xrtRangeValid(pBase, sizeof(Base)) ||
		!__xrtRangeValid(pUpdate, sizeof(Update)) ||
		!__xrtRangeValid(pPriority, sizeof(Priority)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Base, pBase, sizeof(Base));
	memcpy(&Update, pUpdate, sizeof(Update));
	if ( !__xrtHttpPriorityValid(&Base) ||
		!__xrtHttpPriorityValid(&Update) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtHttpPriorityDefault(&Priority);
	if ( (Base.Flags & XHTTP_PRIORITY_HAS_URGENCY) != 0 ) {
		Priority.Urgency = Base.Urgency;
		Priority.Flags |= XHTTP_PRIORITY_HAS_URGENCY;
	}
	if ( (Base.Flags & XHTTP_PRIORITY_HAS_INCREMENTAL) != 0 ) {
		Priority.Incremental = Base.Incremental;
		Priority.Flags |= XHTTP_PRIORITY_HAS_INCREMENTAL;
	}
	if ( (Update.Flags & XHTTP_PRIORITY_HAS_URGENCY) != 0 ) {
		Priority.Urgency = Update.Urgency;
		Priority.Flags |= XHTTP_PRIORITY_HAS_URGENCY;
	}
	if ( (Update.Flags & XHTTP_PRIORITY_HAS_INCREMENTAL) != 0 ) {
		Priority.Incremental = Update.Incremental;
		Priority.Flags |= XHTTP_PRIORITY_HAS_INCREMENTAL;
	}
	memcpy(pPriority, &Priority, sizeof(Priority));
	return true;
}

#endif
