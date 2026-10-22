#ifndef XRT_SINGLE_NET_ENGINE_TEST_H
#define XRT_SINGLE_NET_ENGINE_TEST_H

/* Completion and CLOSED flags precede the final native callback release.
 * Retry only BUSY; retained owners and other failures remain test failures. */
static inline bool testSingleNetEngineDestroy(xnetengine* pEngine)
{
	double Deadline = __xrtWaitAfter(5000);
	for (;;) {
		xnetretireresult Result = xrtNetEngineTryDestroy(pEngine);
		if (Result == XNET_RETIRE_READY) return true;
		if (Result != XNET_RETIRE_BUSY || __xrtWaitExpired(Deadline)) return false;
		xrtThreadYield();
	}
}

#endif
