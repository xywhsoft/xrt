#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件回调只发布一次完成标志。 */
static void testSingleSignalCallback(
	xsignalwatch* pWatch,
	const xsignalevent* pEvent,
	ptr pData
)
{
	xatomic32* pCalled = (xatomic32*)pData;

	(void)pWatch;
	(void)pEvent;
	xrtAtomic32Store(pCalled, 1u, XMEMORY_RELEASE);
}



/* 单头文件必须独立提供完整信号调度与关闭路径。 */
int main(void)
{
	xatomic32 Called = XRT_ATOMIC32_INIT(0u);
	xsignalwatch* pWatch = xrtSignalOn(
		XSIGNAL_INT,
		testSingleSignalCallback,
		&Called
	);
	uint64 iDeadline = xrtClock() + UINT64_C(3000000);

	if ( (pWatch == NULL) || !xrtSignalRaise(XSIGNAL_INT) ) {
		xrtSignalFree(pWatch);
		return 1;
	}
	while ( xrtAtomic32Load(&Called, XMEMORY_ACQUIRE) == 0u ) {
		if ( xrtClock() >= iDeadline ) {
			xrtSignalFree(pWatch);
			return 2;
		}
		xrtSleep(1u);
	}
	xrtSignalFree(pWatch);
	return xrtSignalShutdown() ? 0 : 3;
}
