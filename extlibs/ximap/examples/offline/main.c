#include <ximap.h>

#include <stdio.h>
#include <string.h>

#include <math.h>
static inline double exampleTimerLimit(int64 Timeout)
{
    return Timeout == XRT_WAIT_FOREVER ? INFINITY : xrtTimer() + (double)Timeout / 1000.0;
}
static inline bool exampleTimerExpired(double Limit)
{
    return xrtTimer() >= Limit;
}
static inline int64 exampleTimerRemaining(double Limit)
{
    double Ms;
    if (Limit == INFINITY) return XRT_WAIT_FOREVER;
    Ms = ceil((Limit - xrtTimer()) * 1000.0);
    return Ms <= 0 ? 0 : Ms >= 0x1p63 ? INT64_MAX : (int64)Ms;
}

/* 本地演示域名只解析到本进程的回环 IMAP 服务。 */
static xnetaddr ExampleAddress;

typedef struct example_server {
	xnetlistener* Listener;
	double Deadline;
	bool Success;
} example_server;

static xnetaddrlist* exampleResolve(cstr sHost, xnetfamily Family, ptr pData)
{
	xnetaddr Address = ExampleAddress;
	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "imap.example.invalid") != 0 ) return NULL;
	Address.Port = 0u;
	return xrtNetAddrListCreate(&Address, 1u);
}

static bool exampleSend(xnetstream* pStream, cstr sText, double Deadline)
{
	size_t iSize = strlen(sText);
	for ( ;; ) {
		xnetresult Result = xrtNetStreamSend(pStream, sText, iSize);
		if ( Result == XNET_RESULT_OK ) return true;
		if ( (Result != XNET_RESULT_AGAIN) || !xrtNetStreamWait(
			pStream, XNET_STREAM_WAIT_WRITE,exampleTimerRemaining(Deadline), NULL) ) return false;
	}
}

