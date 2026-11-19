#define XMQTT_EXAMPLE_SERVER_WS
#include "app_listener.h"
#include "app_tls.h"
#include <xmqtt/server_ws.h>
#include <xrt/http1_tls.h>
#include <xrt/websocket_upgrade.h>
#include <xrt/memory.h>
#include <string.h>

typedef struct examplewsrequest {
	serverexample* Example;
	xnetstream* Tcp;
	xtlsstream* Tls;
	uint64 Timer;
	bool Reading, Closed, Attached;
} examplewsrequest;

static void exampleWsAbort(examplewsrequest* Request)
{
	if (Request->Closed) return;
	if (Request->Tls) xrtTlsStreamAbort(Request->Tls);
	else xrtNetStreamAbort(Request->Tcp);
}

static void exampleWsTimeout(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
	(void)Worker;
	examplewsrequest* Request = Data;
	if (Request->Timer != Id) return;
	Request->Timer = 0;
	if (Result == XNET_RESULT_OK) exampleWsAbort(Request);
}

static void exampleWsDone(examplewsrequest* Request)
{
	serverexample* Example = Request->Example;
	uint64 Id = Request->Timer;
	Request->Timer = 0;
	if (Id) xrtNetEngineTimerCancelCurrent(Example->Engine, Id);
	xrtFree(Request);
	xrtAtomic32FetchSub(&Example->Upgrades, 1, XMEMORY_RELEASE);
}

static bool exampleWsSend(examplewsrequest* Request, const void* Data, size_t Bytes)
{
	if (!Request->Tls) return xrtNetStreamSend(Request->Tcp, Data, Bytes) == XNET_RESULT_OK;
	size_t Written;
	return xrtTlsStreamSend(Request->Tls, Data, Bytes, &Written) == XTLS_OK && Written == Bytes;
}

static void exampleWsUpgrade(examplewsrequest* Request, xnetbuf* Buffer)
{
	xhttpfield Fields[16], Reply[XWS_UPGRADE_RESPONSE_FIELDS_MAX];
	xhttp1head Head;
	xhttp1limits Limits;
	xrtHttp1HeadInit(&Head, Fields, sizeof(Fields) / sizeof(Fields[0]));
	xrtHttp1LimitsInit(&Limits);
	xhttp1status Status = Request->Tls ? xrtHttp1RequestParseTls(Request->Tls, &Head, &Limits, NULL) :
		xrtHttp1RequestParseBuffer(Buffer, &Head, &Limits, NULL);
	if (Status == XHTTP1_MORE) return;
	xwsupgradeserverconfig Policy;
	xrtWsUpgradeServerConfigInit(&Policy);
	Policy.Protocols = XRT_STR_LITERAL("mqtt");
	xwsupgrade Upgrade;
	xwsstreamconfig Config;
	char Wire[512];
	size_t Count, Bytes;
	if (Status != XHTTP1_READY || Head.Target.Size != 5 || memcmp(Head.Target.Data, "/mqtt", 5) ||
		!xrtWsUpgradeRequestCheck(&Head, &Policy, &Upgrade) || !Upgrade.Protocol.Size ||
		!xrtWsUpgradeStreamConfig(&Config, XWS_ROLE_SERVER, &Upgrade) ||
		!xrtWsUpgradeResponseFields((xstrview){Upgrade.Accept, XWS_ACCEPT_SIZE},
			Upgrade.Protocol, (xstrview){0}, Reply, XWS_UPGRADE_RESPONSE_FIELDS_MAX, &Count) ||
		!xrtHttp1ResponseWrite(XHTTP_VERSION_1_1, XHTTP_STATUS_SWITCHING_PROTOCOLS,
			XRT_STR_LITERAL("Switching Protocols"), Reply, Count, Wire, sizeof(Wire), &Bytes) ||
		!exampleWsSend(Request, Wire, Bytes)) {
		exampleWsAbort(Request);
		return;
	}
	xmqttserver* Server = Request->Example->Server;
	Request->Attached = Request->Tls ? xmqttServerAttachWss(Server, Request->Tls, Head.Bytes, &Config) :
		xmqttServerAttachWs(Server, Request->Tcp, Head.Bytes, &Config);
	if (!Request->Attached) exampleWsAbort(Request);
}

