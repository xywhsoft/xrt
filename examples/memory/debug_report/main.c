/*
 * 范例：memory/debug_report —— 流式 JSON 内存报告（输出端零耦合）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMemDebugReport      生成报告，分片推送给回调
 *   XMEMDEBUG_REPORT_JSON  报告格式（另有人类可读的 TEXT 格式）
 * 模块宏：XRT_MODULE_MEMORY_DEBUG
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/memory/debug_report/main.c -lws2_32 -liphlpapi
 * 预期输出（地址随运行变化；单行 JSON）：
 *   {"stats":{"enabled":true,"live_count":1,...},
 *    "live_allocations":[{"address":"...","size":64,
 *    "file":"examples/memory/debug_report/main.c","line":19}],
 *    "events":[{"sequence":1,"kind":"alloc",...}]}
 *
 * 流式回调设计的意义：报告器不关心输出去哪——
 *   本例写 stdout，同一个回调换 FILE* 就能写日志/HTTP 响应/内存缓冲，
 *   内存模块与文件模块完全解耦。
 * stats 段的 quarantine/double_free/overflow/use_after_free 计数
 *   直接可用于 CI 断言（非零即失败）。
 */

#include <stdio.h>
#include <xrt.h>



/* 输出回调：把每个分片写入 pUserData 携带的 FILE*。 */
static bool writeReport(xbytesview Data, ptr pUserData)
{
	FILE* pFile = (FILE*)pUserData;

	return fwrite(Data.Data, 1, Data.Size, pFile) == Data.Size;
}



int main(void)
{
	/* 留一条活动分配，让报告的 live_allocations 有内容可写。 */
	ptr pMemory = xrtMalloc(64);
	bool bResult;

	if ( pMemory == NULL ) {
		return 1;
	}

	/* 流式生成：内部分片推送，报告再大也不需要一次性缓冲。 */
	bResult = xrtMemDebugReport(XMEMDEBUG_REPORT_JSON, writeReport, stdout);
	xrtFree(pMemory);
	if ( !xrtMemDebugReset() ) {
		return 1;
	}
	return bResult ? 0 : 1;
}
