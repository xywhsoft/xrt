#include "test.h"
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/thread.h>
#include <xrt/time.h>

typedef struct serverfixture {
	xmqttserver* Server;
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetaddr Address;
	xatomic32 ListenerClosed;
	xallocationbudget* Budget;
} serverfixture;

static bool serverAccept(xnetlistener* Listener, xnetstream* Stream, ptr Data)
{
	(void)Listener;
	serverfixture* Fixture = Data;
	xallocationscope Scope = {0};
	if (Fixture->Budget)
		testRequire(xrtAllocationBudgetEnter(Fixture->Budget, &Scope), "attach budget scope");
	bool Attached = xmqttServerAttach(Fixture->Server, Stream);
	if (Fixture->Budget) testRequire(xrtAllocationBudgetLeave(&Scope), "leave attach budget");
	return Attached;
}

static void serverListenerClosed(xnetlistener* Listener, ptr Data)
{
	(void)Listener;
	serverfixture* Fixture = Data;
	xrtAtomic32Store(&Fixture->ListenerClosed, 1, XMEMORY_RELEASE);
}

static xmqttserverpermission serverAuthorize(xmqttversion Version, const xmqttconnectpacket* Connect, ptr Data)
{
	(void)Version;
	serverfixture* Fixture = Data;
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture->Server, &Info) &&
		xmqttServerWaitFor(Fixture->Server, 0) == XWAIT_ERROR, "authorization runs outside server gate on the worker");
	xrtClearError();
	return Connect->ClientId.Size == 4 && !memcmp(Connect->ClientId.Data, "deny", 4) ?
		XMQTT_SERVER_DENY : XMQTT_SERVER_ALLOW;
}

static void serverStart(serverfixture* Fixture, bool Allow311, size_t Budget)
{
	memset(Fixture, 0, sizeof(*Fixture));
	xrtAtomic32Init(&Fixture->ListenerClosed, 0);
	xnetengineconfig EngineConfig;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
	Fixture->Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(Fixture->Engine && xrtNetEngineStart(Fixture->Engine), "server engine");
	xmqttserverconfig Config;
	testRequire(xmqttServerConfigInit(&Config), "server defaults");
	Config.Allow311 = Allow311;
	Config.MaxConnections = 2;
	Config.ConnectTimeout = 1800;
	Config.Codec.MaxPacketBytes = 256;
	Config.Authorize = serverAuthorize;
	Config.User = Fixture;
	xallocationbudget* CreateBudget;
	xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &CreateBudget) &&
		xrtAllocationBudgetEnter(CreateBudget, &Scope), "server constructor OOM scope");
	xmqttserver* Preserved = (xmqttserver*)(uintptr_t)1;
	testRequire(!xmqttServerCreate(Fixture->Engine, &Config, &Preserved) &&
		Preserved == (xmqttserver*)(uintptr_t)1 && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"constructor OOM preserves output");
	testRequire(xrtAllocationBudgetLeave(&Scope), "leave constructor budget");
	xrtClearError();
	xrtAllocationBudgetFree(CreateBudget);
	testRequire(xmqttServerCreate(Fixture->Engine, &Config, &Fixture->Server), "create broker");
	testRequire(xmqttServerWaitFor(Fixture->Server, 0) == XWAIT_ERROR &&
		!xmqttServerDestroy(Fixture->Server), "open broker cannot be destroyed");
	xrtClearError();
	if (Budget) testRequire(xrtAllocationBudgetCreate(Budget, &Fixture->Budget), "attach quota");

	xnetlistenconfig Listen;
	xrtNetListenConfigInit(&Listen);
	testRequire(xrtNetAddrLoopback(&Listen.Address, XNET_FAMILY_IPV4, 0), "loopback");
	Listen.Distribution = XNET_ACCEPT_ROUND_ROBIN;
	static const xnetlistenerevents Events = {.Accept = serverAccept, .Close = serverListenerClosed};
	Fixture->Listener = xrtNetListen(Fixture->Engine, &Listen, &Events, NULL, Fixture);
	testRequire(Fixture->Listener && xrtNetListenerLocal(Fixture->Listener, &Fixture->Address), "bound MQTT TCP listener");
}

static void serverStop(serverfixture* Fixture)
{
	testRequire(xmqttServerClose(Fixture->Server) && xmqttServerClose(Fixture->Server), "seal broker");
	testRequire(xrtNetListenerClose(Fixture->Listener), "close MQTT TCP listener");
	double Until = xrtTimer() + 5.0;
	while (!xrtAtomic32Load(&Fixture->ListenerClosed, XMEMORY_ACQUIRE)) {
		testRequire(xrtTimer() < Until, "retire MQTT listener");
		xrtSleep(1);
	}
	testRequire(xmqttServerWaitFor(Fixture->Server, 5000) == XWAIT_OK &&
		xmqttServerWaitFor(Fixture->Server, 0) == XWAIT_OK, "retire MQTT streams and timers");
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture->Server, &Info) && Info.Closing &&
		!Info.Connections && !Info.Established, "all server identities and connections drained");
	testRequire(xmqttServerDestroy(Fixture->Server), "destroy MQTT broker");
	xrtNetListenerDestroy(Fixture->Listener);
	testRequire(xrtNetEngineStop(Fixture->Engine) && xrtNetEngineDestroy(Fixture->Engine), "unpin and join MQTT engine");
	if (Fixture->Budget) {
		xallocationbudgetinfo Allocation = {.Size = sizeof(Allocation)};
		testRequire(xrtAllocationBudgetInfo(Fixture->Budget, &Allocation) && !Allocation.LiveBytes &&
			!Allocation.LiveCount && Allocation.Rejections, "attach OOM fully retired");
		xrtAllocationBudgetFree(Fixture->Budget);
	}
}

