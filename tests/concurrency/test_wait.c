#include <xrt/detail/wait.h>
#include "../test.h"
int main(void)
{
    double Limit;
    int64 Remaining;
    testRequire(__xrtWaitAfter(XRT_WAIT_FOREVER) == INFINITY, "infinite wait changed");
    testRequire(!__xrtWaitExpired(INFINITY) && __xrtWaitRemaining(INFINITY) == XRT_WAIT_FOREVER, "infinite budget changed");
    Limit = __xrtWaitAfter(10);
    Remaining = __xrtWaitRemaining(Limit);
    testRequire(Remaining > 0 && Remaining <= 11, "millisecond budget changed");
    xrtSleep(12);
    testRequire(__xrtWaitExpired(Limit) && __xrtWaitRemaining(Limit) == 0, "elapsed budget changed");
    xrtClearError();
    testRequire(isnan(__xrtWaitAfter(-2)) && xrtGetError() != NULL, "negative timeout accepted");
    testRequire(!__xrtWaitValid(NAN), "NaN budget accepted");
    testRequire(isfinite(__xrtWaitAfter(INT64_MAX)), "finite long budget became infinite");
    printf("[PASS] relative-millisecond-wait\n");
    return 0;
}
