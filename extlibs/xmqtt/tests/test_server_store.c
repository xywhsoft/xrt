#include "test.h"
#include <xrt/memory.h>

static xbytesview makeRetained(bytes Wire, size_t Capacity, xstrview Name, xbytesview Payload, uint32 Lifetime)
{
	xmqttpacket Packet;
	testRequire(xmqttPacketInit(XMQTT_PACKET_PUBLISH, &Packet), "initialize retained fixture");
	Packet.Flags = XMQTT_PUBLISH_RETAIN;
	Packet.Body.Publish.Topic = Name;
	Packet.Body.Publish.Payload = Payload;
	xmqttproperty Expiry = {XMQTT_PROPERTY_MESSAGE_EXPIRY, XMQTT_PROPERTY_UINT32, {.Number = Lifetime}};
	if (Lifetime) Packet.Properties = (xmqttpropertylist){&Expiry, 1};
	size_t Bytes;
	testRequire(xmqttPacketWrite5(Wire, Capacity, &Packet, XMQTT_CLIENT_TO_SERVER, NULL, &Bytes),
		"encode retained fixture");
	return (xbytesview){Wire, Bytes};
}

static void requireRetainedCount(xmqttserver* Server, size_t Count)
{
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Server, &Info) && Info.RetainedTopics == Count &&
		(Info.RetainedBytes != 0) == (Count != 0) && !Info.DroppedRetained && !Info.Established,
		"retained recovery counters");
}

static bool visitRetained(const xmqttserverretainedrecord* Record, ptr User)
{
	xmqttserverretainedrecord* Expected = User;
	testRequire(Record->Version == Expected->Version && Record->PublisherId.Size == Expected->PublisherId.Size &&
		!memcmp(Record->PublisherId.Data, Expected->PublisherId.Data, Record->PublisherId.Size) &&
		Record->Publish.Size == Expected->Publish.Size && !memcmp(Record->Publish.Data, Expected->Publish.Data, Record->Publish.Size),
		"checkpoint preserves source version, publisher and wire bytes");
	testRequire(Record->RemainingMilliseconds > 30000 && Record->RemainingMilliseconds <= 55000,
		"checkpoint conserves finite lifetime after downtime");
	return false; /* Deliberately interrupt a checkpoint write. */
}

static void requireSessionCount(xmqttserver* Server, size_t Count)
{
	xmqttserverinfo Info;
	testRequire(xmqttServerInfo(Server, &Info) && Info.PersistentSessions == Count &&
		Info.OfflineSessions == Count && Info.Subscriptions == Count && Info.IncomingPublishes == Count &&
		Info.OutgoingPublishes == Count * 2 && !Info.Established, "session checkpoint accounting");
}

static bool countSessionRecord(const xmqttserversessionrecord* Record, ptr User)
{
	size_t* Counts = User;
	testRequire(Record->Kind >= XMQTT_SERVER_RECORD_SESSION && Record->Kind <= XMQTT_SERVER_RECORD_WILL,
		"recognized session checkpoint record");
	++Counts[Record->Kind - 1];
	return true;
}

static void sessionRestore(xnetengine* Engine, const xmqttserverconfig* Config)
{
	xmqttserver* Server = NULL;
	testRequire(xmqttServerCreate(Engine, Config, &Server), "create detached session checkpoint broker");
	static const unsigned char Publish[] = {0x32, 7, 0, 1, 'a', 0, 1, 0, 'x'};
	xmqttserversessionrecord Records[] = {
		{.Kind = XMQTT_SERVER_RECORD_SESSION, .Data = XRT_BYTES_INIT("session"), .Milliseconds = 60000,
			.Identifier = 20, .Code = XMQTT_VERSION_5},
		{.Kind = XMQTT_SERVER_RECORD_SUBSCRIPTION, .Data = XRT_BYTES_INIT("a/+"), .Group = XRT_BYTES_INIT("g"), .Code = 2},
		{.Kind = XMQTT_SERVER_RECORD_INCOMING_QOS2, .Identifier = 17, .Code = 0x10},
		{.Kind = XMQTT_SERVER_RECORD_QUEUED, .Data = {Publish, sizeof(Publish)}, .Milliseconds = UINT64_MAX},
		{.Kind = XMQTT_SERVER_RECORD_SENT, .Data = {Publish, sizeof(Publish)}, .Identifier = 12, .Code = XMQTT_PACKET_PUBACK},
		{.Kind = XMQTT_SERVER_RECORD_SENT, .Data = {Publish, sizeof(Publish)}, .Identifier = 12, .Code = XMQTT_PACKET_PUBACK}
	};
	testRequire(!xmqttServerRestoreSessions(Server, Records, 6, 0), "reject duplicate outbound IDs");
	requireSessionCount(Server, 0);
	xrtClearError();

	xallocationbudget* Budget;
	xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope),
		"start session recovery allocation failpoint");
	testRequire(!xmqttServerRestoreSessions(Server, Records, 5, 0) && xrtAllocationBudgetLeave(&Scope),
		"fail session recovery allocation");
	xallocationbudgetinfo Allocation = {.Size = sizeof(Allocation)};
	testRequire(xrtAllocationBudgetInfo(Budget, &Allocation) && Allocation.Rejections && !Allocation.LiveBytes,
		"rollback session checkpoint OOM");
	xrtClearError();
	xrtAllocationBudgetFree(Budget);
	requireSessionCount(Server, 0);
	testRequire(xmqttServerRestoreSessions(Server, Records, 5, 70000), "drop expired restored session");
	requireSessionCount(Server, 0);
	testRequire(xmqttServerRestoreSessions(Server, Records, 5, 1000), "restore session checkpoint");
	requireSessionCount(Server, 1);
	testRequire(xmqttServerCloseForCheckpoint(Server) && xmqttServerWaitFor(Server, 5000) == XWAIT_OK,
		"drain broker while preserving its offline state");
	size_t Counts[XMQTT_SERVER_RECORD_WILL] = {0};
	testRequire(xmqttServerVisitSessions(Server, countSessionRecord, Counts), "visit restored state");
	for (size_t Index = 0; Index < XMQTT_SERVER_RECORD_WILL - 1; ++Index)
		testRequire(Counts[Index] == 1, "checkpoint record preserved");
	testRequire(!Counts[XMQTT_SERVER_RECORD_WILL - 1] && xmqttServerDestroy(Server), "retire preserved session list");
}

