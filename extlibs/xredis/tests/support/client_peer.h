#ifndef XREDIS_CLIENT_PEER_H
#define XREDIS_CLIENT_PEER_H
/* Local deterministic protocol peer for examples/tests/workload smoke. This is
 * not a Redis server, interoperability comparator or production benchmark. */
#include <xredis.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct redispeer {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Stream;
	xredisreplyreader* Reader;
	xnetaddr Address;
	xatomic32 Accepted, Commands, Closed, ListenerClosed, Released;
	xatomic32 SyncChecked;
	xredisclient* Client;
	bool Blocked;
	unsigned char Pending[65536];
	size_t PendingBytes;
} redispeer;

static void redisPeerRequire(bool Condition, cstr Message)
{
	if (!Condition) { fprintf(stderr, "[FAIL] redis local peer: %s\n", Message); exit(1); }
}

static void redisPeerWait(xatomic32* Value, uint32 Minimum)
{
	double End = xrtTimer() + 10.0;
	while (xrtAtomic32Load(Value, XMEMORY_ACQUIRE) < Minimum) {
		redisPeerRequire(xrtTimer() < End, "atomic wait expired"); xrtThreadYield();
	}
}

static void redisPeerSend(redispeer* Peer, const void* Data, size_t Size)
{
	if (Peer->Blocked) {
		redisPeerRequire(Size <= sizeof(Peer->Pending) - Peer->PendingBytes, "pending reply cap");
		memcpy(Peer->Pending + Peer->PendingBytes, Data, Size); Peer->PendingBytes += Size;
	} else redisPeerRequire(xrtNetStreamSend(Peer->Stream, Data, Size) == XNET_RESULT_OK, "response send");
}

static bool redisPeerName(xredisreplyvalue Value, cstr Name)
{ return Value.Data.Size == strlen(Name) && memcmp(Value.Data.Data, Name, Value.Data.Size) == 0; }

static void redisPeerCommand(redispeer* Peer, xredisreply* Reply)
{
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisReplyInfo(Reply, &Info);
	xredisreplyvalue Root, Command, Value; xredisReplyGet(Reply, Info.Root, &Root);
	redisPeerRequire(Root.Type == XREDIS_REPLY_ARRAY && Root.ChildCount != 0, "command frame");
	xredisReplyGet(Reply, Root.FirstChild, &Command);
	if (redisPeerName(Command, "HOLD")) { Peer->Blocked = true; redisPeerSend(Peer, "+held\r\n", 7); }
	else if (redisPeerName(Command, "ERROR")) redisPeerSend(Peer, "-ERR deliberate\r\n", 17);
	else if (redisPeerName(Command, "MALFORMED")) redisPeerSend(Peer, "$3\r\nabc\rX", 9);
	else if (redisPeerName(Command, "PARTIAL")) { redisPeerSend(Peer, "$5\r\nab", 6); xrtNetStreamClose(Peer->Stream); }
	else if (redisPeerName(Command, "DROP")) xrtNetStreamAbort(Peer->Stream);
	else if (redisPeerName(Command, "EXTRA")) redisPeerSend(Peer, "+first\r\n+extra\r\n", 16);
	else if (redisPeerName(Command, "PUSH")) redisPeerSend(Peer, "|0\r\n>2\r\n+notice\r\n:9\r\n+after\r\n", 29);
	else if (redisPeerName(Command, "FLOOD")) redisPeerSend(Peer, ">1\r\n+one\r\n>1\r\n+two\r\n+done\r\n", 27);
	else if (redisPeerName(Command, "ID")) {
		xredisReplyGet(Reply, Command.Next, &Value); redisPeerSend(Peer, ":", 1);
		redisPeerSend(Peer, Value.Data.Data, Value.Data.Size); redisPeerSend(Peer, "\r\n", 2);
	} else if (redisPeerName(Command, "ECHO")) {
		xredisReplyGet(Reply, Command.Next, &Value); char Header[40]; int Count = snprintf(Header, sizeof(Header), "$%zu\r\n", Value.Data.Size);
		redisPeerSend(Peer, Header, (size_t)Count); redisPeerSend(Peer, Value.Data.Data, Value.Data.Size); redisPeerSend(Peer, "\r\n", 2);
	} else redisPeerSend(Peer, "+PONG\r\n", 7);
	(void)xrtAtomic32FetchAdd(&Peer->Commands, 1, XMEMORY_RELEASE);
}

static void redisPeerRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data)
{
	(void)Stream; redispeer* Peer = (redispeer*)Data; xnetspan Span;
	while (xrtNetBufFront(Buffer, &Span)) {
		xredisreply* Reply = NULL; size_t Used = 0;
		xredisreplystatus Status = xredisReplyReaderFeed(Peer->Reader, (xbytesview){Span.Data, Span.Size}, &Used, &Reply);
		redisPeerRequire(Status != XREDIS_REPLY_ERROR && Used <= Span.Size, "command decode"); xrtNetBufConsume(Buffer, Used);
		if (Status == XREDIS_REPLY_READY) { redisPeerCommand(Peer, Reply); xredisReplyRelease(Reply); }
		else redisPeerRequire(Used != 0, "command progress");
	}
}

