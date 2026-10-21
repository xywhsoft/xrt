#ifndef XRT_TEST_TCP_SERVER_RETIRE_H
#define XRT_TEST_TCP_SERVER_RETIRE_H
#include <xrt/detail/wait.h>
#include "../test.h"

/* CLOSED/Close callbacks precede owned callback Drop and physical runtime
 * retirement. Retry the real checked operation, not a sleep or fake counter. */
static inline bool testTcpServerEngineDestroy(xnetengine* engine)
{
    double due=__xrtWaitAfter(5000);
    for(;;){xnetretireresult result=xrtNetEngineTryDestroy(engine);
        if(result==XNET_RETIRE_READY)return true;
        if(result!=XNET_RETIRE_BUSY||__xrtWaitExpired(due))return false;
        xrtThreadYield();
    }
}
#endif
