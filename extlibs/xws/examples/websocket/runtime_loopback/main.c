#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xws.h>

/* This example starts after an out-of-band agreement on the protocol and
 * permessage-deflate settings. HTTP upgrade belongs to xws-client/server. */
typedef struct demo demo;

typedef struct endpoint {
	demo* Demo;
	xwsrole Role;
	xwsconn* Connection; /* Creation reference, protected by Demo.Lock. */
	xatomic32 Messages;
	xwsopcode Opcode;
	bool Compressed;
	size_t Size;
	uint8 Data[64];
} endpoint;

struct demo {
	xnetengine* Engine;
	xspinlock Lock;
	bool Stopping;
	xatomic32 Failed;
	xatomic32 Closed;
	xatomic32 Released;
	xatomic32 Pong;
	endpoint Client;
	endpoint Server;
};

static const char* const Messages[] = {
	"copy hello", "owned hello", "compressed hello"
};

static void fail(demo* pDemo, const char* sMessage)
{
	xrtAtomic32Store(&pDemo->Failed, 1, XMEMORY_RELEASE);
	fprintf(stderr, "[FAIL] xws loopback: %s\n", sMessage);
}

static bool waitCounter(demo* pDemo, const xatomic32* pCounter, uint32 iValue)
{
	double Deadline = xrtTimer() + 10.0;
	while ( xrtAtomic32Load(pCounter, XMEMORY_ACQUIRE) < iValue ) {
		if ( xrtAtomic32Load(&pDemo->Failed, XMEMORY_ACQUIRE) ) {
			return false;
		}
		if ( xrtTimer() >= Deadline ) {
			fail(pDemo, "timed out waiting for a callback");
			return false;
		}
		xrtSleep(1);
	}
	return !xrtAtomic32Load(&pDemo->Failed, XMEMORY_ACQUIRE);
}

static bool resolved(demo* pDemo, xfuture* pFuture)
{
	bool bReady = (pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, INT64_C(10000)) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED);
	if ( !bReady ) {
		fail(pDemo, "Future did not resolve");
	}
	xrtFutureDestroy(pFuture);
	return bReady;
}

/* Both accepted and rejected Ref submissions have one clear owner. */
static void releasePayload(ptr pData, cbytes pBytes, size_t iSize)
{
	demo* pDemo = (demo*)pData;
	(void)iSize;
	xrtFree((ptr)pBytes);
	(void)xrtAtomic32FetchAdd(&pDemo->Released, 1, XMEMORY_RELEASE);
}

static bool ownedFragment(xwswriter* pWriter, demo* pDemo,
	xbytesview Data, bool bFinal)
{
	bytes pCopy = (bytes)xrtMalloc(Data.Size);
	xnetref Ref;
	xnetresult Result;
	if ( pCopy == NULL ) {
		return false;
	}
	memcpy(pCopy, Data.Data, Data.Size);
	Ref = (xnetref) { pCopy, Data.Size, releasePayload, pDemo };
	Result = bFinal ? xrtWsWriterFinishRef(pWriter, &Ref) :
		xrtWsWriterWriteRef(pWriter, &Ref);
	if ( Result != XNET_RESULT_OK ) {
		xrtFree(pCopy); /* The Writer did not adopt this fragment. */
	}
	return Result == XNET_RESULT_OK;
}

static void messageBegin(xwsconn* pConnection,
	const xwsmessageinfo* pInfo, ptr pData)
{
	endpoint* pEndpoint = (endpoint*)pData;
	(void)pConnection;
	pEndpoint->Size = 0;
	pEndpoint->Opcode = (xwsopcode)pInfo->Opcode;
	pEndpoint->Compressed = (pInfo->Flags & XWS_MESSAGE_COMPRESSED) != 0;
}

