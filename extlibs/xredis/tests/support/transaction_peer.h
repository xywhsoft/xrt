#ifndef XREDIS_TRANSACTION_PEER_H
#define XREDIS_TRANSACTION_PEER_H
#include "client_peer.h"
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#include "tls_peer.h"
#endif

#define REDIS_TX_SEND(Peer, Text) redisPeerSend((Peer), (Text), sizeof(Text) - 1u)

enum { TX_NORMAL, TX_MULTI_ERROR, TX_BAD_MULTI, TX_BAD_QUEUE, TX_EXEC_ERROR,
    TX_BAD_COUNT, TX_ABORT, TX_DISCARD_ERROR, TX_HOLD_MULTI, TX_HOLD_QUEUE,
    TX_HOLD_EXEC, TX_HOLD_DISCARD, TX_DROP_MULTI, TX_DROP_COMMAND, TX_DROP_EXEC,
    TX_HOLD_BEFORE, TX_PUSH, TX_RUNTIME_ERROR, TX_SYNC, TX_EXEC_UNKNOWN };

typedef struct redistransactionpeer {
    redispeer Plain;
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    redistlspeer TLS;
#endif
    redispeer* Peer;
    redispeerhook Hook;
    redispeerhook* Prior;
    xredisclient* Client;
    unsigned Mode;
    bool Secure, Multi;
    size_t Count, Bytes;
    unsigned char Replies[65536];
    xatomic32 Multis, Bodies, Execs, Discards, Effects, Outside, SyncChecked;
} redistransactionpeer;

static inline void redisTransactionPeerAppend(redistransactionpeer* Fixture, const void* Data, size_t Size)
{
    redisPeerRequire(Size <= sizeof(Fixture->Replies) - Fixture->Bytes, "transaction fixture reply bound");
    memcpy(Fixture->Replies + Fixture->Bytes, Data, Size); Fixture->Bytes += Size;
}

static bool redisTransactionPeerCommand(redispeer* Peer, xredisreply* Reply, ptr Data)
{
    redistransactionpeer* Fixture = Data;
    if (Fixture->Prior && Fixture->Prior->Command(Peer, Reply, Fixture->Prior->Data)) return true;
    xredisreplyvalue Root, Name, Value;
    redisPeerRequire(xredisReplyGet(Reply, 0, &Root) && xredisReplyGet(Reply, Root.FirstChild, &Name), "transaction fixture command");
    if (redisPeerName(Name, "HELLO")) { REDIS_TX_SEND(Peer, "%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n"); return true; }
    if (redisPeerName(Name, "MULTI")) {
        redisPeerRequire(!Fixture->Multi && !Peer->Blocked, "transaction began before preceding request retired");
        xrtAtomic32FetchAdd(&Fixture->Multis, 1, XMEMORY_RELEASE);
        if (Fixture->Mode == TX_SYNC) {
            xfuture* Future = (xfuture*)(uintptr_t)1;
            redisPeerRequire(xredisClientTransaction(Fixture->Client, NULL, 0, NULL, 100, &Future) == XREDIS_ADMISSION_ERROR &&
                Future == (xfuture*)(uintptr_t)1, "sync transaction blocked a different engine worker");
            xrtClearError(); xrtAtomic32Store(&Fixture->SyncChecked, 1, XMEMORY_RELEASE);
        }
        if (Fixture->Mode == TX_DROP_MULTI) { xrtNetStreamAbort(Peer->Stream); return true; }
        if (Fixture->Mode == TX_MULTI_ERROR) { REDIS_TX_SEND(Peer, "-NOPERM MULTI denied\r\n"); return true; }
        Fixture->Multi = true; Fixture->Count = Fixture->Bytes = 0;
        if (Fixture->Mode == TX_HOLD_MULTI) Peer->Blocked = true;
        if (Fixture->Mode == TX_BAD_MULTI) REDIS_TX_SEND(Peer, "+PONG\r\n"); else REDIS_TX_SEND(Peer, "+OK\r\n");
        return true;
    }
    if (redisPeerName(Name, "DISCARD")) {
        redisPeerRequire(Fixture->Multi, "DISCARD outside transaction"); Fixture->Multi = false;
        xrtAtomic32FetchAdd(&Fixture->Discards, 1, XMEMORY_RELEASE);
        if (Fixture->Mode == TX_HOLD_DISCARD) Peer->Blocked = true;
        if (Fixture->Mode == TX_DISCARD_ERROR) REDIS_TX_SEND(Peer, "-NOPERM DISCARD denied\r\n"); else REDIS_TX_SEND(Peer, "+OK\r\n");
        return true;
    }
    if (redisPeerName(Name, "EXEC")) {
        redisPeerRequire(Fixture->Multi && !Peer->Blocked, "EXEC bypassed queue validation");
        xrtAtomic32FetchAdd(&Fixture->Execs, 1, XMEMORY_RELEASE);
        if (Fixture->Mode == TX_EXEC_UNKNOWN) { REDIS_TX_SEND(Peer, "-NOPERM opaque EXEC failure\r\n"); return true; }
        Fixture->Multi = false;
        if (Fixture->Mode == TX_DROP_EXEC) { xrtNetStreamAbort(Peer->Stream); return true; }
        if (Fixture->Mode == TX_HOLD_EXEC) Peer->Blocked = true;
        if (Fixture->Mode == TX_EXEC_ERROR) { REDIS_TX_SEND(Peer, "-EXECABORT deliberate\r\n"); return true; }
        if (Fixture->Mode == TX_ABORT) { REDIS_TX_SEND(Peer, "*-1\r\n"); return true; }
        char Header[48]; size_t Count = Fixture->Count + (Fixture->Mode == TX_BAD_COUNT ? 1u : 0u);
        int Size = snprintf(Header, sizeof(Header), "*%zu\r\n", Count); redisPeerSend(Peer, Header, (size_t)Size);
        redisPeerSend(Peer, Fixture->Replies, Fixture->Bytes);
        if (Fixture->Mode == TX_BAD_COUNT) REDIS_TX_SEND(Peer, "+extra\r\n");
        xrtAtomic32FetchAdd(&Fixture->Effects, (uint32)Fixture->Count, XMEMORY_RELEASE); return true;
    }
    if (!Fixture->Multi) {
        if (redisPeerName(Name, "BEFORE") && Fixture->Mode == TX_HOLD_BEFORE) Peer->Blocked = true;
        if (redisPeerName(Name, "BODY")) xrtAtomic32FetchAdd(&Fixture->Outside, 1, XMEMORY_RELEASE);
        return false;
    }
    redisPeerRequire(!redisPeerName(Name, "AFTER") && (Fixture->Mode != TX_HOLD_MULTI || !Peer->Blocked), "ordinary follower or body entered before transaction boundary");
    xrtAtomic32FetchAdd(&Fixture->Bodies, 1, XMEMORY_RELEASE);
    if (Fixture->Mode == TX_DROP_COMMAND) { xrtNetStreamAbort(Peer->Stream); return true; }
    bool Bad = redisPeerName(Name, "BAD"); ++Fixture->Count;
    if (Bad) REDIS_TX_SEND(Peer, "-ERR queue rejected\r\n");
    else {
        if (Fixture->Mode == TX_PUSH) REDIS_TX_SEND(Peer, ">1\r\n+notice\r\n");
        if (Fixture->Mode == TX_HOLD_QUEUE) Peer->Blocked = true;
        if (Fixture->Mode == TX_BAD_QUEUE) REDIS_TX_SEND(Peer, "+WRONG\r\n"); else REDIS_TX_SEND(Peer, "+QUEUED\r\n");
    }
    if (redisPeerName(Name, "ECHO")) {
        redisPeerRequire(xredisReplyGet(Reply, Name.Next, &Value), "queued binary argument"); char Header[48];
        int Size = snprintf(Header, sizeof(Header), "$%zu\r\n", Value.Data.Size);
        redisTransactionPeerAppend(Fixture, Header, (size_t)Size); redisTransactionPeerAppend(Fixture, Value.Data.Data, Value.Data.Size);
        redisTransactionPeerAppend(Fixture, "\r\n", sizeof("\r\n") - 1u);
    } else if (Fixture->Mode == TX_RUNTIME_ERROR) redisTransactionPeerAppend(Fixture, "-WRONGTYPE deliberate\r\n", sizeof("-WRONGTYPE deliberate\r\n") - 1u);
    else redisTransactionPeerAppend(Fixture, "+OK\r\n", sizeof("+OK\r\n") - 1u);
    return true;
}

