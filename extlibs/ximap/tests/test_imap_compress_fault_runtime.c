#include <xrt/detail/wait.h>
#include "test.h"
#include "test_tls.h"
#include "../src/internal/xrt_imap_client.h"

typedef enum testcompressfault {
	TEST_COMPRESS_WRITE_CANCEL,
	TEST_COMPRESS_WRITE_TIMEOUT,
	TEST_COMPRESS_READ_CANCEL,
	TEST_COMPRESS_READ_TIMEOUT,
	TEST_COMPRESS_INVALID,
	TEST_COMPRESS_TRUNCATED,
	TEST_COMPRESS_LINE_LIMIT,
	TEST_COMPRESS_ENCODER_OOM,
	TEST_COMPRESS_DECODER_OOM,
	TEST_COMPRESS_STREAMING,
	TEST_COMPRESS_READ_BUFFER_OOM,
	TEST_COMPRESS_READ_GROW_OOM,
	TEST_COMPRESS_WRITE_BUFFER_OOM,
	TEST_COMPRESS_WRITE_FLUSH_OOM,
	TEST_COMPRESS_FAULT_COUNT
} testcompressfault;

static const char* TestCompressNames[] = {
	"write-cancel", "write-timeout", "read-cancel", "read-timeout",
	"invalid", "truncated", "line-limit", "encoder-oom", "decoder-oom", "streaming",
	"read-buffer-oom", "read-grow-oom", "write-buffer-oom", "write-flush-oom"
};

typedef struct testcompresssocket {
	xnetstream* Tcp;
	xtlsstream* Tls;
	double Deadline;
	bool PeerClosed;
} testcompresssocket;

typedef struct testcompressserver {
	xnetlistener* Listener;
	const xtlsserverconfig* Tls;
	double Deadline;
	xcancel* Cancel;
	testcompressfault Fault;
	xatomic32 ReadStarted;
	xatomic32 Returned;
	xatomic32 ResponseReady;
	size_t ResponseBytes;
	bool NoFault;
	bool Success;
} testcompressserver;

typedef struct testcompressplain {
	char Data[40064];
	size_t Size;
} testcompressplain;

static xnetaddr TestCompressAddress;

static void testCompressPayload(char* pData, size_t Size)
{
	uint32 State = UINT32_C(0x91fca39b);
	for ( size_t i = 0; i < Size; i++ ) {
		State ^= State << 13; State ^= State >> 17; State ^= State << 5;
		pData[i] = (char)('!' + State % 90u);
	}
}

static bool testCompressWait(xatomic32* pFlag, double Deadline)
{
	while ( xrtAtomic32Load(pFlag, XMEMORY_ACQUIRE) == 0 ) {
		if ( __xrtWaitExpired(Deadline) ) return false;
		xrtThreadYield();
	}
	return true;
}

static xnetaddrlist* testCompressResolve(cstr Host, xnetfamily Family, ptr Data)
{
	xnetaddr Address = TestCompressAddress;
	(void)Family;
	(void)Data;
	if ( strcmp(Host, "compress-fault.test") != 0 ) return NULL;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}

static bool testCompressRawSend(xbytesview Data, ptr pData)
{
	testcompresssocket* pSocket = (testcompresssocket*)pData;
	if ( pSocket->Tls != NULL )
		return testMailTlsSend(pSocket->Tls, (cstr)Data.Data, Data.Size, pSocket->Deadline);
	for ( ;; ) {
		xnetresult Result = xrtNetStreamSend(pSocket->Tcp, Data.Data, Data.Size);
		if ( Result == XNET_RESULT_OK ) return true;
		if ( Result != XNET_RESULT_AGAIN || !__xrtNetStreamWait(pSocket->Tcp,
			XNET_STREAM_WAIT_WRITE, pSocket->Deadline, NULL) ) return false;
	}
}

static xnetbytes* testCompressRawRecv(testcompresssocket* pSocket)
{
	pSocket->PeerClosed = false;
	xfuture* pRead = pSocket->Tls != NULL ? xrtTlsStreamRecvAsync(pSocket->Tls, 256u) :
		xrtNetStreamRecvAsync(pSocket->Tcp, 256u);
	xnetbytes* pBytes = NULL;
	if ( pRead != NULL && __xrtFutureWaitUntil(pRead, pSocket->Deadline) == XWAIT_OK ) {
		xfuturestate State = xrtFutureState(pRead);
		if ( State == XFUTURE_RESOLVED ) {
			pBytes = xrtNetBytesRef((xnetbytes*)xrtFutureValue(pRead));
			pSocket->PeerClosed = pBytes != NULL && xrtNetBytesView(pBytes).Size == 0;
		} else pSocket->PeerClosed = State == XFUTURE_CLOSED || State == XFUTURE_FAILED;
	}
	xrtFutureDestroy(pRead);
	return pBytes;
}

