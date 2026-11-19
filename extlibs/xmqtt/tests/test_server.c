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
	if (Topic.Size == 4 && !memcmp(Topic.Data, "busy", 4)) return XMQTT_SERVER_BUSY;
	return Topic.Size == 3 && !memcmp(Topic.Data, "bad", 3) ? XMQTT_SERVER_DENY : XMQTT_SERVER_ALLOW;
}

static void serverStartLimits(serverfixture* Fixture, bool Allow311, size_t Budget, bool Routing,
	size_t WildcardNodes, size_t RetainedTopics, size_t RetainedBytes, uint16 ReceiveMaximum,
	size_t OutgoingLimit)
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
	Config.MaxRetainedTopics = RetainedTopics;
	Config.MaxRetainedBytes = RetainedBytes;
	Config.ReceiveMaximum = ReceiveMaximum;
	if (OutgoingLimit) Config.MaxOutgoingQos1PerConnection = OutgoingLimit;
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

static void serverStartConfigured(serverfixture* Fixture, bool Allow311, size_t Budget,
	bool Routing, size_t WildcardNodes)
{
	serverStartLimits(Fixture, Allow311, Budget, Routing, WildcardNodes, 4096, 16u * 1024u * 1024u, 64, 0);
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
		!Info.Connections && !Info.Established && !Info.Subscriptions && !Info.WildcardNodes &&
		!Info.IncomingQos1 && !Info.OutgoingQos1 && !Info.OutgoingQos1Bytes,
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

static void serverConnackConfigured(xnetsocket Socket, xmqttversion Version, uint8 Reason, bool Wildcard, bool Retained, uint16 ReceiveMaximum)
{
	unsigned char Reply[32];
	serverRecv(Socket, Reply, 2);
	size_t Bytes = Reply[1];
	testRequire(Reply[0] == 0x20 && Bytes >= 2 && Bytes <= sizeof(Reply) - 2,
		"CONNACK frame");
	serverRecv(Socket, Reply + 2, Bytes);
	testRequire(Reply[2] == 0 && Reply[3] == Reason, "CONNACK session flag and reason");
	if (Version == XMQTT_VERSION_5 && !Reason) {
		const unsigned char Limits[] = {16, 33, (uint8)(ReceiveMaximum >> 8), (uint8)ReceiveMaximum,
			37, Retained, 40, Wildcard, 41, 0, 42, 1, 39, 0, 0, 1, 0};
		testRequire(Bytes == 2 + sizeof(Limits) && !memcmp(Reply + 4, Limits, sizeof(Limits)),
			"CONNACK advertises negotiated limits");
	}
}

static void serverConnack(xnetsocket Socket, xmqttversion Version, uint8 Reason)
{
	serverConnackConfigured(Socket, Version, Reason, true, true, 64);
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
	if (memcmp(Actual, Expected, Bytes))
		fprintf(stderr, "PUBLISH topic=%s version=%u header=%02x expected=%02x\n",
			Topic, (unsigned)Version, Actual[0], Expected[0]);
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
	const uint8 MixedReasons[] = {2, 0x87, 0, 0x97}, Grant[] = {0}, Grant2[] = {2}, Quota[] = {0x97};
	const char* Self[] = {"self"}, *Data[] = {"data"}, *Remove[] = {"data", "data"};
	serverSubscribeWire(Consumer, XMQTT_VERSION_5, true, 1, Mixed, Options, MixedReasons, 4);
	serverSubscribeWire(Consumer, XMQTT_VERSION_5, true, 2, Data, Options + 1, Grant, 1);
	serverSubscribeWire(Other, OtherVersion, true, 1, Data, Options, Grant2, 1);
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

static void serverPublishRetained(xnetsocket Producer, xmqttversion Version, const char* Topic,
	const char* Payload, const unsigned char* Properties, uint8 PropertyBytes)
{
	unsigned char Wire[256];
	size_t Size = serverPublishWire(Wire, Version, Topic, (cbytes)Payload, strlen(Payload),
		Properties, PropertyBytes);
	Wire[0] |= XMQTT_PUBLISH_RETAIN;
	serverSend(Producer, Wire, Size);
	serverPingPeer(Producer);
}

static void serverExpectRetained(xnetsocket Consumer, xmqttversion Version, const char* Topic,
	const char* Payload, const unsigned char* Properties, uint8 PropertyBytes)
{
	unsigned char Wire[256], Actual[256];
	size_t Size = serverPublishWire(Wire, Version, Topic, (cbytes)Payload, strlen(Payload),
		Properties, PropertyBytes);
	Wire[0] |= XMQTT_PUBLISH_RETAIN;
	serverRecv(Consumer, Actual, Size);
	testRequire(!memcmp(Wire, Actual, Size), "retained delivery, properties and payload");
}

static void serverRetainedRoutes(xmqttversion Origin, xmqttversion Target)
{
	serverfixture Fixture;
	serverStart(&Fixture, Origin == XMQTT_VERSION_311 || Target == XMQTT_VERSION_311, 0);
	unsigned char Wire[128];
	xnetsocket Producer = serverDial(&Fixture), Consumer = serverDial(&Fixture);
	serverSend(Producer, Wire, serverConnectWire(Wire, Origin, true, 0, "prod"));
	serverConnack(Producer, Origin, 0);
	serverSend(Consumer, Wire, serverConnectWire(Wire, Target, true, 0, "sink"));
	serverConnack(Consumer, Target, 0);

	const unsigned char Property[] = {0x03, 0, 1, 'x'};
	uint8 InputProperties = Origin == XMQTT_VERSION_5 ? sizeof(Property) : 0;
	uint8 OutputProperties = Origin == XMQTT_VERSION_5 && Target == XMQTT_VERSION_5 ? sizeof(Property) : 0;
	serverPublishRetained(Producer, Origin, "state", "old", Property, InputProperties);
	serverPublishRetained(Producer, Origin, "state", "new", Property, InputProperties);
	serverPublishRetained(Producer, Origin, "$SYS/live", "sys", NULL, 0);
	const char* Parent[] = {"state/#"};
	const uint8 Ok[] = {0}, New[] = {0x10}, Never[] = {0x20};
	const uint8* Initial = Target == XMQTT_VERSION_5 ? New : Ok;
	serverSubscribeWire(Consumer, Target, true, 1, Parent, Initial, Ok, 1);
	serverExpectRetained(Consumer, Target, "state", "new", Property, OutputProperties);
	serverSubscribeWire(Consumer, Target, true, 2, Parent, Initial, Ok, 1);
	if (Target == XMQTT_VERSION_311)
		serverExpectRetained(Consumer, Target, "state", "new", NULL, 0);
	serverPingPeer(Consumer);

	serverPublishRetained(Producer, Origin, "state", "live", Property, InputProperties);
	serverExpectPublish(Consumer, Target, "state", (cbytes)"live", 4, Property, OutputProperties);
	if (Target == XMQTT_VERSION_5) {
		const char* Exact[] = {"state"};
		const uint8 RapNever[] = {0x28};
		serverSubscribeWire(Consumer, Target, true, 3, Exact, RapNever, Ok, 1);
		serverPingPeer(Consumer);
		serverPublishRetained(Producer, Origin, "state", "rap", Property, InputProperties);
		unsigned char Expected[32], Copies[2][32];
		size_t Size = serverPublishWire(Expected, Target, "state", (cbytes)"rap", 3, Property, OutputProperties);
		serverRecv(Consumer, Copies[0], Size);
		serverRecv(Consumer, Copies[1], Size);
		testRequire((Copies[0][0] ^ Copies[1][0]) == XMQTT_PUBLISH_RETAIN,
			"overlap emits one copy for each Retain As Published choice");
		for (size_t Index = 0; Index < 2; ++Index) {
			Copies[Index][0] &= ~XMQTT_PUBLISH_RETAIN;
			testRequire(!memcmp(Copies[Index], Expected, Size), "overlapping retained live frames preserve body");
		}
		serverPingPeer(Consumer);
		serverSubscribeWire(Consumer, Target, false, 4, Exact, NULL, Ok, 1);

		const char* Shared[] = {"$share/team/state"};
		serverSubscribeWire(Consumer, Target, true, 5, Shared, Ok, Ok, 1);
		serverPingPeer(Consumer);
		serverSubscribeWire(Consumer, Target, false, 6, Parent, NULL, Ok, 1);
		const uint8 Rap[] = {0x08};
		serverSubscribeWire(Consumer, Target, true, 7, Shared, Rap, Ok, 1);
		serverPublishRetained(Producer, Origin, "state", "group", NULL, 0);
		serverExpectRetained(Consumer, Target, "state", "group", NULL, 0);
		serverSubscribeWire(Consumer, Target, false, 8, Shared, NULL, Ok, 1);
		serverSubscribeWire(Consumer, Target, true, 9, Parent, Never, Ok, 1);
		serverPingPeer(Consumer);
	}
	serverPublishRetained(Producer, Origin, "state", "", NULL, 0);
	serverExpectPublish(Consumer, Target, "state", (cbytes)"", 0, NULL, 0);
	const char* Root[] = {"#"};
	serverSubscribeWire(Consumer, Target, true, 10, Root, Ok, Ok, 1);
	serverPingPeer(Consumer); /* A root wildcard excludes $SYS and the deleted state. */
	const char* System[] = {"$SYS/+"};
	serverSubscribeWire(Consumer, Target, true, 11, System, Ok, Ok, 1);
	serverExpectRetained(Consumer, Target, "$SYS/live", "sys", NULL, 0);
	serverPingPeer(Consumer);
	if (Origin == XMQTT_VERSION_5) {
		const unsigned char Disconnect[] = {0xe0, 0};
		serverSend(Producer, Disconnect, sizeof(Disconnect));
		serverEof(Producer);
		serverConnections(&Fixture, 1);
		Producer = serverDial(&Fixture);
		serverSend(Producer, Wire, serverConnectWire(Wire, Origin, true, 0, "prod"));
		serverConnack(Producer, Origin, 0);
		const uint8 NoLocal[] = {XMQTT_SUBSCRIBE_NO_LOCAL};
		serverSubscribeWire(Producer, Origin, true, 1, System, NoLocal, Ok, 1);
		serverPingPeer(Producer);
	}
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && Info.RetainedTopics == 1 &&
		Info.RetainedBytes && !Info.DroppedRetained, "retained state has exact topic and byte accounting");
	serverStop(&Fixture);
	serverEof(Producer);
	serverEof(Consumer);
}

static void serverRetainedLimits(void)
{
	serverfixture Fixture;
	serverStartLimits(&Fixture, false, 0, false, 65536, 1, 32, 64, 0);
	unsigned char Wire[128];
	xnetsocket Producer = serverDial(&Fixture), Consumer = serverDial(&Fixture);
	serverSend(Producer, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "prod"));
	serverConnack(Producer, XMQTT_VERSION_5, 0);
	serverSend(Consumer, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "sink"));
	serverConnack(Consumer, XMQTT_VERSION_5, 0);

	const unsigned char Expiry[] = {0x02, 0, 0, 0, 1};
	serverPublishRetained(Producer, XMQTT_VERSION_5, "old", "a", Expiry, sizeof(Expiry));
	serverPublishRetained(Producer, XMQTT_VERSION_5, "not-room", "b", NULL, 0);
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && Info.RetainedTopics == 1 &&
		Info.DroppedRetained == 1, "topic quota declines another topic");
	xrtSleep(1100);
	const char* Root[] = {"#"};
	const uint8 Ok[] = {0};
	serverSubscribeWire(Consumer, XMQTT_VERSION_5, true, 1, Root, Ok, Ok, 1);
	serverPingPeer(Consumer);
	serverPublishRetained(Producer, XMQTT_VERSION_5, "later", "c", NULL, 0);
	serverExpectPublish(Consumer, XMQTT_VERSION_5, "later", (cbytes)"c", 1, NULL, 0);
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && Info.RetainedTopics == 1 &&
		Info.RetainedBytes == 15 && Info.DroppedRetained == 1, "expired topic frees the global quota");
	const char* Large = "0123456789012345678901234567890";
	serverPublishRetained(Producer, XMQTT_VERSION_5, "later", Large, NULL, 0);
	serverExpectPublish(Consumer, XMQTT_VERSION_5, "later", (cbytes)Large, strlen(Large), NULL, 0);
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && !Info.RetainedTopics &&
		!Info.RetainedBytes && Info.DroppedRetained == 2, "oversize replacement removes the old QoS0 message");
	serverSubscribeWire(Consumer, XMQTT_VERSION_5, true, 2, Root, Ok, Ok, 1);
	serverPingPeer(Consumer);
	serverStop(&Fixture);
	serverEof(Producer);
	serverEof(Consumer);
}

