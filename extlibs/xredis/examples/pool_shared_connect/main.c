#include <xredis.h>
#include "../../tests/support/pool_connect_peer.h"
static bool exampleSharedOnline(xredispool* P, uint64 Generations)
{
    double Due = xrtTimer() + 10;
    while (xrtTimer() < Due) {xredispoolinfo I = {0}; I.Size = sizeof(I); xredispoolconnectinfo C = {0}; C.Size = sizeof(C);
        if (!xredisPoolInfo(P, &I) || !xredisPoolConnectInfo(P, &C)) return false;
        if (I.Idle == 2 && C.Connected >= Generations) return true;
        xrtThreadYield();}
    return false;
}
int main(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory F; xredispoolconnectconfig C; redisConnectFactoryConfig(&F, &Peer, &C);
    F.Observe = true; C.RetryMin = C.RetryMax = 5; xredispool* P = NULL;
    if (xredisSharedPoolConnect(Peer.Engine, 0, 2, NULL, &C, &P) != XREDIS_ADMISSION_ACCEPTED) return 1;
    xrtAtomicPtrStore(&F.Pool, P, XMEMORY_RELEASE); if (!exampleSharedOnline(P, 2)) return 2;
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("shared\0repair")}; xfuture* Reply = NULL;
    if (xredisSharedPoolCall(P, Echo, 2, 5000, &Reply) != XREDIS_ADMISSION_ACCEPTED || ((const xredisrequestresult*)xrtFutureValue(Reply))->Status != XREDIS_REQUEST_OK) return 3;
    xbytesview Drop = XRT_BYTES_LITERAL("DROP"); xfuture* Lost = NULL;
    if (xredisSharedPoolCall(P, &Drop, 1, 5000, &Lost) != XREDIS_ADMISSION_ACCEPTED || ((const xredisrequestresult*)xrtFutureValue(Lost))->Status != XREDIS_REQUEST_DISCONNECTED) return 4;
    if (!exampleSharedOnline(P, 3)) return 5;
    xfuture* Closed = xredisPoolClosed(P); xredisPoolClose(P);
    if (xrtFutureWaitFor(Closed, 10000) != XWAIT_OK || xrtAtomic32Load(&F.Drops, XMEMORY_ACQUIRE) != 1) return 6;
    xredisPoolRelease(P); xrtFutureDestroy(Closed); redisConnectPeerStop(&Peer);
    const xredisrequestresult* R = xrtFutureValue(Reply); xredisreplyvalue V;
    if (!xredisReplyGet(R->Reply, 0, &V) || V.Data.Size != 13 || memcmp(V.Data.Data, "shared\0repair", 13)) return 7;
    xrtFutureDestroy(Reply); xrtFutureDestroy(Lost);
    puts("Shared members reconnect; uncertain requests never replay; retained replies survive engine shutdown"); return 0;
}
