#include <xpop3.h>

#include <stdio.h>
#include <string.h>

/* 只供本进程的回环 POP3 演示服务器使用。 */
static xnetaddr ExampleAddress;

typedef struct example_server {
	xnetlistener* Listener;
	xdeadline Deadline;
	bool Success;
} example_server;

static xnetaddrlist* exampleResolve(cstr sHost, xnetfamily Family, ptr pData)
{
	xnetaddr Address = ExampleAddress;
	(void)Family;
	(void)pData;
	if(strcmp(sHost, "pop3.example.invalid") != 0) return NULL;
	Address.Port = 0u;
	return xrtNetAddrListCreate(&Address, 1u);
}

static bool exampleSend(xnetstream* pStream, cstr sText, xdeadline Deadline)
{
	size_t iSize = strlen(sText);
	for(;;)
	{
		xnetresult Result = xrtNetStreamSend(pStream, sText, iSize);
		if(Result == XNET_RESULT_OK) return true;
		if((Result != XNET_RESULT_AGAIN) || !xrtNetStreamWait(
			pStream, XNET_STREAM_WAIT_WRITE, Deadline, NULL)) return false;
	}
}

static bool exampleExpect(xnetstream* pStream, cstr sExpected,
	xdeadline Deadline)
{
	size_t iSize = strlen(sExpected);
	size_t iUsed = 0u;
	while(iUsed < iSize)
	{
		xnetbytes* pBytes = xrtNetStreamRecv(
			pStream, iSize - iUsed, Deadline, NULL);
		xbytesview Bytes;
		if(pBytes == NULL) return false;
		Bytes = xrtNetBytesView(pBytes);
		if((Bytes.Size == 0u) ||
			(memcmp(Bytes.Data, sExpected + iUsed, Bytes.Size) != 0))
		{
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
		pServer->Listener, pServer->Deadline, NULL);
	bool bOk;
	if(pStream == NULL) return 1;
	bOk = exampleSend(pStream, "+OK ready\r\n", pServer->Deadline) &&
		exampleExpect(pStream, "USER demo\r\n", pServer->Deadline) &&
		exampleSend(pStream, "+OK user\r\n", pServer->Deadline) &&
		exampleExpect(pStream, "PASS demo\r\n", pServer->Deadline) &&
		exampleSend(pStream, "+OK mailbox\r\n", pServer->Deadline) &&
		exampleExpect(pStream, "RETR 1\r\n", pServer->Deadline) &&
		exampleSend(pStream,
			"+OK message\r\nSubject: offline example\r\n\r\n"
			"Hello from loopback.\r\n.\r\n", pServer->Deadline) &&
		exampleExpect(pStream, "QUIT\r\n", pServer->Deadline) &&
		exampleSend(pStream, "+OK bye\r\n", pServer->Deadline) &&
		xrtNetStreamClose(pStream) &&
		xrtNetStreamWait(pStream, XNET_STREAM_WAIT_CLOSE,
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
	xpop3clientconfig ClientConfig;
	example_server Server = { 0 };
	xnetengine* pEngine = NULL;
	xnetresolver* pResolver = NULL;
	xnetlistener* pListener = NULL;
	xpop3client* pClient = NULL;
	xthread* pThread = NULL;
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	xstrview Line;
	xmailnext Next;
	size_t iLines = 0u;
	bool bContentOk = true;
	bool bOk = false;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if((pEngine == NULL) || !xrtNetEngineStart(pEngine)) goto Done;
	xrtNetListenConfigInit(&ListenConfig);
	if(!xrtNetAddrLoopback(&ListenConfig.Address,
			XNET_FAMILY_IPV4, 0u)) goto Done;
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	if((pListener == NULL) ||
		!xrtNetListenerLocal(pListener, &ExampleAddress)) goto Done;
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = exampleResolve;
	ResolverConfig.CacheEntries = 0u;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	if(pResolver == NULL) goto Done;
	Server.Listener = pListener;
	Server.Deadline = Deadline;
	pThread = xrtThreadCreate(exampleServer, &Server, 0u);
	if(pThread == NULL) goto Done;
	xrtPop3ClientConfigInit(&ClientConfig);
	ClientConfig.Net.Engine = pEngine;
	ClientConfig.Net.Resolver = pResolver;
	ClientConfig.Net.Host = "pop3.example.invalid";
	ClientConfig.Net.Port = ExampleAddress.Port;
	ClientConfig.ReadCapabilities = false;
	pClient = xrtPop3ClientOpen(&ClientConfig, Deadline, NULL);
	/* 明文凭据仅用于进程内回环演示；真实服务请用 client 范例的 TLS。 */
	if((pClient == NULL) || !xrtPop3ClientLogin(pClient,
			XRT_STR_LITERAL("demo"), XRT_STR_LITERAL("demo"),
			true, Deadline, NULL) ||
		!xrtPop3ClientRetr(pClient, 1u, Deadline, NULL)) goto Done;
	for(;;)
	{
		Next = xrtPop3ClientNext(pClient, &Line, Deadline, NULL);
		if(Next != XMAIL_NEXT_ITEM) break;
		if(iLines == 0u)
			bContentOk = bContentOk &&
				(Line.Size == strlen("Subject: offline example")) &&
				(memcmp(Line.Data, "Subject: offline example",
					Line.Size) == 0);
		else if(iLines == 1u)
			bContentOk = bContentOk && (Line.Size == 0u);
		else if(iLines == 2u)
			bContentOk = bContentOk &&
				(Line.Size == strlen("Hello from loopback.")) &&
				(memcmp(Line.Data, "Hello from loopback.",
					Line.Size) == 0);
		else bContentOk = false;
		iLines++;
	}
	if((Next != XMAIL_NEXT_END) || !bContentOk || (iLines != 3u) ||
		!xrtPop3ClientQuit(pClient, Deadline, NULL)) goto Done;
	bOk = true;

Done:
	if((pClient != NULL) && !bOk) (void)xrtPop3ClientAbort(pClient);
	xrtPop3ClientDestroy(pClient);
	if(pThread != NULL)
	{
		bool bThreadDone = xrtThreadWait(pThread) == XWAIT_OK;
		bOk = bOk && bThreadDone && Server.Success &&
			(xrtThreadExitCode(pThread) == 0);
		xrtThreadDestroy(pThread);
	}
	if(pListener != NULL)
	{
		(void)xrtNetListenerClose(pListener);
		while(xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED)
			xrtThreadYield();
		xrtNetListenerDestroy(pListener);
	}
	if(pResolver != NULL) (void)xrtNetResolverDestroy(pResolver);
	if(pEngine != NULL) (void)xrtNetEngineDestroy(pEngine);
	if(!bOk)
	{
		fputs("offline POP3 retrieval failed\n", stderr);
		return 1;
	}
	puts("offline POP3 retrieval: 3 message lines");
	return 0;
}