static bool testCompressPlainWrite(xbytesview Data, ptr pData)
{
	testcompressplain* pPlain = (testcompressplain*)pData;
	if ( Data.Size > sizeof(pPlain->Data) - pPlain->Size ) return false;
	memcpy(pPlain->Data + pPlain->Size, Data.Data, Data.Size);
	pPlain->Size += Data.Size;
	return true;
}

/* Verify complete wire commands, including continued compressor state. */
static bool testCompressReceive(testcompresssocket* pSocket, xinflate* pInflate, cstr Expected)
{
	testcompressplain Plain = { { 0 }, 0 };
	size_t Size = strlen(Expected);
	while ( Plain.Size < Size ) {
		xnetbytes* pBytes = testCompressRawRecv(pSocket);
		if ( pBytes == NULL ) return false;
		xbytesview Data = xrtNetBytesView(pBytes);
		bool Success = Data.Size != 0 && (pInflate != NULL ?
			xrtInflateWrite(pInflate, Data, false, testCompressPlainWrite, &Plain) :
			testCompressPlainWrite(Data, &Plain));
		xrtNetBytesDestroy(pBytes);
		if ( !Success ) return false;
	}
	return Plain.Size == Size && memcmp(Plain.Data, Expected, Size) == 0;
}

static bool testCompressSend(testcompresssocket* pSocket, xdeflate* pDeflate, cstr Text)
{
	xbytesview Data = { (cbytes)Text, strlen(Text) };
	return pDeflate != NULL ? xrtDeflateWrite(pDeflate, Data, XDEFLATE_FLUSH_SYNC,
		testCompressRawSend, pSocket) : testCompressRawSend(Data, pSocket);
}

static void testCompressAbort(testcompresssocket* pSocket)
{
	if ( pSocket->Tls != NULL ) (void)xrtTlsStreamAbort(pSocket->Tls);
	else if ( pSocket->Tcp != NULL ) (void)xrtNetStreamAbort(pSocket->Tcp);
}

/* The caller has returned but has not destroyed its client. There must be no
 * newly accepted literal byte or command, and the peer must already be closed. */
static bool testCompressPeerClosed(testcompresssocket* pSocket)
{
	xrtClearError();
	xnetbytes* pBytes = testCompressRawRecv(pSocket);
	bool Closed = pSocket->PeerClosed;
	xrtNetBytesDestroy(pBytes);
	return Closed && !__xrtWaitExpired(pSocket->Deadline);
}

/* Stage exactly one real socket response before arming the caller-thread fault.
 * With the codecs already allocated, the next allocation in Next is the
 * decoded Pending buffer rather than a TCP/TLS receive Future. */
static void testCompressStageResponse(ximapclient* pClient, testcompressserver* pServer)
{
	__xmailtransport* pTransport = &pClient->Transport;
	testRequire(testCompressWait(&pServer->ResponseReady, pServer->Deadline),
		"COMPRESS OOM response was not sent");
	testRequire(pServer->ResponseBytes != 0 &&
		pTransport->DeflatePrefixConsumed == pTransport->DeflatePrefixSize &&
		(pTransport->DeflateInput == NULL || pTransport->DeflateInputConsumed ==
			xrtNetBytesView(pTransport->DeflateInput).Size),
		"COMPRESS OOM staging would replace unconsumed bytes");
	xrtFree(pTransport->DeflatePrefix);
	xrtNetBytesDestroy(pTransport->DeflateInput);
	pTransport->DeflateInput = NULL;
	pTransport->DeflateInputConsumed = 0;
	pTransport->DeflatePrefix = (bytes)xrtMalloc(pServer->ResponseBytes);
	testRequire(pTransport->DeflatePrefix != NULL, "COMPRESS OOM prefix allocation failed");
	pTransport->DeflatePrefixSize = pServer->ResponseBytes;
	pTransport->DeflatePrefixConsumed = 0;
	for ( size_t Offset = 0; Offset < pServer->ResponseBytes; ) {
		xnetbytes* pBytes = __xrtMailTransportRawRecv(pTransport, pServer->Deadline, NULL);
		testRequire(pBytes != NULL, "COMPRESS OOM staging receive failed");
		xbytesview Data = xrtNetBytesView(pBytes);
		testRequire(Data.Size != 0 && Data.Size <= pServer->ResponseBytes - Offset,
			"COMPRESS OOM staging response size mismatch");
		memcpy(pTransport->DeflatePrefix + Offset, Data.Data, Data.Size);
		Offset += Data.Size;
		xrtNetBytesDestroy(pBytes);
	}
}

/* A failed final flush can follow an already transmitted literal prefix.
 * Verify the actual prefix and EOF, including any queued bytes, without
 * requiring that a partly transmitted operation was atomic on the wire. */
