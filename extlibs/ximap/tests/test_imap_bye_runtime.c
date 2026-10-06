#include <xrt/detail/ximap_wait.h>
#include <xrt/detail/wait.h>
#include "test.h"




typedef enum testimapbyemode {
	TEST_IMAP_BYE_IDLE,
	TEST_IMAP_BYE_COMMAND,
	TEST_IMAP_BYE_LOGOUT
} testimapbyemode;

typedef struct testimapbyeserver {
	xnetlistener* Listener;
	double Deadline;
	testimapbyemode Mode;
	bool Success;
} testimapbyeserver;

static xnetaddr TestImapByeAddress;




static xnetaddrlist* testImapByeResolve(cstr sHost, xnetfamily Family, ptr pData)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "imap-bye.test") != 0 ) {
		return NULL;
	}
	Address = TestImapByeAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}




static bool testImapByeSend(xnetstream* pStream, cstr sText, double Deadline)
{
	for ( ;; ) {
		xnetresult Result = xrtNetStreamSend(pStream, sText, strlen(sText));

		if ( Result == XNET_RESULT_OK ) {
			return true;
		}
		if ( (Result != XNET_RESULT_AGAIN) || !__xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_WRITE,
			Deadline,
			NULL
		) ) {
			return false;
		}
	}
}




