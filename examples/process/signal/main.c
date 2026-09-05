#include <stdio.h>

#include <xrt.h>



/* 示例回调在普通 XRT 线程上下文记录信号。 */
static void exampleSignal(
	xsignalwatch* pWatch,
	const xsignalevent* pEvent,
	ptr pData
)
{
	xatomic32* pReceived = (xatomic32*)pData;

	(void)pWatch;
	printf("signal=%s count=%u total=%llu\n",
		pEvent->Name,
		(unsigned)pEvent->Count,
		(unsigned long long)pEvent->Total);
	xrtAtomic32Store(pReceived, 1u, XMEMORY_RELEASE);
}



/*
 * 范例：process/signal —— 跨平台信号：订阅、自触发与安全等待
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSignalOn / Free      订阅/退订信号（返回观察器句柄）
 *   XSIGNAL_INT             信号枚举（INT/TERM/HUP/BREAK...）
 *   xrtSignalRaise          向本进程投递信号（自测路径）
 *   xsignalevent            事件：名字 / 连续次数 / 累计总数
 *   xrtSignalShutdown       关停信号后台线程（退出前调用）
 * 模块宏：XRT_MODULE_SIGNAL
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/process/signal/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   signal=INT count=1 total=1
 *
 * 回调运行环境：普通 XRT 工作线程（非信号处理上下文）——
 *   回调里可以安全调用任意 API（printf/malloc 均可），
 *   这是与原生 signal handler 的本质区别。
 * 主线程用原子标志 + 轮询等待（RELEASE/ACQUIRE 配对发布），
 *   3 秒 deadline 防挂死；真实服务里这里通常是事件循环。
 */


/* 订阅本进程中断信号并演示可自动回归的投递路径。 */
int main(void)
{
	xatomic32 Received = XRT_ATOMIC32_INIT(0u);
	xsignalwatch* pWatch = xrtSignalOn(
		XSIGNAL_INT,
		exampleSignal,
		&Received
	);
	uint64 iDeadline = xrtClock() + UINT64_C(3000000);

	if ( (pWatch == NULL) || !xrtSignalRaise(XSIGNAL_INT) ) {
		xrtSignalFree(pWatch);
		return 1;
	}
	while ( xrtAtomic32Load(&Received, XMEMORY_ACQUIRE) == 0u ) {
		if ( xrtClock() >= iDeadline ) {
			xrtSignalFree(pWatch);
			return 2;
		}
		xrtSleep(1u);
	}
	xrtSignalFree(pWatch);
	return xrtSignalShutdown() ? 0 : 3;
}
