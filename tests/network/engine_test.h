#ifndef XRT_TEST_NETWORK_ENGINE_H
#define XRT_TEST_NETWORK_ENGINE_H

#if defined(XRT_FEATURE_NET_ENGINE)
#include <xrt/detail/wait.h>

/* Close callbacks publish observations before their final native releases. */
static inline bool testNetEngineDestroy(xnetengine* pEngine)
{
	double Deadline = __xrtWaitAfter(10000);
	for (;;) {
		xnetretireresult Result = xrtNetEngineTryDestroy(pEngine);
		if (Result == XNET_RETIRE_READY) return true;
		if (Result != XNET_RETIRE_BUSY || __xrtWaitExpired(Deadline)) return false;
		xrtThreadYield();
	}
}
#endif

#endif
