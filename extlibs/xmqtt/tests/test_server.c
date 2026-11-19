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

static xmqttserverpermission serverAuthorizeTopic(xmqttversion Version, xstrview ClientId,
	xstrview Topic, bool Publishing, ptr Data)
{
	(void)Version;
	(void)Publishing;
	serverfixture* Fixture = Data;
	xmqttserverinfo Info;
	testRequire(ClientId.Size == 4 && ClientId.Data && xmqttServerInfo(Fixture->Server, &Info),
		"topic policy receives stable ClientId outside broker gates");
	return Topic.Size == 3 && !memcmp(Topic.Data, "bad", 3) ? XMQTT_SERVER_DENY : XMQTT_SERVER_ALLOW;
}

static void serverStartConfigured(serverfixture* Fixture, bool Allow311, size_t Budget, bool Routing, size_t WildcardNodes)
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
	Config.MaxWildcardNodes = WildcardNodes;
	Config.Authorize = serverAuthorize;
	Config.User = Fixture;
	if (Routing) {
		Config.MaxConnections = 3;
		Config.MaxSubscriptions = 4;
		Config.MaxSubscriptionsPerConnection = 2;
		Config.MaxSubscribersPerTopic = 2;
		Config.AuthorizeTopic = serverAuthorizeTopic;
	}
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
	if (Routing) {
		Listen.Stream.WriteLimit = 128;
		Listen.Stream.WriteHighWater = 64;
		Listen.Stream.WriteLowWater = 32;
	}
	static const xnetlistenerevents Events = {.Accept = serverAccept, .Close = serverListenerClosed};
	Fixture->Listener = xrtNetListen(Fixture->Engine, &Listen, &Events, NULL, Fixture);
	testRequire(Fixture->Listener && xrtNetListenerLocal(Fixture->Listener, &Fixture->Address), "bound MQTT TCP listener");
}

static void serverStart(serverfixture* Fixture, bool Allow311, size_t Budget)
{
	serverStartConfigured(Fixture, Allow311, Budget, false, 65536);
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
		!Info.Connections && !Info.Established && !Info.Subscriptions && !Info.WildcardNodes,
		"all server identities, connections and subscriptions drained");
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

static void serverConnackConfigured(xnetsocket Socket, xmqttversion Version, uint8 Reason, bool Wildcard)
{
	unsigned char Reply[32];
	serverRecv(Socket, Reply, 2);
	size_t Bytes = Reply[1];
	testRequire(Reply[0] == 0x20 && Bytes >= 2 && Bytes <= sizeof(Reply) - 2,
		"CONNACK frame");
	serverRecv(Socket, Reply + 2, Bytes);
	testRequire(Reply[2] == 0 && Reply[3] == Reason, "CONNACK session flag and reason");
	if (Version == XMQTT_VERSION_5 && !Reason) {
		const unsigned char Limits[] = {15, 36, 0, 37, 0, 40, Wildcard, 41, 0, 42, 1, 39, 0, 0, 1, 0};
		testRequire(Bytes == 2 + sizeof(Limits) && !memcmp(Reply + 4, Limits, sizeof(Limits)),
			"CONNACK advertises negotiated limits");
	}
}

