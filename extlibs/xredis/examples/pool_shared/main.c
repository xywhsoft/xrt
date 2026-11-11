#include <xredis.h>
#include "../../tests/support/pool_peer.h"
int main(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 2); xredispool* Pool = NULL;
    if (xredisSharedPoolCreate(Peer.Peers[0].Engine, 0, Peer.Clients, 2, NULL, &Pool) != XREDIS_ADMISSION_ACCEPTED) return 1;
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("shared\0binary")}; xfuture* Calls[8] = {0};
    for (size_t I = 0; I < 8; ++I) if (xredisSharedPoolCallAsync(Pool, Echo, 2, 5000, &Calls[I]) != XREDIS_ADMISSION_ACCEPTED) return 2;
    for (size_t I = 0; I < 8; ++I) if (xrtFutureWaitFor(Calls[I], 10000) != XWAIT_OK || ((const xredisrequestresult*)xrtFutureValue(Calls[I]))->Status != XREDIS_REQUEST_OK) return 3;
    xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool);
    if (xrtFutureWaitFor(Closed, 10000) != XWAIT_OK) return 4;
    xredisPoolRelease(Pool); xrtFutureDestroy(Closed); redisPoolPeerStop(&Peer);
    for (size_t I = 0; I < 8; ++I) {
        xredisreplyvalue V; const xredisrequestresult* R = xrtFutureValue(Calls[I]);
        if (!xredisReplyGet(R->Reply, 0, &V) || V.Data.Size != 13 || memcmp(V.Data.Data, "shared\0binary", 13)) return 5;
        xrtFutureDestroy(Calls[I]);
    }
    puts("Eight native pipelined calls over two shared members; replies survive engine shutdown"); return 0;
}
