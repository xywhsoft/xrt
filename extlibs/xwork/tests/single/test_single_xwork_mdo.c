#define XLLM_TEST_ALLOCATOR 1
#define XLLM_SESSION_TEST_FAULTS 1
#include <xwork/features.h>
#include <xllm-session/features.h>
#include <xllm/features.h>
#include <xhttp-client/features.h>
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static inline void mdo_test_sleep_ms(uint64_t us)
{
    xrtSleep((int64_t)us);
}
static bool test_artifact_publish(xroot, cstr, cstr);
static bool test_process_kill_tree(xprocess *);
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#define XHTTP_POLICY_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp-policy.h"
#define XHTTP_CLIENT_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp-client.h"
#define XLLM_IMPLEMENTATION
#include "../../../../single/extlibs/xllm.h"
#define XLLM_SESSION_IMPLEMENTATION
#include "../../../../single/extlibs/xllm-session.h"
#define xrtRootLinkHard test_artifact_publish
#define xrtProcessKillTree test_process_kill_tree
#define XWORK_IMPLEMENTATION
#include "../../../../single/extlibs/xwork.h"
#undef xrtRootLinkHard
#undef xrtProcessKillTree
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
static inline bool mdo_test_time_make(const xdatetime *d, xtime *t)
{
    xtime v;
    if (!xrtTimeMake(d, &v))
        return false;
    *t = xllmTimeToUnixUs(v);
    return true;
}
static inline bool mdo_test_time_from_local(const xdatetime *d, xtimefold f, xtime *t)
{
    xtime v;
    if (!xrtTimeFromLocal(d, f, &v))
        return false;
    *t = xllmTimeToUnixUs(v);
    return true;
}
static inline bool mdo_test_time_local(xtime t, xdatetime *d)
{
    return xrtTimeLocal(XRT_TIME_UNIX_EPOCH + t / 1000, d);
}
#define main mdo_upstream_main
#include "../test_xwork_mdo.inc"

#undef main
int main(int argc, char **argv)
{
    if (!xrtDirCreateAll("out/mdo-integration/fixtures"))
        return 1;
    return mdo_upstream_main(argc, argv);
}
