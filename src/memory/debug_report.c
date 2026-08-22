#include "../internal/xrt_memory.h"

#include <stdarg.h>
#include <stdio.h>



#if defined(XRT_FEATURE_MEMORY_DEBUG_REPORT)

/* 报告状态封装输出失败，避免后续片段继续写入。 */
typedef struct xrt_memdebug_report_state {
	xmemdebugwriteproc Writer;
	ptr UserData;
	bool Failed;
} xrt_memdebug_report_state;



/* 事件快照在开始输出前固定，避免写入器分配污染本次报告。 */
typedef struct xrt_memdebug_report_events {
	xmemdebugevent Items[XRT_MEMDEBUG_EVENT_LIMIT];
	size_t Count;
} xrt_memdebug_report_events;



/* 写入一段已经确定长度的数据。 */
static bool __xrtMemDebugReportWrite(
	xrt_memdebug_report_state* pState,
	const void* pData,
	size_t iSize
)
{
	xbytesview Data;

	if ( pState->Failed ) {
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	Data.Data = (cbytes)pData;
	Data.Size = iSize;
	if ( !pState->Writer(Data, pState->UserData) ) {
		pState->Failed = true;
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetInvalidState();
		}
		return false;
	}
	return true;
}



/* 写入一段零结尾常量文本。 */
static bool __xrtMemDebugReportText(
	xrt_memdebug_report_state* pState,
	cstr sText
)
{
	return __xrtMemDebugReportWrite(pState, sText, strlen(sText));
}



/* 格式化只包含数字和地址的短报告片段。 */
static bool __xrtMemDebugReportFormat(
	xrt_memdebug_report_state* pState,
	cstr sFormat,
	...
)
{
	char sBuffer[512];
	va_list Args;
	int iLength;

	va_start(Args, sFormat);
	iLength = vsnprintf(sBuffer, sizeof(sBuffer), sFormat, Args);
	va_end(Args);
	if ( (iLength < 0) || ((size_t)iLength >= sizeof(sBuffer)) ) {
		__xrtErrorSetSizeOverflow();
		pState->Failed = true;
		return false;
	}
	return __xrtMemDebugReportWrite(pState, sBuffer, (size_t)iLength);
}



/* 写入一个 UTF-8 JSON 字符串或 null。 */
static bool __xrtMemDebugReportJsonString(
	xrt_memdebug_report_state* pState,
	cstr sText
)
{
	const unsigned char* pStart;
	const unsigned char* pCurrent;

	if ( sText == NULL ) {
		return __xrtMemDebugReportText(pState, "null");
	}
	if ( !__xrtMemDebugReportText(pState, "\"") ) {
		return false;
	}
	pStart = (const unsigned char*)sText;
	pCurrent = pStart;
	while ( *pCurrent != 0 ) {
		cstr sEscape = NULL;
		char sControl[7];

		switch ( *pCurrent ) {
			case '"':
				sEscape = "\\\"";
				break;
			case '\\':
				sEscape = "\\\\";
				break;
			case '\b':
				sEscape = "\\b";
				break;
			case '\f':
				sEscape = "\\f";
				break;
			case '\n':
				sEscape = "\\n";
				break;
			case '\r':
				sEscape = "\\r";
				break;
			case '\t':
				sEscape = "\\t";
				break;
			default:
				break;
		}
		if ( (sEscape == NULL) && (*pCurrent >= 0x20u) ) {
			pCurrent++;
			continue;
		}
		if ( pCurrent != pStart ) {
			if ( !__xrtMemDebugReportWrite(
				pState,
				pStart,
				(size_t)(pCurrent - pStart)
			) ) {
				return false;
			}
		}
		if ( sEscape != NULL ) {
			if ( !__xrtMemDebugReportText(pState, sEscape) ) {
				return false;
			}
		} else {
			(void)snprintf(sControl, sizeof(sControl), "\\u%04x", (unsigned int)*pCurrent);
			if ( !__xrtMemDebugReportWrite(pState, sControl, 6) ) {
				return false;
			}
		}
		pCurrent++;
		pStart = pCurrent;
	}
	if ( pCurrent != pStart ) {
		if ( !__xrtMemDebugReportWrite(
			pState,
			pStart,
			(size_t)(pCurrent - pStart)
		) ) {
			return false;
		}
	}
	return __xrtMemDebugReportText(pState, "\"");
}



/* 收集有界事件快照。 */
static bool __xrtMemDebugReportCollectEvent(
	const xmemdebugevent* pEvent,
	ptr pUserData
)
{
	xrt_memdebug_report_events* pEvents = (xrt_memdebug_report_events*)pUserData;

	if ( pEvents->Count == XRT_MEMDEBUG_EVENT_LIMIT ) {
		return false;
	}
	pEvents->Items[pEvents->Count++] = *pEvent;
	return true;
}



/* 写入文本报告中的调用位置。 */
static bool __xrtMemDebugReportTextSite(
	xrt_memdebug_report_state* pState,
	cstr sFile,
	uint32 iLine
)
{
	if ( !__xrtMemDebugReportText(pState, " site=") ||
		 !__xrtMemDebugReportJsonString(pState, sFile) ) {
		return false;
	}
	return __xrtMemDebugReportFormat(pState, ":%u\n", (unsigned int)iLine);
}