static void redisPeerEnd(xnetstream* Stream, ptr Data)
{ (void)Data; xrtNetStreamAbort(Stream); }

static void redisPeerClosed(xnetstream* Stream, xnetresult Result, const xerror* Error, ptr Data)
{
	(void)Stream; (void)Result; (void)Error; redispeer* Peer = (redispeer*)Data;
	xrtAtomic32Store(&Peer->Closed, 1, XMEMORY_RELEASE);
}

static const xnetstreamevents redisPeerEvents = {NULL, redisPeerRead, redisPeerEnd, NULL, NULL, NULL, redisPeerClosed};

static bool redisPeerAccept(xnetlistener* Listener, xnetstream* Stream, ptr Data)
{
	(void)Listener; redispeer* Peer = (redispeer*)Data;
	redisPeerRequire(xrtNetStreamSetEvents(Stream, &redisPeerEvents, Peer), "accepted events"); Peer->Stream = Stream;
	xrtAtomic32Store(&Peer->Accepted, 1, XMEMORY_RELEASE); return true;
}

static void redisPeerListenerClose(xnetlistener* Listener, ptr Data)
{
	(void)Listener; redispeer* Peer = (redispeer*)Data; xrtAtomic32Store(&Peer->ListenerClosed, 1, XMEMORY_RELEASE);
}

static const xnetlistenerevents redisPeerListenerEvents = {redisPeerAccept, NULL, redisPeerListenerClose};

static void redisPeerStart(redispeer* Peer)
{
	memset(Peer, 0, sizeof(*Peer));
	xredisreplyreaderconfig Reader; xredisReplyReaderConfigInit(&Reader); Reader.Reader.Protocol = XREDIS_RESP2;
	redisPeerRequire(xredisReplyReaderCreate(&Reader, &Peer->Reader), "peer parser");
	xnetengineconfig Engine; xrtNetEngineConfigInit(&Engine); Engine.Workers = 2;
#if defined(REDIS_PEER_BACKEND)
	Engine.Backend = REDIS_PEER_BACKEND;
#endif
	Peer->Engine = xrtNetEngineCreate(&Engine); redisPeerRequire(Peer->Engine != NULL && xrtNetEngineStart(Peer->Engine), "engine start");
	static bool Reported = false;
	if (!Reported) {
		xnetportbackendinfo Backend; redisPeerRequire(xrtNetEngineBackendInfo(Peer->Engine, 0, &Backend), "backend snapshot");
		printf("redis local peer backend: %s\n", Backend.Name); Reported = true;
	}
	xnetlistenconfig Listener; xrtNetListenConfigInit(&Listener);
	xrtNetAddrLoopback(&Listener.Address, XNET_FAMILY_IPV4, 0); Listener.Affinity = 1; Listener.Distribution = XNET_ACCEPT_LOCAL;
	Peer->Listener = xrtNetListen(Peer->Engine, &Listener, &redisPeerListenerEvents, NULL, Peer);
	redisPeerRequire(Peer->Listener != NULL && xrtNetListenerLocal(Peer->Listener, &Peer->Address), "local listener");
}

static void redisPeerFlush(xnetworker* Worker, ptr Data)
{
	(void)Worker; redispeer* Peer = (redispeer*)Data;
	Peer->Blocked = false;
	if (Peer->PendingBytes != 0) {
		redisPeerRequire(xrtNetStreamSend(Peer->Stream, Peer->Pending, Peer->PendingBytes) == XNET_RESULT_OK, "flush replies");
		Peer->PendingBytes = 0;
	}
	xrtAtomic32Store(&Peer->Released, 1, XMEMORY_RELEASE);
}

static inline void redisPeerReleaseBlocked(redispeer* Peer)
{
	redisPeerWait(&Peer->Accepted, 1); xrtAtomic32Store(&Peer->Released, 0, XMEMORY_RELAXED);
	redisPeerRequire(xrtNetEnginePost(Peer->Engine, xrtNetWorkerIndex(xrtNetStreamWorker(Peer->Stream)), redisPeerFlush, Peer), "flush post");
	redisPeerWait(&Peer->Released, 1);
}

static void redisPeerStop(redispeer* Peer)
{
	redisPeerRequire(xrtNetListenerClose(Peer->Listener), "listener close"); redisPeerWait(&Peer->ListenerClosed, 1);
	if (Peer->Stream != NULL) { xrtNetStreamAbort(Peer->Stream); redisPeerWait(&Peer->Closed, 1); }
	redisPeerRequire(xrtNetEngineStop(Peer->Engine), "engine stop");
	xrtNetStreamDestroy(Peer->Stream); xrtNetListenerDestroy(Peer->Listener); xredisReplyReaderDestroy(Peer->Reader);
	redisPeerRequire(xrtNetEngineDestroy(Peer->Engine), "engine destroy");
}
#endif