static void serverConnack(xnetsocket Socket, xmqttversion Version, uint8 Reason)
{
	serverConnackConfigured(Socket, Version, Reason, true);
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

static void serverSubscribeWire(xnetsocket Socket, xmqttversion Version, bool Subscribe, uint8 Id,
	const char* const* Filters, const uint8* Options, const unsigned char* Reasons, size_t Count)
{
	unsigned char Wire[96];
	size_t Pos = 0;
	Wire[Pos++] = Subscribe ? 0x82 : 0xa2;
	Wire[Pos++] = 0; /* Remaining Length is filled after the small literal body. */
	Wire[Pos++] = 0;
	Wire[Pos++] = Id;
	if (Version == XMQTT_VERSION_5) Wire[Pos++] = 0;
	for (size_t Index = 0; Index < Count; ++Index) {
		size_t Size = strlen(Filters[Index]);
		Wire[Pos++] = 0;
		Wire[Pos++] = (unsigned char)Size;
		memcpy(Wire + Pos, Filters[Index], Size);
		Pos += Size;
		if (Subscribe) Wire[Pos++] = Options[Index];
	}
	Wire[1] = (unsigned char)(Pos - 2);
	serverSend(Socket, Wire, Pos);
	serverRecv(Socket, Wire, 4);
	size_t ReasonBytes = Subscribe || Version == XMQTT_VERSION_5 ? Count : 0;
	size_t Extra = (Version == XMQTT_VERSION_5 ? 1u : 0u) + ReasonBytes;
	testRequire(Wire[0] == (Subscribe ? 0x90 : 0xb0) && Wire[1] == 2 + Extra &&
		Wire[2] == 0 && Wire[3] == Id, "control acknowledgement and packet identifier");
	if (Extra) serverRecv(Socket, Wire + 4, Extra);
	if (Version == XMQTT_VERSION_5) testRequire(Wire[4] == 0, "empty ACK properties");
	if (ReasonBytes) testRequire(!memcmp(Wire + 4 + (Version == XMQTT_VERSION_5), Reasons, ReasonBytes),
		"ordered per-filter MQTT reason codes");
}

static size_t serverPublishWire(unsigned char* Wire, xmqttversion Version, const char* Topic,
	const unsigned char* Payload, size_t Size, const unsigned char* Properties, uint8 PropertyBytes)
{
	size_t TopicBytes = strlen(Topic);
	size_t Body = 2 + TopicBytes + Size;
	if (Version == XMQTT_VERSION_5) Body += 1 + PropertyBytes;
	size_t Pos = 0;
	Wire[Pos++] = 0x30;
	Wire[Pos++] = Body < 128 ? (uint8)Body : ((uint8)Body & 0x7f) | 0x80;
	if (Body >= 128) Wire[Pos++] = (uint8)(Body >> 7);
	Wire[Pos++] = 0;
	Wire[Pos++] = (uint8)TopicBytes;
	memcpy(Wire + Pos, Topic, TopicBytes);
	Pos += TopicBytes;
	if (Version == XMQTT_VERSION_5) {
		Wire[Pos++] = PropertyBytes;
		if (PropertyBytes) memcpy(Wire + Pos, Properties, PropertyBytes);
		Pos += PropertyBytes;
	}
	memcpy(Wire + Pos, Payload, Size);
	return Pos + Size;
}

static void serverExpectPublish(xnetsocket Socket, xmqttversion Version, const char* Topic,
	const unsigned char* Payload, size_t Size, const unsigned char* Properties, uint8 PropertyBytes)
{
	unsigned char Actual[256], Expected[256];
	size_t Bytes = serverPublishWire(Expected, Version, Topic, Payload, Size, Properties, PropertyBytes);
	serverRecv(Socket, Actual, Bytes);
	testRequire(!memcmp(Actual, Expected, Bytes), "version-specific routed PUBLISH is exact and ordered");
}

static void serverPingPeer(xnetsocket Socket)
{
	unsigned char Ping[] = {0xc0, 0}, Pong[2];
	serverSend(Socket, Ping, sizeof(Ping));
	serverRecv(Socket, Pong, sizeof(Pong));
	testRequire(Pong[0] == 0xd0 && !Pong[1], "expected PINGRESP with no additional publication");
}

static void serverRouting(xmqttversion OtherVersion)
{
	serverfixture Fixture;
	serverStartConfigured(&Fixture, OtherVersion == XMQTT_VERSION_311, 0, true, 65536);
	unsigned char Wire[256];
	xnetsocket Producer = serverDial(&Fixture), Consumer = serverDial(&Fixture), Other = serverDial(&Fixture);
	serverSend(Producer, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "prod"));
	serverConnack(Producer, XMQTT_VERSION_5, 0);
	serverSend(Consumer, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "sink"));
	serverConnack(Consumer, XMQTT_VERSION_5, 0);
	size_t OtherConnect = serverConnectWire(Wire, OtherVersion, true, 0, "side");
	if (OtherVersion == XMQTT_VERSION_5) {
		const unsigned char MaxPacket[] = {39,0,0,0,64};
		memmove(Wire + 13 + sizeof(MaxPacket), Wire + 13, OtherConnect - 13);
		memcpy(Wire + 13, MaxPacket, sizeof(MaxPacket));
		Wire[12] = sizeof(MaxPacket);
		OtherConnect += sizeof(MaxPacket);
		Wire[1] += sizeof(MaxPacket);
	}
	serverSend(Other, Wire, OtherConnect);
	serverConnack(Other, OtherVersion, 0);

	const char* Mixed[] = {"data", "bad", "aux", "max"};
	const uint8 Options[] = {2, 0, 0, 0};
	const uint8 MixedReasons[] = {0, 0x87, 0, 0x97}, Grant[] = {0}, Quota[] = {0x97};
	const char* Self[] = {"self"}, *Data[] = {"data"}, *Remove[] = {"data", "data"};
	serverSubscribeWire(Consumer, XMQTT_VERSION_5, true, 1, Mixed, Options, MixedReasons, 4);
	serverSubscribeWire(Consumer, XMQTT_VERSION_5, true, 2, Data, Options + 1, Grant, 1);
	serverSubscribeWire(Other, OtherVersion, true, 1, Data, Options, Grant, 1);
	serverSubscribeWire(Producer, XMQTT_VERSION_5, true, 1, Data, Options, Quota, 1);
	const uint8 NoLocal[] = {4};
	serverSubscribeWire(Producer, XMQTT_VERSION_5, true, 2, Self, NoLocal, Grant, 1);
	serverSubscribeWire(Other, OtherVersion, true, 2, Self, Options + 1,
		OtherVersion == XMQTT_VERSION_5 ? Quota : (const uint8[]){0x80}, 1);

	static const unsigned char Content[] = {3,0,1,'x'};
	for (unsigned Index = 0; Index < 6; ++Index) {
		unsigned char Payload[] = {0, (uint8)Index, 255};
		serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, "data", Payload, sizeof(Payload), Content, sizeof(Content)));
	}
	for (unsigned Index = 0; Index < 6; ++Index) {
		unsigned char Payload[] = {0, (uint8)Index, 255};
		serverExpectPublish(Consumer, XMQTT_VERSION_5, "data", Payload, sizeof(Payload), Content, sizeof(Content));
		serverExpectPublish(Other, OtherVersion, "data", Payload, sizeof(Payload), Content, sizeof(Content));
	}
	const unsigned char Payload[] = {0,1,255};
	serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, "self", Payload, sizeof(Payload), NULL, 0));
	serverPingPeer(Producer);

	static const unsigned char Expired[] = {2,0,0,0,0};
	serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, "bad", Payload, sizeof(Payload), NULL, 0));
	serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, "data", Payload, sizeof(Payload), Expired, sizeof(Expired)));
	serverPingPeer(Producer);
	serverPingPeer(Consumer);

	const uint8 Removed[] = {0,0x11};
	serverSubscribeWire(Consumer, XMQTT_VERSION_5, false, 3, Remove, NULL, Removed, 2);
	serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, "data", Payload, sizeof(Payload), NULL, 0));
	serverExpectPublish(Other, OtherVersion, "data", Payload, sizeof(Payload), NULL, 0);
	serverPingPeer(Consumer);
	serverSubscribeWire(Consumer, XMQTT_VERSION_5, true, 4, Data, Options + 1, Grant, 1);

	uint64 Forwarded = 15, Dropped = 2;
	if (OtherVersion == XMQTT_VERSION_5) {
		const unsigned char Medium[70] = {0};
		serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, "data", Medium, sizeof(Medium), NULL, 0));
		serverExpectPublish(Consumer, XMQTT_VERSION_5, "data", Medium, sizeof(Medium), NULL, 0);
		serverPingPeer(Other);
		++Forwarded;
		++Dropped;
	}
	const unsigned char Long[150] = {0};
	serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, "data", Long, sizeof(Long), NULL, 0));
	serverPingPeer(Producer);
	serverPingPeer(Consumer);
	serverPingPeer(Other);
	serverSend(Other, Wire, serverPublishWire(Wire, OtherVersion, "data", Payload, sizeof(Payload), NULL, 0));
	serverExpectPublish(Consumer, XMQTT_VERSION_5, "data", Payload, sizeof(Payload), NULL, 0);
	serverExpectPublish(Other, OtherVersion, "data", Payload, sizeof(Payload), NULL, 0);
	serverPingPeer(Other);

	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && Info.Subscriptions == 4 &&
		Info.Forwarded == Forwarded && Info.RejectedPublications == 2 && Info.DroppedDeliveries == Dropped,
		"bounded subscription, accepted deliveries and overload counters");
	serverStop(&Fixture);
	serverEof(Producer);
	serverEof(Consumer);
	serverEof(Other);
}