static size_t serverInsertId(unsigned char* Wire, size_t Size, size_t At, uint16 Identifier, uint8 Flags)
{
	testRequire(Size + 2 <= 128 && Wire[1] < 126, "short QoS 1 fixture");
	memmove(Wire + At + 2, Wire + At, Size - At);
	Wire[0] = 0x30 | Flags;
	Wire[1] += 2;
	Wire[At] = (uint8)(Identifier >> 8);
	Wire[At + 1] = (uint8)Identifier;
	return Size + 2;
}

static size_t serverPublishQos1Wire(unsigned char* Wire, xmqttversion Version, const char* Topic,
	const char* Payload, uint16 Identifier, uint8 Flags, const unsigned char* Properties, uint8 PropertyBytes)
{
	size_t Size = serverPublishWire(Wire, Version, Topic, (cbytes)Payload, strlen(Payload), Properties, PropertyBytes);
	return serverInsertId(Wire, Size, 4 + strlen(Topic), Identifier, Flags | 2u);
}

static void serverPublishQos1(xnetsocket Socket, xmqttversion Version, const char* Topic,
	const char* Payload, uint16 Identifier, uint8 Flags)
{
	unsigned char Wire[128];
	serverSend(Socket, Wire, serverPublishQos1Wire(Wire, Version, Topic, Payload, Identifier, Flags, NULL, 0));
}

static void serverExpectFlowReply(xnetsocket Socket, xmqttversion Version, uint8 Header, uint16 Identifier, uint8 Reason)
{
	size_t Bytes = Version == XMQTT_VERSION_5 && Reason ? 5 : 4;
	const unsigned char Expected[] = {Header, (uint8)(Bytes - 2), (uint8)(Identifier >> 8),
		(uint8)Identifier, Reason};
	unsigned char Wire[5];
	serverRecv(Socket, Wire, Bytes);
	testRequire(!memcmp(Wire, Expected, Bytes), "QoS control packet, identifier and reason");
}

static void serverExpectPuback(xnetsocket Socket, xmqttversion Version, uint16 Identifier, uint8 Reason)
{
	serverExpectFlowReply(Socket, Version, 0x40, Identifier, Reason);
}

