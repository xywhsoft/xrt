#include "test.h"




typedef enum testimapappendfaultmode {
	TEST_IMAP_APPEND_REJECTIONS,
	TEST_IMAP_APPEND_EARLY_OK,
	TEST_IMAP_APPEND_CANCEL,
	TEST_IMAP_APPEND_DISCONNECT
} testimapappendfaultmode;

typedef struct testimapappendfaultserver {
	xnetlistener* Listener;
	xdeadline Deadline;
	testimapappendfaultmode Mode;
	xatomic32 Release;
	xatomic32 Disconnected;
	bool Success;
} testimapappendfaultserver;

static xnetaddr TestImapAppendFaultAddress;




static xnetaddrlist* testImapAppendFaultResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "imap-append-fault.test") != 0 ) {
		return NULL;
	}
	Address = TestImapAppendFaultAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}




static bool testImapAppendFaultSend(
	xnetstream* pStream,
	cstr sText,
	xdeadline Deadline
)
{
	for ( ;; ) {
		xnetresult Result = xrtNetStreamSend(pStream, sText, strlen(sText));

		if ( Result == XNET_RESULT_OK ) {
			return true;
		}
		if ( (Result != XNET_RESULT_AGAIN) || !xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_WRITE,
			Deadline,
			NULL
		) ) {
			return false;
		}
	}
}




