#define XLLM_TEST_ALLOCATOR 1
#define XLLM_SESSION_TEST_FAULTS 1
#include <xllm-session/features.h>
#include <xllm/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static inline void mdo_test_sleep_ms(uint64_t us)
{
    xrtSleep((int64_t)us);
}
#define XLLM_IMPLEMENTATION
#include "../../../../single/extlibs/xllm.h"
#define XLLM_SESSION_IMPLEMENTATION
#include "../../../../single/extlibs/xllm-session.h"
static inline xnetstream *mdo_test_xrtNetListenerAcceptWait(xnetlistener *l, uint64_t end,
                                                            xcancel *c)
{
    return xrtNetListenerAcceptWait(l, xllmDeadlineWaitMs(end), c);
}
static inline xtlsstream *mdo_test_xrtTlsListenerAcceptWait(xtlslistener *l, uint64_t end,
                                                            xcancel *c)
{
    return xrtTlsListenerAcceptWait(l, xllmDeadlineWaitMs(end), c);
}
static inline bool mdo_test_xrtNetStreamWait(xnetstream *s, xnetstreamwait w, uint64_t end,
                                             xcancel *c)
{
    return xrtNetStreamWait(s, w, xllmDeadlineWaitMs(end), c);
}
static inline xwaitresult mdo_test_future_wait_us(xfuture *f, uint64_t us)
{
    return xrtFutureWaitFor(f, us == UINT64_MAX ? XRT_WAIT_FOREVER
                                                : (int64_t)(us / 1000u + (us % 1000u != 0u)));
}
#define main mdo_upstream_main
#include "../test_xllm_session_mdo.inc"

#undef main
int main(int argc, char **argv)
{
    if (!xrtDirCreateAll("out/mdo-integration/fixtures"))
        return 1;
    return mdo_upstream_main(argc, argv);
}
