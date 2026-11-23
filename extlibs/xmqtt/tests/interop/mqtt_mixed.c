/* One native broker process and one native client workload, over loopback TCP. */
#include "../../../../dev/bench/bench_common.h"
#include "../../examples/server/app_listener.h"

enum { DIRECT = 1, MEMBERS = 2, CLIENTS = 4, BURST = 6, BYTES = 256, REPLAY_INTERVAL = 16,
	DEADLINE = 5000, SAMPLE_LIMIT = 8192 };
static const xstrview Direct = XRT_STR_LITERAL("xrt/soak/direct/binary");
static const xstrview Shared = XRT_STR_LITERAL("xrt/soak/shared/binary");
static const xstrview Retained = XRT_STR_LITERAL("xrt/soak/state");
static const xstrview DirectFilter = XRT_STR_LITERAL("xrt/soak/direct/+");
static const xstrview SharedFilter = XRT_STR_LITERAL("$share/soak/xrt/soak/shared/+");
static const uint64 Ns = UINT64_C(1000000000);

static void require(bool Ok, const char* Message)
{
	if (!Ok) {
		fprintf(stderr, "MQTT mixed: %s\n", Message);
		exit(EXIT_FAILURE);
	}
}

static unsigned argument(const char* Text, unsigned Min, unsigned Max)
{
	char* End;
	errno = 0;
	unsigned long Value = strtoul(Text, &End, 10);
	if (*Text < '0' || *Text > '9' || *End || errno || Value < Min || Value > Max) exit(2);
	return (unsigned)Value;
}

static double cpuSeconds(void)
{
#if defined(_WIN32)
	FILETIME Created, Exited, Kernel, User;
	require(GetProcessTimes(GetCurrentProcess(), &Created, &Exited, &Kernel, &User) != 0, "CPU clock");
	uint64 Work = ((uint64)Kernel.dwHighDateTime << 32) + Kernel.dwLowDateTime;
	Work += ((uint64)User.dwHighDateTime << 32) + User.dwLowDateTime;
	return (double)Work / 10000000.0;
#else
	struct timespec Work;
	require(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &Work) == 0, "CPU clock");
	return (double)Work.tv_sec + (double)Work.tv_nsec / 1000000000.0;
#endif
}

static const void* result(xfuture* Future)
{
	require(Future && xrtFutureWaitFor(Future, DEADLINE) == XWAIT_OK, "future deadline");
	const void* Value = xrtFutureValue(Future);
	require(Value != NULL, "terminal result absent");
	return Value;
}

static xmqttclient* connectClient(xnetengine* Engine, const xnetaddr* Address, const char* Id, unsigned Worker)
{
	xmqttclientconfig Config;
	require(xmqttClientConfigInit(&Config), "client defaults");
	xmqttpacket Packet;
	xmqttPacketInit(XMQTT_PACKET_CONNECT, &Packet);
	Packet.Body.Connect.CleanStart = true;
	Packet.Body.Connect.KeepAlive = 5;
	Packet.Body.Connect.ClientId = (xstrview){Id, strlen(Id)};
	xmqttclient* Client = NULL;
	require(xmqttClientConnect(Engine, Address, Worker, &Config, &Packet, &Client), "client connect");
	xfuture* Ready = xmqttClientReady(Client);
	const xmqttreadyresult* Outcome = result(Ready);
	if (Outcome->Status != XMQTT_STATUS_OK || !Outcome->Negotiated.Accepted ||
		Outcome->Negotiated.SessionPresent) {
		fprintf(stderr, "MQTT mixed readiness: client=%s status=%d reason=0x%02x accepted=%u session_present=%u connack=%u\n",
			Id, (int)Outcome->Status, (unsigned)Outcome->Reason, (unsigned)Outcome->Negotiated.Accepted,
			(unsigned)Outcome->Negotiated.SessionPresent, (unsigned)(Outcome->Connack != NULL));
		for (const xerror* Cause = Outcome->Cause; Cause; Cause = xrtErrorCause(Cause))
			fprintf(stderr, "MQTT mixed readiness cause: domain=%s operation=%s code=%d system=%d\n",
				xrtErrorDomain(Cause), xrtErrorOperation(Cause), (int)xrtErrorCode(Cause), (int)xrtErrorSystemCode(Cause));
		require(false, "fresh MQTT 5 CONNACK");
	}
	xrtFutureDestroy(Ready);
	return Client;
}