static void exampleWsRead(examplewsrequest* Request, xnetbuf* Buffer)
{
	if (Request->Reading) return;
	Request->Reading = true;
	exampleWsUpgrade(Request, Buffer);
	Request->Reading = false;
	if (Request->Attached || Request->Closed) exampleWsDone(Request);
}

static void exampleWsTcpRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data)
{
	(void)Stream;
	exampleWsRead(Data, Buffer);
}

static void exampleWsTlsRead(xtlsstream* Stream, const xnetbuf* Buffer, ptr Data)
{
	(void)Stream;
	(void)Buffer;
	exampleWsRead(Data, NULL);
}

static void exampleWsClosed(examplewsrequest* Request)
{
	Request->Closed = true;
	if (!Request->Reading) exampleWsDone(Request);
}

static void exampleWsTcpClosed(xnetstream* Stream, xnetresult Result, const xerror* Error, ptr Data)
{
	(void)Result;
	(void)Error;
	xrtNetStreamDestroy(Stream);
	exampleWsClosed(Data);
}

static void exampleWsTlsClosed(xtlsstream* Stream, xnetresult Result, const xerror* Error, ptr Data)
{
	(void)Result;
	(void)Error;
	xrtTlsStreamDestroy(Stream);
	exampleWsClosed(Data);
}

static bool exampleServerWsAccept(serverexample* Example, xnetstream* Stream)
{
	if (xrtAtomic32FetchAdd(&Example->Upgrades, 1, XMEMORY_RELAXED) >= 64) {
		xrtAtomic32FetchSub(&Example->Upgrades, 1, XMEMORY_RELEASE);
		return false;
	}
	examplewsrequest* Request = xrtCalloc(1, sizeof(*Request));
	if (!Request) {
		xrtAtomic32FetchSub(&Example->Upgrades, 1, XMEMORY_RELEASE);
		return false;
	}
	Request->Example = Example;
	Request->Tcp = Stream;
	Request->Timer = xrtNetEngineAfter(Example->Engine, xrtNetWorkerIndex(xrtNetStreamWorker(Stream)),
		3000, exampleWsTimeout, Request);
	static const xnetstreamevents TcpEvents = {.Read = exampleWsTcpRead, .Close = exampleWsTcpClosed};
	static const xtlsstreamevents TlsEvents = {.Read = exampleWsTlsRead, .Close = exampleWsTlsClosed};
	if (Request->Timer && (Example->Tls ?
		xrtTlsStreamAccept(Stream, Example->Tls, NULL, &TlsEvents, Request, &Request->Tls) :
		xrtNetStreamSetEvents(Stream, &TcpEvents, Request))) return true;
	exampleWsDone(Request);
	return false;
}

int main(int Argc, char** Argv)
{
	if (Argc != 2 && Argc != 4) {
		puts("usage: examples_server_ws wait [certificate.der rsa-private-key.der]");
		return Argc == 1 ? 0 : 1;
	}
	xtlsserverconfig Tls;
	if (Argc == 4 && !exampleServerLoadTls(&Tls, Argv[2], Argv[3])) return 1;
	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	Config.Allow311 = true;
#endif
	serverexample Example = {.Tls = Argc == 4 ? &Tls : NULL};
	char Endpoint[80];
	bool Ready = exampleServerStart(&Example, &Config, 2) &&
		xrtNetAddrEndpointText(&Example.Address, Endpoint, sizeof(Endpoint));
	if (Ready) {
		puts(Endpoint);
		fflush(stdout);
		(void)getchar();
	}
	bool Closed = exampleServerStop(&Example);
	if (Argc == 4) exampleServerReleaseTls(&Tls);
	return Ready && Closed ? 0 : 1;
}