static void serverPublishCheck(xnetsocket Producer, xnetsocket Consumer, xmqttversion Version,
	const char* Name, bool Expected)
{
	const unsigned char Payload[] = {0, 0x74, 0xff};
	unsigned char Wire[60];
	serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, Name, Payload, sizeof(Payload), NULL, 0));
	serverPingPeer(Producer);
	if (Expected) serverExpectPublish(Consumer, Version, Name, Payload, sizeof(Payload), NULL, 0);
	serverPingPeer(Consumer);
}

static void serverWildcardRoutes(xmqttversion Version)
{
	serverfixture Fixture;
	serverStart(&Fixture, Version == XMQTT_VERSION_311, 0);
	unsigned char Wire[32];
	xnetsocket Producer = serverDial(&Fixture), Consumer = serverDial(&Fixture);
	serverSend(Producer, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "wild"));
	serverConnack(Producer, XMQTT_VERSION_5, 0);
	serverSend(Consumer, Wire, serverConnectWire(Wire, Version, true, 0, "sink"));
	serverConnack(Consumer, Version, 0);

	const char* Filters[] = {"a/#", "a/+/c", "a//c", "$SYS/+", "/+"};
	const uint8 Granted[5] = {0};
	serverSubscribeWire(Consumer, Version, true, 1, Filters, Granted, Granted, 5);
	serverPublishCheck(Producer, Consumer, Version, "a", true);
	serverPublishCheck(Producer, Consumer, Version, "a//c", true);
	serverPublishCheck(Producer, Consumer, Version, "/", true);
	serverPublishCheck(Producer, Consumer, Version, "$SYS/time", true);
	serverPublishCheck(Producer, Consumer, Version, "$SYS/time/extra", false);
	serverPublishCheck(Producer, Consumer, Version, "A", false);
	serverSubscribeWire(Consumer, Version, false, 2, Filters, NULL, Granted, 1);
	serverPublishCheck(Producer, Consumer, Version, "a", false);
	serverPublishCheck(Producer, Consumer, Version, "a//c", true);
	const char* Root[] = {"+/#", "#"};
	serverSubscribeWire(Consumer, Version, true, 3, Root, Granted, Granted, 2);
	serverPublishCheck(Producer, Consumer, Version, "alone", true);
	serverPublishCheck(Producer, Consumer, Version, "$OTHER/status", false);
	serverPublishCheck(Producer, Consumer, Version, "$SYS/time", true);
	/* A matching No Local branch must not suppress overlapping matches
	 * whose options permit the same publisher to receive the publication. */
	const char* Own[] = {"local/#", "local/+"};
	const uint8 NoLocal[] = {4, 0};
	serverSubscribeWire(Producer, XMQTT_VERSION_5, true, 4, Own, NoLocal, Granted, 2);
	const unsigned char Payload[] = {0x68, 0, 0xff};
	serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, "local/x", Payload, sizeof(Payload), NULL, 0));
	serverExpectPublish(Producer, XMQTT_VERSION_5, "local/x", Payload, sizeof(Payload), NULL, 0);
	serverExpectPublish(Consumer, Version, "local/x", Payload, sizeof(Payload), NULL, 0);
	serverPingPeer(Producer);
	serverPingPeer(Consumer);
	/* Retirement of a clean session also prunes the exact and wildcard
	 * index entries. All socket owners are still alive during broker Close. */
	serverStop(&Fixture);
	serverEof(Producer);
	serverEof(Consumer);
}

