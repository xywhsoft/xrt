#define XREDIS_MODULE_XREDIS_POOL_SHARED
#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_THREAD
#include <xredis/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XREDIS_IMPLEMENTATION
#include "../../../../single/extlibs/xredis.h"
#define main redisSharedPoolPublicTests
#include "../test_pool_shared.c"
#undef main

int main(void)
{
    redispoolpeer Peer;
    xredispool* Pool = sharedStart(&Peer, 1, NULL);
    redisPeerWait(&Peer.Peers[0].Commands, 1);
    const uint32 Artificial = UINT32_MAX / 2u;
    xredisclient* Client = Peer.Clients[0];
    xbytesview Ping = XRT_BYTES_LITERAL("PING");
    xfuture* Output = (xfuture*)(uintptr_t)1;
    /* Raise only the public-reference counter, then restore the artificial
       offset before checking the API result or closing the fixture. */
    uint32 Native = xrtAtomic32FetchAdd(&Client->References, Artificial, XMEMORY_RELAXED);
    testRequire(Native > 0 && Native < Artificial, "valid fixture reference baseline");
    xredisadmission Result = xredisSharedPoolCallAsync(Pool, &Ping, 1, 5000, &Output);
    xrtAtomic32FetchSub(&Client->References, Artificial, XMEMORY_RELAXED);
    testRequire(Result == XREDIS_ADMISSION_ERROR && Output == (xfuture*)(uintptr_t)1,
        "shared admission must stop when the client reference cannot be retained");
    testRequire(xrtAtomic32Load(&Peer.Peers[0].Commands, XMEMORY_ACQUIRE) == 1,
        "reference failure submitted a wire command");
    xrtClearError();
    Output = NULL;
    testRequire(xredisSharedPoolCall(Pool, &Ping, 1, 5000, &Output) == XREDIS_ADMISSION_ACCEPTED &&
        sharedWait(Output)->Status == XREDIS_REQUEST_OK, "shared member remains usable after reference refusal");
    xrtFutureDestroy(Output);
    redisPeerWait(&Peer.Peers[0].Commands, 2);
    sharedStop(&Peer, Pool);
    puts("[PASS] shared client reference refusal preserves output and pool ownership");
    return redisSharedPoolPublicTests();
}