static xnetsocket serverDial(const serverfixture* Fixture)
{
	xnetsocket Socket = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
	testRequire(Socket && xrtNetSocketConnect(Socket, &Fixture->Address) == XNET_RESULT_OK, "raw loopback MQTT peer");
	testRequire(xrtNetSocketSet(Socket, XNET_OPTION_NONBLOCK, 1), "bounded nonblocking test read");
	return Socket;
}

static void serverSend(xnetsocket Socket, const void* Wire, size_t Bytes)
{
	double Until = xrtTimer() + 5.0;
	size_t Offset = 0;
	while (Offset < Bytes) {
		size_t Sent;
		xnetresult Status = xrtNetSocketSend(Socket, (const unsigned char*)Wire + Offset, Bytes - Offset, &Sent);
		testRequire(xrtTimer() < Until && (Status == XNET_RESULT_OK || Status == XNET_RESULT_AGAIN),
			"send literal MQTT peer data");
		if (Status == XNET_RESULT_OK) Offset += Sent;
		else xrtSleep(1);
	}
}

static void serverRecv(xnetsocket Socket, unsigned char* Expected, size_t Bytes)
{
	double Until = xrtTimer() + 5.0;
	size_t Offset = 0;
	while (Offset < Bytes) {
		size_t Read;
		xnetresult Status = xrtNetSocketRecv(Socket, Expected + Offset, Bytes - Offset, &Read);
		testRequire(xrtTimer() < Until && (Status == XNET_RESULT_OK || Status == XNET_RESULT_AGAIN),
			"read literal MQTT response");
		if (Status == XNET_RESULT_OK) Offset += Read;
		else xrtSleep(1);
	}
}

static void serverEof(xnetsocket Socket)
{
	double Until = xrtTimer() + 5.0;
	for (;;) {
		unsigned char Extra;
		size_t Read;
		xnetresult Status = xrtNetSocketRecv(Socket, &Extra, 1, &Read);
		if (Status == XNET_RESULT_CLOSED || Status == XNET_RESULT_ERROR) break;
		testRequire(xrtTimer() < Until && Status == XNET_RESULT_AGAIN, "expected EOF/reset without extra MQTT output");
		xrtSleep(1);
	}
	xrtClearError();
	xrtNetSocketClose(Socket);
}

static size_t serverConnectWire(unsigned char* Wire, xmqttversion Version, bool Clean, uint8 KeepAlive, const char Id[4])
{
	/* Independent CONNECT fixture, with no generated encoder or reader. */
	const unsigned char Common[] = {0x10, 16, 0, 4, 'M', 'Q', 'T', 'T', (unsigned char)Version,
		Clean ? 2 : 0, 0, KeepAlive};
	memcpy(Wire, Common, sizeof(Common));
	size_t Pos = sizeof(Common);
	if (Version == XMQTT_VERSION_5) {
		++Wire[1];
		Wire[Pos++] = 0;
	}
	Wire[Pos++] = 0;
	Wire[Pos++] = 4;
	memcpy(Wire + Pos, Id, 4);
	return Pos + 4;
}

static void serverConnack(xnetsocket Socket, xmqttversion Version, uint8 Reason)
{
	unsigned char Reply[32];
	serverRecv(Socket, Reply, 2);
	size_t Bytes = Reply[1];
	testRequire(Reply[0] == 0x20 && Bytes >= 2 && Bytes <= sizeof(Reply) - 2,
		"CONNACK frame");
	serverRecv(Socket, Reply + 2, Bytes);
	testRequire(Reply[2] == 0 && Reply[3] == Reason, "CONNACK session flag and reason");
	if (Version == XMQTT_VERSION_5 && !Reason) {
		const unsigned char Limits[] = {15, 36, 0, 37, 0, 40, 0, 41, 0, 42, 0, 39, 0, 0, 1, 0};
		testRequire(Bytes == 2 + sizeof(Limits) && !memcmp(Reply + 4, Limits, sizeof(Limits)),
			"CONNACK advertises negotiated limits");
	}
}

static void serverConnections(serverfixture* Fixture, size_t Count)
{
	double Until = xrtTimer() + 5.0;
	for (;;) {
		xmqttserverinfo Info;
		testRequire(xmqttServerInfo(Fixture->Server, &Info), "server count snapshot");
		if (Info.Connections == Count) break;
		testRequire(xrtTimer() < Until, "server connection capacity returns");
		xrtSleep(1);
	}
}