static void serverInboundQos1(xmqttversion Origin, xmqttversion Target)
{
	serverfixture Fixture;
	serverStartLimits(&Fixture, Origin == XMQTT_VERSION_311 || Target == XMQTT_VERSION_311,
		0, true, 65536, 1, 34, 1, 0);
	unsigned char Wire[128];
	xnetsocket Producer = serverDial(&Fixture), Consumer = serverDial(&Fixture);
	serverSend(Producer, Wire, serverConnectWire(Wire, Origin, true, 0, "qos1"));
	serverConnackConfigured(Producer, Origin, 0, true, true, 1);
	serverSend(Consumer, Wire, serverConnectWire(Wire, Target, true, 0, "sink"));
	serverConnackConfigured(Consumer, Target, 0, true, true, 1);

	const char* Root[] = {"work/#"};
	const uint8 Granted[] = {0}, Requested[] = {0};
	serverSubscribeWire(Consumer, Target, true, 1, Root, Requested, Granted, 1);
	for (unsigned Index = 0; Index < 64; ++Index) {
		uint8 Flags = Index ? XMQTT_PUBLISH_DUP : 0;
		serverPublishQos1(Producer, Origin, "work/x", "payload", 257, Flags);
		serverExpectPuback(Producer, Origin, 257, 0);
		/* Reusing an acknowledged ID, even with DUP, is a new QoS 1 message. */
		serverExpectPublish(Consumer, Target, "work/x", (cbytes)"payload", 7, NULL, 0);
	}
	serverPublishQos1(Producer, Origin, "idle", "none", 2, 0);
	serverExpectPuback(Producer, Origin, 2, 0x10);
	serverPingPeer(Consumer);

	const char* State = "work/state";
	serverPublishQos1(Producer, Origin, State, "old", 3, XMQTT_PUBLISH_RETAIN);
	serverExpectPuback(Producer, Origin, 3, 0);
	serverExpectPublish(Consumer, Target, State, (cbytes)"old", 3, NULL, 0);
	if (Origin == XMQTT_VERSION_5) {
		serverPublishQos1(Producer, Origin, State, "too large for this broker's retained byte budget", 4,
			XMQTT_PUBLISH_RETAIN);
		serverExpectPuback(Producer, Origin, 4, 0x97);
		serverPingPeer(Consumer);
	}
	const char* Exact[] = {State};
	serverSubscribeWire(Consumer, Target, true, 2, Exact, Requested, Granted, 1);
	serverExpectRetained(Consumer, Target, State, "old", NULL, 0);
	serverPublishQos1(Producer, Origin, State, "", 5, XMQTT_PUBLISH_RETAIN);
	serverExpectPuback(Producer, Origin, 5, 0);
	serverExpectPublish(Consumer, Target, State, (cbytes)"", 0, NULL, 0);
	serverSubscribeWire(Consumer, Target, true, 3, Exact, Requested, Granted, 1);
	serverPingPeer(Consumer);

	if (Origin == XMQTT_VERSION_5) {
		serverPublishQos1(Producer, Origin, "busy", "refused", 6, 0);
		serverExpectPuback(Producer, Origin, 6, 0x97);
	}
	serverPublishQos1(Producer, Origin, "bad", "refused", 6, 0);
	if (Origin == XMQTT_VERSION_5) {
		serverExpectPuback(Producer, Origin, 6, 0x87);
		serverPingPeer(Producer);
	} else {
		serverEof(Producer);
		Producer = NULL;
	}
	serverStop(&Fixture);
	if (Producer) serverEof(Producer);
	serverEof(Consumer);
}

static void serverExpectQos1(xnetsocket Socket, xmqttversion Version, const char* Topic,
	const char* Payload, uint16 Identifier, uint8 Flags, const unsigned char* Properties, uint8 PropertyBytes)
{
	unsigned char Wire[128], Expected[128];
	size_t Bytes = serverPublishQos1Wire(Expected, Version, Topic, Payload, Identifier, Flags, Properties, PropertyBytes);
	serverRecv(Socket, Wire, Bytes);
	testRequire(!memcmp(Wire, Expected, Bytes), "outgoing QoS 1 wire, receiver ID and body");
}

static void serverSendFlowReply(xnetsocket Socket, xmqttversion Version, uint8 Header, uint16 Id, uint8 Reason)
{
	size_t Bytes = Version == XMQTT_VERSION_5 && Reason ? 5 : 4;
	const unsigned char Wire[] = {Header, (uint8)(Bytes - 2), (uint8)(Id >> 8), (uint8)Id, Reason};
	serverSend(Socket, Wire, Bytes);
}

static void serverAckDelivery(xnetsocket Socket, xmqttversion Version, uint16 Id, uint8 Reason)
{
	serverSendFlowReply(Socket, Version, 0x40, Id, Reason);
}

static void serverQos1Connect(xnetsocket Socket, xmqttversion Version, const char Name[4])
{
	unsigned char Wire[32];
	size_t Bytes = serverConnectWire(Wire, Version, true, 0, Name);
	if (Version == XMQTT_VERSION_5) {
		const unsigned char ReceiveOne[] = {33,0,1};
		memmove(Wire + 13 + sizeof(ReceiveOne), Wire + 13, Bytes - 13);
		memcpy(Wire + 13, ReceiveOne, sizeof(ReceiveOne));
		Wire[12] = sizeof(ReceiveOne);
		Wire[1] += sizeof(ReceiveOne);
		Bytes += sizeof(ReceiveOne);
	}
	serverSend(Socket, Wire, Bytes);
	serverConnack(Socket, Version, 0);
}

static void serverWaitOutgoing(xmqttserver* Server, size_t Count, uint64 Overloads)
{
	double Until = xrtTimer() + 5.0;
	for (;;) {
		xmqttserverinfo Info;
		testRequire(xmqttServerInfo(Server, &Info), "outgoing QoS 1 stats");
		if (Info.OutgoingQos1 == Count && Info.OutgoingQos1Overloads == Overloads &&
			(Info.OutgoingQos1Bytes != 0) == (Count != 0)) return;
		testRequire(xrtTimer() < Until, "outgoing QoS 1 obligations retire");
		xrtSleep(1);
	}
}

static void serverOutgoingQos1(xmqttversion Origin, xmqttversion Target)
{
	serverfixture Fixture;
	serverStartConfigured(&Fixture, Origin == XMQTT_VERSION_311 || Target == XMQTT_VERSION_311, 0, true, 65536);
	xnetsocket Producer = serverDial(&Fixture), Consumer = serverDial(&Fixture);
	serverQos1Connect(Producer, Origin, "prod");
	serverQos1Connect(Consumer, Target, "sink");
	const char* Overlap[] = {"order/x", "order/#"};
	const uint8 Options[] = {0,1}, Grants[] = {0,1}, One[] = {1};
	serverSubscribeWire(Consumer, Target, true, 1, Overlap, Options, Grants, 2);

	serverPublishQos1(Producer, Origin, "order/x", "first", 91, XMQTT_PUBLISH_DUP);
	serverExpectPuback(Producer, Origin, 91, 0);
	serverExpectQos1(Consumer, Target, "order/x", "first", 1, 0, NULL, 0);
	serverPublishQos1(Producer, Origin, "order/x", "second", 91, 0);
	serverExpectPuback(Producer, Origin, 91, 0);
	serverWaitOutgoing(Fixture.Server, 2, 0);
	if (Target == XMQTT_VERSION_5)
		serverPingPeer(Consumer); /* No PUBLISH can precede PINGRESP while a PUBACK is outstanding. */
	serverAckDelivery(Consumer, Target, 1, 0x80);
	serverExpectQos1(Consumer, Target, "order/x", "second", 2, 0, NULL, 0);
	serverAckDelivery(Consumer, Target, 2, 0);
	serverWaitOutgoing(Fixture.Server, 0, 0);

	serverPublishQos1(Producer, Origin, "state", "retain", 92, XMQTT_PUBLISH_RETAIN);
	serverExpectPuback(Producer, Origin, 92, 0x10);
	serverSubscribeWire(Consumer, Target, false, 2, Overlap, NULL, (const uint8[]){0,0}, 2);
	const char* State[] = {"state"};
	serverSubscribeWire(Consumer, Target, true, 3, State, One, One, 1);
	serverExpectQos1(Consumer, Target, "state", "retain", 3, XMQTT_PUBLISH_RETAIN, NULL, 0);
	serverAckDelivery(Consumer, Target, 3, 0);
	serverWaitOutgoing(Fixture.Server, 0, 0);
	serverAckDelivery(Consumer, Target, 63, 0);
	if (Target == XMQTT_VERSION_5) {
		unsigned char Wire[3];
		serverRecv(Consumer, Wire, sizeof(Wire));
		testRequire(!memcmp(Wire, (const unsigned char[]){0xe0,1,0x82}, sizeof(Wire)), "unsolicited PUBACK is a protocol error");
	}
	serverEof(Consumer);
	xrtNetSocketClose(Producer);
	serverStop(&Fixture);
}

static void serverPublishQos2(xnetsocket Socket, xmqttversion Version, const char* Topic, const char* Payload,
	uint16 Id, uint8 Flags, bool Receive)
{
	unsigned char Wire[128];
	size_t Bytes = serverPublishWire(Wire, Version, Topic, (cbytes)Payload, strlen(Payload), NULL, 0);
	Bytes = serverInsertId(Wire, Bytes, 4 + strlen(Topic), Id, Flags | 4u);
	if (Receive) {
		unsigned char Actual[128];
		serverRecv(Socket, Actual, Bytes);
		testRequire(!memcmp(Actual, Wire, Bytes), "QoS 2 PUBLISH body and packet ID");
	} else serverSend(Socket, Wire, Bytes);
}

static void serverReleaseQos2(xnetsocket Peer, xmqttversion Version, uint16 Id)
{
	serverSendFlowReply(Peer, Version, 0x62, Id, 0);
	serverExpectFlowReply(Peer, Version, 0x70, Id, 0);
}