static void messageData(xwsconn* pConnection, xbytesview Data, ptr pData)
{
	endpoint* pEndpoint = (endpoint*)pData;
	(void)pConnection;
	if ( Data.Size > sizeof(pEndpoint->Data) - pEndpoint->Size ) {
		fail(pEndpoint->Demo, "message exceeds the application buffer");
		return;
	}
	if ( Data.Size != 0 ) {
		memcpy(pEndpoint->Data + pEndpoint->Size, Data.Data, Data.Size);
	}
	pEndpoint->Size += Data.Size;
}

/* The callback borrows Data. The Writer copies or adopts each fragment before
 * returning; no borrowed receive view is retained by the application. */
static void messageEnd(xwsconn* pConnection, ptr pData)
{
	endpoint* pEndpoint = (endpoint*)pData;
	demo* pDemo = pEndpoint->Demo;
	uint32 iMessage = xrtAtomic32Load(&pEndpoint->Messages, XMEMORY_ACQUIRE);
	if ( (iMessage >= 3u) || (pEndpoint->Opcode != XWS_OPCODE_TEXT) ||
		(pEndpoint->Size != strlen(Messages[iMessage])) ||
		(memcmp(pEndpoint->Data, Messages[iMessage], pEndpoint->Size) != 0) ||
		(pEndpoint->Compressed != (iMessage == 2u)) ) {
		fail(pDemo, "received message or compression flag mismatch");
		return;
	}
	if ( pEndpoint->Role == XWS_ROLE_SERVER ) {
		xwswriter* pWriter = pEndpoint->Compressed ?
			xrtWsConnBeginTextCompressed(pConnection) :
			xrtWsConnBeginText(pConnection);
		size_t iHalf = pEndpoint->Size / 2u;
		xbytesview First = { pEndpoint->Data, iHalf };
		xbytesview Last = { pEndpoint->Data + iHalf, pEndpoint->Size - iHalf };
		bool bSent;
		if ( pWriter == NULL ) {
			fail(pDemo, "could not begin an echo Writer");
			return;
		}
		bSent = iMessage == 1u ?
			(ownedFragment(pWriter, pDemo, First, false) &&
			 ownedFragment(pWriter, pDemo, Last, true)) :
			((xrtWsWriterWrite(pWriter, First) == XNET_RESULT_OK) &&
			 (xrtWsWriterFinish(pWriter, Last) == XNET_RESULT_OK));
		xrtWsWriterDestroy(pWriter);
		if ( !bSent ) {
			fail(pDemo, "echo fragments were not accepted");
			return;
		}
	}
	(void)xrtAtomic32FetchAdd(&pEndpoint->Messages, 1, XMEMORY_RELEASE);
}

static void pong(xwsconn* pConnection, xbytesview Payload, ptr pData)
{
	endpoint* pEndpoint = (endpoint*)pData;
	(void)pConnection;
	if ( (pEndpoint->Role != XWS_ROLE_CLIENT) || (Payload.Size != 5u) ||
		(memcmp(Payload.Data, "probe", 5u) != 0) ) {
		fail(pEndpoint->Demo, "Pong payload mismatch");
		return;
	}
	(void)xrtAtomic32FetchAdd(&pEndpoint->Demo->Pong, 1, XMEMORY_RELEASE);
}

static void connectionError(xwsconn* pConnection,
	const xerror* pError, ptr pData)
{
	endpoint* pEndpoint = (endpoint*)pData;
	(void)pConnection;
	(void)pError;
	fail(pEndpoint->Demo, "connection error callback");
}

static void connectionClose(xwsconn* pConnection,
	const xwsconnclose* pClose, ptr pData)
{
	endpoint* pEndpoint = (endpoint*)pData;
	(void)pConnection;
	if ( ((pClose->Flags & XWS_CONN_CLOSE_CLEAN) == 0) ||
		(pClose->Transport != XNET_RESULT_OK) ||
		(pClose->LocalCode != XWS_CLOSE_NORMAL) ||
		(pClose->RemoteCode != XWS_CLOSE_NORMAL) ||
		(pClose->Reason.Size != 4u) ||
		(memcmp(pClose->Reason.Data, "done", 4u) != 0) ) {
		fail(pEndpoint->Demo, "close handshake was not clean");
	}
	(void)xrtAtomic32FetchAdd(&pEndpoint->Demo->Closed, 1, XMEMORY_RELEASE);
}