static bool exampleExpect(xnetstream* pStream, cstr sExpected,
	double Deadline)
{
	size_t iSize = strlen(sExpected);
	size_t iUsed = 0u;
	while ( iUsed < iSize ) {
		xnetbytes* pBytes = xrtNetStreamRecv(
			pStream, iSize - iUsed,exampleTimerRemaining(Deadline), NULL);
		xbytesview Bytes;
		if ( pBytes == NULL ) return false;
		Bytes = xrtNetBytesView(pBytes);
		if ( (Bytes.Size == 0u) ||
			(memcmp(Bytes.Data, sExpected + iUsed, Bytes.Size) != 0) ) {
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		iUsed += Bytes.Size;
		xrtNetBytesDestroy(pBytes);
	}
	return true;
}

static int32 exampleServer(ptr pData)
{
	example_server* pServer = (example_server*)pData;
	xnetstream* pStream = xrtNetListenerAcceptWait(
		pServer->Listener,exampleTimerRemaining(pServer->Deadline), NULL);
	bool bOk;
	if ( pStream == NULL ) return 1;
	bOk = exampleSend(pStream, "* OK imap.example.invalid ready\r\n",
		pServer->Deadline) &&
		exampleExpect(pStream, "A00000001 CAPABILITY\r\n",
			pServer->Deadline) &&
		exampleSend(pStream,
			"* CAPABILITY IMAP4rev1\r\n"
			"A00000001 OK capability complete\r\n",
			pServer->Deadline) &&
		exampleExpect(pStream,
			"A00000002 LOGIN \"demo\" \"demo\"\r\n",
			pServer->Deadline) &&
		exampleSend(pStream, "A00000002 OK login complete\r\n",
			pServer->Deadline) &&
		exampleExpect(pStream, "A00000003 EXAMINE \"INBOX\"\r\n",
			pServer->Deadline) &&
		exampleSend(pStream,
			"* 2 EXISTS\r\n"
			"* 0 RECENT\r\n"
			"* OK [UIDVALIDITY 42] valid\r\n"
			"A00000003 OK [READ-ONLY] examined\r\n",
			pServer->Deadline) &&
		exampleExpect(pStream, "A00000004 LOGOUT\r\n",
			pServer->Deadline) &&
		exampleSend(pStream,
			"* BYE signing off\r\n"
			"A00000004 OK logout complete\r\n",
			pServer->Deadline) &&
		xrtNetStreamClose(pStream) &&
		xrtNetStreamWait(pStream, XNET_STREAM_WAIT_CLOSE,exampleTimerRemaining(pServer->Deadline), NULL);
	pServer->Success = bOk;
	xrtNetStreamDestroy(pStream);
	return bOk ? 0 : 2;
}

int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	ximapclientconfig ClientConfig;
	ximapauthconfig Auth;
	ximapmailboxinfo Mailbox;
	example_server Server = { 0 };
	xnetengine* pEngine = NULL;
	xnetresolver* pResolver = NULL;
	xnetlistener* pListener = NULL;
	ximapclient* pClient = NULL;
	xthread* pThread = NULL;
	double Deadline = exampleTimerLimit(INT64_C(10000));
	bool bOk = false;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) goto Done;
	xrtNetListenConfigInit(&ListenConfig);
	if ( !xrtNetAddrLoopback(&ListenConfig.Address,
		XNET_FAMILY_IPV4, 0u) ) goto Done;
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	if ( (pListener == NULL) ||
		!xrtNetListenerLocal(pListener, &ExampleAddress) ) goto Done;
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = exampleResolve;
	ResolverConfig.CacheEntries = 0u;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	if ( pResolver == NULL ) goto Done;
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	pThread = xrtThreadCreate(exampleServer, &Server, 0u);
	if ( pThread == NULL ) goto Done;
	xrtImapClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = "imap.example.invalid";
	ClientConfig.Net.Port = ExampleAddress.Port;
	pClient = xrtImapClientOpen(&ClientConfig,exampleTimerRemaining(Deadline), NULL);
	if ( pClient == NULL ) goto Done;
	xrtImapAuthConfigInit(&Auth);
	Auth.Method = XIMAP_AUTH_LOGIN;
	Auth.Username = XRT_STR_LITERAL("demo");
	Auth.Secret = XRT_STR_LITERAL("demo");
	/* 明文凭据仅供进程内回环演示；真实服务使用 TLS 范例。 */
	Auth.AllowPlaintext = true;
	xrtImapMailboxInfoInit(&Mailbox);
	if ( !xrtImapClientAuth(pClient, &Auth,exampleTimerRemaining(Deadline), NULL) ||
		!xrtImapClientExamine(pClient, XRT_STR_LITERAL("INBOX"),
			&Mailbox,exampleTimerRemaining(Deadline), NULL) ||
		(Mailbox.Exists != 2u) || (Mailbox.Recent != 0u) ||
		(Mailbox.UidValidity != 42u) || !Mailbox.ReadOnly ||
		(xrtImapClientState(pClient) != XIMAP_CLIENT_SELECTED) ||
		!xrtImapClientLogout(pClient,exampleTimerRemaining(Deadline), NULL) ) goto Done;
	bOk = true;

Done:
	if ( (pClient != NULL) && !bOk ) (void)xrtImapClientAbort(pClient);
	xrtImapClientDestroy(pClient);
	if ( pThread != NULL ) {
		bool bThreadDone = xrtThreadWait(pThread) == XWAIT_OK;
		bOk = bOk && bThreadDone && Server.Success &&
			(xrtThreadExitCode(pThread) == 0);
		xrtThreadDestroy(pThread);
	}
	if ( pListener != NULL ) {
		(void)xrtNetListenerClose(pListener);
		while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED )
			xrtThreadYield();
		xrtNetListenerDestroy(pListener);
	}
	if ( pResolver != NULL ) (void)xrtNetResolverDestroy(pResolver);
	if ( pEngine != NULL ) (void)xrtNetEngineDestroy(pEngine);
	if ( !bOk ) {
		fputs("offline IMAP EXAMINE failed\n", stderr);
		return 1;
	}
	puts("offline IMAP EXAMINE: INBOX has 2 messages (read-only)");
	return 0;
}