static void serverQos2(xmqttversion Origin, xmqttversion Target)
{
	serverfixture Fixture;
	serverStartConfigured(&Fixture, Origin == XMQTT_VERSION_311 || Target == XMQTT_VERSION_311, 0, true, 65536);
	xnetsocket Producer = serverDial(&Fixture), Consumer = serverDial(&Fixture);
	serverQos1Connect(Producer, Origin, "prod");
	serverQos1Connect(Consumer, Target, "sink");
	const char* Filter[] = {Origin == XMQTT_VERSION_311 ? "$share/sink/phase/#" : "phase/#"};
	const uint8 Two[] = {2};
	serverSubscribeWire(Consumer, Target, true, 1, Filter, Two, Two, 1);

	if (Origin == XMQTT_VERSION_5) {
		serverPublishQos2(Producer, Origin, "bad", "denied", 81, 0, false);
		serverExpectFlowReply(Producer, Origin, 0x50, 81, 0x87);
	}
	serverPublishQos2(Producer, Origin, "phase/x", "first", 81, 0, false);
	serverExpectFlowReply(Producer, Origin, 0x50, 81, 0);
	serverPublishQos2(Consumer, Target, "phase/x", "first", 1, 0, true);
	serverPublishQos2(Producer, Origin, "phase/x", "duplicate", 81, XMQTT_PUBLISH_DUP, false);
	serverExpectFlowReply(Producer, Origin, 0x50, 81, 0);
	serverPingPeer(Consumer);
	serverReleaseQos2(Producer, Origin, 81);
	serverSendFlowReply(Producer, Origin, 0x62, 81, 0);
	serverExpectFlowReply(Producer, Origin, 0x70, 81, 0x92);

	serverSendFlowReply(Consumer, Target, 0x50, 1, 0);
	serverExpectFlowReply(Consumer, Target, 0x62, 1, 0);
	serverSendFlowReply(Consumer, Target, 0x50, 1, 0);
	serverExpectFlowReply(Consumer, Target, 0x62, 1, 0);
	if (Target == XMQTT_VERSION_311) serverSendFlowReply(Consumer, Target, 0x70, 1, 0);
	serverPublishQos2(Producer, Origin, "phase/x", "second", 81, 0, false);
	serverExpectFlowReply(Producer, Origin, 0x50, 81, 0);
	serverReleaseQos2(Producer, Origin, 81);
	if (Target == XMQTT_VERSION_5) {
		serverWaitOutgoing(Fixture.Server, 2, 0);
		serverPingPeer(Consumer);
		serverSendFlowReply(Consumer, Target, 0x70, 1, 0);
	}
	serverPublishQos2(Consumer, Target, "phase/x", "second", 2, 0, true);
	if (Target == XMQTT_VERSION_5) {
		serverSendFlowReply(Consumer, Target, 0x50, 2, 0x80);
		serverSendFlowReply(Consumer, Target, 0x50, 2, 0x80);
		serverPingPeer(Consumer);
		serverSendFlowReply(Consumer, Target, 0x50, 37, 0);
		serverExpectFlowReply(Consumer, Target, 0x62, 37, 0x92);
		serverSendFlowReply(Consumer, Target, 0x70, 37, 0x92);
		serverPingPeer(Consumer);
	} else {
		serverSendFlowReply(Consumer, Target, 0x50, 2, 0);
		serverExpectFlowReply(Consumer, Target, 0x62, 2, 0);
		serverSendFlowReply(Consumer, Target, 0x70, 2, 0);
	}
	serverWaitOutgoing(Fixture.Server, 0, 0);

	serverPublishQos2(Producer, Origin, "state", "retain", 82, XMQTT_PUBLISH_RETAIN, false);
	serverExpectFlowReply(Producer, Origin, 0x50, 82, 0x10);
	serverReleaseQos2(Producer, Origin, 82);
	const char* State[] = {"state"};
	serverSubscribeWire(Consumer, Target, true, 2, State, Two, Two, 1);
	serverPublishQos2(Consumer, Target, "state", "retain", 3, XMQTT_PUBLISH_RETAIN, true);
	serverSendFlowReply(Consumer, Target, 0x50, 3, 0);
	serverExpectFlowReply(Consumer, Target, 0x62, 3, 0);
	serverSendFlowReply(Consumer, Target, 0x70, 3, 0);
	serverWaitOutgoing(Fixture.Server, 0, 0);

	if (Origin == XMQTT_VERSION_5) {
		unsigned char Wire[128];
		const unsigned char Expired[] = {2,0,0,0,0};
		size_t Bytes = serverPublishWire(Wire, Origin, "phase/x", (cbytes)"expired", 7, Expired, sizeof(Expired));
		serverSend(Producer, Wire, serverInsertId(Wire, Bytes, 11, 89, 4));
		serverExpectFlowReply(Producer, Origin, 0x50, 89, 0);
		serverReleaseQos2(Producer, Origin, 89);
		serverPingPeer(Consumer);
	}

	/* Both sides have taken ownership when the sockets close here. */
	serverPublishQos2(Producer, Origin, "phase/x", "close", 83, 0, false);
	serverExpectFlowReply(Producer, Origin, 0x50, 83, 0);
	serverPublishQos2(Consumer, Target, "phase/x", "close", 4, 0, true);
	serverSendFlowReply(Consumer, Target, 0x50, 4, 0);
	serverExpectFlowReply(Consumer, Target, 0x62, 4, 0);
	xrtNetSocketClose(Producer);
	xrtNetSocketClose(Consumer);
	serverStop(&Fixture);
}

static void serverQos2Window(void)
{
	serverfixture Fixture;
	serverStartLimits(&Fixture, false, 0, false, 65536, 4096, 65536, 1, 0);
	xnetsocket Publisher = serverDial(&Fixture);
	unsigned char Wire[32];
	serverSend(Publisher, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "prod"));
	serverConnackConfigured(Publisher, XMQTT_VERSION_5, 0, true, true, 1);
	for (unsigned Index = 0; Index < 2; ++Index) {
		serverPublishQos2(Publisher, XMQTT_VERSION_5, "work", "waiting", 11, Index ? XMQTT_PUBLISH_DUP : 0, false);
		serverExpectFlowReply(Publisher, XMQTT_VERSION_5, 0x50, 11, 0x10);
	}
	serverPublishQos1(Publisher, XMQTT_VERSION_5, "work", "overflow", 12, 0);
	serverRecv(Publisher, Wire, 3);
	testRequire(!memcmp(Wire, (const unsigned char[]){0xe0,1,0x93}, 3), "QoS 1 and 2 share the receive window");
	serverEof(Publisher);
	serverStop(&Fixture);
}

static void serverOutgoingExpiry(void)
{
	serverfixture Fixture;
	serverStart(&Fixture, false, 0);
	xnetsocket Producer = serverDial(&Fixture), Consumer = serverDial(&Fixture);
	serverQos1Connect(Producer, XMQTT_VERSION_5, "prod");
	serverQos1Connect(Consumer, XMQTT_VERSION_5, "sink");
	const char* Filter[] = {"$share/team/expiry"};
	const uint8 One[] = {1};
	serverSubscribeWire(Consumer, XMQTT_VERSION_5, true, 1, Filter, One, One, 1);
	serverPublishQos1(Producer, XMQTT_VERSION_5, "expiry", "first", 1, 0);
	serverExpectPuback(Producer, XMQTT_VERSION_5, 1, 0);
	serverExpectQos1(Consumer, XMQTT_VERSION_5, "expiry", "first", 1, 0, NULL, 0);
	unsigned char Wire[128];
	const unsigned char Gone[] = {2,0,0,0,1};
	const unsigned char Live[] = {0x26,0,1,'x',0,1,'1',2,0,0,0,5,0x26,0,1,'x',0,1,'2'};
	serverSend(Producer, Wire, serverPublishQos1Wire(Wire, XMQTT_VERSION_5, "expiry", "gone", 2, 0, Gone, sizeof(Gone)));
	serverExpectPuback(Producer, XMQTT_VERSION_5, 2, 0);
	serverSend(Producer, Wire, serverPublishQos1Wire(Wire, XMQTT_VERSION_5, "expiry", "live", 3, 0, Live, sizeof(Live)));
	serverExpectPuback(Producer, XMQTT_VERSION_5, 3, 0);
	serverWaitOutgoing(Fixture.Server, 3, 0);
	xrtSleep(1150);
	serverAckDelivery(Consumer, XMQTT_VERSION_5, 1, 0);
	const unsigned char Reduced[] = {2,0,0,0,4,0x26,0,1,'x',0,1,'1',0x26,0,1,'x',0,1,'2'};
	serverExpectQos1(Consumer, XMQTT_VERSION_5, "expiry", "live", 2, 0, Reduced, sizeof(Reduced));
	serverAckDelivery(Consumer, XMQTT_VERSION_5, 2, 0);
	const unsigned char MaxExpiry[] = {2,0xff,0xff,0xff,0xff};
	serverSend(Producer, Wire, serverPublishQos1Wire(Wire, XMQTT_VERSION_5, "expiry", "max", 4, 0, MaxExpiry, sizeof(MaxExpiry)));
	serverExpectPuback(Producer, XMQTT_VERSION_5, 4, 0);
	serverExpectQos1(Consumer, XMQTT_VERSION_5, "expiry", "max", 3, 0, MaxExpiry, sizeof(MaxExpiry));
	serverAckDelivery(Consumer, XMQTT_VERSION_5, 3, 0);
	serverWaitOutgoing(Fixture.Server, 0, 0);
	xrtNetSocketClose(Consumer);
	xrtNetSocketClose(Producer);
	serverStop(&Fixture);
}

