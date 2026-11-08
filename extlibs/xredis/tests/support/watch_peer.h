#ifndef XREDIS_WATCH_PEER_H
#define XREDIS_WATCH_PEER_H
#include "transaction_peer.h"
enum { W_NORMAL, W_HOLD_START, W_BAD_START, W_DENY_START, W_DROP_START,
    W_BAD_CLEANUP, W_DENY_CLEANUP, W_HOLD_CLEANUP };
typedef struct rediswatchpeer {
    redistransactionpeer Tx;
    redispeerhook Hook;
    unsigned Mode;
    bool Watched;
    xatomic32 Starts, Cleanups;
} rediswatchpeer;
static bool redisWatchPeerCommand(redispeer* Peer, xredisreply* Reply, ptr Data)
{
    rediswatchpeer* Fixture = Data; xredisreplyvalue Root, Name;
    redisPeerRequire(xredisReplyGet(Reply, 0, &Root) && xredisReplyGet(Reply, Root.FirstChild, &Name), "watch fixture command");
    if (redisPeerName(Name, "WATCH")) {
        redisPeerRequire(!Fixture->Watched && !Fixture->Tx.Multi && Root.ChildCount > 1, "nested or empty WATCH");
        Fixture->Watched = true; xrtAtomic32FetchAdd(&Fixture->Starts, 1, XMEMORY_RELEASE);
        if (Fixture->Mode == W_DROP_START) { xrtNetStreamAbort(Peer->Stream); return true; }
        if (Fixture->Mode == W_HOLD_START) Peer->Blocked = true;
        if (Fixture->Mode == W_BAD_START) REDIS_TX_SEND(Peer, "+wrong\r\n");
        else if (Fixture->Mode == W_DENY_START) REDIS_TX_SEND(Peer, "-NOPERM WATCH denied\r\n");
        else REDIS_TX_SEND(Peer, "+OK\r\n");
        return true;
    }
    if (redisPeerName(Name, "UNWATCH")) {
        redisPeerRequire(!Fixture->Tx.Multi, "UNWATCH inside MULTI");
        xrtAtomic32FetchAdd(&Fixture->Cleanups, 1, XMEMORY_RELEASE);
        if (Fixture->Mode == W_HOLD_CLEANUP) Peer->Blocked = true;
        if (Fixture->Mode == W_BAD_CLEANUP) REDIS_TX_SEND(Peer, "+wrong\r\n");
        else if (Fixture->Mode == W_DENY_CLEANUP) REDIS_TX_SEND(Peer, "-NOPERM UNWATCH denied\r\n");
        else { Fixture->Watched = false; REDIS_TX_SEND(Peer, "+OK\r\n"); }
        return true;
    }
    bool Clear = (redisPeerName(Name, "EXEC") && Fixture->Tx.Mode != TX_EXEC_UNKNOWN) || redisPeerName(Name, "DISCARD");
    bool Handled = redisTransactionPeerCommand(Peer, Reply, &Fixture->Tx);
    if (Clear) Fixture->Watched = false;
    return Handled;
}
static inline void redisWatchPeerStart(rediswatchpeer* Fixture, unsigned Mode, unsigned TxMode, bool Secure)
{
    memset(Fixture, 0, sizeof(*Fixture)); Fixture->Mode = Mode;
    redisTransactionPeerStart(&Fixture->Tx, TxMode, Secure);
    Fixture->Hook = (redispeerhook){redisWatchPeerCommand, Fixture};
    xrtAtomicPtrStore(&Fixture->Tx.Peer->Hook, &Fixture->Hook, XMEMORY_RELEASE);
}
static inline void redisWatchPeerReady(xredisclient* Client)
{
    xfuture* Future = xredisClientReady(Client);
    redisPeerRequire(xrtFutureWaitFor(Future, 10000) == XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Future))->Status == XREDIS_READY_OK, "watch managed readiness");
    xrtFutureDestroy(Future);
    double End = xrtTimer() + 10;
    for (;;) {
        xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Client, &Info);
        if (!Info.Requests) return;
        redisPeerRequire(xrtTimer() < End, "watch native idle"); xrtThreadYield();
    }
}
#endif