static void serverSharedRoutes(void)
{
	serverfixture Fixture;
	serverStartConfigured(&Fixture, false, 0, true, 65536);
	unsigned char Wire[64];
	xnetsocket Producer = serverDial(&Fixture), A = serverDial(&Fixture), B = serverDial(&Fixture);
	serverSend(Producer, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "prod"));
	serverConnack(Producer, XMQTT_VERSION_5, 0);
	serverSend(A, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "team"));
	serverConnack(A, XMQTT_VERSION_5, 0);
	serverSend(B, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "peer"));
	serverConnack(B, XMQTT_VERSION_5, 0);

	const char* Team[] = {"$share/team/data/#"};
	const char* Ordinary[] = {"data/item"};
	const char* OtherGroup[] = {"$share/alt/+/#"};
	const char* ExactGroup[] = {"$share/exact/data/item"};
	const uint8 Grant[] = {0}, Absent[] = {0x11}, Quota[] = {0x97};
	serverSubscribeWire(A, XMQTT_VERSION_5, true, 1, Team, Grant, Grant, 1);
	serverSubscribeWire(A, XMQTT_VERSION_5, true, 2, Team, Grant, Grant, 1);
	serverSubscribeWire(B, XMQTT_VERSION_5, true, 1, Team, Grant, Grant, 1);
	serverSubscribeWire(A, XMQTT_VERSION_5, true, 3, Ordinary, Grant, Grant, 1);
	serverSubscribeWire(B, XMQTT_VERSION_5, true, 2, OtherGroup, Grant, Grant, 1);
	serverSubscribeWire(A, XMQTT_VERSION_5, true, 4, ExactGroup, Grant, Quota, 1);

	const unsigned char Payload[] = {0, 0x74, 0xff};
	for (unsigned Round = 0; Round < 2; ++Round) {
		serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, "data/item",
			Payload, sizeof(Payload), NULL, 0));
		serverPingPeer(Producer);
		/* The same shared filter alternates independently of the ordinary
		 * match and the second, overlapping shared filter. */
		for (unsigned I = 0; I < 2 - Round; ++I)
			serverExpectPublish(A, XMQTT_VERSION_5, "data/item", Payload, sizeof(Payload), NULL, 0);
		for (unsigned I = 0; I < 1 + Round; ++I)
			serverExpectPublish(B, XMQTT_VERSION_5, "data/item", Payload, sizeof(Payload), NULL, 0);
		serverPingPeer(A);
		serverPingPeer(B);
	}
	serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, "$SYS/time",
		Payload, sizeof(Payload), NULL, 0));
	serverPingPeer(Producer);
	serverPingPeer(A);
	serverPingPeer(B);

	serverSubscribeWire(A, XMQTT_VERSION_5, false, 5, Team, NULL, Grant, 1);
	serverSubscribeWire(A, XMQTT_VERSION_5, false, 6, Team, NULL, Absent, 1);
	serverSubscribeWire(A, XMQTT_VERSION_5, true, 7, ExactGroup, Grant, Grant, 1);
	serverSend(Producer, Wire, serverPublishWire(Wire, XMQTT_VERSION_5, "data/item",
		Payload, sizeof(Payload), NULL, 0));
	serverPingPeer(Producer);
	for (unsigned I = 0; I < 2; ++I) {
		serverExpectPublish(A, XMQTT_VERSION_5, "data/item", Payload, sizeof(Payload), NULL, 0);
		serverExpectPublish(B, XMQTT_VERSION_5, "data/item", Payload, sizeof(Payload), NULL, 0);
	}

	const unsigned char Disconnect[] = {0xe0, 0};
	serverSend(B, Disconnect, sizeof(Disconnect));
	serverEof(B);
	serverConnections(&Fixture, 2);
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture.Server, &Info) &&
		Info.Subscriptions == 2 && !Info.WildcardNodes, "retirement removes empty shared groups and trie branches");
	serverSubscribeWire(A, XMQTT_VERSION_5, false, 8, Ordinary, NULL, Grant, 1);
	serverPublishCheck(Producer, A, XMQTT_VERSION_5, "data/item", true);
	serverStop(&Fixture);
	serverEof(Producer);
	serverEof(A);
}

