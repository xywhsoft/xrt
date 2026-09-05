/*
 * 范例：memory/fail_inject —— OOM 故障注入：FailAfter 全链路
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMemDebugEnable / Enabled       开关与状态查询
 *   xrtMemDebugFailAfter              第 N 次分配精确失败
 *   xrtMemDebugFailTriggered          注入是否已触发
 *   xrtMemDebugFailClear              清除未触发的注入
 *   xrtMemDebugVisit                  按时间序访问调试事件
 *   xrtMemDebugEventName              事件枚举 → 稳定名称
 * 模块宏：XRT_MODULE_MEMORY_DEBUG
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/memory/fail_inject/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   enabled=1
 *   fail-after-2: 1st=ok 2nd=NULL triggered=1
 *   event[0]: alloc
 *   cleared: 3rd=ok triggered-still=0
 *   reset=1
 *
 * FailAfter(1) 的实测语义：第 1 次分配成功、第 2 次
 *   失败（前 N 次成功，第 N+1 次失败）。FailClear 会
 *   连同"已触发"标志一并复位。_oom 测试变体
 *   全部建立在这组入口上——"验证失败路径的回滚"。
 */

#include <stdio.h>
#include <xrt.h>

/* 事件访问器：只打印前几个 alloc 事件的名字。 */
static bool printEvent(const xmemdebugevent* pEvent, ptr pUserData)
{
	size_t* pCount = (size_t*)pUserData;

	if ( *pCount < 1u && pEvent->Kind == XMEMDEBUG_ALLOC ) {
		printf("event[%zu]: %s\n", *pCount,
			xrtMemDebugEventName(pEvent->Kind));
	}
	(*pCount)++;
	return true;
}

int main(void)
{
	ptr pFirst;
	ptr pSecond;
	ptr pThird;
	size_t iEvents = 0;

	(void)xrtMemDebugReset();
	(void)xrtMemDebugEnable(true);
	printf("enabled=%d\n", xrtMemDebugEnabled() ? 1 : 0);

	/* 注入：第 2 次分配失败。 */
	(void)xrtMemDebugFailAfter(1u);
	pFirst = xrtMalloc(16);
	pSecond = xrtMalloc(16);
	printf("fail-after-2: 1st=%s 2nd=%s triggered=%d\n",
		pFirst != NULL ? "ok" : "NULL",
		pSecond != NULL ? "ok" : "NULL",
		xrtMemDebugFailTriggered() ? 1 : 0);
	xrtFree(pFirst);

	/* 事件流：最早的 alloc 事件可按名访问。 */
	(void)xrtMemDebugVisit(printEvent, &iEvents);

	/* 清除未触发注入后再分配恢复成功；已触发的标志保持。 */
	xrtMemDebugFailClear();
	pThird = xrtMalloc(16);
	printf("cleared: 3rd=%s triggered-still=%d\n",
		pThird != NULL ? "ok" : "NULL",
		xrtMemDebugFailTriggered() ? 1 : 0);
	xrtFree(pThird);

	(void)xrtMemDebugEnable(false);
	printf("reset=%d\n", xrtMemDebugReset() ? 1 : 0);
	return 0;
}