static bool testCompressLiteralPrefix(testcompresssocket* pSocket, xinflate* pDecoder,
	testcompressplain* pPlain, bool UntilClosed)
{
	char Expected[40000];
	testCompressPayload(Expected, sizeof(Expected));
	for ( ;; ) {
		xnetbytes* pBytes = testCompressRawRecv(pSocket);
		if ( pBytes == NULL ) return UntilClosed && pSocket->PeerClosed &&
			!__xrtWaitExpired(pSocket->Deadline) && pPlain->Size != 0;
		xbytesview Data = xrtNetBytesView(pBytes);
		if ( Data.Size == 0 && pSocket->PeerClosed ) {
			xrtNetBytesDestroy(pBytes);
			return UntilClosed && !__xrtWaitExpired(pSocket->Deadline) && pPlain->Size != 0;
		}
		bool Success = Data.Size != 0 && xrtInflateWrite(pDecoder, Data, false,
			testCompressPlainWrite, pPlain);
		xrtNetBytesDestroy(pBytes);
		if ( !Success || pPlain->Size > sizeof(Expected) ||
			memcmp(pPlain->Data, Expected, pPlain->Size) != 0 ) return false;
		if ( !UntilClosed && pPlain->Size != 0 ) return true;
	}
}

static int32 testCompressServer(ptr pData)
{
	testcompressserver* pServer = (testcompressserver*)pData;
	testcompresssocket Socket = { NULL, NULL, pServer->Deadline, false };
	Socket.Tcp = __xrtNetListenerAcceptWait(pServer->Listener, pServer->Deadline, NULL);
	if ( Socket.Tcp == NULL ) return 1;
	if ( pServer->Tls != NULL ) {
		Socket.Tls = testMailTlsUpgrade(&Socket.Tcp, pServer->Tls, pServer->Deadline);
		if ( Socket.Tls == NULL ) { xrtNetStreamDestroy(Socket.Tcp); return 2; }
	}
	bool Success = testCompressSend(&Socket, NULL, "* PREAUTH compress-fault.test ready\r\n") &&
		testCompressReceive(&Socket, NULL, "A00000001 CAPABILITY\r\n") &&
		testCompressSend(&Socket, NULL,
			"* CAPABILITY IMAP4rev1 COMPRESS=DEFLATE\r\nA00000001 OK capabilities\r\n") &&
		testCompressReceive(&Socket, NULL, "A00000002 COMPRESS DEFLATE\r\n") &&
		testCompressSend(&Socket, NULL, "A00000002 OK compression active\r\n");
	xdeflateconfig Deflate;
	xinflateconfig Inflate;
	xrtDeflateConfigInit(&Deflate);
	xrtInflateConfigInit(&Inflate);
	Deflate.Format = XDEFLATE_RAW;
	Inflate.Format = XINFLATE_RAW;
	xdeflate* pEncoder = Success ? xrtDeflateCreate(&Deflate) : NULL;
	xinflate* pDecoder = Success ? xrtInflateCreate(&Inflate) : NULL;
	Success = Success && pEncoder != NULL && pDecoder != NULL;
	testcompressplain Partial = { { 0 }, 0 };
	if ( Success && pServer->Fault == TEST_COMPRESS_STREAMING ) {
		char Literal[40003];
		testCompressPayload(Literal, 40000u);
		Literal[40000] = '\r'; Literal[40001] = '\n'; Literal[40002] = 0;
		Success = testCompressReceive(&Socket, pDecoder, "A00000003 APPEND \"INBOX\" {40000}\r\n") &&
			testCompressSend(&Socket, pEncoder, "+ send literal\r\n") &&
			testCompressReceive(&Socket, pDecoder, Literal) &&
			testCompressSend(&Socket, pEncoder, "A00000003 OK append complete\r\n") &&
			testCompressReceive(&Socket, pDecoder, "A00000004 LOGOUT\r\n") &&
			testCompressSend(&Socket, pEncoder,
				"* BYE finished\r\nA00000004 OK logout complete\r\n");
		if ( Success && Socket.Tls != NULL ) {
			xfuture* pClose = xrtTlsStreamWaitAsync(Socket.Tls, XTLS_STREAM_WAIT_CLOSE);
			Success = pClose != NULL && xrtTlsStreamClose(Socket.Tls) &&
				testMailTlsFuture(pClose, Socket.Deadline);
			xrtFutureDestroy(pClose);
		} else if ( Success ) {
			Success = xrtNetStreamClose(Socket.Tcp) && __xrtNetStreamWait(Socket.Tcp,
				XNET_STREAM_WAIT_CLOSE, Socket.Deadline, NULL);
		}
	} else if ( Success && (pServer->Fault == TEST_COMPRESS_WRITE_BUFFER_OOM ||
		pServer->Fault == TEST_COMPRESS_WRITE_FLUSH_OOM) ) {
		Success = testCompressReceive(&Socket, pDecoder,
			"A00000003 APPEND \"INBOX\" {40000}\r\n") &&
			testCompressSend(&Socket, pEncoder, "+ send literal\r\n");
		if ( Success && pServer->Fault == TEST_COMPRESS_WRITE_FLUSH_OOM ) {
			Success = testCompressLiteralPrefix(&Socket, pDecoder, &Partial, false);
			if ( Success ) xrtAtomic32Store(&pServer->ReadStarted, 1u, XMEMORY_RELEASE);
		}
	} else if ( Success && (pServer->Fault == TEST_COMPRESS_READ_BUFFER_OOM ||
		pServer->Fault == TEST_COMPRESS_READ_GROW_OOM) ) {
		Success = testCompressReceive(&Socket, pDecoder, "A00000003 NOOP\r\n");
		if ( Success && pServer->Fault == TEST_COMPRESS_READ_GROW_OOM ) {
			Success = testCompressSend(&Socket, pEncoder, "* 1 EXISTS\r\n") &&
				testCompressWait(&pServer->ReadStarted, pServer->Deadline);
		}
		if ( Success ) {
			char Response[4160];
			memcpy(Response, "* OK ", 5u);
			memset(Response + 5u, 'x', 4096u);
			strcpy(Response + 4101u, "\r\nA00000003 OK complete\r\n");
			uint64 Before = xrtDeflateOutputSize(pEncoder);
			Success = testCompressSend(&Socket, pEncoder, Response);
			pServer->ResponseBytes = (size_t)(xrtDeflateOutputSize(pEncoder) - Before);
			if ( Success ) xrtAtomic32Store(&pServer->ResponseReady, 1u, XMEMORY_RELEASE);
		}
		if ( Success && pServer->NoFault ) {
			Success = testCompressReceive(&Socket, pDecoder, "A00000004 LOGOUT\r\n") &&
				testCompressSend(&Socket, pEncoder,
					"* BYE finished\r\nA00000004 OK logout complete\r\n");
			if ( Success && Socket.Tls != NULL ) {
				xfuture* pClose = xrtTlsStreamWaitAsync(Socket.Tls, XTLS_STREAM_WAIT_CLOSE);
				Success = pClose != NULL && xrtTlsStreamClose(Socket.Tls) &&
					testMailTlsFuture(pClose, Socket.Deadline);
				xrtFutureDestroy(pClose);
			} else if ( Success ) {
				Success = xrtNetStreamClose(Socket.Tcp) && __xrtNetStreamWait(Socket.Tcp,
					XNET_STREAM_WAIT_CLOSE, Socket.Deadline, NULL);
			}
		}
	} else if ( Success && pServer->Fault <= TEST_COMPRESS_WRITE_TIMEOUT ) {
		Success = testCompressReceive(&Socket, pDecoder, "A00000003 APPEND \"INBOX\" {8}\r\n") &&
			testCompressSend(&Socket, pEncoder, "+ send literal\r\n");
	} else if ( Success && pServer->Fault < TEST_COMPRESS_ENCODER_OOM ) {
		Success = testCompressReceive(&Socket, pDecoder, "A00000003 NOOP\r\n");
		if ( Success && pServer->Fault == TEST_COMPRESS_INVALID ) {
			static const unsigned char Invalid[] = { 0x07 }; /* reserved BTYPE */
			Success = testCompressRawSend((xbytesview) { Invalid, sizeof(Invalid) }, &Socket);
		} else if ( Success && pServer->Fault == TEST_COMPRESS_LINE_LIMIT ) {
			char Oversize[131072];
			memset(Oversize, 'x', sizeof(Oversize) - 1u);
			Oversize[0] = '*'; Oversize[1] = ' '; Oversize[sizeof(Oversize) - 1u] = 0;
			Success = testCompressSend(&Socket, pEncoder, Oversize);
		} else if ( Success ) {
			Success = testCompressSend(&Socket, pEncoder,
				"* 1 EXISTS\r\nA00000003 OK incomplete");
			if ( Success && pServer->Fault == TEST_COMPRESS_TRUNCATED ) {
				Success = testCompressWait(&pServer->ReadStarted, pServer->Deadline);
				if ( Success ) testCompressAbort(&Socket);
			}
			if ( Success && pServer->Fault == TEST_COMPRESS_READ_CANCEL )
				Success = testCompressWait(&pServer->ReadStarted, pServer->Deadline) &&
					xrtCancelRequest(pServer->Cancel);
		}
	}
	Success = Success && testCompressWait(&pServer->Returned, pServer->Deadline);
	if ( Success && pServer->Fault == TEST_COMPRESS_WRITE_FLUSH_OOM )
		Success = testCompressLiteralPrefix(&Socket, pDecoder, &Partial, true);
	else if ( Success && !pServer->NoFault && pServer->Fault != TEST_COMPRESS_TRUNCATED &&
		pServer->Fault != TEST_COMPRESS_STREAMING )
		Success = testCompressPeerClosed(&Socket);
	testCompressAbort(&Socket);
	xrtTlsStreamDestroy(Socket.Tls);
	xrtNetStreamDestroy(Socket.Tcp);
	xrtDeflateDestroy(pEncoder);
	xrtInflateDestroy(pDecoder);
	pServer->Success = Success;
	return Success ? 0 : 3;
}