static inline void redisTransactionPeerStart(redistransactionpeer* Fixture, unsigned Mode, bool Secure)
{
    memset(Fixture, 0, sizeof(*Fixture)); Fixture->Mode = Mode; Fixture->Secure = Secure;
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    if (Secure) { redisTlsPeerStart(&Fixture->TLS, false, false, 0); Fixture->Peer = &Fixture->TLS.Dial.Init.Peer; }
    else
#else
    redisPeerRequire(!Secure, "TLS fixture selection unavailable");
#endif
    { redisPeerStart(&Fixture->Plain); Fixture->Peer = &Fixture->Plain; }
    Fixture->Prior = (redispeerhook*)xrtAtomicPtrLoad(&Fixture->Peer->Hook, XMEMORY_ACQUIRE);
    Fixture->Hook = (redispeerhook){redisTransactionPeerCommand, Fixture}; xrtAtomicPtrStore(&Fixture->Peer->Hook, &Fixture->Hook, XMEMORY_RELEASE);
}

static inline xredisclient* redisTransactionPeerConnect(redistransactionpeer* Fixture, const xredisclientconfig* Config)
{
    xredisclient* Client = NULL;
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    if (Fixture->Secure) {
        xredisclienttlsconfig TLS; redisTlsPeerConfig(&Fixture->TLS, &TLS); TLS.WriteBytes = 3;
        redisPeerRequire(xredisClientTlsDial(Fixture->Peer->Engine, Fixture->TLS.Dial.Resolver, XRT_STR_LITERAL("redis.fixture"),
            Fixture->Peer->Address.Port, 0, Config, NULL, &TLS, &Client), "transaction TLS connect");
    } else
#endif
        redisPeerRequire(xredisClientConnectInit(Fixture->Peer->Engine, &Fixture->Peer->Address, 0, Config, NULL, &Client), "transaction managed connect");
    Fixture->Client = Client; return Client;
}

static inline void redisTransactionPeerStop(redistransactionpeer* Fixture)
{
    xfuture* Closed = xredisClientClosed(Fixture->Client); redisPeerRequire(xredisClientClose(Fixture->Client) &&
        xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "transaction physical close");
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info);
    redisPeerRequire(xredisClientInfo(Fixture->Client, &Info) && Info.Requests == 0 && Info.CommandBytes == 0 &&
        Info.Accepted == Info.Completed, "transaction request/wire tails retained at close");
    xredisClientRelease(Fixture->Client); xrtFutureDestroy(Closed);
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    if (Fixture->Secure) { redisTlsPeerStop(&Fixture->TLS); return; }
#endif
    redisPeerStop(Fixture->Peer);
}
#endif
