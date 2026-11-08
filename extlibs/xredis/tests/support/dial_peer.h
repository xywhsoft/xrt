#ifndef XREDIS_DIAL_PEER_H
#define XREDIS_DIAL_PEER_H
#include "client_init_peer.h"

#define REDIS_DIAL_BLOCK 1u
#define REDIS_DIAL_MISSING 2u
#define REDIS_DIAL_DUAL 4u
typedef struct redisdialpeer {
	redisinitpeer Init;
	xnetresolver* Resolver;
	xnetsocket Refused;
	xnetaddr RefusedAddress;
	xatomic32 Lookups, Entered, Gate, Exited;
	unsigned Flags;
} redisdialpeer;

static xnetaddrlist* redisDialPeerLookup(cstr Host, xnetfamily Family, ptr Data)
{
	redisdialpeer* Peer = (redisdialpeer*)Data;
	redisPeerRequire(strcmp(Host, "redis.fixture") == 0, "copied/canonicalized host lifetime");
	(void)xrtAtomic32FetchAdd(&Peer->Lookups, 1, XMEMORY_RELAXED); xrtAtomic32Store(&Peer->Entered, 1, XMEMORY_RELEASE);
	if (Peer->Flags & REDIS_DIAL_BLOCK) {
		double End = xrtTimer() + 10.0;
		while (!xrtAtomic32Load(&Peer->Gate, XMEMORY_ACQUIRE)) { redisPeerRequire(xrtTimer() < End, "lookup fixture not released"); xrtThreadYield(); }
	}
	xnetaddrlist* Result = NULL;
	if (Peer->Flags & REDIS_DIAL_MISSING) xrtSetErrorInfo(XERR_NOT_FOUND, "fixture.dns", 1, "fixture host is missing");
	else {
		xnetaddr Addresses[2]; size_t Count = 0;
		if (Family != XNET_FAMILY_IPV4 && (Peer->Flags & REDIS_DIAL_DUAL)) {
			xrtNetAddrLoopback(&Addresses[Count++], XNET_FAMILY_IPV6, 0);
		}
		if (Family != XNET_FAMILY_IPV6) {
			Addresses[Count] = Peer->Init.Peer.Address; Addresses[Count++].Port = 0; /* Resolver results contain IPs; Dial supplies the port. */
		}
		Result = xrtNetAddrListCreate(Addresses, Count);
	}
	xrtAtomic32Store(&Peer->Exited, 1, XMEMORY_RELEASE); return Result;
}

static void redisDialPeerStartScript(redisdialpeer* Peer, unsigned Flags, size_t Requests, size_t HostLimit,
	const redisinitpeerstep* Steps, size_t Count)
{
	memset(Peer, 0, sizeof(*Peer)); Peer->Flags = Flags; redisInitPeerStart(&Peer->Init, Steps, Count);
	Peer->Refused = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, XNET_SOCKET_NONBLOCK);
	xnetaddr Local; xrtNetAddrLoopback(&Local, XNET_FAMILY_IPV4, 0);
	redisPeerRequire(Peer->Refused != NULL && xrtNetSocketBind(Peer->Refused, &Local) && xrtNetSocketLocal(Peer->Refused, &Peer->RefusedAddress), "owned bound/non-listening refusal socket");
	xnetresolverconfig Config; xrtNetResolverConfigInit(&Config); Config.Workers = 1;
	Config.Lookup = redisDialPeerLookup; Config.LookupData = Peer;
	if (Requests != 0) Config.RequestLimit = Requests;
	if (HostLimit != 0) Config.HostLimit = HostLimit;
	Peer->Resolver = xrtNetResolverCreate(&Config); redisPeerRequire(Peer->Resolver != NULL, "fixture Resolver");
}

static inline void redisDialPeerStart(redisdialpeer* Peer, unsigned Flags, size_t Requests, size_t HostLimit)
{
	static const redisinitpeerstep Hello = {{XRT_BYTES_LITERAL("HELLO"), XRT_BYTES_LITERAL("3")}, 2, XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY), false, false};
	redisDialPeerStartScript(Peer, Flags, Requests, HostLimit, &Hello, 1);
}

static inline xfuture* redisDialReady(xredisclient* Client)
{
	xfuture* Future = xredisClientReady(Client); redisPeerRequire(Future != NULL && xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "Dial readiness wait"); return Future;
}

static inline void redisDialClientClose(xredisclient* Client)
{
	xfuture* Closed = xredisClientClosed(Client); redisPeerRequire(xredisClientClose(Client) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "Dial close barrier");
	xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Client, &Info);
	redisPeerRequire(Info.State == XREDIS_CLIENT_CLOSED && Info.Requests == 0 && Info.CommandBytes == 0 && Info.InitBytes == 0 &&
		Info.Accepted == Info.Completed, "Dial request/wire retirement");
	xrtFutureDestroy(Closed); xredisClientRelease(Client);
}

static void redisDialPeerStop(redisdialpeer* Peer)
{
	xrtAtomic32Store(&Peer->Gate, 1, XMEMORY_RELEASE);
	redisPeerRequire(xrtNetResolverDestroy(Peer->Resolver), "fixture Resolver join");
	redisPeerStop(&Peer->Init.Peer); redisPeerRequire(xrtNetSocketClose(Peer->Refused), "refusal socket close");
}
#endif
