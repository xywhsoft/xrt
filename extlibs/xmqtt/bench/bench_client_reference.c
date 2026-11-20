#include "../../../dev/bench/bench_common.h"
#include <xmqtt.h>
#if defined(XMQTT_BENCH_PAHO)
#include <MQTTAsync.h>
#endif

enum { MAX_WINDOW = 64, PAYLOAD = 32 };
static const char Topic[] = "bench/client/reference";
static const char Identity[] = "bench-client-reference";
static const uint64 NsPerSecond = UINT64_C(1000000000);
static const uint64 Deadline = UINT64_C(5000000000);

typedef struct publication {
	uint64 Due, Submitted;
	size_t Index;
	bool Active;
#if defined(XMQTT_BENCH_PAHO)
	xatomic32 Done;
	int Token, CompletedToken, Reason;
#else
	xfuture* Future;
#endif
} publication;

static void require(bool Ok, const char* Message)
{
	if (!Ok) {
		fprintf(stderr, "MQTT reference workload: %s\n", Message);
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

static void fillPayload(unsigned char* Output, uint64 Sequence)
{
	for (size_t Index = 0; Index < 8; ++Index) Output[Index] = (unsigned char)(Sequence >> ((7 - Index) * 8));
	for (size_t Index = 8; Index < PAYLOAD; ++Index) Output[Index] = (unsigned char)(Sequence * 17 + Index * 131);
}

static double cpuSeconds(void)
{
#if defined(_WIN32)
	FILETIME Created, Exited, Kernel, User;
	require(GetProcessTimes(GetCurrentProcess(), &Created, &Exited, &Kernel, &User) != 0, "process CPU clock");
	uint64 Work = ((uint64)Kernel.dwHighDateTime << 32) + Kernel.dwLowDateTime;
	Work += ((uint64)User.dwHighDateTime << 32) + User.dwLowDateTime;
	return (double)Work / 10000000.0;
#else
	struct timespec Work;
	require(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &Work) == 0, "process CPU clock");
	return (double)Work.tv_sec + (double)Work.tv_nsec / 1000000000.0;
#endif
}

#if defined(XMQTT_BENCH_PAHO)
typedef MQTTAsync publisher;
static const char Implementation[] = "paho";

static void succeeded(void* Context, MQTTAsync_successData5* Response)
{
	publication* Pending = Context;
	Pending->CompletedToken = Response ? Response->token : 0;
	Pending->Reason = Response ? (int)Response->reasonCode : 0;
	xrtAtomic32Store(&Pending->Done, 1, XMEMORY_RELEASE);
}

static void failed(void* Context, MQTTAsync_failureData5* Response)
{
	(void)Response;
	publication* Pending = Context;
	xrtAtomic32Store(&Pending->Done, 2, XMEMORY_RELEASE);
}

static void awaitControl(publication* Pending)
{
	uint64 End = xbenchNowNs() + Deadline;
	uint32 Status;
	while ((Status = xrtAtomic32Load(&Pending->Done, XMEMORY_ACQUIRE)) == 0) {
		require(xbenchNowNs() < End, "Paho control deadline");
		xrtSleep(1);
	}
	require(Status == 1 && Pending->Reason == 0, "Paho control rejected");
}

static publisher connectPublisher(unsigned Port, unsigned Window)
{
	const MQTTAsync_nameValue* Version = MQTTAsync_getVersionInfo();
	for (; Version->name && strcmp(Version->name, "Version"); ++Version) {}
	require(Version->name && !strcmp(Version->value, "1.3.15"), "Paho 1.3.15 identity");
	char Address[64];
	snprintf(Address, sizeof(Address), "tcp://127.0.0.1:%u", Port);
	publisher Client;
	MQTTAsync_createOptions Create = MQTTAsync_createOptions_initializer5;
	require(MQTTAsync_createWithOptions(&Client, Address, Identity, MQTTCLIENT_PERSISTENCE_NONE, NULL, &Create) == MQTTASYNC_SUCCESS, "Paho client");
	publication State = {0};
	xrtAtomic32Init(&State.Done, 0);
	MQTTAsync_connectOptions Connect = MQTTAsync_connectOptions_initializer5;
	Connect.cleanstart = 1;
	Connect.keepAliveInterval = 60;
	Connect.connectTimeout = 5;
	Connect.maxInflight = (int)Window;
	Connect.onSuccess5 = succeeded;
	Connect.onFailure5 = failed;
	Connect.context = &State;
	require(MQTTAsync_connect(Client, &Connect) == MQTTASYNC_SUCCESS, "Paho connect admission");
	awaitControl(&State);
	return Client;
}

static void publish(publisher Client, publication* Pending, unsigned Qos, unsigned char* Data)
{
	MQTTAsync_responseOptions Response = MQTTAsync_responseOptions_initializer;
	Response.context = Pending;
	Response.onSuccess5 = succeeded;
	Response.onFailure5 = failed;
	MQTTAsync_message Message = MQTTAsync_message_initializer;
	Message.qos = (int)Qos;
	Message.payload = Data;
	Message.payloadlen = PAYLOAD;
	xrtAtomic32Store(&Pending->Done, 0, XMEMORY_RELAXED);
	require(MQTTAsync_sendMessage(Client, Topic, &Message, &Response) == MQTTASYNC_SUCCESS && Response.token > 0, "Paho publication admission");
	Pending->Token = Response.token;
}

static bool complete(publication* Pending, unsigned Qos)
{
	(void)Qos;
	uint32 Status = xrtAtomic32Load(&Pending->Done, XMEMORY_ACQUIRE);
	if (!Status) return false;
	require(Status == 1 && Pending->CompletedToken == Pending->Token && Pending->Reason == 0, "Paho terminal acknowledgement");
	return true;
}

static void closePublisher(publisher Client)
{
	publication State = {0};
	xrtAtomic32Init(&State.Done, 0);
	MQTTAsync_disconnectOptions Disconnect = MQTTAsync_disconnectOptions_initializer5;
	Disconnect.timeout = 5000;
	Disconnect.context = &State;
	Disconnect.onSuccess5 = succeeded;
	Disconnect.onFailure5 = failed;
	require(MQTTAsync_disconnect(Client, &Disconnect) == MQTTASYNC_SUCCESS, "Paho disconnect admission");
	awaitControl(&State);
	MQTTAsync_destroy(&Client);
}
#else
typedef xmqttclient* publisher;
static xnetengine* Engine;
static const char Implementation[] = "xmqtt";

static publisher connectPublisher(unsigned Port, unsigned Window)
{
	(void)Window;
	xnetengineconfig EngineOptions;
	xrtNetEngineConfigInit(&EngineOptions);
	EngineOptions.Workers = 1;
	Engine = xrtNetEngineCreate(&EngineOptions);
	require(Engine && xrtNetEngineStart(Engine), "xmqtt engine");
	xnetaddr Address;
	require(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, (uint16)Port), "loopback address");
	xmqttclientconfig Settings;
	xmqttClientConfigInit(&Settings);
	Settings.Version = XMQTT_VERSION_5;
	/* Paho 1.3.15 uses the default TCP Nagle setting for this socket. */
	Settings.Stream.NoDelay = false;
	xmqttpacket Connect;
	xmqttPacketInit(XMQTT_PACKET_CONNECT, &Connect);
	Connect.Body.Connect.CleanStart = true;
	Connect.Body.Connect.KeepAlive = 60;
	Connect.Body.Connect.ClientId = (xstrview){Identity, sizeof(Identity) - 1};
	publisher Client;
	require(xmqttClientConnect(Engine, &Address, 0, &Settings, &Connect, &Client), "xmqtt connect admission");
	xfuture* Ready = xmqttClientReady(Client);
	require(Ready && xrtFutureWaitFor(Ready, 5000) == XWAIT_OK, "xmqtt connect deadline");
	const xmqttreadyresult* Outcome = xrtFutureValue(Ready);
	require(Outcome && Outcome->Status == XMQTT_STATUS_OK, "xmqtt CONNACK");
	xrtFutureDestroy(Ready);
	return Client;
}