static void serverOutgoingLimit(void)
{
	serverfixture Fixture;
	serverStartLimits(&Fixture, false, 0, false, 65536, 4096, 65536, 64, 2);
	xnetsocket Producer = serverDial(&Fixture), Consumer = serverDial(&Fixture);
	serverQos1Connect(Producer, XMQTT_VERSION_5, "prod");
	serverQos1Connect(Consumer, XMQTT_VERSION_5, "slow");
	const char* Filter[] = {"queue"};
	const uint8 One[] = {1};
	serverSubscribeWire(Consumer, XMQTT_VERSION_5, true, 1, Filter, One, One, 1);
	for (uint16 Id = 1; Id <= 3; ++Id) {
		serverPublishQos1(Producer, XMQTT_VERSION_5, "queue", "payload", Id, 0);
		serverExpectPuback(Producer, XMQTT_VERSION_5, Id, 0);
		if (Id == 1) serverExpectQos1(Consumer, XMQTT_VERSION_5, "queue", "payload", 1, 0, NULL, 0);
	}
	unsigned char Wire[3];
	serverRecv(Consumer, Wire, sizeof(Wire));
	testRequire(!memcmp(Wire, (const unsigned char[]){0xe0,1,0x97}, sizeof(Wire)), "outgoing quota ends clean session");
	serverEof(Consumer);
	serverWaitOutgoing(Fixture.Server, 0, 1);
	serverPingPeer(Producer);
	xrtNetSocketClose(Producer);
	serverStop(&Fixture);
}

typedef struct serverWildcardSender {
	xnetsocket Socket;
	xatomic32* Start;
	const char* Topic;
	uint8 Source, Rounds;
	bool Qos1;
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
		unsigned char Wire[128];
		const unsigned char Payload[] = {Sender->Source, Index};
		size_t Size = serverPublishWire(Wire, XMQTT_VERSION_5, Sender->Topic, Payload, sizeof(Payload), NULL, 0);
		if (Sender->Qos1) Size = serverInsertId(Wire, Size, 4 + strlen(Sender->Topic), Index + 1, 2);
		serverSend(Sender->Socket, Wire, Size);
		if (Sender->Qos1) serverExpectPuback(Sender->Socket, XMQTT_VERSION_5, Index + 1, 0);
	}
	serverPingPeer(Sender->Socket);
	return 0;
}

typedef enum serverroutepattern { SERVER_EXACT, SERVER_WILDCARD, SERVER_SHARED } serverroutepattern;

static void serverConcurrentRoutes(serverroutepattern Pattern, bool Qos1)
{
	serverfixture Fixture;
	serverStartConfigured(&Fixture, false, 0, true, 65536);
	xnetsocket Socket[3];
	const char* Ids[] = {"src0", "src1", "sink"};
	for (size_t Index = 0; Index < 3; ++Index) {
		Socket[Index] = serverDial(&Fixture);
		serverQos1Connect(Socket[Index], XMQTT_VERSION_5, Ids[Index]);
	}
	bool Shared = Pattern == SERVER_SHARED;
	bool Exact = Pattern == SERVER_EXACT;
	const char* Filters[] = {Exact ? "a/x" : "#", Shared ? "$share/group/+/+" : "a/+"};
	const uint8 Accepted[] = {Qos1, Qos1};
	serverSubscribeWire(Socket[2], XMQTT_VERSION_5, true, 1, Filters, Accepted, Accepted, Exact ? 1 : 2);
	const char* Topics[] = {"a/x", Exact ? "a/x" : "b/x"};
	xatomic32 Start;
	xrtAtomic32Init(&Start, 0);
	const uint8 Copies = Shared ? 2 : 1, Rounds = Qos1 ? 24 : (Shared ? 3 : 4);
	serverWildcardSender Senders[] = {
		{Socket[0], &Start, Topics[0], 0, Rounds, Qos1}, {Socket[1], &Start, Topics[1], 1, Rounds, Qos1}
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
		unsigned char Received[12], Expected[128];
		size_t Bytes = Qos1 ? 12 : 10;
		serverRecv(Socket[2], Received, Bytes);
		uint8 Source = Received[Bytes - 2];
		testRequire(Source < 2 && Next[Source] < Copies * Rounds, "bounded wildcard source sequence");
		const unsigned char Payload[] = {Source, Next[Source]++ / Copies};
		size_t Size = serverPublishWire(Expected, XMQTT_VERSION_5, Topics[Source], Payload, sizeof(Payload), NULL, 0);
		if (Qos1) Size = serverInsertId(Expected, Size, 7, (uint16)(Index + 1), 2);
		testRequire(Size == Bytes && !memcmp(Received, Expected, Bytes), "concurrent publication order and bytes");
		if (Qos1) serverAckDelivery(Socket[2], XMQTT_VERSION_5, (uint16)(Index + 1), 0);
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
	serverStartLimits(&Fixture, false, 0, false, 0, 4096, 0, 64, 0);
	static const struct {unsigned char Wire[24]; size_t Size; uint8 Reason;} Cases[] = {
		{{0x82,8,0,1,0,0,2,'a','#',0}, 10, 0}, /* Incorrect wildcard syntax closes without response. */
		{{0x82,9,0,1,0,0,3,'a','/','#',0}, 11, 0xa2},
		{{0x82,16,0,1,0,0,10,'$','s','h','a','r','e','/','g','/','a',4}, 18, 0},
		{{0x82,9,0,1,2,11,1,0,1,'a',0}, 11, 0xa1},
		{{0x31,4,0,1,'a',0}, 6, 0x9a},
		{{0x30,7,0,1,'a',3,35,0,1}, 9, 0x94}
	};
	for (size_t Index = 0; Index < sizeof(Cases)/sizeof(Cases[0]); ++Index) {
		unsigned char Wire[32];
		xnetsocket Socket = serverDial(&Fixture);
		serverSend(Socket, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "caps"));
		serverConnackConfigured(Socket, XMQTT_VERSION_5, 0, false, false, 64);
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
typedef struct serverackfixture {
	xmqttserverpeer* Peer;
	xmqttserverack* Held;
	xatomic32 Done;
} serverackfixture;

static void serverAckFixture(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	serverackfixture* Guard = Data;
	if (Guard->Held) {
		xmqttServerReleaseAck(Guard->Held, NULL, 0);
	} else {
		uint8 Reason;
		Guard->Held = xmqttServerReserveAck(Guard->Peer, 7, &Reason);
		testRequire(Guard->Held != NULL, "hold one native acknowledgement slot");
	}
	xrtAtomic32Store(&Guard->Done, 1, XMEMORY_RELEASE);
}

static void serverPostAckFixture(serverfixture* Fixture, uint32 Worker, serverackfixture* Guard)
{
	xrtAtomic32Store(&Guard->Done, 0, XMEMORY_RELEASE);
	testRequire(xrtNetEnginePost(Fixture->Engine, Worker, serverAckFixture, Guard), "post ACK fixture");
	double Deadline = xrtTimer() + 5;
	while (!xrtAtomic32Load(&Guard->Done, XMEMORY_ACQUIRE)) {
		testRequire(xrtTimer() < Deadline, "ACK fixture worker finishes");
		xrtSleep(1);
	}
}

static void serverQos1Exceeded(void)
{
	serverfixture Fixture;
	serverStartLimits(&Fixture, false, 0, false, 65536, 4096, 8u * 1024u * 1024u, 1, 0);
	xnetsocket Socket = serverDial(&Fixture);
	unsigned char Wire[32];
	serverSend(Socket, Wire, serverConnectWire(Wire, XMQTT_VERSION_5, true, 0, "full"));
	serverConnackConfigured(Socket, XMQTT_VERSION_5, 0, true, true, 1);

	serverackfixture Guard = {0};
	xrtAtomic32Init(&Guard.Done, 0);
	xrtMutexLock(&Fixture.Server->Gate);
	Guard.Peer = Fixture.Server->Peers;
	uint32 Worker = Guard.Peer->Worker;
	xrtMutexUnlock(&Fixture.Server->Gate);
	serverPostAckFixture(&Fixture, Worker, &Guard);
	serverPublishQos1(Socket, XMQTT_VERSION_5, "overflow", "x", 8, 0);
	serverRecv(Socket, Wire, 3);
	testRequire(Wire[0] == 0xe0 && Wire[1] == 1 && Wire[2] == 0x93,
		"live MQTT 5 peer receives Receive Maximum exceeded");
	serverEof(Socket);
	serverPostAckFixture(&Fixture, Worker, &Guard);
	serverConnections(&Fixture, 0);
	serverStop(&Fixture);
}

static void serverAckQuota(void)
{
	serverfixture Fixture;
	serverStart(&Fixture, false, 0);
	Fixture.Server->Config.ReceiveMaximum = 2;
	Fixture.Server->Config.MaxIncomingQos1 = 2;
	xmqttserversession FirstSession = {0}, SecondSession = {0};
	xmqttserverpeer First = {.Server = Fixture.Server, .Session = &FirstSession};
	xmqttserverpeer Second = {.Server = Fixture.Server, .Session = &SecondSession};
	xmqttServerSessionInit(&FirstSession, &First, XRT_STR_LITERAL("one"));
	xmqttServerSessionInit(&SecondSession, &Second, XRT_STR_LITERAL("two"));
	xrtAtomic32Init(&First.References, 1);
	xrtAtomic32Init(&Second.References, 1);
	uint8 Reason;
	xmqttserverack* Inline = xmqttServerReserveAck(&First, 11, &Reason);
	testRequire(Inline == &FirstSession.InlineAck && !xmqttServerReserveAck(&First, 11, &Reason) &&
		Reason == 0x82, "active identifier remains reserved until ACK release");
	xallocationbudget* Budget;
	xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope),
		"ACK allocation failpoint");
	testRequire(!xmqttServerReserveAck(&First, 12, &Reason) && Reason == 0x97,
		"failed second ACK allocation releases its global reservation");
	testRequire(xrtAllocationBudgetLeave(&Scope), "leave ACK failpoint");
	xrtClearError();
	xrtAllocationBudgetFree(Budget);
	xmqttserverack* Dynamic = xmqttServerReserveAck(&First, 12, &Reason);
	testRequire(Dynamic && Dynamic != Inline && !xmqttServerReserveAck(&First, 13, &Reason) && Reason == 0x93,
		"Receive Maximum rejects additional in-flight identifiers");
	testRequire(!xmqttServerReserveAck(&Second, 11, &Reason) && Reason == 0x97,
		"aggregate acknowledgement quota spans peers");
	xmqttServerReleaseAck(Inline, NULL, 0);
	xmqttserverack* Reused = xmqttServerReserveAck(&Second, 11, &Reason);
	testRequire(Reused == &SecondSession.InlineAck, "release returns global acknowledgement quota");
	xmqttServerReleaseAck(Dynamic, NULL, 0);
	xmqttServerReleaseAck(Reused, NULL, 0);
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && !Info.IncomingQos1 &&
		xrtAtomic32Load(&First.References, XMEMORY_RELAXED) == 1 &&
		xrtAtomic32Load(&Second.References, XMEMORY_RELAXED) == 1, "all ACK credits and real peer refs retired");
	serverStop(&Fixture);
}

