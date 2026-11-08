/* From repository root (Python 3 + C11 compiler):
 * python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool --no-single
 * Self-contained local peer, not a Redis interoperability/performance oracle.
 * Assertions cover two exclusive members, native requests and physical return.
 * All observers, clients, listeners and the engine are closed below. */
#include <xredis.h>
#include "../../tests/support/pool_peer.h"
int main(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 2); xredispool* Pool = NULL;
    redisPeerRequire(xredisPoolCreate(Peer.Peers[0].Engine, 0, Peer.Clients, 2, NULL, &Pool) == XREDIS_ADMISSION_ACCEPTED, "example pool create");
    xfuture *A = NULL, *B = NULL;
    redisPeerRequire(xredisPoolAcquire(Pool, 5000, &A) == XREDIS_ADMISSION_ACCEPTED && xredisPoolAcquire(Pool, 5000, &B) == XREDIS_ADMISSION_ACCEPTED, "example acquire");
    const xredisleaseresult *LA = xrtFutureValue(A), *LB = xrtFutureValue(B);
    redisPeerRequire(LA && LB && LA->Lease && LB->Lease && LA->Lease != LB->Lease, "example exclusive leases");
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("pool\0binary")}; xfuture* Call = NULL;
    redisPeerRequire(xredisLeaseCall(LA->Lease, Echo, 2, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED &&
        ((const xredisrequestresult*)xrtFutureValue(Call))->Status == XREDIS_REQUEST_OK, "example native request");
    xfuture *CA = xredisLeaseClosed(LA->Lease), *CB = xredisLeaseClosed(LB->Lease);
    xredisLeaseReturn(LA->Lease); xredisLeaseReturn(LB->Lease);
    redisPeerRequire(xrtFutureWaitFor(CA, 10000) == XWAIT_OK && xrtFutureWaitFor(CB, 10000) == XWAIT_OK &&
        ((const xredisleaseclosedresult*)xrtFutureValue(CA))->Reusable && ((const xredisleaseclosedresult*)xrtFutureValue(CB))->Reusable, "example lease return");
    xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool);
    redisPeerRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "example pool physical close");
    xredisPoolRelease(Pool); redisPoolPeerStop(&Peer);
    xrtFutureDestroy(A); xrtFutureDestroy(B); xrtFutureDestroy(CA); xrtFutureDestroy(CB); xrtFutureDestroy(Call); xrtFutureDestroy(Closed);
    puts("[PASS] two-member exclusive pool / binary native request / physical return / close"); return 0;
}
