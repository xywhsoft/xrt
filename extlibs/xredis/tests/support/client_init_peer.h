#ifndef XREDIS_CLIENT_INIT_PEER_H
#define XREDIS_CLIENT_INIT_PEER_H
#include "client_peer.h"

/* Exact command/response script for initialization lifecycle tests and the
 * runnable example. Synthetic credentials only; no external Redis claim. */
#define REDIS_INIT_HELLO_REPLY "%4\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n+extension\r\n$3\r\na\0b\r\n"
typedef struct redisinitpeerstep {
	xbytesview Arguments[7];
	size_t Count;
	xbytesview Response;
	bool Hold, Abort;
} redisinitpeerstep;

typedef struct redisinitpeer {
	redispeer Peer;
	redispeerhook Hook;
	const redisinitpeerstep* Steps;
	size_t Count, Index;
	xatomic32 StepsSeen;
} redisinitpeer;

static bool redisInitPeerCommand(redispeer* Peer, xredisreply* Reply, ptr Data)
{
	redisinitpeer* Init = (redisinitpeer*)Data;
	if (Init->Index == Init->Count) return false;
	const redisinitpeerstep* Step = &Init->Steps[Init->Index];
	xredisreplyvalue Root; redisPeerRequire(xredisReplyGet(Reply, 0, &Root) && Root.ChildCount == Step->Count, "setup argument count/order");
	size_t Node = Root.FirstChild;
	for (size_t Index = 0; Index < Step->Count; ++Index) {
		xredisreplyvalue Arg; redisPeerRequire(xredisReplyGet(Reply, Node, &Arg), "setup argument node");
		redisPeerRequire(Arg.Type == XREDIS_REPLY_BULK_STRING && Arg.Data.Size == Step->Arguments[Index].Size &&
			(Arg.Data.Size == 0 || memcmp(Arg.Data.Data, Step->Arguments[Index].Data, Arg.Data.Size) == 0), "setup argument bytes/order");
		Node = Arg.Next;
	}
	++Init->Index;
	if (Step->Abort) xrtNetStreamAbort(Peer->Stream);
	else {
		if (Step->Hold) Peer->Blocked = true;
		if (Step->Response.Size != 0) redisPeerSend(Peer, Step->Response.Data, Step->Response.Size);
	}
	xrtAtomic32Store(&Init->StepsSeen, (uint32)Init->Index, XMEMORY_RELEASE); return true;
}

static void redisInitPeerStart(redisinitpeer* Init, const redisinitpeerstep* Steps, size_t Count)
{
	memset(Init, 0, sizeof(*Init)); Init->Steps = Steps; Init->Count = Count;
	Init->Hook = (redispeerhook){redisInitPeerCommand, Init}; redisPeerStart(&Init->Peer);
	xrtAtomicPtrStore(&Init->Peer.Hook, &Init->Hook, XMEMORY_RELEASE);
}

static inline void redisInitPeerStepWait(redisinitpeer* Init, uint32 Count)
{
	redisPeerWait(&Init->StepsSeen, Count);
	/* The hook publishes its step before returning to the outer command
	 * dispatcher. Wait for that dispatcher too before observing its counter. */
	redisPeerWait(&Init->Peer.Commands, Count);
}
#endif