static void subscribe(xmqttclient* Client, xstrview Filter, uint8 Options)
{
	xmqttsubscription Subscription = {Filter, Options};
	xmqttpacket Packet;
	xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE, &Packet);
	Packet.Body.Topics = (xmqttsubscriptionlist){&Subscription, 1};
	xfuture* Future = NULL;
	require(xmqttClientSubscribeAsync(Client, &Packet, DEADLINE, &Future) == XMQTT_ADMISSION_ACCEPTED, "subscribe admission");
	const xmqttsubscriptionresult* Outcome = result(Future);
	require(Outcome->Status == XMQTT_STATUS_OK && Outcome->Acknowledged && Outcome->Requested == 1 &&
		Outcome->Granted == 1 && !Outcome->Rejected, "subscription grant");
	xrtFutureDestroy(Future);
}

static void retire(xmqttclient* Client, uint64 Received)
{
	xmqttclientinfo Info;
	require(xmqttClientInfo(Client, &Info) && Info.Received == Received && !Info.Messages, "client delivery retirement ledger");
	xfuture* Closed = NULL;
	require(xmqttClientDisconnectAsync(Client, NULL, DEADLINE, &Closed) == XMQTT_ADMISSION_ACCEPTED &&
		((const xmqttwriteresult*)result(Closed))->Status == XMQTT_STATUS_OK, "graceful disconnect");
	xrtFutureDestroy(Closed);
	require(xmqttClientWaitFor(Client, DEADLINE) == XWAIT_OK && xmqttClientInfo(Client, &Info) &&
		!Info.Requests && !Info.OutgoingBytes && !Info.StartupBytes && !Info.Messages &&
		Info.Accepted == Info.Completed && xmqttClientDestroy(Client), "physical client retirement");
}

static xfuture* publish(xmqttclient* Client, xstrview Topic, xbytesview Data, unsigned Qos, bool Retain)
{
	xmqttpacket Packet;
	xmqttPacketInit(XMQTT_PACKET_PUBLISH, &Packet);
	Packet.Flags = (uint8)((Qos << 1) | (Retain ? 1u : 0u));
	Packet.Body.Publish.Topic = Topic;
	Packet.Body.Publish.Payload = Data;
	xfuture* Future = NULL;
	require(xmqttClientPublishAsync(Client, &Packet, DEADLINE, &Future) == XMQTT_ADMISSION_ACCEPTED, "publish admission");
	return Future;
}

static void published(xfuture* Future, unsigned Qos)
{
	const xmqttwriteresult* Outcome = result(Future);
	if (Outcome->Status != XMQTT_STATUS_OK)
		fprintf(stderr, "publication status=%d delivery=%d\n", (int)Outcome->Status, (int)Outcome->Delivery);
	require(Outcome->Status == XMQTT_STATUS_OK && Outcome->AcceptedBytes &&
		(!Qos || (Outcome->Delivery == XMQTT_PEER_ACCEPTED && Outcome->Identifier)) &&
		(Qos != 1 || Outcome->Puback) &&
		(Qos != 2 || (Outcome->PubrecReceived && Outcome->PubcompReceived && Outcome->Pubrec && Outcome->Pubcomp)),
		"terminal publication result");
	xrtFutureDestroy(Future);
}

static void payload(unsigned char* Data, size_t Length, uint64 Sequence, unsigned Lane)
{
	Data[0] = (unsigned char)Lane;
	for (size_t Index = 1; Index < 9; ++Index) Data[Index] = (unsigned char)(Sequence >> ((8 - Index) * 8));
	for (size_t Index = 9; Index < Length; ++Index) Data[Index] = (unsigned char)(Sequence * 17 + Index * 131 + Lane);
}

static void delivery(const xmqttpacket* Packet, xstrview Topic, const unsigned char* Data,
	size_t Length, unsigned Qos, bool Retain)
{
	require(Packet && Packet->Type == XMQTT_PACKET_PUBLISH && (Packet->Flags & 15u) == ((Qos << 1) | (Retain ? 1u : 0u)) &&
		Packet->Body.Publish.Topic.Size == Topic.Size && !memcmp(Packet->Body.Publish.Topic.Data, Topic.Data, Topic.Size) &&
		Packet->Body.Publish.Payload.Size == Length && !memcmp(Packet->Body.Publish.Payload.Data, Data, Length),
		"delivery envelope or binary identity");
}