/* 写入便于人直接阅读的报告。 */
static bool __xrtMemDebugReportWriteText(
	xrt_memdebug_report_state* pState,
	const xmemdebugsnapshot* pSnapshot,
	const xmemdebugallocation* pAllocations,
	size_t iAllocationCount,
	const xrt_memdebug_report_events* pEvents
)
{
	if ( !__xrtMemDebugReportText(pState, "XRT memory debug report\n") ||
		 !__xrtMemDebugReportFormat(
			pState,
			"enabled=%s\nlive_count=%llu\nlive_bytes=%llu\n"
			"peak_count=%llu\npeak_bytes=%llu\n"
			"quarantine_count=%llu\nquarantine_bytes=%llu\n",
			pSnapshot->Enabled ? "true" : "false",
			(unsigned long long)pSnapshot->LiveCount,
			(unsigned long long)pSnapshot->LiveBytes,
			(unsigned long long)pSnapshot->PeakCount,
			(unsigned long long)pSnapshot->PeakBytes,
			(unsigned long long)pSnapshot->QuarantineCount,
			(unsigned long long)pSnapshot->QuarantineBytes
		) ||
		 !__xrtMemDebugReportFormat(
			pState,
			"alloc_count=%llu\nfree_count=%llu\nrealloc_count=%llu\n"
			"double_free_count=%llu\ninvalid_free_count=%llu\n"
			"overflow_count=%llu\nunderflow_count=%llu\n"
			"use_after_free_count=%llu\n",
			(unsigned long long)pSnapshot->AllocCount,
			(unsigned long long)pSnapshot->FreeCount,
			(unsigned long long)pSnapshot->ReallocCount,
			(unsigned long long)pSnapshot->DoubleFreeCount,
			(unsigned long long)pSnapshot->InvalidFreeCount,
			(unsigned long long)pSnapshot->OverflowCount,
			(unsigned long long)pSnapshot->UnderflowCount,
			(unsigned long long)pSnapshot->UseAfterFreeCount
		) ||
		 !__xrtMemDebugReportFormat(
			pState,
			"temp_current_bytes=%llu\ntemp_peak_bytes=%llu\n"
			"temp_reset_count=%llu\nevent_count=%llu\n",
			(unsigned long long)pSnapshot->TempCurrentBytes,
			(unsigned long long)pSnapshot->TempPeakBytes,
			(unsigned long long)pSnapshot->TempResetCount,
			(unsigned long long)pEvents->Count
		) ||
		 !__xrtMemDebugReportText(pState, "\n[live_allocations]\n") ) {
		return false;
	}

	for ( size_t i = 0; i < iAllocationCount; i++ ) {
		if ( !__xrtMemDebugReportFormat(
			pState,
			"address=%p size=%llu",
			pAllocations[i].Address,
			(unsigned long long)pAllocations[i].Size
		) ||
			 !__xrtMemDebugReportTextSite(
				pState,
				pAllocations[i].File,
				pAllocations[i].Line
			) ) {
			return false;
		}
	}
	if ( !__xrtMemDebugReportText(pState, "\n[events]\n") ) {
		return false;
	}
	for ( size_t i = 0; i < pEvents->Count; i++ ) {
		const xmemdebugevent* pEvent = &pEvents->Items[i];

		if ( !__xrtMemDebugReportFormat(
			pState,
			"sequence=%llu kind=%s address=%p size=%llu",
			(unsigned long long)pEvent->Sequence,
			xrtMemDebugEventName(pEvent->Kind),
			pEvent->Address,
			(unsigned long long)pEvent->Size
		) ||
			 !__xrtMemDebugReportTextSite(pState, pEvent->File, pEvent->Line) ) {
			return false;
		}
	}
	return true;
}



/* 写入机器可读的 JSON 统计对象。 */
static bool __xrtMemDebugReportWriteJsonStats(
	xrt_memdebug_report_state* pState,
	const xmemdebugsnapshot* pSnapshot
)
{
	if ( !__xrtMemDebugReportFormat(
		pState,
		"\"enabled\":%s,\"live_count\":%llu,\"live_bytes\":%llu,"
		"\"peak_count\":%llu,\"peak_bytes\":%llu,"
		"\"quarantine_count\":%llu,\"quarantine_bytes\":%llu,"
		"\"alloc_count\":%llu,\"free_count\":%llu,\"realloc_count\":%llu",
		pSnapshot->Enabled ? "true" : "false",
		(unsigned long long)pSnapshot->LiveCount,
		(unsigned long long)pSnapshot->LiveBytes,
		(unsigned long long)pSnapshot->PeakCount,
		(unsigned long long)pSnapshot->PeakBytes,
		(unsigned long long)pSnapshot->QuarantineCount,
		(unsigned long long)pSnapshot->QuarantineBytes,
		(unsigned long long)pSnapshot->AllocCount,
		(unsigned long long)pSnapshot->FreeCount,
		(unsigned long long)pSnapshot->ReallocCount
	) ) {
		return false;
	}
	return __xrtMemDebugReportFormat(
		pState,
		",\"double_free_count\":%llu,\"invalid_free_count\":%llu,"
		"\"overflow_count\":%llu,\"underflow_count\":%llu,"
		"\"use_after_free_count\":%llu,\"temp_current_bytes\":%llu,"
		"\"temp_peak_bytes\":%llu,\"temp_reset_count\":%llu",
		(unsigned long long)pSnapshot->DoubleFreeCount,
		(unsigned long long)pSnapshot->InvalidFreeCount,
		(unsigned long long)pSnapshot->OverflowCount,
		(unsigned long long)pSnapshot->UnderflowCount,
		(unsigned long long)pSnapshot->UseAfterFreeCount,
		(unsigned long long)pSnapshot->TempCurrentBytes,
		(unsigned long long)pSnapshot->TempPeakBytes,
		(unsigned long long)pSnapshot->TempResetCount
	);
}