typedef struct serverWildcardSender {
	xnetsocket Socket;
	xatomic32* Start;
	const char* Topic;
	uint8 Source, Rounds;
} serverWildcardSender;

static int32 serverWildcardSend(ptr Data)
{
	serverWildcardSender* Sender = Data;
	double Until = xrtTimer() + 5.0;
	while (!xrtAtomic32Load(Sender->Start, XMEMORY_ACQUIRE)) {
		testRequire(xrtTimer() < Until, "wildcard producer start");
		xrtSleep(1);
	}
	for (uint8 Index = 0; Index < Sender->Rounds; ++Index) {
		unsigned char Wire[32];
		const unsigned char Payload[] = {Sender->Source, Index};
		size_t Size = serverPublishWire(Wire, XMQTT_VERSION_5, Sender->Topic, Payload, sizeof(Payload), NULL, 0);
		serverSend(Sender->Socket, Wire, Size);
	}
	serverPingPeer(Sender->Socket);
	return 0;
}

static void serverConcurrentRoutes(bool Shared)
{
	serverfixture Fixture;
	serverStartConfigured(&Fixture, false, 0, true, 65536);
	xnetsocket Socket[3];
	const char* Ids[] = {"src0", "src1", "sink"};
	for (size_t Index = 0; Index < 3; ++Index) {
		unsigned char Wire[32];
		Socket[Index] = serverDial(&Fixture);
		size_t Size = serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, Ids[Index]);
		serverSend(Socket[Index], Wire, Size);
		serverConnack(Socket[Index], XMQTT_VERSION_5, 0);
	}
	const char* Filters[] = {"#", Shared ? "$share/group/+/+" : "a/+"};
	const uint8 Accepted[] = {0, 0};
	serverSubscribeWire(Socket[2], XMQTT_VERSION_5, true, 1, Filters, Accepted, Accepted, 2);
	const char* Topics[] = {"a/x", "b/x"};
	xatomic32 Start;
	xrtAtomic32Init(&Start, 0);
	const uint8 Copies = Shared ? 2 : 1, Rounds = Shared ? 3 : 4;
	serverWildcardSender Senders[] = {
		{Socket[0], &Start, Topics[0], 0, Rounds}, {Socket[1], &Start, Topics[1], 1, Rounds}
	};
	xthread* Threads[2] = {
		xrtThreadCreate(serverWildcardSend, &Senders[0], 0), xrtThreadCreate(serverWildcardSend, &Senders[1], 0)
	};
	testRequire(Threads[0] && Threads[1], "concurrent wildcard producers");
	xrtAtomic32Store(&Start, 1, XMEMORY_RELEASE);
	uint8 Next[] = {0, 0};
	for (size_t Index = 0; Index < 2u * Copies * Rounds; ++Index) {
		/* Overlapping ordinary matches give one copy. A matching shared
		 * subscription contributes its own delivery to the same peer. */
		unsigned char Received[10];
		serverRecv(Socket[2], Received, sizeof(Received));
		uint8 Source = Received[8];
		testRequire(Source < 2 && Next[Source] < Copies * Rounds, "bounded wildcard source sequence");
		unsigned char Expected[10];
		const unsigned char Payload[] = {Source, Next[Source]++ / Copies};
		testRequire(serverPublishWire(Expected, XMQTT_VERSION_5, Topics[Source], Payload, sizeof(Payload),
			NULL, 0) == sizeof(Expected) && !memcmp(Received, Expected, sizeof(Expected)),
			"concurrent wildcard publication order and bytes");
	}
	serverPingPeer(Socket[2]);
	for (size_t Index = 0; Index < 2; ++Index) {
		testRequire(xrtThreadWaitFor(Threads[Index], 5000) == XWAIT_OK, "wildcard producer joins");
		xrtThreadDestroy(Threads[Index]);
	}
	testRequire(Next[0] == Copies * Rounds && Next[1] == Copies * Rounds, "both wildcard producers fully delivered");
	serverStop(&Fixture);
	for (size_t Index = 0; Index < 3; ++Index) serverEof(Socket[Index]);
}