typedef struct serverbackpressure {
	xmqttserverpeer* Peer;
	xatomic32 Ready, Full;
} serverbackpressure;

static void serverHoldWriteCredit(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	serverbackpressure* State = Data;
	xmqttserverpeer* Peer = State->Peer;
	testRequire(Peer->Session->Waiting && Peer->Session->Outgoing.Count == 1, "outbound publish waits behind a full receive window");
	xrtAtomic32Store(&State->Ready, 1, XMEMORY_RELEASE);
	double Until = xrtTimer() + 5.0;
	while (!xrtAtomic32Load(&State->Full, XMEMORY_ACQUIRE)) {
		testRequire(xrtTimer() < Until, "external writer fills native queue");
		xrtSleep(1);
	}
	xmqttpacket Ack = {.Type = XMQTT_PACKET_PUBACK, .Identifier = 1};
	xmqttServerOnline(Peer, &Ack, NULL, xrtTimer());
	testRequire(!Peer->Closing && !Peer->Session->Outgoing.Count && Peer->Session->Waiting,
		"native AGAIN keeps the pending delivery without charging an extra packet ID");
}

static void serverOutgoingBackpressure(void)
{
	serverfixture Fixture;
	serverStartConfigured(&Fixture, false, 0, true, 65536);
	xnetsocket Producer = serverDial(&Fixture), Consumer = serverDial(&Fixture);
	serverQos1Connect(Producer, XMQTT_VERSION_5, "prod");
	serverQos1Connect(Consumer, XMQTT_VERSION_5, "sink");
	const char* Filter[] = {"drain"};
	const uint8 One[] = {1};
	serverSubscribeWire(Consumer, XMQTT_VERSION_5, true, 1, Filter, One, One, 1);
	serverPublishQos1(Producer, XMQTT_VERSION_5, "drain", "first", 1, 0);
	serverExpectPuback(Producer, XMQTT_VERSION_5, 1, 0);
	serverExpectQos1(Consumer, XMQTT_VERSION_5, "drain", "first", 1, 0, NULL, 0);
	serverPublishQos1(Producer, XMQTT_VERSION_5, "drain", "second", 2, 0);
	serverExpectPuback(Producer, XMQTT_VERSION_5, 2, 0);
	serverWaitOutgoing(Fixture.Server, 2, 0);
	serverPingPeer(Consumer);

	serverbackpressure State;
	xrtAtomic32Init(&State.Ready, 0);
	xrtAtomic32Init(&State.Full, 0);
	xrtMutexLock(&Fixture.Server->Gate);
	State.Peer = Fixture.Server->Peers;
	for (; State.Peer && (!State.Peer->Session || memcmp(State.Peer->Session->ClientName.Data, "sink", 4));
		State.Peer = State.Peer->Next) { }
	testRequire(State.Peer != NULL, "backpressured subscriber still exists");
	xnetworker* Worker = xrtNetStreamWorker(State.Peer->Stream);
	xrtMutexUnlock(&Fixture.Server->Gate);
	xnetpost Post;
	testRequire(xrtNetPostInit(&Post) && xrtNetPost(Worker, &Post, serverHoldWriteCredit, &State), "hold subscriber worker");
	double Until = xrtTimer() + 5.0;
	while (!xrtAtomic32Load(&State.Ready, XMEMORY_ACQUIRE)) {
		testRequire(xrtTimer() < Until, "subscriber worker reached credit fixture");
		xrtSleep(1);
	}
	unsigned char Fill[128] = {0}, Wire[128];
	for (size_t Index = 0; Index < sizeof(Fill); Index += 2) Fill[Index] = 0xd0;
	testRequire(xrtNetStreamSend(State.Peer->Stream, Fill, sizeof(Fill)) == XNET_RESULT_OK, "reserve the entire TCP send queue");
	xrtAtomic32Store(&State.Full, 1, XMEMORY_RELEASE);
	serverRecv(Consumer, Wire, sizeof(Wire));
	testRequire(!memcmp(Wire, Fill, sizeof(Wire)), "native send completes ahead of pending publish");
	serverExpectQos1(Consumer, XMQTT_VERSION_5, "drain", "second", 2, 0, NULL, 0);
	serverAckDelivery(Consumer, XMQTT_VERSION_5, 2, 0);
	serverWaitOutgoing(Fixture.Server, 0, 0);
	xrtNetSocketClose(Consumer);
	xrtNetSocketClose(Producer);
	serverStop(&Fixture);
}

