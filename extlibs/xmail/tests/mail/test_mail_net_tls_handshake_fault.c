#include <xrt/detail/wait.h>
#include "../../src/internal/xrt_mail_net.h"
#include "../../../../tests/fixtures/tls_server.h"
#include "../test.h"




static xnetaddr TestMailNetHandshakeAddress;




typedef struct testmailhandshakedial {
	xmailnetconfig Config;
	xcancel* Cancel;
	int64 Timeout;
	xerrkind ErrorKind;
	bool HasError;
	bool Opened;
	bool Active;
} testmailhandshakedial;




typedef enum testmailhandshakemode {
	TEST_MAIL_HANDSHAKE_CANCEL,
	TEST_MAIL_HANDSHAKE_TIMEOUT,
	TEST_MAIL_HANDSHAKE_CLOSE
} testmailhandshakemode;




static xnetaddrlist* testMailHandshakeResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address = TestMailNetHandshakeAddress;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "mail.test") != 0 ) {
		return NULL;
	}
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}




static int32 testMailHandshakeDial(ptr pData)
{
	testmailhandshakedial* pDial = (testmailhandshakedial*)pData;
	__xmailtransport Transport;
	const xerror* pError;

	memset(&Transport, 0, sizeof(Transport));
	xrtClearError();
	pDial->Opened = __xrtMailTransportOpen(
		&Transport,
		&pDial->Config,
		__xrtWaitAfter(pDial->Timeout),
		pDial->Cancel
	);
	pError = xrtGetError();
	pDial->HasError = pError != NULL;
	pDial->ErrorKind = pError != NULL ?
		xrtErrorKind(pError) : XERR_STATE;
	pDial->Active = (Transport.Tcp != NULL) || (Transport.Tls != NULL);
	__xrtMailTransportDestroy(&Transport);
	xrtClearError();
	return 0;
}




/* 读取记录头，证明 TCP 已连接且客户端实际进入 TLS 握手阶段。 */
static void testMailHandshakeHello(xnetstream* pServer)
{
	uint8 Header[5];
	size_t iOffset = 0;
	double Deadline = __xrtWaitAfter(INT64_C(3000));

	while ( iOffset < sizeof(Header) ) {
		xnetbytes* pBytes = __xrtNetStreamRecv(
			pServer, sizeof(Header) - iOffset, Deadline, NULL
		);
		xbytesview Data;
		size_t iCopy;

		testRequire(pBytes != NULL,
			"mail TLS handshake ClientHello was not received");
		Data = xrtNetBytesView(pBytes);
		testRequire(Data.Size != 0,
			"mail TLS handshake closed before ClientHello");
		iCopy = Data.Size < sizeof(Header) - iOffset ?
			Data.Size : sizeof(Header) - iOffset;
		memcpy(Header + iOffset, Data.Data, iCopy);
		iOffset += iCopy;
		xrtNetBytesDestroy(pBytes);
	}
	testRequire((Header[0] == 0x16u) && (Header[1] == 0x03u),
		"mail TLS handshake did not send a TLS handshake record");
}




static void testMailHandshakeCase(
	xnetlistener* pListener,
	const xmailnetconfig* pConfig,
	testmailhandshakemode Mode
)
{
	testmailhandshakedial Dial;
	xthread* pThread;
	xnetstream* pServer;
	double Deadline = __xrtWaitAfter(INT64_C(5000));

	memset(&Dial, 0, sizeof(Dial));
	Dial.Config = *pConfig;
	Dial.Timeout = Mode == TEST_MAIL_HANDSHAKE_TIMEOUT ?
		INT64_C(2000) : INT64_C(5000);
	if ( Mode == TEST_MAIL_HANDSHAKE_CANCEL ) {
		Dial.Cancel = xrtCancelCreate();
		testRequire(Dial.Cancel != NULL,
			"mail TLS handshake cancel creation failed");
	}
	pThread = xrtThreadCreate(testMailHandshakeDial, &Dial, 0);
	testRequire(pThread != NULL,
		"mail TLS handshake dial thread creation failed");
	pServer = __xrtNetListenerAcceptWait(pListener, Deadline, NULL);
	testRequire(pServer != NULL,
		"mail TLS handshake TCP connection was not accepted");
	testMailHandshakeHello(pServer);
	if ( Mode == TEST_MAIL_HANDSHAKE_CANCEL ) {
		testRequire(xrtCancelRequest(Dial.Cancel),
			"mail TLS handshake cancel request failed");
	} else if ( Mode == TEST_MAIL_HANDSHAKE_CLOSE ) {
		testRequire(xrtNetStreamAbort(pServer),
			"mail TLS handshake server abort failed");
	}
	testRequire(xrtThreadWaitFor(pThread, INT64_C(5000)) ==
		XWAIT_OK && xrtThreadExitCode(pThread) == 0,
		"mail TLS handshake dial did not finish");
	testRequire(!Dial.Opened && !Dial.Active && Dial.HasError &&
		(Mode == TEST_MAIL_HANDSHAKE_CLOSE ?
			(Dial.ErrorKind == XERR_CLOSED ||
			 Dial.ErrorKind == XERR_IO ||
			 Dial.ErrorKind == XERR_PROTOCOL) :
			Dial.ErrorKind == (Mode == TEST_MAIL_HANDSHAKE_CANCEL ?
				XERR_CANCELLED : XERR_TIMEOUT)),
		"mail TLS handshake terminal error or transport state mismatch");
	xrtThreadDestroy(pThread);
	xrtCancelDestroy(Dial.Cancel);
	(void)xrtNetStreamAbort(pServer);
	xrtNetStreamDestroy(pServer);
}




int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetresolverconfig ResolverConfig;
	xtlsverifierconfig VerifierConfig;
	xmailnetconfig Config;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetresolver* pResolver;
	xtlscontext* pContext;
	xtlsverifier* pVerifier;

	pContext = testTlsServerContext();
	testRequire(pContext != NULL,
		"mail TLS handshake context creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"mail TLS handshake verifier creation failed");
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL && xrtNetEngineStart(pEngine),
		"mail TLS handshake engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(&ListenConfig.Address,
		XNET_FAMILY_IPV4, 0),
		"mail TLS handshake listener address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire(pListener != NULL && xrtNetListenerLocal(
		pListener, &TestMailNetHandshakeAddress
	), "mail TLS handshake TCP listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testMailHandshakeResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"mail TLS handshake resolver creation failed");
	xrtMailNetConfigInit(&Config);
	Config.Engine = pEngine;
	Config.Resolver = pResolver;
	Config.Host = "mail.test";
	Config.Port = TestMailNetHandshakeAddress.Port;
	Config.Security = XMAIL_SECURITY_TLS;
	Config.Tls.Context = pContext;
	Config.Tls.Verifier = pVerifier;
	testRequire(xrtMailNetConfigValid(&Config),
		"mail TLS handshake config was invalid");
	testMailHandshakeCase(pListener, &Config, TEST_MAIL_HANDSHAKE_CANCEL);
	testMailHandshakeCase(pListener, &Config, TEST_MAIL_HANDSHAKE_TIMEOUT);
	testMailHandshakeCase(pListener, &Config, TEST_MAIL_HANDSHAKE_CLOSE);
	testRequire(xrtNetListenerClose(pListener),
		"mail TLS handshake listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"mail TLS handshake resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsContextRelease(pContext);
	testRequire(xrtNetEngineDestroy(pEngine),
		"mail TLS handshake engine destroy failed");
	return 0;
}