static void serverWildcardQuota(void)
{
	serverfixture Fixture;
	serverStartConfigured(&Fixture, false, 0, false, 3);
	unsigned char Wire[32];
	xnetsocket Socket = serverDial(&Fixture);
	serverSend(Socket, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "quot"));
	serverConnack(Socket, XMQTT_VERSION_5, 0);
	unsigned char Granted[] = {0}, Quota[] = {0x97};
	const char* First[] = {"a/#"}, *Second[] = {"b/h/#"};
	serverSubscribeWire(Socket, XMQTT_VERSION_5, true, 1, First, Granted, Granted, 1);
	serverSubscribeWire(Socket, XMQTT_VERSION_5, true, 2, Second, Granted, Quota, 1);
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && Info.Subscriptions == 1 && Info.WildcardNodes == 2,
		"node capacity failure prunes partially inserted prefix and keeps old route");
	serverSubscribeWire(Socket, XMQTT_VERSION_5, false, 3, First, NULL, Granted, 1);
	serverSubscribeWire(Socket, XMQTT_VERSION_5, true, 4, Second, Granted, Granted, 1);
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && Info.Subscriptions == 1 && Info.WildcardNodes == 3,
		"remaining trie budget is reusable after unsubscribe");
	serverStop(&Fixture);
	serverEof(Socket);
}

static void serverAdvertisedCapabilities(void)
{
	serverfixture Fixture;
	serverStartConfigured(&Fixture, false, 0, false, 0);
	static const struct {unsigned char Wire[24]; size_t Size; uint8 Reason;} Cases[] = {
		{{0x82,8,0,1,0,0,2,'a','#',0}, 10, 0}, /* Incorrect wildcard syntax closes without response. */
		{{0x82,9,0,1,0,0,3,'a','/','#',0}, 11, 0xa2},
		{{0x82,16,0,1,0,0,10,'$','s','h','a','r','e','/','g','/','a',4}, 18, 0},
		{{0x82,9,0,1,2,11,1,0,1,'a',0}, 11, 0xa1},
		{{0x31,4,0,1,'a',0}, 6, 0x9a},
		{{0x32,6,0,1,'a',0,1,0}, 8, 0x9b},
		{{0x30,7,0,1,'a',3,35,0,1}, 9, 0x94}
	};
	for (size_t Index = 0; Index < sizeof(Cases)/sizeof(Cases[0]); ++Index) {
		unsigned char Wire[32];
		xnetsocket Socket = serverDial(&Fixture);
		serverSend(Socket, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "caps"));
		serverConnackConfigured(Socket, XMQTT_VERSION_5, 0, false);
		serverSend(Socket, Cases[Index].Wire, Cases[Index].Size);
		if (Cases[Index].Reason) {
			serverRecv(Socket, Wire, 3);
			testRequire(Wire[0] == 0xe0 && Wire[1] == 1 && Wire[2] == Cases[Index].Reason,
				"MQTT 5 reason for negotiated capability violation");
		}
		serverEof(Socket);
		serverConnections(&Fixture, 0);
	}
	serverStop(&Fixture);
}

