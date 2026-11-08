#include <xredis.h>
#include "../../tests/support/pool_connect_peer.h"
/* Runnable without a Redis installation: one local protocol fixture. See the
 * pool-connect guide for build commands and production factory ownership. */
int main(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
    redisConnectFactoryConfig(&Factory, &Peer, &Config); Config.RetryMin = Config.RetryMax = 5;
    xredispool* Pool = NULL;
    redisPeerRequire(xredisPoolConnect(Peer.Engine, 0, 1, NULL, &Config, &Pool) == XREDIS_ADMISSION_ACCEPTED, "automatic example pool");
    for (unsigned Index = 0; Index < 2; ++Index) {
        xfuture *Acquired = NULL, *Call = NULL; xbytesview Ping = XRT_BYTES_LITERAL("PING");
        redisPeerRequire(xredisPoolAcquire(Pool, 5000, &Acquired) == XREDIS_ADMISSION_ACCEPTED, "automatic example acquire");
        const xredisleaseresult* Result = xrtFutureValue(Acquired); redisPeerRequire(Result && Result->Lease, "automatic example lease");
        redisPeerRequire(xredisLeaseCall(Result->Lease, &Ping, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredisrequestresult*)xrtFutureValue(Call))->Status == XREDIS_REQUEST_OK, "automatic example PING");
        xfuture* Returned = xredisLeaseClosed(Result->Lease); if (!Index) xredisLeaseAbort(Result->Lease); else xredisLeaseReturn(Result->Lease);
        redisPeerRequire(xrtFutureWaitFor(Returned, 5000) == XWAIT_OK, "automatic example physical return");
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Call); xrtFutureDestroy(Returned);
    }
    xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool); redisPeerRequire(xrtFutureWaitFor(Closed, 5000) == XWAIT_OK, "automatic example close");
    redisPeerRequire(xrtAtomic32Load(&Factory.Drops, XMEMORY_ACQUIRE) == 1, "factory context retired");
    xredisPoolRelease(Pool); xrtFutureDestroy(Closed); redisConnectPeerStop(&Peer); puts("[PASS] automatically replaced one aborted connection"); return 0;
}