/* 写入机器可读的 JSON 报告。 */
static bool __xrtMemDebugReportWriteJson(
	xrt_memdebug_report_state* pState,
	const xmemdebugsnapshot* pSnapshot,
	const xmemdebugallocation* pAllocations,
	size_t iAllocationCount,
	const xrt_memdebug_report_events* pEvents
)
{
	if ( !__xrtMemDebugReportText(pState, "{\"stats\":{") ||
		 !__xrtMemDebugReportWriteJsonStats(pState, pSnapshot) ||
		 !__xrtMemDebugReportText(pState, "},\"live_allocations\":[") ) {
		return false;
	}
	for ( size_t i = 0; i < iAllocationCount; i++ ) {
		if ( (i != 0) && !__xrtMemDebugReportText(pState, ",") ) {
			return false;
		}
		if ( !__xrtMemDebugReportFormat(
			pState,
			"{\"address\":\"%p\",\"size\":%llu,\"file\":",
			pAllocations[i].Address,
			(unsigned long long)pAllocations[i].Size
		) ||
			 !__xrtMemDebugReportJsonString(pState, pAllocations[i].File) ||
			 !__xrtMemDebugReportFormat(
				pState,
				",\"line\":%u}",
				(unsigned int)pAllocations[i].Line
			) ) {
			return false;
		}
	}
	if ( !__xrtMemDebugReportText(pState, "],\"events\":[") ) {
		return false;
	}
	for ( size_t i = 0; i < pEvents->Count; i++ ) {
		const xmemdebugevent* pEvent = &pEvents->Items[i];

		if ( (i != 0) && !__xrtMemDebugReportText(pState, ",") ) {
			return false;
		}
		if ( !__xrtMemDebugReportFormat(
			pState,
			"{\"sequence\":%llu,\"kind\":\"%s\",\"address\":\"%p\","
			"\"size\":%llu,\"file\":",
			(unsigned long long)pEvent->Sequence,
			xrtMemDebugEventName(pEvent->Kind),
			pEvent->Address,
			(unsigned long long)pEvent->Size
		) ||
			 !__xrtMemDebugReportJsonString(pState, pEvent->File) ||
			 !__xrtMemDebugReportFormat(
				pState,
				",\"line\":%u}",
				(unsigned int)pEvent->Line
			) ) {
			return false;
		}
	}
	return __xrtMemDebugReportText(pState, "]}\n");
}



/* 捕获一次诊断状态并流式写出，输出阶段不会污染本次快照。 */
XRT_API bool xrtMemDebugReport(
	xmemdebugreportformat Format,
	xmemdebugwriteproc pWriter,
	ptr pUserData
)
{
	xrt_memdebug_report_state tState;
	xrt_memdebug_report_events tEvents;
	xmemdebugallocation* pAllocations;
	xmemdebugsnapshot tSnapshot;
	size_t iAllocationCount;
	bool bResult;

	if ( ((Format != XMEMDEBUG_REPORT_TEXT) &&
		 (Format != XMEMDEBUG_REPORT_JSON)) ||
		 (pWriter == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&tState, 0, sizeof(tState));
	memset(&tEvents, 0, sizeof(tEvents));
	tState.Writer = pWriter;
	tState.UserData = pUserData;
	xrtMemDebugSnapshot(&tSnapshot);
	(void)xrtMemDebugVisit(__xrtMemDebugReportCollectEvent, &tEvents);
	if ( !__xrtMemDebugCaptureLive(&pAllocations, &iAllocationCount) ) {
		return false;
	}

	if ( Format == XMEMDEBUG_REPORT_TEXT ) {
		bResult = __xrtMemDebugReportWriteText(
			&tState,
			&tSnapshot,
			pAllocations,
			iAllocationCount,
			&tEvents
		);
	} else {
		bResult = __xrtMemDebugReportWriteJson(
			&tState,
			&tSnapshot,
			pAllocations,
			iAllocationCount,
			&tEvents
		);
	}
	__xrtBackingFree(pAllocations);
	return bResult && !tState.Failed;
}

#endif
