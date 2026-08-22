#include "../test_allocator.h"



/* OOM 测试回调永远不会进入。 */
static void testSignalOOMCallback(
	xsignalwatch* pWatch,
	const xsignalevent* pEvent,
	ptr pData
)
{
	(void)pWatch;
	(void)pEvent;
	(void)pData;
}



/* 首次监听分配失败不能安装处理器或接管用户数据。 */
int main(void)
{
	testRequire(xrtSignalSupported(XSIGNAL_INT),
		"signal state initialization failed");
	testRequire(testInstallFailAllocator(),
		"failure allocator install failed");
	testRequire(xrtSignalOn(
		XSIGNAL_INT,
		testSignalOOMCallback,
		NULL
	) == NULL, "signal watch ignored allocation failure");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"signal watch allocation error mismatch");
	return 0;
}
