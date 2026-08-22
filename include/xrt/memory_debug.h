#ifndef XRT_MEMORY_DEBUG_H
#define XRT_MEMORY_DEBUG_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_MEMORY_DEBUG_REPORT) && !defined(XRT_FEATURE_MEMORY_DEBUG)
	#error "XRT_FEATURE_MEMORY_DEBUG_REPORT requires XRT_FEATURE_MEMORY_DEBUG"
#endif



#if defined(XRT_FEATURE_MEMORY_DEBUG)

/* 调试器固定保留最近 512 条事件。 */
#define XRT_MEMDEBUG_EVENT_LIMIT 512u



/* 内存调试事件类型。 */
typedef enum xmemdebugeventkind {
	XMEMDEBUG_ALLOC = 1,
	XMEMDEBUG_FREE,
	XMEMDEBUG_REALLOC,
	XMEMDEBUG_DOUBLE_FREE,
	XMEMDEBUG_INVALID_FREE,
	XMEMDEBUG_OVERFLOW,
	XMEMDEBUG_UNDERFLOW,
	XMEMDEBUG_USE_AFTER_FREE,
	XMEMDEBUG_TEMP_ALLOC,
	XMEMDEBUG_TEMP_REWIND,
	XMEMDEBUG_TEMP_RESET
} xmemdebugeventkind;



/* 内存调试事件是只包含借用信息的值对象。 */
typedef struct xmemdebugevent {
	xmemdebugeventkind Kind;
	uint64 Sequence;
	ptr Address;
	size_t Size;
	cstr File;
	uint32 Line;
} xmemdebugevent;



/* 内存调试快照用于测试、诊断和外部报告。 */
typedef struct xmemdebugsnapshot {
	bool Enabled;
	size_t LiveCount;
	size_t LiveBytes;
	size_t PeakCount;
	size_t PeakBytes;
	size_t QuarantineCount;
	size_t QuarantineBytes;
	uint64 AllocCount;
	uint64 FreeCount;
	uint64 ReallocCount;
	uint64 DoubleFreeCount;
	uint64 InvalidFreeCount;
	uint64 OverflowCount;
	uint64 UnderflowCount;
	uint64 UseAfterFreeCount;
	size_t TempCurrentBytes;
	size_t TempPeakBytes;
	uint64 TempResetCount;
	size_t EventCount;
} xmemdebugsnapshot;



/* 事件访问器返回 false 时停止遍历。 */
typedef bool (*xmemdebugvisitor)(const xmemdebugevent* pEvent, ptr pUserData);



/* 活动分配记录借用分配点字符串，不转移内存所有权。 */
typedef struct xmemdebugallocation {
	ptr Address;
	size_t Size;
	cstr File;
	uint32 Line;
} xmemdebugallocation;



/* 活动分配访问器返回 false 时停止遍历。 */
typedef bool (*xmemdebugallocationvisitor)(const xmemdebugallocation* pAllocation, ptr pUserData);



#if defined(XRT_FEATURE_MEMORY_DEBUG_REPORT)
/* 报告格式与具体输出目标解耦。 */
typedef enum xmemdebugreportformat {
	XMEMDEBUG_REPORT_TEXT = 1,
	XMEMDEBUG_REPORT_JSON
} xmemdebugreportformat;



/* 报告写入器成功消费全部数据时返回 true。 */
typedef bool (*xmemdebugwriteproc)(xbytesview Data, ptr pUserData);
#endif



XRT_EXTERN_C_BEGIN



/* 返回稳定的事件名称，未知事件返回 unknown。 */
XRT_API cstr xrtMemDebugEventName(xmemdebugeventkind Kind);



/* 在没有活动分配时开启或关闭运行时内存调试记录。 */
XRT_API bool xrtMemDebugEnable(bool bEnable);



/* 返回运行时内存调试是否开启。 */
XRT_API bool xrtMemDebugEnabled(void);



/* 当前线程允许指定次数成功分配后，让下一次逻辑分配失败一次。 */
XRT_API bool xrtMemDebugFailAfter(uint64 iSuccessfulAllocations);



/* 清除当前线程尚未触发的分配故障。 */
XRT_API void xrtMemDebugFailClear(void);



/* 返回当前线程最近配置的分配故障是否已经触发。 */
XRT_API bool xrtMemDebugFailTriggered(void);



/* 在没有活动分配时清空统计、事件和隔离队列。 */
XRT_API bool xrtMemDebugReset(void);



/* 获取一致的内存调试统计快照。 */
XRT_API void xrtMemDebugSnapshot(xmemdebugsnapshot* pSnapshot);



/* 按时间顺序访问当前保留的调试事件。 */
XRT_API size_t xrtMemDebugVisit(xmemdebugvisitor pVisitor, ptr pUserData);



/* 访问内部锁线性化点捕获的完整活动分配快照。 */
XRT_API size_t xrtMemDebugVisitLive(xmemdebugallocationvisitor pVisitor, ptr pUserData);



#if defined(XRT_FEATURE_MEMORY_DEBUG_REPORT)
/* 把当前调试快照流式写为文本或 JSON。 */
XRT_API bool xrtMemDebugReport(
	xmemdebugreportformat Format,
	xmemdebugwriteproc pWriter,
	ptr pUserData
);
#endif



XRT_EXTERN_C_END

#endif

#endif
