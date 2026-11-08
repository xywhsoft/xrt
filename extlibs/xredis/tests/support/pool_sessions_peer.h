#ifndef XREDIS_POOL_SESSIONS_PEER_H
#define XREDIS_POOL_SESSIONS_PEER_H
#include "watch_peer.h"
typedef struct redispoolsessionspeer {
    rediswatchpeer Redis;
    xredispool* Pool;
    xredisclient* Client;
} redispoolsessionspeer;
static inline void redisPoolSessionsPeerStart(redispoolsessionspeer* Fixture, unsigned WatchMode,
    unsigned TransactionMode, bool Secure, const xredispoolconfig* Config)
{
    memset(Fixture, 0, sizeof(*Fixture)); redisWatchPeerStart(&Fixture->Redis, WatchMode, TransactionMode, Secure);
    Fixture->Client = redisTransactionPeerConnect(&Fixture->Redis.Tx, NULL); redisWatchPeerReady(Fixture->Client);
    /* Deliberately use a different pool worker from the client's worker. */
    redisPeerRequire(xredisPoolCreate(Fixture->Redis.Tx.Peer->Engine, 1, &Fixture->Client, 1, Config, &Fixture->Pool) == XREDIS_ADMISSION_ACCEPTED, "session pool create");
}
static inline void redisPoolSessionsPeerStop(redispoolsessionspeer* Fixture)
{
    xfuture* Closed = xredisPoolClosed(Fixture->Pool); xredisPoolClose(Fixture->Pool);
    redisPeerRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "session pool physical close");
    xredispoolinfo Info = {0}; Info.Size = sizeof(Info);
    redisPeerRequire(xredisPoolInfo(Fixture->Pool, &Info) && !Info.Requests && !Info.CommandBytes && !Info.Pending && !Info.Leased && Info.Retired == 1,
        "session pool retained physical reservation");
    xredisPoolRelease(Fixture->Pool); xrtFutureDestroy(Closed); redisTransactionPeerStop(&Fixture->Redis.Tx);
}
#endif