typedef struct servertakeovergate {
	xatomic32 Ready, Release;
} servertakeovergate;

static void serverHoldOldWorker(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	servertakeovergate* Gate = Data;
	xrtAtomic32Store(&Gate->Ready, 1, XMEMORY_RELEASE);
	double Until = xrtTimer() + 5.0;
	while (!xrtAtomic32Load(&Gate->Release, XMEMORY_ACQUIRE)) {
		testRequire(xrtTimer() < Until, "takeover test releases the old worker");
		xrtSleep(1);
	}
}

static void serverTakeoverRoutes(void)
{
	serverfixture Fixture;
	serverStart(&Fixture, false, 0);
	xnetsocket Old = serverDial(&Fixture);
	serverQos1Connect(Old, XMQTT_VERSION_5, "same");
	const char* Filters[] = {"gone/#", "$share/team/gone/x"};
	const uint8 Grants[] = {1,1};
	serverSubscribeWire(Old, XMQTT_VERSION_5, true, 1, Filters, Grants, Grants, 2);

	xmqttserverpeer Attempt = {.Server = Fixture.Server};
	xallocationbudget* Budget;
	xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope),
		"session allocation failpoint");
	bool Replaced = xmqttServerClaimId(&Attempt, XRT_STR_LITERAL("same"));
	testRequire(xrtAllocationBudgetLeave(&Scope) && !Replaced && !Attempt.Session,
		"failed session allocation leaves existing owner installed");
	xrtClearError();
	xrtAllocationBudgetFree(Budget);
	serverPingPeer(Old);

	xrtMutexLock(&Fixture.Server->Gate);
	xmqttserverpeer* OldPeer = Fixture.Server->Peers;
	xnetworker* Worker = xrtNetStreamWorker(OldPeer->Stream);
	xrtMutexUnlock(&Fixture.Server->Gate);
	/* Accept the second TCP stream before blocking the listener's worker. */
	xnetsocket New = serverDial(&Fixture);
	serverConnections(&Fixture, 2);
	xrtMutexLock(&Fixture.Server->Gate);
	testRequire(Fixture.Server->Peers->Worker != OldPeer->Worker, "takeover uses a second worker");
	xrtMutexUnlock(&Fixture.Server->Gate);
	servertakeovergate Gate;
	xrtAtomic32Init(&Gate.Ready, 0);
	xrtAtomic32Init(&Gate.Release, 0);
	xnetpost Post;
	testRequire(xrtNetPostInit(&Post) && xrtNetPost(Worker, &Post, serverHoldOldWorker, &Gate),
		"hold the old stream before its subscriptions retire");
	double Until = xrtTimer() + 5.0;
	while (!xrtAtomic32Load(&Gate.Ready, XMEMORY_ACQUIRE)) {
		testRequire(xrtTimer() < Until, "old stream worker is held");
		xrtSleep(1);
	}

	serverQos1Connect(New, XMQTT_VERSION_5, "same");
	serverPublishQos1(New, XMQTT_VERSION_5, "gone/x", "discard", 1, 0);
	serverExpectPuback(New, XMQTT_VERSION_5, 1, 0x10);
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && Info.Connections == 2 && Info.Established == 1 &&
		Info.Subscriptions == 2 && !Info.Forwarded && !Info.OutgoingPublishes,
		"superseded wildcard and shared links stay indexed but cannot accept deliveries");
	xrtAtomic32Store(&Gate.Release, 1, XMEMORY_RELEASE);
	serverEof(Old);
	serverConnections(&Fixture, 1);
	serverPingPeer(New);
	xrtNetSocketClose(New);
	serverStop(&Fixture);
}

static void serverOutgoingOom(void)
{
	serverfixture Fixture;
	serverStart(&Fixture, false, 0);
	xnetsocket Consumer = serverDial(&Fixture);
	serverQos1Connect(Consumer, XMQTT_VERSION_5, "sink");
	unsigned char Wire[128];
	size_t Bytes = serverPublishQos1Wire(Wire, XMQTT_VERSION_5, "fault", "data", 1, 0, NULL, 0);
	xmqttserverpublication* Frame = xmqttServerCopyPublication((xbytesview){Wire, Bytes}, 2, (xstrview){0});
	testRequire(Frame != NULL, "outgoing OOM fixture");
	Frame->IdOffset = 9;
	xallocationbudget* Budget;
	xallocationscope Scope = {0};
	xrtMutexLock(&Fixture.Server->Gate);
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope),
		"outgoing delivery allocation failpoint");
	bool Queued = xmqttServerQueueOutgoing(Fixture.Server->Peers, Frame);
	testRequire(xrtAllocationBudgetLeave(&Scope) && !Queued &&
		xrtAtomic32Load(&Frame->References, XMEMORY_RELAXED) == 1, "failed delivery leaves payload ownership unchanged");
	xrtMutexUnlock(&Fixture.Server->Gate);
	xrtClearError();
	xallocationbudgetinfo Allocation = {.Size = sizeof(Allocation)};
	testRequire(xrtAllocationBudgetInfo(Budget, &Allocation) && Allocation.Rejections &&
		!Allocation.LiveBytes && !Allocation.LiveCount, "failed delivery has no surviving allocation");
	xrtAllocationBudgetFree(Budget);
	xmqttServerFreePublication(Frame, NULL, 0);
	serverRecv(Consumer, Wire, 3);
	testRequire(!memcmp(Wire, (const unsigned char[]){0xe0,1,0x97}, 3), "allocation failure ends QoS 1 clean session");
	serverEof(Consumer);
	serverWaitOutgoing(Fixture.Server, 0, 1);
	serverStop(&Fixture);
}

static void serverRetainedOomAndExpiry(void)
{
	serverfixture Fixture;
	serverStart(&Fixture, false, 0);
	xmqttserversession Session = {0};
	xmqttserverpeer Peer = {.Server = Fixture.Server, .Version = XMQTT_VERSION_5, .Session = &Session};
	xmqttServerSessionInit(&Session, &Peer, XRT_STR_LITERAL("prod"));
	const char* Topic = "stored";
	const unsigned char Value[] = {'p'}, Expiry[] = {2, 0, 0, 0, 3}, Reduced[] = {2, 0, 0, 0, 2};
	unsigned char Wire[32];
	size_t Size = serverPublishWire(Wire, XMQTT_VERSION_5, Topic, Value, sizeof(Value), Expiry, sizeof(Expiry));
	Wire[0] |= XMQTT_PUBLISH_RETAIN;
	xmqttproperty Decoded[1];
	const xmqttcodecbuffers Scratch = {Decoded, 1, NULL, 0};
	xmqttpacket Packet;
	testRequire(xmqttPacketRead5((xbytesview){Wire, Size}, XMQTT_CLIENT_TO_SERVER, &Fixture.Server->Config.Codec,
		&Scratch, &Packet, NULL) == XMQTT_CODEC_DONE, "decode expiring retained fixture");
	const size_t Limits[] = {1, sizeof(xmqttserverpublication) + Size + Session.ClientName.Size};
	xmqttserverinfo Info;
	for (size_t Index = 0; Index < sizeof(Limits) / sizeof(*Limits); ++Index) {
		xallocationbudget* Budget;
		xallocationscope Scope = {0};
		testRequire(xrtAllocationBudgetCreate(Limits[Index], &Budget) &&
			xrtAllocationBudgetEnter(Budget, &Scope), "retained failpoint");
		xmqttServerStoreRetained(&Peer, &Packet, Decoded, 7);
		testRequire(xrtAllocationBudgetLeave(&Scope), "leave retained failpoint");
		xallocationbudgetinfo BudgetInfo = {.Size = sizeof(BudgetInfo)};
		testRequire(xrtAllocationBudgetInfo(Budget, &BudgetInfo) && BudgetInfo.Rejections &&
			!BudgetInfo.LiveBytes && xmqttServerInfo(Fixture.Server, &Info) && !Info.RetainedBytes &&
			!Info.RetainedTopics && Info.DroppedRetained == Index + 1, "retained allocation rollback");
		xrtClearError();
		xrtAllocationBudgetFree(Budget);
	}
	xmqttServerStoreRetained(&Peer, &Packet, Decoded, 7);
	xbytesview Key = XRT_BYTES_LITERAL("stored");
	xmqttservershard* Shard = xmqttServerWildcardShard(Fixture.Server, Key);
	const xmqttserverretained* Entry = xmqttServerFindKey(&Shard->Retained,
		xmqttServerIdHash(Key, &Fixture.Server->HashKey), Key);
	testRequire(Entry && Entry->Received == 7 && Entry->Expires == 10, "one deadline from receipt");
	xmqttserverpublication* Copy = xmqttServerRecodeRetained(&Peer, Entry, 1, false);
	unsigned char Expected[32];
	size_t ExpectedSize = serverPublishWire(Expected, XMQTT_VERSION_5, Topic, Value, sizeof(Value), Reduced, sizeof(Reduced));
	Expected[0] |= XMQTT_PUBLISH_RETAIN;
	testRequire(Copy && Copy->Size == ExpectedSize && !memcmp(Copy->Wire, Expected, ExpectedSize),
		"retained replay decrements Message Expiry Interval");
	xmqttServerFreePublication(Copy, NULL, 0);
	/* Destroy releases the retained publication and its publisher ID. */
	serverStop(&Fixture);
}