static void publish(publisher Client, publication* Pending, unsigned Qos, unsigned char* Data)
{
	xmqttpacket Packet;
	xmqttPacketInit(XMQTT_PACKET_PUBLISH, &Packet);
	Packet.Flags = (uint8)(Qos << 1);
	Packet.Body.Publish.Topic = (xstrview){Topic, sizeof(Topic) - 1};
	Packet.Body.Publish.Payload = (xbytesview){Data, PAYLOAD};
	require(xmqttClientPublishAsync(Client, &Packet, 5000, &Pending->Future) == XMQTT_ADMISSION_ACCEPTED, "xmqtt publication admission");
}

static bool complete(publication* Pending, unsigned Qos)
{
	if (!xrtFutureDone(Pending->Future)) return false;
	const xmqttwriteresult* Result = xrtFutureValue(Pending->Future);
	require(Result && Result->Status == XMQTT_STATUS_OK && Result->Delivery == XMQTT_PEER_ACCEPTED &&
		Result->Identifier && (Qos == 1 ? Result->Puback != NULL : Result->PubrecReceived &&
		Result->PubcompReceived && Result->Pubrec && Result->Pubcomp), "xmqtt terminal acknowledgement");
	xrtFutureDestroy(Pending->Future);
	Pending->Future = NULL;
	return true;
}

