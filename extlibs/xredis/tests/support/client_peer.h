#ifndef XREDIS_CLIENT_PEER_H
#define XREDIS_CLIENT_PEER_H
/* Local deterministic protocol peer for examples/tests/workload smoke. This is
 * not a Redis server, interoperability comparator or production benchmark. */
#include <xredis.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct redispeer redispeer;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
typedef struct redistlsoptions {
	xtlsserverconfig Session;
	xtlsstreamconfig Stream;
	bool HoldHandshake;
} redistlsoptions;
#endif
typedef struct redispeerhook {
	bool (*Command)(redispeer* Peer, xredisreply* Reply, ptr Data);
	ptr Data;
} redispeerhook;

struct redispeer {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Stream;
	xredisreplyreader* Reader;
	xnetaddr Address;
	xatomic32 Accepted, Commands, Closed, ListenerClosed, Released;
	xatomic32 SyncChecked;
	/* Publish one immutable fixture descriptor before admitting a connection. */
	xatomicptr Hook;
	xredisclient* Client;
	bool Blocked;
	unsigned char Pending[65536];
	size_t PendingBytes;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	xatomicptr TlsOptions;
	xtlsstream* Secure;
	xatomic32 PlainBytes;
	bool TlsFlushing;
#endif
};

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

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static void redisPeerTlsFlush(redispeer* Peer)
{
	if (Peer->TlsFlushing || Peer->Blocked) return;
	Peer->TlsFlushing = true;
	while (Peer->PendingBytes != 0) {
		size_t Written = 0; xtlsresult Result = xrtTlsStreamSend(Peer->Secure, Peer->Pending, Peer->PendingBytes, &Written);
		redisPeerRequire(Result == XTLS_OK || Result == XTLS_AGAIN, "fixture TLS response send");
		memmove(Peer->Pending, Peer->Pending + Written, Peer->PendingBytes - Written); Peer->PendingBytes -= Written;
		if (Written == 0) break;
	}
	Peer->TlsFlushing = false;
}
#endif

static void redisPeerSend(redispeer* Peer, const void* Data, size_t Size)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Peer->Secure != NULL) {
		redisPeerRequire(Size <= sizeof(Peer->Pending) - Peer->PendingBytes, "TLS pending reply cap");
		memcpy(Peer->Pending + Peer->PendingBytes, Data, Size); Peer->PendingBytes += Size; redisPeerTlsFlush(Peer); return;
	}
#endif
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
	redispeerhook* Hook = (redispeerhook*)xrtAtomicPtrLoad(&Peer->Hook, XMEMORY_ACQUIRE);
	if (Hook != NULL && Hook->Command(Peer, Reply, Hook->Data)) { /* Fixture supplied the response. */ }
	else if (redisPeerName(Command, "HOLD")) { Peer->Blocked = true; redisPeerSend(Peer, "+held\r\n", 7); }
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
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	const redistlsoptions* TLS = (const redistlsoptions*)xrtAtomicPtrLoad(&Peer->TlsOptions, XMEMORY_ACQUIRE);
	if (TLS != NULL && TLS->HoldHandshake) { xrtNetBufConsume(Buffer, xrtNetBufSize(Buffer)); return; }
#endif
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

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static void redisPeerTlsRead(xtlsstream* Stream, const xnetbuf* Buffer, ptr Data)
{
	redispeer* Peer = (redispeer*)Data; xnetspan Span;
	while (xrtNetBufFront(Buffer, &Span)) {
		xredisreply* Reply = NULL; size_t Used = 0;
		xredisreplystatus Status = xredisReplyReaderFeed(Peer->Reader, (xbytesview){Span.Data, Span.Size}, &Used, &Reply);
		redisPeerRequire(Status != XREDIS_REPLY_ERROR && Used <= Span.Size && xrtTlsStreamConsume(Stream, Used), "TLS fixture command decode/consume");
		(void)xrtAtomic32FetchAdd(&Peer->PlainBytes, (uint32)Used, XMEMORY_RELEASE);
		if (Status == XREDIS_REPLY_READY) { redisPeerCommand(Peer, Reply); xredisReplyRelease(Reply); }
		else redisPeerRequire(Used != 0, "TLS fixture command progress");
	}
}
static void redisPeerTlsWritable(xtlsstream* Stream, ptr Data)
{ (void)Stream; redisPeerTlsFlush((redispeer*)Data); }
static void redisPeerTlsEnd(xtlsstream* Stream, ptr Data)
{ (void)Data; (void)xrtTlsStreamClose(Stream); }
static void redisPeerTlsClosed(xtlsstream* Stream, xnetresult Result, const xerror* Error, ptr Data)
{ (void)Stream; redisPeerClosed(NULL, Result, Error, Data); }
static const xtlsstreamevents redisPeerTlsEvents = {NULL, redisPeerTlsRead, redisPeerTlsEnd, redisPeerTlsWritable, redisPeerTlsWritable, redisPeerTlsClosed, NULL};
#endif

static bool redisPeerAccept(xnetlistener* Listener, xnetstream* Stream, ptr Data)
{
	(void)Listener; redispeer* Peer = (redispeer*)Data;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	const redistlsoptions* TLS = (const redistlsoptions*)xrtAtomicPtrLoad(&Peer->TlsOptions, XMEMORY_ACQUIRE);
	if (TLS != NULL && !TLS->HoldHandshake) {
		redisPeerRequire(xrtTlsStreamAccept(Stream, &TLS->Session, &TLS->Stream, &redisPeerTlsEvents, Peer, &Peer->Secure), "fixture TLS accept");
		Peer->Stream = Stream; /* Borrowed from Secure. */
		xrtAtomic32Store(&Peer->Accepted, 1, XMEMORY_RELEASE); return true;
	}
#endif
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
#if defined(REDIS_PEER_NODE_CACHE_BYTES)
	Engine.NodeCacheBytes = REDIS_PEER_NODE_CACHE_BYTES;
#endif
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
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		if (Peer->Secure != NULL) redisPeerTlsFlush(Peer); else
#endif
		{
		redisPeerRequire(xrtNetStreamSend(Peer->Stream, Peer->Pending, Peer->PendingBytes) == XNET_RESULT_OK, "flush replies");
		Peer->PendingBytes = 0;
		}
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
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Peer->Secure != NULL) xrtTlsStreamDestroy(Peer->Secure); else
#endif
	xrtNetStreamDestroy(Peer->Stream);
	xrtNetListenerDestroy(Peer->Listener); xredisReplyReaderDestroy(Peer->Reader);
	redisPeerRequire(xrtNetEngineDestroy(Peer->Engine), "engine destroy");
}
#endif