static xwsconn* attach(xnetstream* pStream, endpoint* pEndpoint)
{
	xwsconnconfig Config;
	xwsconnevents Events = { 0 };
	xrtWsConnConfigInit(&Config);
	Config.Role = pEndpoint->Role;
	Config.Protocol = XRT_STR_LITERAL("loopback.v1");
	xrtWsDeflateInit(&Config.Deflate);
	Config.DeflateEnabled = true;
	Events.MessageBegin = messageBegin;
	Events.MessageData = messageData;
	Events.MessageEnd = messageEnd;
	Events.Pong = pong;
	Events.Error = connectionError;
	Events.Close = connectionClose;
	return xrtWsConnAttach(pStream, &Config, &Events, pEndpoint);
}

/* Serialize publication with cleanup, including a late Open/Accept callback.
 * No network API or user callback is invoked while the application lock is held. */
static void publish(endpoint* pEndpoint, xwsconn* pConnection)
{
	demo* pDemo = pEndpoint->Demo;
	bool bStopping;
	(void)xrtSpinLock(&pDemo->Lock);
	bStopping = pDemo->Stopping;
	if ( !bStopping ) {
		pEndpoint->Connection = pConnection;
	}
	(void)xrtSpinUnlock(&pDemo->Lock);
	if ( bStopping ) {
		(void)xrtWsConnAbort(pConnection);
		xrtWsConnDestroy(pConnection);
	}
}

static bool accepted(xnetlistener* pListener, xnetstream* pStream, ptr pData)
{
	demo* pDemo = (demo*)pData;
	xwsconn* pConnection;
	(void)pListener;
	pConnection = attach(pStream, &pDemo->Server);
	if ( pConnection == NULL ) {
		fail(pDemo, "server Attach failed");
		return false; /* Listener retains responsibility for the rejected Stream. */
	}
	publish(&pDemo->Server, pConnection);
	return true;
}

static void opened(xnetstream* pStream, ptr pData)
{
	demo* pDemo = (demo*)pData;
	xnetstream* pOwned = xrtNetStreamRef(pStream);
	xwsconn* pConnection = pOwned != NULL ? attach(pOwned, &pDemo->Client) : NULL;
	if ( pConnection == NULL ) {
		xrtNetStreamDestroy(pOwned);
		fail(pDemo, "client Attach failed");
		return;
	}
	/* Attach adopts only this extra reference; main still owns Connect's result. */
	publish(&pDemo->Client, pConnection);
}

static void listenerError(xnetlistener* pListener,
	const xerror* pError, ptr pData)
{
	(void)pListener;
	(void)pError;
	fail((demo*)pData, "listener error callback");
}

static void streamClosed(xnetstream* pStream, xnetresult Result,
	const xerror* pError, ptr pData)
{
	(void)pStream;
	(void)Result;
	(void)pError;
	fail((demo*)pData, "TCP closed before Attach");
}