static void testCompressCase(xnetengine* pEngine, xnetresolver* pResolver,
	xnetlistener* pListener, xtlscontext* pContext, xtlsverifier* pVerifier,
	const xtlsserverconfig* pTls, testcompressfault Fault, bool Inject)
{
	testcompressserver Server;
	memset(&Server, 0, sizeof(Server));
	Server.Listener = pListener; Server.Tls = pTls; Server.Fault = Fault;
	Server.NoFault = !Inject;
	Server.Deadline = __xrtWaitAfter(UINT64_C(10000000));
	Server.Cancel = xrtCancelCreate();
	testRequire(Server.Cancel != NULL, "COMPRESS fault cancel create failed");
	xrtAtomic32Init(&Server.ReadStarted, 0u);
	xrtAtomic32Init(&Server.Returned, 0u);
	xrtAtomic32Init(&Server.ResponseReady, 0u);
	xthread* pThread = xrtThreadCreate(testCompressServer, &Server, 0);
	testRequire(pThread != NULL, "COMPRESS fault server create failed");
	ximapclientconfig Config;
	xrtImapClientConfigInit(&Config);
	Config.Net.Engine = pEngine; Config.Net.Resolver = pResolver;
	Config.Net.Host = "compress-fault.test"; Config.Net.Port = TestCompressAddress.Port;
	Config.Net.LineLimit = 128u;
	if ( Fault == TEST_COMPRESS_READ_BUFFER_OOM || Fault == TEST_COMPRESS_READ_GROW_OOM ) {
		Config.Net.LineLimit = 8192u;
		Config.Net.ReadChunk = 32u;
	}
	Config.Net.Security = pTls != NULL ? XMAIL_SECURITY_TLS : XMAIL_SECURITY_PLAIN;
	Config.Net.Tls.Context = pContext; Config.Net.Tls.Verifier = pVerifier;
	ximapclient* pClient = __xrtImapClientOpen(&Config, Server.Deadline, NULL);
	testRequire(pClient != NULL, "COMPRESS fault client open failed");
	ximapcompressconfig Compress;
	xrtImapCompressConfigInit(&Compress);
	if ( Fault == TEST_COMPRESS_STREAMING ) {
		ximapappendconfig Append;
		char Literal[40000];
		testCompressPayload(Literal, sizeof(Literal));
		xrtImapAppendConfigInit(&Append);
		Append.Mailbox = XRT_STR_LITERAL("INBOX"); Append.Size = sizeof(Literal);
		Append.Literal = XIMAP_LITERAL_SYNC;
		testRequire(__xrtImapClientCompress(pClient, &Compress, Server.Deadline, NULL) &&
			__xrtImapClientAppendBegin(pClient, &Append, Server.Deadline, NULL) &&
			__xrtImapClientAppendWrite(pClient, Literal, 7u, Server.Deadline, NULL) &&
			__xrtImapClientAppendWrite(pClient, Literal + 7u, 32770u, Server.Deadline, NULL) &&
			__xrtImapClientAppendWrite(pClient, Literal + 32777u, 7223u, Server.Deadline, NULL) &&
			xrtImapClientAppendRemaining(pClient) == 0 &&
			__xrtImapClientAppendEnd(pClient, NULL, Server.Deadline, NULL) &&
			__xrtImapClientLogout(pClient, Server.Deadline, NULL) &&
			xrtImapClientState(pClient) == XIMAP_CLIENT_CLOSED,
			"COMPRESS continued streaming did not preserve literal bytes and command boundaries");
		goto finished;
	}
	ximapevent Event;
	xerrkind Expected;
	bool Result;
	if ( Fault == TEST_COMPRESS_ENCODER_OOM || Fault == TEST_COMPRESS_DECODER_OOM ) {
		/* Complete the actual public negotiation first, then inject precisely at
		 * the internal installation operation used by ClientCompress after OK. */
		testRequire(__xrtImapClientBegin(pClient, XRT_STR_LITERAL("COMPRESS"),
			XRT_STR_LITERAL("DEFLATE"), Server.Deadline, NULL) &&
			__xrtImapClientNext(pClient, &Event, Server.Deadline, NULL) == XMAIL_NEXT_END,
			"COMPRESS OOM acknowledgement failed");
		xdeflateconfig Deflate;
		xinflateconfig Inflate;
		xrtDeflateConfigInit(&Deflate); xrtInflateConfigInit(&Inflate);
		Deflate.Format = XDEFLATE_RAW; Inflate.Format = XINFLATE_RAW;
		xrtClearError();
		testRequire(xrtMemDebugFailAfter(Fault == TEST_COMPRESS_ENCODER_OOM ? 0u : 1u),
			"COMPRESS installation injection failed");
		Result = __xrtImapClientCompressStart(pClient, &Deflate, &Inflate);
		bool Triggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		testRequire(Triggered && !xrtImapClientCompressed(pClient) &&
			pClient->Transport.Deflater == NULL && pClient->Transport.Inflater == NULL,
			"COMPRESS installation left a partially installed codec");
		Expected = XERR_MEMORY;
	} else if ( Fault == TEST_COMPRESS_READ_BUFFER_OOM || Fault == TEST_COMPRESS_READ_GROW_OOM ) {
		testRequire(__xrtImapClientCompress(pClient, &Compress, Server.Deadline, NULL) &&
			__xrtImapClientBegin(pClient, XRT_STR_LITERAL("NOOP"), XRT_STR_LITERAL(""),
				Server.Deadline, NULL), "COMPRESS read OOM negotiation failed");
		if ( Fault == TEST_COMPRESS_READ_GROW_OOM ) {
			testRequire(__xrtImapClientNext(pClient, &Event, Server.Deadline, NULL) == XMAIL_NEXT_ITEM &&
				testMailViewEqual(Event.Response.Text, XRT_STR_LITERAL("1 EXISTS")),
				"COMPRESS growth OOM first response failed");
			testRequire(pClient->Transport.Pending != NULL &&
				pClient->Transport.PendingCapacity < 4096u,
				"COMPRESS growth OOM did not retain a small decoded buffer");
			xrtAtomic32Store(&Server.ReadStarted, 1u, XMEMORY_RELEASE);
		} else testRequire(pClient->Transport.Pending == NULL &&
			pClient->Transport.PendingCapacity == 0,
			"COMPRESS first output OOM buffer was already allocated");
		testCompressStageResponse(pClient, &Server);
		if ( !Inject ) {
			testRequire(__xrtImapClientNext(pClient, &Event, Server.Deadline, NULL) == XMAIL_NEXT_ITEM &&
				Event.Response.Status == XIMAP_STATUS_OK && Event.Response.Text.Size == 4096u &&
				pClient->Transport.PendingCapacity >= 4096u,
				"COMPRESS decoded output control did not grow and parse the response");
			for ( size_t i = 0; i < Event.Response.Text.Size; i++ )
				testRequire(Event.Response.Text.Data[i] == 'x', "COMPRESS decoded output control data changed");
			testRequire(__xrtImapClientNext(pClient, &Event, Server.Deadline, NULL) == XMAIL_NEXT_END &&
				__xrtImapClientLogout(pClient, Server.Deadline, NULL) &&
				xrtImapClientState(pClient) == XIMAP_CLIENT_CLOSED,
				"COMPRESS decoded output control lost continued command or close state");
			goto finished;
		}
		xrtClearError();
		testRequire(xrtMemDebugFailAfter(0), "COMPRESS decoded output injection failed");
		Result = __xrtImapClientNext(pClient, &Event, Server.Deadline, NULL) != XMAIL_NEXT_ERROR;
		testRequire(xrtMemDebugFailTriggered(), "COMPRESS decoded output allocation was not hit");
		xrtMemDebugFailClear();
		Expected = XERR_MEMORY;
	} else if ( Fault == TEST_COMPRESS_WRITE_BUFFER_OOM || Fault == TEST_COMPRESS_WRITE_FLUSH_OOM ) {
		ximapappendconfig Append;
		char Literal[40000];
		testCompressPayload(Literal, sizeof(Literal));
		xrtImapAppendConfigInit(&Append);
		Append.Mailbox = XRT_STR_LITERAL("INBOX"); Append.Size = sizeof(Literal);
		Append.Literal = XIMAP_LITERAL_SYNC;
		testRequire(__xrtImapClientCompress(pClient, &Compress, Server.Deadline, NULL) &&
			__xrtImapClientAppendBegin(pClient, &Append, Server.Deadline, NULL),
			"COMPRESS write OOM continuation failed");
		if ( Fault == TEST_COMPRESS_WRITE_FLUSH_OOM ) {
			uint64 Before = xrtDeflateOutputSize(pClient->Transport.Deflater);
			testRequire(__xrtImapClientAppendWrite(pClient, Literal, sizeof(Literal), Server.Deadline, NULL) &&
				xrtImapClientAppendRemaining(pClient) == 0 &&
				xrtDeflateOutputSize(pClient->Transport.Deflater) > Before &&
				testCompressWait(&Server.ReadStarted, Server.Deadline),
				"COMPRESS flush OOM had no transmitted literal prefix");
		}
		xrtClearError();
		testRequire(xrtMemDebugFailAfter(0), "COMPRESS transport output injection failed");
		if ( Fault == TEST_COMPRESS_WRITE_BUFFER_OOM ) {
			Result = __xrtImapClientAppendWrite(pClient, Literal, sizeof(Literal), Server.Deadline, NULL);
			testRequire(xrtImapClientAppendRemaining(pClient) == sizeof(Literal),
				"COMPRESS failed write counted literal bytes");
		} else {
			ximapappendresult Output, Original;
			memset(&Output, 0xA5, sizeof(Output)); Original = Output;
			Result = __xrtImapClientAppendEnd(pClient, &Output, Server.Deadline, NULL);
			testRequire(memcmp(&Output, &Original, sizeof(Output)) == 0,
				"COMPRESS failed flush modified APPEND output");
		}
		testRequire(xrtMemDebugFailTriggered(), "COMPRESS transport output allocation was not hit");
		xrtMemDebugFailClear();
		Expected = XERR_MEMORY;
	} else {
		testRequire(__xrtImapClientCompress(pClient, &Compress, Server.Deadline, NULL),
			"COMPRESS fault negotiation failed");
		if ( Fault <= TEST_COMPRESS_WRITE_TIMEOUT ) {
			ximapappendconfig Append;
			xrtImapAppendConfigInit(&Append);
			Append.Mailbox = XRT_STR_LITERAL("INBOX"); Append.Size = 8u;
			Append.Literal = XIMAP_LITERAL_SYNC;
			testRequire(__xrtImapClientAppendBegin(pClient, &Append, Server.Deadline, NULL),
				"compressed APPEND continuation failed");
			if ( Fault == TEST_COMPRESS_WRITE_CANCEL )
				testRequire(xrtCancelRequest(Server.Cancel), "COMPRESS write cancel request failed");
			xrtClearError();
			Result = __xrtImapClientAppendWrite(pClient, "abcdefgh", 8u,
				Fault == TEST_COMPRESS_WRITE_TIMEOUT ? __xrtWaitAfter(0) : Server.Deadline,
				Server.Cancel);
			Expected = Fault == TEST_COMPRESS_WRITE_CANCEL ? XERR_CANCELLED : XERR_TIMEOUT;
			testRequire(!Result, "compressed APPEND accepted expired or cancelled buffered bytes");
			testRequire(xrtImapClientAppendRemaining(pClient) == 8u,
				"compressed APPEND counted bytes after cancellation or timeout");
		} else {
			testRequire(__xrtImapClientBegin(pClient, XRT_STR_LITERAL("NOOP"),
				XRT_STR_LITERAL(""), Server.Deadline, NULL), "COMPRESS fault NOOP failed");
			if ( Fault == TEST_COMPRESS_READ_CANCEL || Fault == TEST_COMPRESS_READ_TIMEOUT ||
				Fault == TEST_COMPRESS_TRUNCATED ) {
				testRequire(__xrtImapClientNext(pClient, &Event, Server.Deadline, NULL) == XMAIL_NEXT_ITEM &&
					testMailViewEqual(Event.Response.Text, XRT_STR_LITERAL("1 EXISTS")),
					"COMPRESS partial response was not decoded first");
			}
			xrtClearError();
			xrtAtomic32Store(&Server.ReadStarted, 1u, XMEMORY_RELEASE);
			Result = __xrtImapClientNext(pClient, &Event,
				Fault == TEST_COMPRESS_READ_TIMEOUT ? __xrtWaitAfter(UINT64_C(100000)) :
				Server.Deadline, Server.Cancel) != XMAIL_NEXT_ERROR;
			Expected = Fault == TEST_COMPRESS_READ_CANCEL ? XERR_CANCELLED :
				Fault == TEST_COMPRESS_READ_TIMEOUT ? XERR_TIMEOUT :
				Fault == TEST_COMPRESS_LINE_LIMIT ? XERR_RANGE : XERR_PROTOCOL;
		}
	}
	xerror* pFailure = xrtTakeError();
	testRequire(!Result && pFailure != NULL &&
		(Fault == TEST_COMPRESS_TRUNCATED ?
			(xrtErrorKind(pFailure) == XERR_CLOSED || xrtErrorKind(pFailure) == XERR_IO ||
			 xrtErrorKind(pFailure) == XERR_PROTOCOL) : xrtErrorKind(pFailure) == Expected),
		"COMPRESS fault lost its original error");
	testRequire(xrtImapClientState(pClient) == XIMAP_CLIENT_FAILED,
		"COMPRESS fault session remained reusable");
	ximapresponseview Last;
	testRequire(xrtImapClientLastResponse(pClient, &Last) && Last.Status == XIMAP_STATUS_OK &&
		testMailViewEqual(Last.Text, XRT_STR_LITERAL("compression active")),
		"COMPRESS fault replaced the last complete response");
	xrtErrorFree(pFailure);
	xrtClearError();
	testRequire(!__xrtImapClientBegin(pClient, XRT_STR_LITERAL("NOOP"),
		XRT_STR_LITERAL(""), Server.Deadline, NULL) && xrtErrorKind(xrtGetError()) == XERR_STATE,
		"COMPRESS failed session accepted another command");
	xrtClearError();
finished:
	xrtAtomic32Store(&Server.Returned, 1u, XMEMORY_RELEASE);
	testRequire(__xrtThreadWaitUntil(pThread, Server.Deadline) == XWAIT_OK && Server.Success &&
		xrtThreadExitCode(pThread) == 0, "COMPRESS fault did not close peer before destruction");
	xrtThreadDestroy(pThread);
	xrtImapClientDestroy(pClient);
	xrtCancelDestroy(Server.Cancel);
	printf("[PASS] COMPRESS %s/%s%s\n", pTls != NULL ? "tls" : "tcp",
		TestCompressNames[Fault], Inject ? "" : "-control");
}

