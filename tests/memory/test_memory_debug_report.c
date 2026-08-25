#include "../test.h"



/* 固定测试缓冲足以容纳完整的 512 条事件报告。 */
typedef struct test_report_buffer {
	char Data[131072];
	size_t Size;
	bool Reject;
} test_report_buffer;



/* 报告 OOM 注入状态具有进程生命周期。 */
static bool __testFailNextAllocation;



/* 在指定语义边界拒绝下一次底层分配。 */
static ptr testAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	if ( __testFailNextAllocation ) {
		__testFailNextAllocation = false;
		return NULL;
	}
	return malloc(iSize);
}



/* 调整底层内存时使用同一故障开关。 */
static ptr testRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	if ( __testFailNextAllocation ) {
		__testFailNextAllocation = false;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放底层测试内存。 */
static void testFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 收集报告片段，或按测试要求拒绝写入。 */
static bool testReportWrite(xbytesview Data, ptr pUserData)
{
	test_report_buffer* pBuffer = (test_report_buffer*)pUserData;

	if ( pBuffer->Reject ||
		 (Data.Size > ((sizeof(pBuffer->Data) - 1u) - pBuffer->Size)) ) {
		return false;
	}
	memcpy(pBuffer->Data + pBuffer->Size, Data.Data, Data.Size);
	pBuffer->Size += Data.Size;
	pBuffer->Data[pBuffer->Size] = 0;
	return true;
}



/* OOM 测试中的访问器不修改快照。 */
static bool testVisitLive(const xmemdebugallocation* pAllocation, ptr pUserData)
{
	(void)pAllocation;
	(void)pUserData;
	return true;
}



/* 事件 OOM 测试中的访问器不修改快照。 */
static bool testVisitEvent(const xmemdebugevent* pEvent, ptr pUserData)
{
	(void)pEvent;
	(void)pUserData;
	return true;
}



/* 验证旧版文本和 JSON 报告能力由无文件耦合的流式层完整承接。 */
int main(void)
{
	xallocator Allocator;
	test_report_buffer tBuffer;
	ptr pLive;

	Allocator.Context = NULL;
	Allocator.Alloc = testAlloc;
	Allocator.Realloc = testRealloc;
	Allocator.Free = testFree;
	testRequire(xrtSetAllocator(&Allocator), "test allocator installation failed");
	testRequire(xrtMemDebugReset(), "initial memory debug reset failed");
	pLive = xrtMalloc(48);
	testRequire(pLive != NULL, "report live allocation failed");

	__testFailNextAllocation = true;
	testRequire(xrtMemDebugVisit(testVisitEvent, NULL) == 0,
		"event snapshot OOM must not visit events");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"event snapshot OOM must report memory error");
	xrtClearError();

	__testFailNextAllocation = true;
	testRequire(xrtMemDebugVisitLive(testVisitLive, NULL) == 0, "snapshot OOM must not visit allocations");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "snapshot OOM must report memory error");
	xrtClearError();

	memset(&tBuffer, 0, sizeof(tBuffer));
	testRequire(
		xrtMemDebugReport(XMEMDEBUG_REPORT_TEXT, testReportWrite, &tBuffer),
		"text report failed"
	);
	testRequire(strstr(tBuffer.Data, "XRT memory debug report") != NULL, "text report header missing");
	testRequire(strstr(tBuffer.Data, "live_count=1") != NULL, "text live count missing");
	testRequire(strstr(tBuffer.Data, "[live_allocations]") != NULL, "text live section missing");
	testRequire(strstr(tBuffer.Data, "kind=alloc") != NULL, "text event name missing");

	memset(&tBuffer, 0, sizeof(tBuffer));
	testRequire(
		xrtMemDebugReport(XMEMDEBUG_REPORT_JSON, testReportWrite, &tBuffer),
		"json report failed"
	);
	testRequire(strstr(tBuffer.Data, "\"stats\"") != NULL, "json stats missing");
	testRequire(strstr(tBuffer.Data, "\"live_allocations\"") != NULL, "json live section missing");
	testRequire(strstr(tBuffer.Data, "\"kind\":\"alloc\"") != NULL, "json event name missing");
	testRequire(strcmp(xrtMemDebugEventName((xmemdebugeventkind)999), "unknown") == 0,
		"unknown event name mismatch");

	memset(&tBuffer, 0, sizeof(tBuffer));
	tBuffer.Reject = true;
	testRequire(
		!xrtMemDebugReport(XMEMDEBUG_REPORT_TEXT, testReportWrite, &tBuffer),
		"writer rejection must fail the report"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "writer rejection must report state error");
	xrtClearError();
	testRequire(
		!xrtMemDebugReport((xmemdebugreportformat)0, testReportWrite, &tBuffer),
		"invalid report format must fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "invalid format must report argument error");
	xrtClearError();

	xrtFree(pLive);
	testRequire(xrtMemDebugReset(), "final memory debug reset failed");
	printf("[PASS] memory_debug_report\n");
	return 0;
}