int main(void)
{
	demo Demo;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig StreamConfig;
	xnetlistenerevents ListenerEvents = { 0 };
	xnetstreamevents StreamEvents = { 0 };
	xnetlistener* pListener = NULL;
	xnetstream* pStream = NULL;
	xwsconn* pClient = NULL;
	xwsconn* pServer = NULL;
	xfuture* pClosed = NULL;
	xnetaddr Address;
	xnetenginestats Stats;
	double Deadline;
	bool bSuccess = false;
	memset(&Demo, 0, sizeof(Demo));
	Demo.Client.Demo = Demo.Server.Demo = &Demo;
	Demo.Client.Role = XWS_ROLE_CLIENT;
	Demo.Server.Role = XWS_ROLE_SERVER;
	xrtAtomic32Init(&Demo.Failed, 0);
	xrtAtomic32Init(&Demo.Closed, 0);
	xrtAtomic32Init(&Demo.Released, 0);
	xrtAtomic32Init(&Demo.Pong, 0);
	xrtAtomic32Init(&Demo.Client.Messages, 0);
	xrtAtomic32Init(&Demo.Server.Messages, 0);
	if ( !xrtSpinInit(&Demo.Lock) ) {
		return 1;
	}
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	Demo.Engine = xrtNetEngineCreate(&EngineConfig);
	if ( (Demo.Engine == NULL) || !xrtNetEngineStart(Demo.Engine) ) {
		fail(&Demo, "could not start the network Engine");
		goto Cleanup;
	}
	xrtNetListenConfigInit(&ListenConfig);
	if ( !xrtNetAddrLoopback(&ListenConfig.Address, XNET_FAMILY_IPV4, 0) ) {
		fail(&Demo, "could not prepare the loopback address");
		goto Cleanup;
	}
	ListenerEvents.Accept = accepted;
	ListenerEvents.Error = listenerError;
	pListener = xrtNetListen(Demo.Engine, &ListenConfig, &ListenerEvents, NULL, &Demo);
	if ( (pListener == NULL) || !xrtNetListenerLocal(pListener, &Address) ) {
		fail(&Demo, "could not listen on the loopback address");
		goto Cleanup;
	}
	xrtNetStreamConfigInit(&StreamConfig);
	StreamEvents.Open = opened;
	StreamEvents.Close = streamClosed;
	pStream = xrtNetStreamConnect(Demo.Engine, &Address, 1,
		&StreamConfig, &StreamEvents, &Demo);
	Deadline = xrtTimer() + 10.0;
	while ( pStream != NULL ) {
		(void)xrtSpinLock(&Demo.Lock);
		pClient = Demo.Client.Connection;
		pServer = Demo.Server.Connection;
		(void)xrtSpinUnlock(&Demo.Lock);
		if ( (pClient != NULL) && (pServer != NULL) ) {
			break;
		}
		if ( xrtAtomic32Load(&Demo.Failed, XMEMORY_ACQUIRE) ||
			(xrtTimer() >= Deadline) ) {
			break;
		}
		xrtSleep(1);
	}
	if ( (pClient == NULL) || (pServer == NULL) ) {
		fail(&Demo, "connections were not published");
		goto Cleanup;
	}
	if ( !resolved(&Demo, xrtWsConnTextAsync(pClient, XRT_STR_LITERAL("copy hello"))) ||
		!waitCounter(&Demo, &Demo.Client.Messages, 1) ) {
		goto Cleanup;
	}
	{
		static const char Text[] = "owned hello";
		bytes pCopy = (bytes)xrtMalloc(sizeof(Text) - 1u);
		xnetref Ref;
		xfuture* pSent;
		if ( pCopy == NULL ) {
			fail(&Demo, "could not allocate the owned message");
			goto Cleanup;
		}
		memcpy(pCopy, Text, sizeof(Text) - 1u);
		Ref = (xnetref) { pCopy, sizeof(Text) - 1u, releasePayload, &Demo };
		pSent = xrtWsConnTextRefAsync(pClient, &Ref);
		if ( pSent == NULL ) {
			xrtFree(pCopy); /* A failed submission never adopted this Ref. */
		}
		if ( !resolved(&Demo, pSent) ||
			!waitCounter(&Demo, &Demo.Client.Messages, 2) ) {
			goto Cleanup;
		}
	}
	if ( !resolved(&Demo, xrtWsConnTextCompressedAsync(pClient,
		XRT_STR_LITERAL("compressed hello"))) ||
		!waitCounter(&Demo, &Demo.Client.Messages, 3) ||
		!waitCounter(&Demo, &Demo.Released, 3) ||
		!resolved(&Demo, xrtWsConnPingAsync(pClient, XRT_BYTES_LITERAL("probe"))) ||
		!waitCounter(&Demo, &Demo.Pong, 1) ) {
		goto Cleanup;
	}
	pClosed = xrtWsConnWaitAsync(pClient, XWS_CONN_WAIT_CLOSE);
	if ( !resolved(&Demo, xrtWsConnCloseAsync(pClient,
		XWS_CLOSE_NORMAL, XRT_STR_LITERAL("done"))) ) {
		goto Cleanup;
	}
	{
		xfuture* pWait = pClosed;
		pClosed = NULL;
		if ( !resolved(&Demo, pWait) || !waitCounter(&Demo, &Demo.Closed, 2) ) {
			goto Cleanup;
		}
	}
	bSuccess = true;

Cleanup:
	xrtFutureDestroy(pClosed);
	(void)xrtSpinLock(&Demo.Lock);
	Demo.Stopping = true;
	pClient = Demo.Client.Connection;
	pServer = Demo.Server.Connection;
	Demo.Client.Connection = Demo.Server.Connection = NULL;
	(void)xrtSpinUnlock(&Demo.Lock);
	(void)xrtWsConnAbort(pClient);
	(void)xrtWsConnAbort(pServer);
	xrtWsConnDestroy(pClient);
	xrtWsConnDestroy(pServer);
	if ( pStream != NULL ) {
		(void)xrtNetStreamAbort(pStream);
		xrtNetStreamDestroy(pStream);
	}
	if ( pListener != NULL ) {
		(void)xrtNetListenerClose(pListener);
		xrtNetListenerDestroy(pListener);
	}
	/* A Close callback can precede its final native releases. Join before reading
 * final counters or releasing the callback context and application lock. */
	Deadline = xrtTimer() + 10.0;
	while ( Demo.Engine != NULL ) {
		xnetretireresult Result = xrtNetEngineTryStop(Demo.Engine);
		if ( Result == XNET_RETIRE_READY ) {
			break;
		}
		if ( (Result == XNET_RETIRE_ERROR) || (xrtTimer() >= Deadline) ) {
			fprintf(stderr, "[FAIL] xws loopback: Engine did not retire\n");
			exit(1); /* Keep the callback context alive until process termination. */
		}
		xrtSleep(1);
	}
	if ( Demo.Engine != NULL ) {
		if ( !xrtNetEngineStats(Demo.Engine, &Stats) ||
			(Stats.LiveObjects != 0) || (Stats.WaitErrors != 0) ||
			(Stats.WakeErrors != 0) ) {
			fail(&Demo, "final Engine statistics failed");
		}
		if ( !xrtNetEngineDestroy(Demo.Engine) ) {
			fail(&Demo, "Engine Destroy failed");
		}
	}
	(void)xrtSpinUnit(&Demo.Lock);
	xrtClearError();
	if ( !bSuccess || xrtAtomic32Load(&Demo.Failed, XMEMORY_ACQUIRE) ||
		(xrtAtomic32Load(&Demo.Client.Messages, XMEMORY_ACQUIRE) != 3u) ||
		(xrtAtomic32Load(&Demo.Server.Messages, XMEMORY_ACQUIRE) != 3u) ||
		(xrtAtomic32Load(&Demo.Released, XMEMORY_ACQUIRE) != 3u) ||
		(xrtAtomic32Load(&Demo.Closed, XMEMORY_ACQUIRE) != 2u) ||
		(xrtAtomic32Load(&Demo.Pong, XMEMORY_ACQUIRE) != 1u) ) {
		return 1;
	}
	puts("[PASS] xws loopback: copy/ref/compressed Writer echoes, Pong, clean Close; releases=3 live=0");
	return 0;
}
