#include <stdio.h>

#include <xrt.h>



/*
 * 范例：concurrency/sync —— 互斥量：最简临界区
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMutexInit / Lock / Unlock / Unit   互斥量四件套
 * 模块宏：XRT_MODULE_MUTEX（地基）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/sync/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   protected section
 *
 * 栈上初始化的普通互斥量：Windows SRWLOCK / POSIX
 *   ERRORCHECK 实现。同族还有条件变量、信号量、读写锁、
 *   once——五件套构成并发原语层（教程第 41 章逐个精讲）。
 */


/* 展示嵌入式同步对象的基础用法。 */
int main(void)
{
	xmutex tMutex;

	if ( !xrtMutexInit(&tMutex) ) {
		return 1;
	}
	(void)xrtMutexLock(&tMutex);
	printf("protected section\n");
	(void)xrtMutexUnlock(&tMutex);
	(void)xrtMutexUnit(&tMutex);
	return 0;
}
