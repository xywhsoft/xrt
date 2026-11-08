#ifndef XREDIS_POOL_CONNECT_PEER_H
#define XREDIS_POOL_CONNECT_PEER_H
#include "client_peer.h"
#define REDIS_CONNECT_PEERS 16u
enum { PC_NORMAL, PC_HOLD, PC_BAD_HELLO, PC_DROP_HELLO };
typedef struct redisconnectpeer {
    xnetengine* Engine;
    xnetlistener* Listener;
    xnetaddr Address;
    redispeer* Peers;
    redispeerhook Hook;
    xatomic32 Accepted, ListenerClosed, Mode, Commands, Hello;
} redisconnectpeer;
static bool redisConnectPeerCommand(redispeer* Peer, xredisreply* Reply, ptr Data)
{
    redisconnectpeer* Fixture = Data; xredisreplyvalue Root, Name;
    redisPeerRequire(xredisReplyGet(Reply, 0, &Root) && xredisReplyGet(Reply, Root.FirstChild, &Name), "reconnect command frame");
    if (!redisPeerName(Name, "HELLO")) {
        xrtAtomic32FetchAdd(&Fixture->Commands, 1, XMEMORY_RELEASE);
        if (redisPeerName(Name, "WATCH") || redisPeerName(Name, "UNWATCH")) { redisPeerSend(Peer, "+OK\r\n", 5); return true; }
        return false;
    }
    xrtAtomic32FetchAdd(&Fixture->Hello, 1, XMEMORY_RELEASE);
    switch (xrtAtomic32Load(&Fixture->Mode, XMEMORY_ACQUIRE)) {
    case PC_HOLD: break;
    case PC_BAD_HELLO: redisPeerSend(Peer, "-WRONGPASS fixture\r\n", 20); break;
    case PC_DROP_HELLO: xrtNetStreamAbort(Peer->Stream); break;
    default: { static const char Hello[] = "%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n";
        redisPeerSend(Peer, Hello, sizeof(Hello) - 1u); break; }
    }
    return true;
}
static bool redisConnectPeerAccept(xnetlistener* Listener, xnetstream* Stream, ptr Data)
{
    (void)Listener; redisconnectpeer* Fixture = Data;
    uint32 Count = xrtAtomic32Load(&Fixture->Accepted, XMEMORY_RELAXED); redispeer* Peer = &Fixture->Peers[Count % REDIS_CONNECT_PEERS];
    if (Peer->Stream) {
        redisPeerRequire(xrtAtomic32Load(&Peer->Closed, XMEMORY_ACQUIRE) != 0, "reconnect fixture ring still live");
        xrtNetStreamDestroy(Peer->Stream); xredisReplyReaderDestroy(Peer->Reader);
    }
    memset(Peer, 0, sizeof(*Peer)); Peer->Engine = Fixture->Engine;
    xredisreplyreaderconfig Reader; xredisReplyReaderConfigInit(&Reader); Reader.Reader.Protocol = XREDIS_RESP2;
    redisPeerRequire(xredisReplyReaderCreate(&Reader, &Peer->Reader), "reconnect fixture parser");
    xrtAtomicPtrStore(&Peer->Hook, &Fixture->Hook, XMEMORY_RELEASE);
    redisPeerRequire(redisPeerAccept(NULL, Stream, Peer), "reconnect fixture accept");
    xrtAtomic32Store(&Fixture->Accepted, Count + 1u, XMEMORY_RELEASE); return true;
}
static void redisConnectPeerListenerClosed(xnetlistener* Listener, ptr Data)
{ (void)Listener; xrtAtomic32Store(&((redisconnectpeer*)Data)->ListenerClosed, 1, XMEMORY_RELEASE); }
static const xnetlistenerevents redisConnectPeerListenerEvents = {redisConnectPeerAccept, NULL, redisConnectPeerListenerClosed};
static void redisConnectPeerStart(redisconnectpeer* Fixture)
{
    (void)redisPeerStart; (void)redisPeerStop;
    memset(Fixture, 0, sizeof(*Fixture)); Fixture->Peers = xrtCalloc(REDIS_CONNECT_PEERS, sizeof(redispeer));
    redisPeerRequire(Fixture->Peers != NULL, "reconnect fixture storage"); Fixture->Hook = (redispeerhook){redisConnectPeerCommand, Fixture};
    xnetengineconfig Engine; xrtNetEngineConfigInit(&Engine); Engine.Workers = 2;
#if defined(REDIS_PEER_NODE_CACHE_BYTES)
    Engine.NodeCacheBytes = REDIS_PEER_NODE_CACHE_BYTES;
#endif
    Fixture->Engine = xrtNetEngineCreate(&Engine); redisPeerRequire(Fixture->Engine && xrtNetEngineStart(Fixture->Engine), "reconnect fixture engine");
    xnetlistenconfig Listen; xrtNetListenConfigInit(&Listen); Listen.Affinity = 1; Listen.Distribution = XNET_ACCEPT_LOCAL;
    xrtNetAddrLoopback(&Listen.Address, XNET_FAMILY_IPV4, 0);
    Fixture->Listener = xrtNetListen(Fixture->Engine, &Listen, &redisConnectPeerListenerEvents, NULL, Fixture);
    redisPeerRequire(Fixture->Listener && xrtNetListenerLocal(Fixture->Listener, &Fixture->Address), "reconnect fixture listener");
}
static void redisConnectPeerStop(redisconnectpeer* Fixture)
{
    redisPeerRequire(xrtNetListenerClose(Fixture->Listener), "reconnect listener close"); redisPeerWait(&Fixture->ListenerClosed, 1);
    for (size_t Index = 0; Index < REDIS_CONNECT_PEERS; ++Index) if (Fixture->Peers[Index].Stream) {
        xrtNetStreamAbort(Fixture->Peers[Index].Stream); redisPeerWait(&Fixture->Peers[Index].Closed, 1);
    }
    redisPeerRequire(xrtNetEngineStop(Fixture->Engine), "reconnect engine stop");
    for (size_t Index = 0; Index < REDIS_CONNECT_PEERS; ++Index) {
        xrtNetStreamDestroy(Fixture->Peers[Index].Stream); xredisReplyReaderDestroy(Fixture->Peers[Index].Reader);
    }
    xrtNetListenerDestroy(Fixture->Listener); xrtFree(Fixture->Peers);
    redisPeerRequire(xrtNetEngineDestroy(Fixture->Engine), "reconnect engine destroy");
}
typedef struct redisconnectfactory {
    redisconnectpeer* Peer;
    xatomic32 Calls, Drops;
    uint32 FailFirst;
    bool Raw, Resp2, Observe;
    xatomicptr Pool;
    xfuture* Closed[256];
    double Times[256];
} redisconnectfactory;
static bool redisConnectFactory(xnetengine* Engine, size_t Slot, uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Client)
{
    redisconnectfactory* Factory = Data; uint32 Call = xrtAtomic32Load(&Factory->Calls, XMEMORY_RELAXED);
    if (Call < 256) Factory->Times[Call] = xrtTimer();
    xrtAtomic32Store(&Factory->Calls, Call + 1u, XMEMORY_RELEASE);
    (void)Attempt; if (Call < Factory->FailFirst) return false;
    xredisclientconfig Config; xredisClientConfigInit(&Config);
    if (Factory->Raw) return xredisClientConnect(Engine, &Factory->Peer->Address, 1u - Slot % 2u, &Config, Client);
    xredisclientinitconfig Init; xredisClientInitConfigInit(&Init); Init.Timeout = Timeout; if (Factory->Resp2) Init.Mode = XREDIS_INIT_RESP2;
    bool Result = xredisClientConnectInit(Engine, &Factory->Peer->Address, 1u - Slot % 2u, &Config, &Init, Client);
    if (Result && Factory->Observe && Call < 256) Factory->Closed[Call] = xredisClientClosed(*Client);
    return Result;
}
static void redisConnectFactoryDrop(ptr Data)
{
    redisconnectfactory* Factory = Data;
    for (size_t Index = 0; Index < 256; ++Index) if (Factory->Closed[Index]) {
        redisPeerRequire(xrtFutureState(Factory->Closed[Index]) == XFUTURE_RESOLVED, "context drop preceded candidate physical close");
        xrtFutureDestroy(Factory->Closed[Index]); Factory->Closed[Index] = NULL;
    }
    xredispool* Pool = xrtAtomicPtrLoad(&Factory->Pool, XMEMORY_ACQUIRE);
    if (Pool) {
        xredispoolinfo Info = {0}; Info.Size = sizeof(Info);
        redisPeerRequire(xredisPoolInfo(Pool, &Info) && !Info.Live && !Info.Leased && !Info.Requests && !Info.Pending, "context drop did not join pool duties/reentrant snapshot");
    }
    xrtAtomic32FetchAdd(&Factory->Drops, 1, XMEMORY_RELEASE);
}
static void redisConnectFactoryConfig(redisconnectfactory* Factory, redisconnectpeer* Peer, xredispoolconnectconfig* Config)
{
    memset(Factory, 0, sizeof(*Factory)); Factory->Peer = Peer; xredisPoolConnectConfigInit(Config);
    Config->Connect = redisConnectFactory; Config->Drop = redisConnectFactoryDrop; Config->Data = Factory;
}
#endif
