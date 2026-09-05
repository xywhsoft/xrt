#include <xrt.h>
#include <stdio.h>



/* 示例任务异步等待后打印编号。 */
static ptr exampleTask(ptr pData)
{
	int iTask = (int)(intptr_t)pData;

	(void)xrtCoSleep((uint64)iTask * 1000u);
	printf("task %d\n", iTask);
	return pData;
}



/* 调度器 post 在所属线程中创建真正的协程任务。 */
static void examplePost(xcosched* pSched, ptr pData)
{
	(void)xrtCoGo(pSched, exampleTask, pData, NULL);
}



/*
 * 范例：concurrency/coroutine_scheduler —— 调度器：投递与运行
 * ----------------------------------------------------------------
 * 演示 API：
 *   轻量任务投递（post）
 *   调度器运行至全部完成（投递任务 + 协程）
 * 模块宏：XRT_MODULE_COROUTINE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/coroutine_scheduler/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   task 1
 *   task 2
 *   task 3
 *
 * 调度器 = 协程的执行器：投递的任务按序/并发运行，
 *   Run 驱动到全部完成才返回——网络引擎的协程面
 *   （tls/dial 即此形态）。
 */


/* 先投递多个轻量任务，再运行到投递和协程全部完成。 */
int main(void)
{
	xcosched* pSched = xrtCoSchedCreate();

	if ( pSched == NULL ) {
		return 1;
	}
	for ( int i = 1; i <= 3; i++ ) {
		if ( !xrtCoSchedPost(
			pSched,
			examplePost,
			(ptr)(intptr_t)i
		) ) {
			return 2;
		}
	}
	if ( !xrtCoSchedRun(pSched) ) {
		return 3;
	}
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	return 0;
}