static void exchange(xmqttclient** Client, uint64 Sequence, uint64* SharedCount)
{
	unsigned char Data[BURST][BYTES];
	xfuture* Pending[BURST];
	size_t Length = Sequence % 4 == 0 ? BYTES : 32;
	for (unsigned Lane = 0; Lane < BURST; ++Lane) {
		payload(Data[Lane], Length, Sequence, Lane);
		Pending[Lane] = publish(Client[0], Lane % 2 ? Shared : Direct, (xbytesview){Data[Lane], Length}, Lane / 2, false);
	}
	for (unsigned Lane = 0; Lane < BURST; ++Lane) published(Pending[Lane], Lane / 2);
	unsigned Seen = 0;
	uint64 Deadline = xbenchNowNs() + 5 * Ns;
	while (Seen != (1u << BURST) - 1u) {
		for (unsigned Dest = DIRECT; Dest < CLIENTS; ++Dest) {
			xmqttmessage* Message = NULL;
			if (!xmqttClientTakeMessage(Client[Dest], &Message)) continue;
			const xmqttpacket* Packet = xmqttMessagePacket(Message);
			require(Packet && Packet->Type == XMQTT_PACKET_PUBLISH && Packet->Body.Publish.Payload.Size == Length,
				"mixed delivery packet type or length");
			unsigned Lane = Packet->Body.Publish.Payload.Data[0];
			require(Lane < BURST && !(Seen & (1u << Lane)) && (Lane % 2 == (unsigned)(Dest != DIRECT)),
				"duplicate, unexpected or misrouted delivery");
			delivery(Packet, Lane % 2 ? Shared : Direct, Data[Lane], Length, Lane / 2, false);
			Seen |= 1u << Lane;
			if (Dest != DIRECT) ++SharedCount[Dest - 2u];
			xmqttMessageRelease(Message);
		}
		require(xbenchNowNs() < Deadline, "delivery deadline");
		if (Seen != (1u << BURST) - 1u) xrtThreadYield();
	}
}

static void replay(xnetengine* Engine, const xnetaddr* Address, xmqttclient* Publisher, uint64 Sequence)
{
	unsigned char Data[32];
	payload(Data, sizeof(Data), Sequence, BURST);
	published(publish(Publisher, Retained, (xbytesview){Data, sizeof(Data)}, 1, true), 1);
	xmqttclient* Client = connectClient(Engine, Address, "soak-replay", 1);
	subscribe(Client, Retained, 1);
	xmqttmessage* Message = NULL;
	uint64 Deadline = xbenchNowNs() + 5 * Ns;
	while (!xmqttClientTakeMessage(Client, &Message)) {
		require(xbenchNowNs() < Deadline, "retained replay deadline");
		xrtThreadYield();
	}
	delivery(xmqttMessagePacket(Message), Retained, Data, sizeof(Data), 1, true);
	xmqttMessageRelease(Message);
	retire(Client, 1);
}

static int compare(const void* Left, const void* Right)
{
	double A = *(const double*)Left, B = *(const double*)Right;
	return (A > B) - (A < B);
}

static double tail(const double* Samples, size_t Count, unsigned Thousandths)
{ return Samples[(Count * Thousandths + 999u) / 1000u - 1u]; }

static void tick(uint64 Rounds, uint64 Replays, const uint64* SharedCount, double Seconds, double Cpu,
	double* Samples, size_t Count)
{
	qsort(Samples, Count, sizeof(*Samples), compare);
	printf("{\"rounds\":%" PRIu64 ",\"replays\":%" PRIu64 ",\"shared0\":%" PRIu64 ",\"shared1\":%" PRIu64
		",\"seconds\":%.6f,\"cpu_seconds\":%.6f,\"samples\":%zu,\"p50_us\":%.3f,\"p95_us\":%.3f,"
		"\"p99_us\":%.3f,\"p999_us\":%.3f}\n", Rounds, Replays, SharedCount[0], SharedCount[1], Seconds, Cpu, Count,
		tail(Samples, Count, 500), tail(Samples, Count, 950), tail(Samples, Count, 990), tail(Samples, Count, 999));
	fflush(stdout);
}