static bool testImapByeReceive(
	xnetstream* pStream,
	cstr sExpected,
	double Deadline
)
{
	size_t iReceived = 0;
	size_t iExpected = strlen(sExpected);

	while ( iReceived < iExpected ) {
		xnetbytes* pBytes = __xrtNetStreamRecv(
			pStream,
			iExpected - iReceived,
			Deadline,
			NULL
		);
		xbytesview Bytes;

		if ( pBytes == NULL ) {
			return false;
		}
		Bytes = xrtNetBytesView(pBytes);
		if ( (Bytes.Size == 0) ||
			(memcmp(Bytes.Data, sExpected + iReceived, Bytes.Size) != 0) ) {
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		iReceived += Bytes.Size;
		xrtNetBytesDestroy(pBytes);
	}
	return true;
}




static int32 testImapByeServer(ptr pData)
{
	testimapbyeserver* pServer = (testimapbyeserver*)pData;
	xnetstream* pStream = __xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testImapByeSend(
		pStream,
		"* PREAUTH imap-bye.test ready\r\n",
		pServer->Deadline
	) && testImapByeReceive(
		pStream,
		"A00000001 CAPABILITY\r\n",
		pServer->Deadline
	) && testImapByeSend(
		pStream,
		"* CAPABILITY IMAP4rev1\r\nA00000001 OK capability complete\r\n",
		pServer->Deadline
	);
	if ( bSuccess && (pServer->Mode == TEST_IMAP_BYE_IDLE) ) {
		bSuccess = testImapByeSend(
			pStream,
			"* BYE idle shutdown\r\n",
			pServer->Deadline
		);
	} else if ( bSuccess && (pServer->Mode == TEST_IMAP_BYE_COMMAND) ) {
		bSuccess = testImapByeReceive(
			pStream,
			"A00000002 NOOP\r\n",
			pServer->Deadline
		) && testImapByeSend(
			pStream,
			"* BYE command shutdown\r\n",
			pServer->Deadline
		);
	} else if ( bSuccess ) {
		bSuccess = testImapByeReceive(
			pStream,
			"L1 LOGOUT\r\n",
			pServer->Deadline
		) && testImapByeSend(
			pStream,
			"* BYE signing off\r\nL1 OK logout complete\r\n",
			pServer->Deadline
		);
	}
	bSuccess = bSuccess && xrtNetStreamClose(pStream) && __xrtNetStreamWait(
		pStream,
		XNET_STREAM_WAIT_CLOSE,
		pServer->Deadline,
		NULL
	);
	pServer->Success = bSuccess;
	xrtNetStreamDestroy(pStream);
	return bSuccess ? 0 : 2;
}




int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	ximapclientconfig Config;
	testimapbyeserver Server;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	double Deadline;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"IMAP BYE engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "IMAP BYE loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestImapByeAddress
	), "IMAP BYE listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testImapByeResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "IMAP BYE resolver creation failed");
	xrtImapClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "imap-bye.test";
	Config.Net.Port = TestImapByeAddress.Port;
	Server.Listener = pListener;

	for ( int iMode = TEST_IMAP_BYE_IDLE;
		iMode <= TEST_IMAP_BYE_LOGOUT; iMode++ ) {
		ximapclient* pClient;
		xthread* pThread;
		ximapevent Event;
		ximapresponseview Last;

		Deadline = __xrtWaitAfter(INT64_C(10000));
		Server.Deadline = Deadline;
		Server.Mode = (testimapbyemode)iMode;
		Server.Success = false;
		pThread = xrtThreadCreate(testImapByeServer, &Server, 0);
		testRequire(pThread != NULL, "IMAP BYE server thread failed");
		pClient = __xrtImapClientOpen(&Config, Deadline, NULL);
		testRequire((pClient != NULL) &&
			(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED),
			"IMAP BYE session open failed");
		if ( iMode == TEST_IMAP_BYE_IDLE ) {
			testRequire(__xrtImapClientReceive(pClient, &Event, Deadline, NULL) &&
				(Event.Response.Status == XIMAP_STATUS_BYE) &&
				(xrtImapClientState(pClient) == XIMAP_CLIENT_FAILED) &&
				xrtImapClientLastResponse(pClient, &Last) &&
				testMailViewEqual(Last.Text, XRT_STR_LITERAL("idle shutdown")),
				"IMAP unsolicited BYE did not fail the session");
			xrtClearError();
			testRequire(!__xrtImapClientBegin(
				pClient,
				XRT_STR_LITERAL("NOOP"),
				XRT_STR_LITERAL(""),
				Deadline,
				NULL
			) &&
				(xrtErrorKind(xrtGetError()) == XERR_STATE),
				"IMAP sent a command after unsolicited BYE");
		} else if ( iMode == TEST_IMAP_BYE_COMMAND ) {
			xrtClearError();
			testRequire(__xrtImapClientBegin(
				pClient,
				XRT_STR_LITERAL("NOOP"),
				XRT_STR_LITERAL(""),
				Deadline,
				NULL
			) && (__xrtImapClientNext(pClient, &Event, Deadline, NULL) ==
				XMAIL_NEXT_ERROR) &&
				(xrtErrorKind(xrtGetError()) == XERR_CLOSED) &&
				(xrtImapClientState(pClient) == XIMAP_CLIENT_FAILED) &&
				xrtImapClientLastResponse(pClient, &Last) &&
				testMailViewEqual(Last.Text, XRT_STR_LITERAL("command shutdown")),
				"IMAP command BYE did not preserve close reason");
		} else {
			testRequire(__xrtImapClientSend(
				pClient,
				XRT_STR_LITERAL("L1"),
				XRT_STR_LITERAL("LOGOUT"),
				XRT_STR_LITERAL(""),
				Deadline,
				NULL
			), "IMAP low-level LOGOUT send failed");
			xrtClearError();
			testRequire(!__xrtImapClientSend(
				pClient,
				XRT_STR_LITERAL("L2"),
				XRT_STR_LITERAL("NOOP"),
				XRT_STR_LITERAL(""),
				Deadline,
				NULL
			) && (xrtErrorKind(xrtGetError()) == XERR_CLOSED),
				"IMAP accepted a command after LOGOUT");
			xrtClearError();
			testRequire(__xrtImapClientReceive(pClient, &Event, Deadline, NULL) &&
				(Event.Response.Status == XIMAP_STATUS_BYE) &&
				xrtImapClientLastResponse(pClient, &Last) &&
				testMailViewEqual(Last.Text, XRT_STR_LITERAL("signing off")) &&
				__xrtImapClientReceive(pClient, &Event, Deadline, NULL) &&
				(Event.Response.Kind == XIMAP_RESPONSE_TAGGED) &&
				(Event.Response.Status == XIMAP_STATUS_OK) &&
				testMailViewEqual(Event.Response.Tag, XRT_STR_LITERAL("L1")) &&
				xrtImapClientLastResponse(pClient, &Last) &&
				testMailViewEqual(Last.Text,
					XRT_STR_LITERAL("logout complete")),
				"IMAP low-level LOGOUT lost its tagged completion");
		}
		xrtImapClientDestroy(pClient);
		testRequire(__xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
			"IMAP BYE server did not finish");
		testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
			"IMAP BYE server transcript mismatch");
		xrtThreadDestroy(pThread);
	}

	testRequire(xrtNetListenerClose(pListener),
		"IMAP BYE listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"IMAP BYE resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"IMAP BYE engine destroy failed");
	return 0;
}
