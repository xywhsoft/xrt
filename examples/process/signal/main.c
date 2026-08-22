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