static int load(unsigned Port, unsigned Seconds, unsigned Pause)
{
	xnetengineconfig EngineOptions;
	xrtNetEngineConfigInit(&EngineOptions);
	EngineOptions.Workers = 2;
	xnetengine* Engine = xrtNetEngineCreate(&EngineOptions);
	require(Engine && xrtNetEngineStart(Engine), "client engine");
	xnetaddr Address;
	require(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, (uint16)Port), "broker address");
	static const char* const Ids[CLIENTS] = {"soak-publisher", "soak-direct", "soak-shared-a", "soak-shared-b"};
	xmqttclient* Client[CLIENTS];
	for (unsigned Index = 0; Index < CLIENTS; ++Index) Client[Index] = connectClient(Engine, &Address, Ids[Index], Index % 2);
	subscribe(Client[0], DirectFilter, 6); /* QoS 2, No Local. */
	subscribe(Client[DIRECT], DirectFilter, 2);
	for (unsigned Index = 0; Index < MEMBERS; ++Index) subscribe(Client[2 + Index], SharedFilter, 2);
	uint64 Rounds = 0, Replays = 0, SharedCount[MEMBERS] = {0};
	size_t Count = 0;
	double Samples[SAMPLE_LIMIT], Cpu = cpuSeconds();
	uint64 Start = xbenchNowNs(), Now = Start, Last = Start;
	while (Now - Start < Seconds * Ns) {
		uint64 Begin = xbenchNowNs();
		exchange(Client, Rounds, SharedCount);
		if (Rounds % REPLAY_INTERVAL == 0) {
			replay(Engine, &Address, Client[0], Rounds);
			++Replays;
		}
		++Rounds;
		Now = xbenchNowNs();
		require(Count < SAMPLE_LIMIT, "bounded sample window");
		Samples[Count++] = (double)(Now - Begin) / 1000.0;
		if (Now - Last >= 5 * Ns || Now - Start >= Seconds * Ns) {
			tick(Rounds, Replays, SharedCount, (double)(Now - Start) / Ns, cpuSeconds() - Cpu, Samples, Count);
			Count = 0;
			Last = Now;
		}
		if (Now - Start < Seconds * Ns) xrtSleep(Pause);
		Now = xbenchNowNs();
	}
	if (Count) tick(Rounds, Replays, SharedCount, (double)(Now - Start) / Ns, cpuSeconds() - Cpu, Samples, Count);
	published(publish(Client[0], Retained, (xbytesview){NULL, 0}, 1, true), 1);
	retire(Client[0], 0);
	retire(Client[DIRECT], 3 * Rounds);
	for (unsigned Index = 0; Index < MEMBERS; ++Index) retire(Client[2 + Index], SharedCount[Index]);
	require(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "client engine retirement");
	printf("{\"complete\":true,\"rounds\":%" PRIu64 ",\"replays\":%" PRIu64 ",\"deliveries\":%" PRIu64
		",\"seconds\":%.6f}\n", Rounds, Replays, Rounds * BURST + Replays, (double)(Now - Start) / Ns);
	return 0;
}

static int broker(void)
{
	xmqttserverconfig Config;
	require(xmqttServerConfigInit(&Config), "broker defaults");
	Config.ReceiveMaximum = 16;
	Config.MaxConnections = 32;
	Config.MaxRetainedTopics = 1;
	serverexample Example = {0};
	require(exampleServerStart(&Example, &Config, 2), "broker start");
	printf("{\"port\":%u}\n", (unsigned)Example.Address.Port);
	fflush(stdout);
	(void)getchar();
	require(exampleServerDrain(&Example), "broker drain");
	xmqttserverinfo Info;
	require(xmqttServerInfo(Example.Server, &Info) && !Info.Connections && !Info.Established &&
		!Info.Subscriptions && !Info.WildcardNodes && !Info.PersistentSessions && !Info.OfflineSessions &&
		!Info.IncomingPublishes && !Info.OutgoingPublishes && !Info.OutgoingPublishBytes &&
		!Info.WillBytes && !Info.RetainedTopics && !Info.RetainedBytes && !Info.Refused &&
		!Info.TimedOut && !Info.DroppedDeliveries && !Info.RejectedPublications && !Info.DroppedRetained &&
		!Info.OutgoingPublishOverloads, "broker retirement ledger");
	require(exampleServerStop(&Example), "broker engine retirement");
	printf("{\"complete\":true,\"accepted\":%" PRIu64 ",\"forwarded\":%" PRIu64 "}\n", Info.Accepted, Info.Forwarded);
	return 0;
}

int main(int Argc, char** Argv)
{
	if (Argc == 2 && !strcmp(Argv[1], "--broker")) return broker();
	if (Argc == 5 && !strcmp(Argv[1], "--load"))
		return load(argument(Argv[2], 1, UINT16_MAX), argument(Argv[3], 1, 73 * 3600), argument(Argv[4], 2, 100));
	return 2;
}
