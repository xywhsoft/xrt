#ifndef XHTTP_SERVER_TEST_H
#define XHTTP_SERVER_TEST_H

#include "../../../tests/test.h"
#include <xhttp-server.h>

#if defined(XRT_FEATURE_NET_ENGINE)
#include <xrt/detail/wait.h>

/* Close flags precede callback/native release tails. Only BUSY permits a
 * retry; a leaked owner still fails the test at the bounded deadline. */
static inline bool testHttpServerEngineDestroy(xnetengine* pEngine)
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