#if defined(XMQTT_IMPLEMENTATION_ONCE)
static void serverRoutingOom(void)
{
	serverfixture Fixture;
	serverStart(&Fixture, false, 0);
	xmqttserverpeer Peer = {.Server = Fixture.Server};
	const size_t Limits[] = {1, sizeof(xmqttserverlink)};
	for (size_t Index = 0; Index < sizeof(Limits)/sizeof(Limits[0]); ++Index) {
		xallocationbudget* Budget;
		xallocationscope Scope = {0};
		testRequire(xrtAllocationBudgetCreate(Limits[Index], &Budget) &&
			xrtAllocationBudgetEnter(Budget, &Scope), "index failpoint scope");
		uint8 Reason = xmqttServerSubscribe(&Peer, XRT_BYTES_LITERAL("index-failure"), (xbytesview){0}, false);
		testRequire(xrtAllocationBudgetLeave(&Scope) && Reason == 0x83 && !Peer.Links &&
			!Peer.LinkCount, "subscription node or topic insertion OOM rolls back");
		xallocationbudgetinfo Allocation = {.Size = sizeof(Allocation)};
		testRequire(xrtAllocationBudgetInfo(Budget, &Allocation) && Allocation.Rejections &&
			!Allocation.LiveBytes, "no live charges after index failure");
		xrtClearError();
		xrtAllocationBudgetFree(Budget);
	}
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && !Info.Subscriptions, "aggregate quota restored");
	testRequire(xmqttServerSubscribe(&Peer, XRT_BYTES_LITERAL("index-failure"), (xbytesview){0}, false) == 0 &&
		xmqttServerUnsubscribe(&Peer, XRT_BYTES_LITERAL("index-failure"), (xbytesview){0}), "successful retry after OOM");
	xallocationbudget* TrieBudget;
	xallocationscope TrieScope = {0};
	testRequire(xrtAllocationBudgetCreate(sizeof(xmqttserverwildnode) + 1, &TrieBudget) &&
		xrtAllocationBudgetEnter(TrieBudget, &TrieScope), "trie node failpoint scope");
	xmqttservertopic Filter = {.Key = XRT_BYTES_LITERAL("x/y/+/#")};
	xmqttservershard* Shard = xmqttServerWildcardShard(Fixture.Server, Filter.Key);
	uint8 Reason = xmqttServerAddWildcard(Fixture.Server, Shard, &Filter);
	testRequire(xrtAllocationBudgetLeave(&TrieScope) && Reason == 0x83 && !Filter.Wildcard &&
		!Shard->WildcardRoot.Children.Root && xmqttServerInfo(Fixture.Server, &Info) &&
		!Info.WildcardNodes, "trie OOM prunes partial branch");
	xallocationbudgetinfo TrieAllocation = {.Size = sizeof(TrieAllocation)};
	testRequire(xrtAllocationBudgetInfo(TrieBudget, &TrieAllocation) &&
		TrieAllocation.Rejections && !TrieAllocation.LiveBytes, "partial trie leaves no live allocations");
	xrtClearError();
	xrtAllocationBudgetFree(TrieBudget);
	unsigned char Deep[XMQTT_SERVER_MAX_WILDCARD_LEVELS + 1];
	memset(Deep, '/', sizeof(Deep) - 1);
	Deep[sizeof(Deep) - 1] = '#';
	Filter.Key = (xbytesview){Deep, sizeof(Deep)};
	Shard = xmqttServerWildcardShard(Fixture.Server, Filter.Key);
	testRequire(xmqttServerAddWildcard(Fixture.Server, Shard, &Filter) == 0x97 &&
		!Shard->WildcardRoot.Children.Root && xmqttServerInfo(Fixture.Server, &Info) &&
		!Info.WildcardNodes, "depth limit rolls back empty path");

	xmqttservertargets Targets = {0};
	Targets.Slots = Targets.Local;
	Targets.Capacity = sizeof(Targets.Local) / sizeof(Targets.Local[0]);
	xmqttserverpeer TargetsFixture[49];
	for (size_t Index = 0; Index < 48; ++Index)
		testRequire(xmqttServerAddTarget(&Targets, TargetsFixture + Index), "local recipient set");
	xallocationbudget* SetBudget;
	xallocationscope SetScope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &SetBudget) &&
		xrtAllocationBudgetEnter(SetBudget, &SetScope), "recipient growth failpoint scope");
	bool Added = xmqttServerAddTarget(&Targets, TargetsFixture + 48);
	testRequire(xrtAllocationBudgetLeave(&SetScope) && !Added && Targets.Count == 48 &&
		Targets.Slots == Targets.Local, "recipient set growth failure keeps original entries");
	xrtClearError();
	xrtAllocationBudgetFree(SetBudget);
	testRequire(xmqttServerAddTarget(&Targets, TargetsFixture + 48) && Targets.Count == 49 &&
		xmqttServerAddTarget(&Targets, TargetsFixture) && Targets.Count == 49,
		"recipient set retry and deduplication after growth");
	xrtFree(Targets.Slots);

	xbytesview SharedKey = XRT_BYTES_LITERAL("shared/#"), Team = XRT_BYTES_LITERAL("team");
	testRequire(xmqttServerSubscribe(&Peer, SharedKey, (xbytesview){0}, false) == 0,
		"ordinary route anchors the group allocation failpoint");
	xallocationbudget* GroupBudget;
	xallocationscope GroupScope = {0};
	testRequire(xrtAllocationBudgetCreate(sizeof(xmqttserverlink), &GroupBudget) &&
		xrtAllocationBudgetEnter(GroupBudget, &GroupScope), "shared group failpoint scope");
	uint8 SharedResult = xmqttServerSubscribe(&Peer, SharedKey, Team, false);
	testRequire(xrtAllocationBudgetLeave(&GroupScope) && SharedResult == 0x83 &&
		Peer.LinkCount == 1 && !Peer.Links->Topic->Shares, "shared group OOM preserves existing route");
	xallocationbudgetinfo GroupAllocation = {.Size = sizeof(GroupAllocation)};
	testRequire(xrtAllocationBudgetInfo(GroupBudget, &GroupAllocation) &&
		GroupAllocation.Rejections && !GroupAllocation.LiveBytes, "shared group OOM rolls back its link and quota");
	xrtClearError();
	xrtAllocationBudgetFree(GroupBudget);
	testRequire(xmqttServerSubscribe(&Peer, SharedKey, Team, false) == 0 &&
		xmqttServerUnsubscribe(&Peer, SharedKey, Team) &&
		xmqttServerUnsubscribe(&Peer, SharedKey, (xbytesview){0}), "shared group allocation retries cleanly");

	xmqttservershare Shares[17] = {0};
	for (size_t Index = 0; Index < 15; ++Index) Shares[Index].Next = Shares + Index + 1;
	xmqttservertopic SharedTopic = {.Shares = Shares};
	Targets.Shares = Targets.LocalShares;
	Targets.ShareCapacity = sizeof(Targets.LocalShares) / sizeof(Targets.LocalShares[0]);
	testRequire(xmqttServerCollectShares(&Targets, &SharedTopic) && Targets.ShareCount == 16,
		"shared groups fit within the local batch");
	SharedTopic.Shares = Shares + 16;
	xallocationbudget* ShareBatchBudget;
	xallocationscope ShareBatchScope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &ShareBatchBudget) &&
		xrtAllocationBudgetEnter(ShareBatchBudget, &ShareBatchScope), "group batch growth failpoint");
	bool Collected = xmqttServerCollectShares(&Targets, &SharedTopic);
	testRequire(xrtAllocationBudgetLeave(&ShareBatchScope) && !Collected && Targets.ShareCount == 16 &&
		Targets.Shares == Targets.LocalShares, "group batch allocation failure preserves matching groups");
	xrtClearError();
	xrtAllocationBudgetFree(ShareBatchBudget);
	testRequire(xmqttServerCollectShares(&Targets, &SharedTopic) && Targets.ShareCount == 17 &&
		Targets.Shares[0] == Shares && Targets.Shares[16] == Shares + 16, "group batch growth retry");
	xrtFree(Targets.Shares);
	serverStop(&Fixture);
}
#endif

int main(void)
{
	serverControl();
	serverRouting(XMQTT_VERSION_5);
	serverAdvertisedCapabilities();
	serverWildcardRoutes(XMQTT_VERSION_5);
	serverWildcardQuota();
	serverSharedRoutes();
	serverConcurrentRoutes(false);
	serverConcurrentRoutes(true);
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	serverCompatibility();
	serverRouting(XMQTT_VERSION_311);
	serverWildcardRoutes(XMQTT_VERSION_311);
#endif
#if defined(XMQTT_IMPLEMENTATION_ONCE)
	serverRoutingOom();
#endif
	serverfixture Fixture;
	serverStart(&Fixture, false, 1);
	xnetsocket Socket = serverDial(&Fixture);
	serverEof(Socket);
	serverStop(&Fixture);
	puts("[PASS] MQTT TCP CONNECT, exact, wildcard and shared QoS0 routing, quotas, authorization, takeover, deadlines, OOM and retirement");
	return 0;
}
