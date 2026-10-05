#include <xrt/detail/wait.h>
#include <xsmtp.h>

#include <stdio.h>
#include <string.h>

/* 本地演示域名只解析到本进程的回环 SMTP 服务。 */
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
	if ( strcmp(sHost, "smtp.example.invalid") != 0 ) return NULL;
	Address.Port = 0u;
	return xrtNetAddrListCreate(&Address, 1u);
}

static bool exampleSend(xnetstream* pStream, cstr sText, double Deadline)
{
	size_t iSize = strlen(sText);
	for ( ;; ) {
		xnetresult Result = xrtNetStreamSend(pStream, sText, iSize);
		if ( Result == XNET_RESULT_OK ) return true;
		if ( (Result != XNET_RESULT_AGAIN) || !__xrtNetStreamWait(
			pStream, XNET_STREAM_WAIT_WRITE, Deadline, NULL) ) return false;
	}
}

static bool exampleExpect(xnetstream* pStream, cstr sExpected,
	double Deadline)
{
	size_t iSize = strlen(sExpected);
	size_t iUsed = 0u;
	while ( iUsed < iSize ) {
		xnetbytes* pBytes = __xrtNetStreamRecv(
			pStream, iSize - iUsed, Deadline, NULL);
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

/* 有界读取 DATA，逐字节读取可避免越过终止符消费下一条命令。 */
static bool exampleExpectMessage(xnetstream* pStream, double Deadline)
{
	char sMessage[8193];
	size_t iUsed = 0u;
	while ( iUsed < sizeof(sMessage) - 1u ) {
		xnetbytes* pBytes = __xrtNetStreamRecv(pStream, 1u, Deadline, NULL);
		xbytesview Bytes;
		if ( pBytes == NULL ) return false;
		Bytes = xrtNetBytesView(pBytes);
		if ( (Bytes.Size != 1u) || (Bytes.Data[0] == 0u) ) {
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		sMessage[iUsed++] = (char)Bytes.Data[0];
		xrtNetBytesDestroy(pBytes);
		if ( (iUsed >= 5u) &&
			(memcmp(sMessage + iUsed - 5u, "\r\n.\r\n", 5u) == 0) ) {
			sMessage[iUsed] = 0;
			return strstr(sMessage, "Subject: Offline SMTP example") != NULL &&
				strstr(sMessage, "Hello from loopback.") != NULL &&
				strstr(sMessage, "sender@example.test") != NULL &&
				strstr(sMessage, "recipient@example.test") != NULL;
		}
	}
	return false;
}

static int32 exampleServer(ptr pData)
{
	example_server* pServer = (example_server*)pData;
	xnetstream* pStream = __xrtNetListenerAcceptWait(
		pServer->Listener, pServer->Deadline, NULL);
	bool bOk;
	if ( pStream == NULL ) return 1;
	bOk = exampleSend(pStream, "220 smtp.example.invalid ready\r\n",
		pServer->Deadline) &&
		exampleExpect(pStream, "EHLO offline.example\r\n",
			pServer->Deadline) &&
		exampleSend(pStream, "250 smtp.example.invalid\r\n",
			pServer->Deadline) &&
		exampleExpect(pStream, "MAIL FROM:<sender@example.test>\r\n",
			pServer->Deadline) &&
		exampleSend(pStream, "250 sender accepted\r\n",
			pServer->Deadline) &&
		exampleExpect(pStream, "RCPT TO:<recipient@example.test>\r\n",
			pServer->Deadline) &&
		exampleSend(pStream, "250 recipient accepted\r\n",
			pServer->Deadline) &&
		exampleExpect(pStream, "DATA\r\n", pServer->Deadline) &&
		exampleSend(pStream, "354 send message\r\n", pServer->Deadline) &&
		exampleExpectMessage(pStream, pServer->Deadline) &&
		exampleSend(pStream, "250 queued\r\n", pServer->Deadline) &&
		exampleExpect(pStream, "QUIT\r\n", pServer->Deadline) &&
		exampleSend(pStream, "221 closing\r\n", pServer->Deadline) &&
		xrtNetStreamClose(pStream) &&
		__xrtNetStreamWait(pStream, XNET_STREAM_WAIT_CLOSE,
			pServer->Deadline, NULL);
	pServer->Success = bOk;
	xrtNetStreamDestroy(pStream);
	return bOk ? 0 : 2;
}

int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xsmtpclientconfig ClientConfig;
	xmailmessage Message;
	xmailaddress To;
	example_server Server = { 0 };
	xnetengine* pEngine = NULL;
	xnetresolver* pResolver = NULL;
	xnetlistener* pListener = NULL;
	xsmtpclient* pClient = NULL;
	xthread* pThread = NULL;
	double Deadline = __xrtWaitAfter(UINT64_C(10000000));
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
	xrtSmtpClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = "smtp.example.invalid";
	ClientConfig.Net.Port = ExampleAddress.Port;
	ClientConfig.Hello = XRT_STR_LITERAL("offline.example");
	pClient = __xrtSmtpClientOpen(&ClientConfig, Deadline, NULL);
	if ( pClient == NULL ) goto Done;
	xrtMailMessageInit(&Message);
	Message.From = (xmailaddress){
		XRT_STR_LITERAL(""), XRT_STR_LITERAL("sender@example.test")
	};
	To = (xmailaddress){
		XRT_STR_LITERAL(""), XRT_STR_LITERAL("recipient@example.test")
	};
	Message.To = &To;
	Message.ToCount = 1u;
	Message.Subject = XRT_STR_LITERAL("Offline SMTP example");
	Message.Text = XRT_STR_LITERAL("Hello from loopback.");
	if ( !__xrtSmtpSubmit(pClient, &Message, Deadline, NULL) ||
		!__xrtSmtpClientQuit(pClient, Deadline, NULL) ) goto Done;
	bOk = true;

Done:
	if ( (pClient != NULL) && !bOk ) (void)xrtSmtpClientAbort(pClient);
	xrtSmtpClientDestroy(pClient);
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
		fputs("offline SMTP submission failed\n", stderr);
		return 1;
	}
	puts("offline SMTP submission: message accepted");
	return 0;
}