static void serverControl(void)
{
	serverfixture Fixture;
	serverStart(&Fixture, false, 0);
	const unsigned char Ping[] = {0xc0, 0};
	const unsigned char Disconnect[] = {0xe0, 0};
	unsigned char Wire[32], Pong[4];
	size_t Bytes = serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "same");
	xnetsocket Old = serverDial(&Fixture);
	for (size_t Index = 0; Index + 1 < Bytes; ++Index) serverSend(Old, Wire + Index, 1);
	Wire[Bytes] = 0xc0; Wire[Bytes + 1] = 0;
	serverSend(Old, Wire + Bytes - 1, 3);
	serverConnack(Old, XMQTT_VERSION_5, 0);
	serverRecv(Old, Pong, 2);
	testRequire(Pong[0] == 0xd0 && !Pong[1], "coalesced PING follows CONNACK");

	xnetsocket Current = serverDial(&Fixture);
	serverSend(Current, Wire, Bytes);
	serverConnack(Current, XMQTT_VERSION_5, 0);
	serverEof(Old);
	serverConnections(&Fixture, 1);
	serverSend(Current, Ping, sizeof(Ping));
	serverRecv(Current, Pong, 2);
	testRequire(Pong[0] == 0xd0 && !Pong[1], "replacement retains active identity after old drop");
	xnetsocket Idle = serverDial(&Fixture);
	serverConnections(&Fixture, 2);
	xnetsocket Over = serverDial(&Fixture);
	serverEof(Over);
	serverEof(Idle); /* Pending CONNECT timer frees one slot. */
	serverConnections(&Fixture, 1);

	xnetsocket P = serverDial(&Fixture);
	Bytes = serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "deny");
	serverSend(P, Wire, Bytes);
	serverConnack(P, XMQTT_VERSION_5, 0x87);
	serverEof(P);
	serverConnections(&Fixture, 1);
	P = serverDial(&Fixture);
	Bytes = serverConnectWire(Wire, XMQTT_VERSION_311, true, 0, "v311");
	serverSend(P, Wire, Bytes);
	serverConnack(P, XMQTT_VERSION_311, 1);
	serverEof(P);
	serverConnections(&Fixture, 1);

	P = serverDial(&Fixture);
	static const unsigned char Oversized[] = {0x10, 0x80, 0x10};
	serverSend(P, Oversized, sizeof(Oversized));
	serverEof(P);
	serverConnections(&Fixture, 1);
	P = serverDial(&Fixture);
	serverSend(P, Ping, sizeof(Ping));
	serverEof(P);
	serverConnections(&Fixture, 1);

	P = serverDial(&Fixture);
	Bytes = serverConnectWire(Wire, XMQTT_VERSION_5, true, 1, "idle");
	serverSend(P, Wire, Bytes);
	serverConnack(P, XMQTT_VERSION_5, 0);
	xrtSleep(650);
	serverSend(P, Ping, sizeof(Ping));
	serverRecv(P, Pong, 2);
	testRequire(Pong[0] == 0xd0 && !Pong[1], "keep alive extends on complete control packet");
	double LastPing = xrtTimer();
	serverEof(P);
	testRequire(xrtTimer() - LastPing > 1.10, "idle timeout observes updated keep-alive deadline");
	serverConnections(&Fixture, 1);
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && Info.TimedOut == 2 && Info.Established == 1 &&
		Info.Refused >= 3, "pending CONNECT and MQTT keep alive counted separately from active identities");

	/* Graceful peer close and active server close each release their stream. */
	serverSend(Current, Disconnect, sizeof(Disconnect));
	serverEof(Current);
	serverConnections(&Fixture, 0);
	P = serverDial(&Fixture);
	serverConnections(&Fixture, 1);
	serverStop(&Fixture);
	serverEof(P);
}

#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
static void serverCompatibility(void)
{
	serverfixture Fixture;
	serverStart(&Fixture, true, 0);
	unsigned char Wire[32];
	xnetsocket Socket = serverDial(&Fixture);
	size_t Bytes = serverConnectWire(Wire, XMQTT_VERSION_311, true, 1, "v311");
	serverSend(Socket, Wire, Bytes);
	serverConnack(Socket, XMQTT_VERSION_311, 0);
	const unsigned char PingAndQuit[] = {0xc0,0x80,0,0xe0,0};
	serverSend(Socket, PingAndQuit, sizeof(PingAndQuit));
	serverRecv(Socket, Wire, 2);
	testRequire(Wire[0] == 0xd0 && !Wire[1], "compatible MQTT 3.1.1 nonminimal PING");
	serverEof(Socket);
	serverStop(&Fixture);
}
#endif

int main(void)
{
	serverControl();
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	serverCompatibility();
#endif
	serverfixture Fixture;
	serverStart(&Fixture, false, 1);
	xnetsocket Socket = serverDial(&Fixture);
	serverEof(Socket);
	serverStop(&Fixture);
	puts("[PASS] MQTT server TCP CONNECT, limits, authorization, takeover, PING, idle deadlines, OOM and retirement");
	return 0;
}