static void serverRoutingOom(void)
{
	serverfixture Fixture;
	serverStart(&Fixture, false, 0);
	xmqttserversession Session = {0};
	xmqttserverpeer Peer = {.Server = Fixture.Server, .Session = &Session};
	xmqttServerSessionInit(&Session, &Peer, XRT_STR_LITERAL("test"));
	const size_t Limits[] = {1, sizeof(xmqttserverlink)};
	for (size_t Index = 0; Index < sizeof(Limits)/sizeof(Limits[0]); ++Index) {
		xallocationbudget* Budget;
		xallocationscope Scope = {0};
		testRequire(xrtAllocationBudgetCreate(Limits[Index], &Budget) &&
			xrtAllocationBudgetEnter(Budget, &Scope), "index failpoint scope");
		uint8 Reason = xmqttServerSubscribe(&Peer, XRT_BYTES_LITERAL("index-failure"), (xbytesview){0}, 0, NULL);
		testRequire(xrtAllocationBudgetLeave(&Scope) && Reason == 0x83 && !Session.Links &&
			!Session.LinkCount, "subscription node or topic insertion OOM rolls back");
		xallocationbudgetinfo Allocation = {.Size = sizeof(Allocation)};
		testRequire(xrtAllocationBudgetInfo(Budget, &Allocation) && Allocation.Rejections &&
			!Allocation.LiveBytes, "no live charges after index failure");
		xrtClearError();
		xrtAllocationBudgetFree(Budget);
	}
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Fixture.Server, &Info) && !Info.Subscriptions, "aggregate quota restored");
	testRequire(xmqttServerSubscribe(&Peer, XRT_BYTES_LITERAL("index-failure"), (xbytesview){0}, 0, NULL) == 0 &&
		xmqttServerUnsubscribe(&Peer, XRT_BYTES_LITERAL("index-failure"), (xbytesview){0}), "successful retry after OOM");
	xallocationbudget* TrieBudget;
	xallocationscope TrieScope = {0};
	testRequire(xrtAllocationBudgetCreate(sizeof(xmqttserverwildnode) + 1, &TrieBudget) &&
		xrtAllocationBudgetEnter(TrieBudget, &TrieScope), "trie node failpoint scope");
	xmqttservertopic Filter = {.Key.Text = XRT_BYTES_LITERAL("x/y/+/#")};
	xmqttservershard* Shard = xmqttServerWildcardShard(Fixture.Server, Filter.Key.Text);
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
	Filter.Key.Text = (xbytesview){Deep, sizeof(Deep)};
	Shard = xmqttServerWildcardShard(Fixture.Server, Filter.Key.Text);
	testRequire(xmqttServerAddWildcard(Fixture.Server, Shard, &Filter) == 0x97 &&
		!Shard->WildcardRoot.Children.Root && xmqttServerInfo(Fixture.Server, &Info) &&
		!Info.WildcardNodes, "depth limit rolls back empty path");

	xmqttservertargets Targets = {0};
	Targets.Slots = Targets.Local;
	Targets.Capacity = sizeof(Targets.Local) / sizeof(Targets.Local[0]);
	xmqttserversession TargetSessions[49] = {0};
	xmqttserverlink TargetsFixture[49] = {0};
	for (size_t Index = 0; Index < 49; ++Index) TargetsFixture[Index].Session = TargetSessions + Index;
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
	testRequire(xmqttServerSubscribe(&Peer, SharedKey, (xbytesview){0}, 0, NULL) == 0,
		"ordinary route anchors the group allocation failpoint");
	xallocationbudget* GroupBudget;
	xallocationscope GroupScope = {0};
	testRequire(xrtAllocationBudgetCreate(sizeof(xmqttserverlink), &GroupBudget) &&
		xrtAllocationBudgetEnter(GroupBudget, &GroupScope), "shared group failpoint scope");
	uint8 SharedResult = xmqttServerSubscribe(&Peer, SharedKey, Team, 0, NULL);
	testRequire(xrtAllocationBudgetLeave(&GroupScope) && SharedResult == 0x83 &&
		Session.LinkCount == 1 && !Session.Links->Topic->Shares, "shared group OOM preserves existing route");
	xallocationbudgetinfo GroupAllocation = {.Size = sizeof(GroupAllocation)};
	testRequire(xrtAllocationBudgetInfo(GroupBudget, &GroupAllocation) &&
		GroupAllocation.Rejections && !GroupAllocation.LiveBytes, "shared group OOM rolls back its link and quota");
	xrtClearError();
	xrtAllocationBudgetFree(GroupBudget);
	testRequire(xmqttServerSubscribe(&Peer, SharedKey, Team, 0, NULL) == 0 &&
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
		Targets.Shares[0].Share == Shares && Targets.Shares[16].Share == Shares + 16, "group batch growth retry");
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
	serverRetainedRoutes(XMQTT_VERSION_5, XMQTT_VERSION_5);
	serverRetainedLimits();
	serverInboundQos1(XMQTT_VERSION_5, XMQTT_VERSION_5);
	serverOutgoingQos1(XMQTT_VERSION_5, XMQTT_VERSION_5);
	serverQos2(XMQTT_VERSION_5, XMQTT_VERSION_5);
	serverQos2Window();
	serverOutgoingExpiry();
	serverOutgoingLimit();
	serverConcurrentRoutes(SERVER_EXACT, false);
	serverConcurrentRoutes(SERVER_WILDCARD, false);
	serverConcurrentRoutes(SERVER_SHARED, false);
	serverConcurrentRoutes(SERVER_EXACT, true);
	serverConcurrentRoutes(SERVER_SHARED, true);
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	serverCompatibility();
	serverRouting(XMQTT_VERSION_311);
	serverWildcardRoutes(XMQTT_VERSION_311);
	serverRetainedRoutes(XMQTT_VERSION_5, XMQTT_VERSION_311);
	serverRetainedRoutes(XMQTT_VERSION_311, XMQTT_VERSION_5);
	serverInboundQos1(XMQTT_VERSION_311, XMQTT_VERSION_5);
	serverInboundQos1(XMQTT_VERSION_5, XMQTT_VERSION_311);
	serverOutgoingQos1(XMQTT_VERSION_311, XMQTT_VERSION_5);
	serverOutgoingQos1(XMQTT_VERSION_5, XMQTT_VERSION_311);
	serverQos2(XMQTT_VERSION_311, XMQTT_VERSION_5);
	serverQos2(XMQTT_VERSION_5, XMQTT_VERSION_311);
#endif
#if defined(XMQTT_IMPLEMENTATION_ONCE)
	serverRoutingOom();
	serverRetainedOomAndExpiry();
	serverOutgoingOom();
	serverTakeoverRoutes();
	serverOutgoingBackpressure();
	serverAckQuota();
	serverQos1Exceeded();
#endif
	serverfixture Fixture;
	serverStart(&Fixture, false, 1);
	xnetsocket Socket = serverDial(&Fixture);
	serverEof(Socket);
	serverStop(&Fixture);
	puts("[PASS] MQTT TCP CONNECT, exact, wildcard, shared and retained routing, bidirectional QoS 1/2, quotas, authorization, takeover, deadlines, OOM and retirement");
	return 0;
}
