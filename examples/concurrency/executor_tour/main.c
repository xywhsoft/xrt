/*
 * 范例：concurrency/executor_tour —— 执行器收口补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【提交】    xrtExecutorSubmitBatch（原子批量受理）
 *   【收口】    xrtExecutorClose / Wait / WaitFor / WaitUntil
 *   【统计】    xrtExecutorGet（快照六计数字段）
 * 模块宏：XRT_MODULE_EXECUTOR
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/executor_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   executor: batch=3 stats(submitted>=3 completed=3) ok
 *   executor: close+wait closed=1 queued=0 ok
 *
 * 批量三项各计数一次；Close 后 Wait 收口，
 *   WaitFor 短窗演示未关闭时的 TIMEOUT 路径。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

static volatile int g_Ran = 0;

static void exampleJob(ptr pData)
{
	(void)pData;
	g_Ran = g_Ran + 1;
}

int main(void)
{
	xexecutorconfig Config = { 2, 8, 0 };
	xexecutoritem Items[3];
	xexecutorstats Stats;
	xexecutor* pExecutor = NULL;
	int iResult = 1;

	memset(&Stats, 0, sizeof(Stats));
	pExecutor = xrtExecutorCreate(&Config);
	if ( pExecutor == NULL ) {
		goto Cleanup;
	}
	/* SubmitBatch：三项原子受理。 */
	Items[0].Proc = exampleJob;
	Items[0].Data = NULL;
	Items[0].Destroy = NULL;
	Items[0].DestroyContext = NULL;
	Items[1] = Items[0];
	Items[2] = Items[0];
	g_Ran = 0;
	if ( !xrtExecutorSubmitBatch(pExecutor, Items, 3u) ) {
		goto Cleanup;
	}
	/* 等三个工作执行完（Wait 族只对已关闭执行器有意义——
	 * 未关闭时调用返回 ERROR，不是等新工作的手段）。 */
	{
		xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(3000000));

		while ( g_Ran < 3 ) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				goto Cleanup;
			}
			xrtThreadYield();
		}
	}
	/* 统计：受理 >= 3、完成 = 3。 */
	if ( !xrtExecutorGet(pExecutor, &Stats) ||
		(Stats.Submitted < 3u) ||
		(Stats.Completed != 3u) ||
		(Stats.Threads != 2u) ) {
		goto Cleanup;
	}
	printf("executor: batch=3 stats(submitted>=%llu completed=%llu)"
		" ok\n",
		(unsigned long long)Stats.Submitted,
		(unsigned long long)Stats.Completed);
	/* Close → WaitFor（短窗）与 WaitUntil 双形态收口。 */
	if ( !xrtExecutorClose(pExecutor) ||
		(xrtExecutorWaitFor(pExecutor,
			UINT64_C(3000000)) != XWAIT_OK) ||
		(xrtExecutorWaitUntil(pExecutor,
			xrtDeadlineAfter(UINT64_C(3000000))) !=
			XWAIT_OK) ||
		(xrtExecutorWait(pExecutor) != XWAIT_OK) ||
		!xrtExecutorGet(pExecutor, &Stats) ||
		!Stats.Closed ||
		(Stats.Queued != 0u) ) {
		goto Cleanup;
	}
	printf("executor: close+wait closed=1 queued=0 ok\n");
	iResult = 0;

Cleanup:
	xrtExecutorDestroy(pExecutor);
	return iResult;
}
