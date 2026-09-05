#include <stdio.h>

#include <xrt.h>



/*
 * 范例：concurrency/once —— once：恰好执行一次的延迟初始化
 * ----------------------------------------------------------------
 * 演示 API：
 *   XRT_ONCE_INIT   静态初始化（零运行时构造）
 *   xrtOnce         首个到达者执行回调，其余直接通过
 * 模块宏：XRT_MODULE_MUTEX（once 同族）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/once/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   config: 42
 *
 * 双检锁的正确替身：多线程同时首次调用时回调只跑一次，
 *   其余线程阻塞等待完成后拿到同一结果——
 *   进程级配置/全局连接池的初始化标准姿势。
 */


/* 延迟初始化一个进程级配置值。 */
static bool initializeConfig(ptr pData)
{
	*(int*)pData = 42;
	return true;
}



/* 多次访问只执行一次初始化过程。 */
int main(void)
{
	static xonce tOnce = XRT_ONCE_INIT;
	static int iConfig;

	if ( !xrtOnce(&tOnce, initializeConfig, &iConfig) ) {
		return 1;
	}
	printf("config: %d\n", iConfig);
	return 0;
}
