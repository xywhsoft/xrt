/*
 * 范例：memory/debug —— 内存调试：活动分配快照 + 逐条访问器
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMemDebugReset     清空调试状态（范例起点的干净基线）
 *   xrtMemDebugSnapshot  一次取回统计结构（活动数/字节数/峰值/计数器）
 *   xrtMemDebugVisitLive 遍历当前活动分配（带分配点文件:行号）
 *   xmemdebugallocation  访问器收到的单条分配记录
 * 模块宏：XRT_MODULE_MEMORY_DEBUG
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/memory/debug/main.c -lws2_32 -liphlpapi
 * 预期输出（地址随运行变化；site 精确到本文件分配行）：
 *   live_count=1 live_bytes=64 peak_bytes=64
 *   live address=000002bd... size=64 site=examples/memory/debug/main.c:30
 *   alloc_count=1 free_count=1 events=2
 *
 * 调试堆的日常姿势：在怀疑泄漏的代码段前 Reset、后 Snapshot，
 *   LiveCount 不归零 + VisitLive 打出的分配点 = 泄漏现场。
 * 更强的能力（本例未展开）：FailAfter 故障注入、隔离区捕获
 *   双释放/UAF（quarantine 计数出现在 JSON 报告里）。
 */

#include <stdio.h>

#include <xrt.h>



/*
 * 访问器：每条活动分配回调一次。
 * 返回 false 可提前终止遍历；File 为 NULL 时（无调用点信息）兜底显示。
 */
static bool printAllocation(const xmemdebugallocation* pAllocation, ptr pUserData)
{
	(void)pUserData;
	printf("live address=%p size=%zu site=%s:%u\n",
		pAllocation->Address,
		pAllocation->Size,
		pAllocation->File != NULL ? pAllocation->File : "unknown",
		(unsigned int)pAllocation->Line);
	return true;
}



int main(void)
{
	xmemdebugsnapshot tSnapshot;
	unsigned char* pMemory;

	/* 起点清零：本范例观察到的所有事件都来自接下来的代码。 */
	if ( !xrtMemDebugReset() ) {
		return 1;
	}
	pMemory = (unsigned char*)xrtMalloc(64);
	if ( pMemory == NULL ) {
		return 2;
	}
	pMemory[0] = 0x5A;

	/* 泄漏现场：64 字节仍在活动列表，分配点精确到本文件第 30 行。 */
	xrtMemDebugSnapshot(&tSnapshot);
	printf("live_count=%zu live_bytes=%zu peak_bytes=%zu\n",
		tSnapshot.LiveCount,
		tSnapshot.LiveBytes,
		tSnapshot.PeakBytes);
	(void)xrtMemDebugVisitLive(printAllocation, NULL);

	/* 释放后再拍快照：Live 归零，累计计数器对账（1 alloc + 1 free）。 */
	xrtFree(pMemory);
	xrtMemDebugSnapshot(&tSnapshot);
	printf("alloc_count=%llu free_count=%llu events=%zu\n",
		(unsigned long long)tSnapshot.AllocCount,
		(unsigned long long)tSnapshot.FreeCount,
		tSnapshot.EventCount);

	/* 收尾复位，不影响后续程序段的调试观察。 */
	return xrtMemDebugReset() ? 0 : 3;
}
