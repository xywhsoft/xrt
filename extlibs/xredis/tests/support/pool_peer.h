#ifndef XREDIS_POOL_PEER_H
#define XREDIS_POOL_PEER_H
#include "client_peer.h"
#define REDIS_POOL_PEER_MAX 8
typedef struct redispoolpeer {
    redispeer Peers[REDIS_POOL_PEER_MAX];
    redispeerhook Hooks[REDIS_POOL_PEER_MAX];
    xredisclient* Clients[REDIS_POOL_PEER_MAX];
    size_t Count;
} redispoolpeer;
static bool redisPoolPeerCommand(redispeer* Peer, xredisreply* Reply, ptr Data)
{
    (void)Data; xredisreplyvalue Root, Name;
    redisPeerRequire(xredisReplyGet(Reply, 0, &Root) && xredisReplyGet(Reply, Root.FirstChild, &Name), "pool peer command");
    if (!redisPeerName(Name, "HELLO")) return false;
    static const char Hello[] = "%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n";
    redisPeerSend(Peer, Hello, sizeof(Hello) - 1u); return true;
}
static inline void redisPoolPeerStart(redispoolpeer* Fixture, size_t Count)
{
    redisPeerRequire(Count && Count <= REDIS_POOL_PEER_MAX, "pool peer count"); memset(Fixture, 0, sizeof(*Fixture)); Fixture->Count = Count;
    redisPeerStart(&Fixture->Peers[0]); xnetengine* Engine = Fixture->Peers[0].Engine;
    for (size_t Index = 0; Index < Count; ++Index) {
        redispeer* Peer = &Fixture->Peers[Index];
        if (Index) {
            Peer->Engine = Engine; xredisreplyreaderconfig Reader; xredisReplyReaderConfigInit(&Reader); Reader.Reader.Protocol = XREDIS_RESP2;
            redisPeerRequire(xredisReplyReaderCreate(&Reader, &Peer->Reader), "pool member peer parser");
            xnetlistenconfig Listen; xrtNetListenConfigInit(&Listen); xrtNetAddrLoopback(&Listen.Address, XNET_FAMILY_IPV4, 0);
            Listen.Affinity = 1; Listen.Distribution = XNET_ACCEPT_LOCAL;
            Peer->Listener = xrtNetListen(Engine, &Listen, &redisPeerListenerEvents, NULL, Peer);
            redisPeerRequire(Peer->Listener && xrtNetListenerLocal(Peer->Listener, &Peer->Address), "pool shared-engine listener");
        }
        Fixture->Hooks[Index] = (redispeerhook){redisPoolPeerCommand, NULL}; xrtAtomicPtrStore(&Peer->Hook, &Fixture->Hooks[Index], XMEMORY_RELEASE);
        redisPeerRequire(xredisClientConnectInit(Engine, &Peer->Address, Index % 2u, NULL, NULL, &Fixture->Clients[Index]), "pool fixture client");
        xfuture* Ready = xredisClientReady(Fixture->Clients[Index]);
        redisPeerRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status == XREDIS_READY_OK, "pool fixture readiness");
        xrtFutureDestroy(Ready);
    }
}
static inline void redisPoolPeerStop(redispoolpeer* Fixture)
{
    for (size_t Index = 0; Index < Fixture->Count; ++Index) {
        xredisclient* Client = Fixture->Clients[Index]; xfuture* Closed = xredisClientClosed(Client);
        redisPeerRequire(xredisClientClose(Client) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "pool fixture client retirement");
        xredisclientinfo Info = {0}; Info.Size = sizeof(Info);
        redisPeerRequire(xredisClientInfo(Client, &Info) && !Info.Requests && !Info.CommandBytes, "pool fixture native storage retained");
        xredisClientRelease(Client); xrtFutureDestroy(Closed);
    }
    for (size_t Index = 1; Index < Fixture->Count; ++Index) {
        redispeer* Peer = &Fixture->Peers[Index];
        redisPeerRequire(xrtNetListenerClose(Peer->Listener), "pool member listener close"); redisPeerWait(&Peer->ListenerClosed, 1);
        if (Peer->Stream) { xrtNetStreamAbort(Peer->Stream); redisPeerWait(&Peer->Closed, 1); }
        xrtNetStreamDestroy(Peer->Stream); xrtNetListenerDestroy(Peer->Listener); xredisReplyReaderDestroy(Peer->Reader);
    }
    redisPeerStop(&Fixture->Peers[0]);
}
#endif