int main(void)
{
	xnetengineconfig EngineConfig;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	xnetengine* Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(Engine && xrtNetEngineStart(Engine), "start checkpoint test engine");
	xmqttserverconfig Config;
	xmqttserver* Server = NULL;
	testRequire(xmqttServerConfigInit(&Config) && xmqttServerCreate(Engine, &Config, &Server),
		"create detached checkpoint test broker");
	unsigned char A[96], B[1400], Payload[1024] = {0};
	xmqttserverretainedrecord Records[] = {
		{XMQTT_VERSION_5, XRT_STR_INIT("publisher"), makeRetained(A, sizeof(A), XRT_STR_LITERAL("a"),
			XRT_BYTES_LITERAL("x"), 60), 59000},
		{XMQTT_VERSION_5, XRT_STR_INIT("publisher"), makeRetained(B, sizeof(B), XRT_STR_LITERAL("b"),
			(xbytesview){Payload, sizeof(Payload)}, 5), 3500}
	};
	testRequire(!xmqttServerVisitRetained(Server, visitRetained, Records), "live broker cannot checkpoint");
	xrtClearError();
	xmqttserverretainedrecord Duplicate[] = {Records[0], Records[0]};
	testRequire(!xmqttServerRestoreRetained(Server, Duplicate, 2, 0), "reject repeated live topics");
	requireRetainedCount(Server, 0);
	xrtClearError();
	xmqttserverretainedrecord Invalid[] = {Records[0], Records[1]};
	Invalid[1].RemainingMilliseconds = 5001;
	testRequire(!xmqttServerRestoreRetained(Server, Invalid, 2, 0), "reject excess expiry");
	requireRetainedCount(Server, 0);
	xrtClearError();
	--Invalid[1].Publish.Size;
	testRequire(!xmqttServerRestoreRetained(Server, Invalid, 2, 0), "reject truncated wire");
	requireRetainedCount(Server, 0);
	xrtClearError();

	xallocationscope Scope = {0};
	xallocationbudget* Budget;
	testRequire(xrtAllocationBudgetCreate(512, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope),
		"start recovery allocation failpoint");
	testRequire(!xmqttServerRestoreRetained(Server, Records, 2, 0) && xrtAllocationBudgetLeave(&Scope),
		"rollback after a later retained allocation fails");
	xallocationbudgetinfo Allocation = {.Size = sizeof(Allocation)};
	testRequire(xrtAllocationBudgetInfo(Budget, &Allocation) && Allocation.Rejections && !Allocation.LiveBytes,
		"release the entire provisional retained set");
	xrtClearError();
	xrtAllocationBudgetFree(Budget);
	requireRetainedCount(Server, 0);

	testRequire(xmqttServerRestoreRetained(Server, Records, 2, 4000), "restore with downtime");
	requireRetainedCount(Server, 1);
	testRequire(!xmqttServerRestoreRetained(Server, Records, 2, 4000), "refuse to overwrite a restored set");
	xrtClearError();
	testRequire(xmqttServerClose(Server) && xmqttServerWaitFor(Server, 5000) == XWAIT_OK &&
		!xmqttServerVisitRetained(Server, visitRetained, Records), "visitor can stop a closed broker checkpoint");
	testRequire(xmqttServerDestroy(Server), "retire recovered records");

	Config.MaxRetainedBytes = 64;
	testRequire(xmqttServerCreate(Engine, &Config, &Server) &&
		!xmqttServerRestoreRetained(Server, Records, 2, 0), "reject set exceeding retained byte quota");
	requireRetainedCount(Server, 0);
	xrtClearError();
	testRequire(xmqttServerClose(Server) && xmqttServerDestroy(Server), "retire retained quota broker");
	sessionRestore(Engine, &Config);
	testRequire(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "retire checkpoint test engine");
	puts("[PASS] retained and session checkpoint validation, downtime, lifecycle and OOM rollback");
	return 0;
}