static bool testImapAppendFaultReceive(
	xnetstream* pStream,
	cstr sExpected,
	xdeadline Deadline
)
{
	size_t iReceived = 0;
	size_t iExpected = strlen(sExpected);

	while ( iReceived < iExpected ) {
		xnetbytes* pBytes = xrtNetStreamRecv(
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




static bool testImapAppendFaultExchange(
	xnetstream* pStream,
	cstr sCommand,
	cstr sResponse,
	xdeadline Deadline
)
{
	return testImapAppendFaultReceive(pStream, sCommand, Deadline) &&
		testImapAppendFaultSend(pStream, sResponse, Deadline);
}




static int32 testImapAppendFaultServer(ptr pData)
{
	testimapappendfaultserver* pServer =
		(testimapappendfaultserver*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	bool bSuccess;

	if ( pStream == NULL ) {
		return 1;
	}
	bSuccess = testImapAppendFaultSend(
		pStream,
		"* PREAUTH imap-append-fault.test ready\r\n",
		pServer->Deadline
	) && testImapAppendFaultExchange(
		pStream,
		"A00000001 CAPABILITY\r\n",
		"* CAPABILITY IMAP4rev1 LITERAL+\r\n"
		"A00000001 OK capability complete\r\n",
		pServer->Deadline
	);
	if ( bSuccess && (pServer->Mode == TEST_IMAP_APPEND_REJECTIONS) ) {
		bSuccess = testImapAppendFaultExchange(
			pStream,
			"A00000002 APPEND \"INBOX\" {5}\r\n",
			"A00000002 NO mailbox read-only\r\n",
			pServer->Deadline
		) && testImapAppendFaultExchange(
			pStream,
			"A00000003 APPEND \"INBOX\" {5}\r\n",
			"A00000003 BAD invalid arguments\r\n",
			pServer->Deadline
		) && testImapAppendFaultExchange(
			pStream,
			"A00000004 APPEND \"INBOX\" {5+}\r\nhello\r\n",
			"A00000004 NO quota exceeded\r\n",
			pServer->Deadline
		) && testImapAppendFaultExchange(
			pStream,
			"A00000005 APPEND \"INBOX\" {5+}\r\nhello\r\n",
			"A00000005 BAD invalid message\r\n",
			pServer->Deadline
		) && testImapAppendFaultExchange(
			pStream,
			"A00000006 LOGOUT\r\n",
			"* BYE signing off\r\nA00000006 OK logout complete\r\n",
			pServer->Deadline
		);
	} else if ( bSuccess && (pServer->Mode == TEST_IMAP_APPEND_EARLY_OK) ) {
		bSuccess = testImapAppendFaultExchange(
			pStream,
			"A00000002 APPEND \"INBOX\" {5}\r\n",
			"A00000002 OK accepted without message\r\n",
			pServer->Deadline
		);
	} else if ( bSuccess && (pServer->Mode == TEST_IMAP_APPEND_CANCEL) ) {
		xnetbytes* pUnexpected;

		bSuccess = testImapAppendFaultExchange(
			pStream,
			"A00000002 APPEND \"INBOX\" {5}\r\n",
			"+ send literal\r\n",
			pServer->Deadline
		);
		if ( bSuccess ) {
			pUnexpected = xrtNetStreamRecv(
				pStream,
				1u,
				xrtDeadlineAfter(UINT64_C(100000)),
				NULL
			);
			bSuccess = (pUnexpected == NULL) &&
				(xrtNetStreamState(pStream) != XNET_STREAM_OPEN);
			xrtNetBytesDestroy(pUnexpected);
		}
	} else if ( bSuccess ) {
		bSuccess = testImapAppendFaultExchange(
			pStream,
			"A00000002 APPEND \"INBOX\" {5}\r\n",
			"+ send literal\r\n",
			pServer->Deadline
		);
		while ( bSuccess && !xrtAtomic32Load(&pServer->Release,
			XMEMORY_ACQUIRE) ) {
			if ( xrtDeadlineExpired(pServer->Deadline) ) {
				bSuccess = false;
				break;
			}
			xrtThreadYield();
		}
		bSuccess = bSuccess && xrtNetStreamAbort(pStream);
		while ( bSuccess &&
			(xrtNetStreamState(pStream) != XNET_STREAM_CLOSED) ) {
			if ( xrtDeadlineExpired(pServer->Deadline) ) {
				bSuccess = false;
				break;
			}
			xrtThreadYield();
		}
		xrtAtomic32Store(&pServer->Disconnected, 1u, XMEMORY_RELEASE);
		pServer->Success = bSuccess;
		xrtNetStreamDestroy(pStream);
		return bSuccess ? 0 : 2;
	}
	if ( bSuccess &&
		(xrtNetStreamState(pStream) == XNET_STREAM_OPEN) ) {
		bSuccess = xrtNetStreamClose(pStream) && xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_CLOSE,
			pServer->Deadline,
			NULL
		);
	}
	pServer->Success = bSuccess;
	xrtNetStreamDestroy(pStream);
	return bSuccess ? 0 : 2;
}




static bool testImapAppendFaultLast(
	ximapclient* pClient,
	ximapstatus Status,
	xstrview Text
)
{
	ximapresponseview Last;

	return xrtImapClientLastResponse(pClient, &Last) &&
		(Last.Status == Status) && testMailViewEqual(Last.Text, Text);
}




int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	ximapclientconfig ClientConfig;
	testimapappendfaultserver Server;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"IMAP APPEND fault engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "IMAP APPEND fault loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestImapAppendFaultAddress
	), "IMAP APPEND fault listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testImapAppendFaultResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"IMAP APPEND fault resolver creation failed");
	xrtImapClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = "imap-append-fault.test";
	ClientConfig.Net.Port = TestImapAppendFaultAddress.Port;
	Server.Listener = pListener;

	for ( int iMode = TEST_IMAP_APPEND_REJECTIONS;
		iMode <= TEST_IMAP_APPEND_DISCONNECT; iMode++ ) {
		ximapclient* pClient;
		ximapappendconfig AppendConfig;
		xthread* pThread;
		xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

		Server.Deadline = Deadline;
		Server.Mode = (testimapappendfaultmode)iMode;
		xrtAtomic32Init(&Server.Release, 0u);
		xrtAtomic32Init(&Server.Disconnected, 0u);
		Server.Success = false;
		pThread = xrtThreadCreate(testImapAppendFaultServer, &Server, 0);
		testRequire(pThread != NULL,
			"IMAP APPEND fault server thread failed");
		pClient = xrtImapClientOpen(&ClientConfig, Deadline, NULL);
		testRequire((pClient != NULL) &&
			(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED),
			"IMAP APPEND fault client open failed");
		xrtImapAppendConfigInit(&AppendConfig);
		AppendConfig.Mailbox = XRT_STR_LITERAL("INBOX");
		AppendConfig.Size = 5u;
		AppendConfig.Literal = XIMAP_LITERAL_SYNC;
		if ( iMode == TEST_IMAP_APPEND_REJECTIONS ) {
			xrtClearError();
			testRequire(!xrtImapClientAppendBegin(
				pClient, &AppendConfig, Deadline, NULL
			) && (xrtErrorKind(xrtGetError()) == XERR_PERMISSION) &&
				(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED) &&
				testImapAppendFaultLast(pClient, XIMAP_STATUS_NO,
					XRT_STR_LITERAL("mailbox read-only")),
				"IMAP APPEND early NO recovery failed");
			xrtClearError();
			testRequire(!xrtImapClientAppendBegin(
				pClient, &AppendConfig, Deadline, NULL
			) && (xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
				(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED) &&
				testImapAppendFaultLast(pClient, XIMAP_STATUS_BAD,
					XRT_STR_LITERAL("invalid arguments")),
				"IMAP APPEND early BAD recovery failed");
			AppendConfig.Literal = XIMAP_LITERAL_NONSYNC;
			xrtClearError();
			testRequire(!xrtImapClientAppend(
				pClient, &AppendConfig, "hello", NULL, Deadline, NULL
			) && (xrtErrorKind(xrtGetError()) == XERR_PERMISSION) &&
				(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED) &&
				testImapAppendFaultLast(pClient, XIMAP_STATUS_NO,
					XRT_STR_LITERAL("quota exceeded")),
				"IMAP APPEND final NO recovery failed");
			xrtClearError();
			testRequire(!xrtImapClientAppend(
				pClient, &AppendConfig, "hello", NULL, Deadline, NULL
			) && (xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
				(xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED) &&
				testImapAppendFaultLast(pClient, XIMAP_STATUS_BAD,
					XRT_STR_LITERAL("invalid message")),
				"IMAP APPEND final BAD recovery failed");
			testRequire(xrtImapClientLogout(pClient, Deadline, NULL),
				"IMAP APPEND rejection recovery LOGOUT failed");
		} else if ( iMode == TEST_IMAP_APPEND_EARLY_OK ) {
			xrtClearError();
			testRequire(!xrtImapClientAppendBegin(
				pClient, &AppendConfig, Deadline, NULL
			) && (xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
				(xrtImapClientState(pClient) == XIMAP_CLIENT_FAILED) &&
				testImapAppendFaultLast(pClient, XIMAP_STATUS_OK,
					XRT_STR_LITERAL("accepted without message")),
				"IMAP APPEND accepted early OK without literal");
		} else if ( iMode == TEST_IMAP_APPEND_CANCEL ) {
			xcancel* pCancel = xrtCancelCreate();

			testRequire(pCancel != NULL,
				"IMAP APPEND cancellation creation failed");
			testRequire(xrtImapClientAppendBegin(
				pClient, &AppendConfig, Deadline, NULL
			), "IMAP APPEND cancellation begin failed");
			testRequire(xrtCancelRequest(pCancel),
				"IMAP APPEND cancellation request failed");
			xrtClearError();
			testRequire(!xrtImapClientAppendWrite(
				pClient, "hello", 5u, Deadline, pCancel
			) && (xrtErrorKind(xrtGetError()) == XERR_CANCELLED) &&
				(xrtImapClientState(pClient) == XIMAP_CLIENT_FAILED) &&
				(xrtImapClientAppendRemaining(pClient) == 5u),
				"IMAP APPEND accepted a canceled literal write");
			xrtCancelDestroy(pCancel);
		} else {
			bool bAppendOk;

			testRequire(xrtImapClientAppendBegin(
				pClient, &AppendConfig, Deadline, NULL
			), "IMAP APPEND disconnect begin failed");
			xrtAtomic32Store(&Server.Release, 1u, XMEMORY_RELEASE);
			while ( !xrtAtomic32Load(&Server.Disconnected,
				XMEMORY_ACQUIRE) ) {
				testRequire(!xrtDeadlineExpired(Deadline),
					"IMAP APPEND server did not disconnect");
				xrtThreadYield();
			}
			xrtClearError();
			bAppendOk = xrtImapClientAppendWrite(
				pClient, "hello", 5u, Deadline, NULL
			);
			if ( bAppendOk ) {
				bAppendOk = xrtImapClientAppendEnd(
					pClient, NULL, Deadline, NULL
				);
			}
			testRequire(!bAppendOk &&
				(xrtImapClientState(pClient) == XIMAP_CLIENT_FAILED),
				"IMAP APPEND reused a disconnected upload session");
		}
		if ( (iMode == TEST_IMAP_APPEND_CANCEL) ||
			(iMode == TEST_IMAP_APPEND_DISCONNECT) ) {
			testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
				"IMAP APPEND cancel server did not finish");
			xrtImapClientDestroy(pClient);
		} else {
			xrtImapClientDestroy(pClient);
			testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
				"IMAP APPEND fault server did not finish");
		}
		testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
			"IMAP APPEND fault transcript mismatch");
		xrtThreadDestroy(pThread);
	}

	testRequire(xrtNetListenerClose(pListener),
		"IMAP APPEND fault listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"IMAP APPEND fault resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"IMAP APPEND fault engine destroy failed");
	return 0;
}