static void closePublisher(publisher Client)
{
	xmqttpacket Packet;
	xmqttPacketInit(XMQTT_PACKET_DISCONNECT, &Packet);
	xfuture* Closed = NULL;
	require(xmqttClientDisconnect(Client, &Packet, 5000, &Closed) == XMQTT_ADMISSION_ACCEPTED &&
		Closed && xrtFutureWaitFor(Closed, 5000) == XWAIT_OK && xmqttClientWaitFor(Client, 5000) == XWAIT_OK,
		"xmqtt final disconnect");
	const xmqttwriteresult* Result = xrtFutureValue(Closed);
	require(Result && Result->Status == XMQTT_STATUS_OK, "xmqtt disconnect result");
	xrtFutureDestroy(Closed);
	xmqttclientinfo Info;
	require(xmqttClientInfo(Client, &Info) && !Info.Requests && !Info.OutgoingBytes && Info.Accepted == Info.Completed, "xmqtt physical requests");
	require(xmqttClientDestroy(Client) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "xmqtt physical close");
}
#endif

static void phase(publisher Client, unsigned Count, unsigned Window, unsigned Rate, unsigned Qos,
	size_t Base, double* Latency, double* Lag)
{
	publication Pending[MAX_WINDOW] = {0};
#if defined(XMQTT_BENCH_PAHO)
	for (unsigned Index = 0; Index < Window; ++Index) xrtAtomic32Init(&Pending[Index].Done, 0);
#endif
	unsigned Submitted = 0, Completed = 0;
	double Cpu = cpuSeconds();
	uint64 Start = xbenchNowNs();
	while (Completed < Count) {
		bool Progress = false;
		for (unsigned Index = 0; Index < Window; ++Index) {
			publication* Slot = &Pending[Index];
			if (!Slot->Active) continue;
			uint64 Now = xbenchNowNs();
			require(Now - Slot->Due < Deadline, "publication deadline");
			if (!complete(Slot, Qos)) continue;
			if (Latency) {
				Latency[Slot->Index] = (double)(Now - Slot->Due) / 1000.0;
				Lag[Slot->Index] = (double)(Slot->Submitted - Slot->Due) / 1000.0;
			}
			Slot->Active = false;
			++Completed;
			Progress = true;
		}
		while (Submitted < Count) {
			publication* Slot = &Pending[Submitted % Window];
			uint64 Now = xbenchNowNs();
			uint64 Due = Rate ? Start + (uint64)Submitted * NsPerSecond / Rate : Now;
			if (Now < Due || Slot->Active) break;
			unsigned char Data[PAYLOAD];
			fillPayload(Data, Base + Submitted);
			Slot->Index = Submitted++;
			Slot->Due = Due;
			Slot->Submitted = xbenchNowNs();
			require(Slot->Submitted - Due < Deadline, "scheduled admission deadline");
			publish(Client, Slot, Qos, Data);
			Slot->Active = true;
			Progress = true;
		}
		if (!Progress) xrtThreadYield();
	}
	if (Latency) {
		double Wall = (double)(xbenchNowNs() - Start) / (double)NsPerSecond;
		printf("\"accepted\":%u,\"completed\":%u,\"seconds\":%.9f,\"cpu_seconds\":%.7f,\"publishes_per_sec\":%.6f,",
			Submitted, Completed, Wall, cpuSeconds() - Cpu, (double)Completed / Wall);
	}
}

static int compare(const void* Left, const void* Right)
{
	double A = *(const double*)Left, B = *(const double*)Right;
	return (A > B) - (A < B);
}

static double quantile(const double* Samples, unsigned Count, unsigned Thousandths)
{ return Samples[((size_t)Count * Thousandths + 999) / 1000 - 1]; }

int main(int Argc, char** Argv)
{
	if (Argc != 6) return 2;
	unsigned Port = argument(Argv[1], 1, UINT16_MAX);
	unsigned Count = argument(Argv[2], 1, 100000);
	unsigned Rate = argument(Argv[3], 1, 100000);
	unsigned Window = argument(Argv[4], 1, MAX_WINDOW);
	unsigned Qos = argument(Argv[5], 1, 2);
	require(Count <= Rate * 15u, "bounded offer horizon");
	unsigned Warmup = 128;
	double* Latency = malloc(2 * Count * sizeof(*Latency));
	require(Latency != NULL, "sample storage");
	double* Lag = Latency + Count;
	publisher Client = connectPublisher(Port, Window);
	phase(Client, Warmup, Window, 0, Qos, 0, NULL, NULL);
	printf("{\"client\":\"%s\",\"version\":5,\"qos\":%u,\"window\":%u,\"count\":%u,\"rate\":%u,\"warmup\":%u,\"bytes\":%d,",
		Implementation, Qos, Window, Count, Rate, Warmup, PAYLOAD);
	phase(Client, Count, Window, Rate, Qos, Warmup, Latency, Lag);
	closePublisher(Client);
	qsort(Latency, Count, sizeof(*Latency), compare);
	qsort(Lag, Count, sizeof(*Lag), compare);
	printf("\"p50_us\":%.3f,\"p95_us\":%.3f,\"p99_us\":%.3f,\"p999_us\":%.3f,\"offer_lag_p99_us\":%.3f,\"teardown\":true}\n",
		quantile(Latency, Count, 500), quantile(Latency, Count, 950), quantile(Latency, Count, 990),
		quantile(Latency, Count, 999), quantile(Lag, Count, 990));
	free(Latency);
	return 0;
}
