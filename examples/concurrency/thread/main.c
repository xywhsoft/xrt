#include <stdio.h>

#include <xrt.h>



/*
 * 范例：concurrency/thread —— 线程：创建、等待、退出码
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtThreadCreate     启动线程（入口 + 参数 + 栈选项）
 *   xrtThreadWait       等待结束（xwaitresult 统一等待语义）
 *   xrtThreadExitCode   读取入口函数返回值
 *   xrtThreadCurrentId  线程内查自身 ID（日志标注用）
 *   xrtThreadDestroy    释放线程对象（Wait 之后）
 * 模块宏：XRT_MODULE_THREAD
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/thread/main.c -lws2_32 -liphlpapi
 * 预期输出（线程 ID 随运行变化）：
 *   worker alpha: 13300
 *   exit: 42
 *
 * 入口签名 int32 worker(ptr)：返回值即退出码（42），
 *   参数是单个 ptr——传复杂状态用结构体指针。
 */


/* 在线程中执行一个短任务。 */
static int32 worker(ptr pData)
{
	cstr sName = (cstr)pData;

	printf("worker %s: %llu\n", sName,
		(unsigned long long)xrtThreadCurrentId());
	return 42;
}



/* 创建、等待并释放线程。 */
int main(void)
{
	xthread* pThread = xrtThreadCreate(worker, (ptr)"alpha", 0);

	if ( pThread == NULL ) {
		return 1;
	}
	if ( xrtThreadWait(pThread) != XWAIT_OK ) {
		xrtThreadDestroy(pThread);
		return 1;
	}
	printf("exit: %d\n", xrtThreadExitCode(pThread));
	xrtThreadDestroy(pThread);
	return 0;
}