int main(int argc, char** argv)
{
	int SelectedTls = -1, SelectedFault = -1;
	if ( argc != 1 ) {
		testRequire(argc == 3 && (strcmp(argv[1], "tcp") == 0 || strcmp(argv[1], "tls") == 0),
			"COMPRESS fault usage: [tcp|tls case]");
		SelectedTls = strcmp(argv[1], "tls") == 0;
		for ( int i = 0; i < TEST_COMPRESS_FAULT_COUNT; i++ )
			if ( strcmp(argv[2], TestCompressNames[i]) == 0 ) SelectedFault = i;
		testRequire(SelectedFault >= 0, "unknown COMPRESS fault case");
	}
	xnetengineconfig Engine;
	xnetlistenconfig Listen;
	xnetresolverconfig Resolver;
	xtlsverifierconfig Verify;
	xtlsserverconfig Tls;
	xtlscontext* pContext = testTlsServerContext();
	xtlsidentity* pIdentity = testTlsServerIdentity();
	xrtTlsVerifierConfigInit(&Verify); Verify.Verify = testTlsServerAccept;
	xtlsverifier* pVerifier = xrtTlsVerifierCreate(&Verify);
	testRequire(pContext != NULL && pIdentity != NULL && pVerifier != NULL, "COMPRESS TLS fixture failed");
	xrtTlsServerConfigInit(&Tls); Tls.Context = pContext; Tls.Identity = pIdentity;
	xrtNetEngineConfigInit(&Engine); Engine.Backend = XNET_PORT_SELECT; Engine.Workers = 2u;
	xnetengine* pEngine = xrtNetEngineCreate(&Engine);
	testRequire(pEngine != NULL && xrtNetEngineStart(pEngine), "COMPRESS engine failed");
	xrtNetListenConfigInit(&Listen);
	testRequire(xrtNetAddrLoopback(&Listen.Address, XNET_FAMILY_IPV4, 0), "COMPRESS address failed");
	xnetlistener* pListener = xrtNetListen(pEngine, &Listen, NULL, NULL, NULL);
	testRequire(pListener != NULL && xrtNetListenerLocal(pListener, &TestCompressAddress), "COMPRESS listen failed");
	xrtNetResolverConfigInit(&Resolver); Resolver.Lookup = testCompressResolve;
	Resolver.Workers = 1u; Resolver.CacheEntries = 0;
	xnetresolver* pResolver = xrtNetResolverCreate(&Resolver);
	testRequire(pResolver != NULL, "COMPRESS resolver failed");
	for ( int tls = 0; tls < 2; tls++ )
		for ( int fault = 0; fault < TEST_COMPRESS_FAULT_COUNT; fault++ )
			if ( (SelectedTls < 0 || SelectedTls == tls) && (SelectedFault < 0 || SelectedFault == fault) ) {
				if ( fault == TEST_COMPRESS_READ_BUFFER_OOM || fault == TEST_COMPRESS_READ_GROW_OOM )
					testCompressCase(pEngine, pResolver, pListener, pContext, pVerifier,
						tls != 0 ? &Tls : NULL, (testcompressfault)fault, false);
				testCompressCase(pEngine, pResolver, pListener, pContext, pVerifier,
					tls != 0 ? &Tls : NULL, (testcompressfault)fault, true);
			}
	testRequire(xrtNetListenerClose(pListener), "COMPRESS listener close failed");
	double Retire = __xrtWaitAfter(UINT64_C(5000000));
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!__xrtWaitExpired(Retire), "COMPRESS listener did not retire");
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver), "COMPRESS resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier); xrtTlsIdentityRelease(pIdentity); xrtTlsContextRelease(pContext);
	for ( ;; ) {
		xnetretireresult Result = xrtNetEngineTryDestroy(pEngine);
		if ( Result == XNET_RETIRE_READY ) break;
		testRequire(Result != XNET_RETIRE_ERROR && !__xrtWaitExpired(Retire), "COMPRESS engine did not retire");
		xrtThreadYield();
	}
	xrtClearError();
	testRequire(xrtMemDebugReset(), "COMPRESS cases retained logical allocations after engine retirement");
	return 0;
}
