/* Receiver-side peer comparison. The peer measures terminal MQTT ACKs. */
#include "../../../dev/bench/bench_common.h"
#include <xrt/sync.h>
#include <xmqtt.h>
#if defined(XMQTT_BENCH_PAHO)
#include <MQTTAsync.h>
#elif defined(XMQTT_BENCH_RECEIVER_TLS)
#include "../examples/tls/material.h"
#endif

enum { WARMUP = 128, PAYLOAD_BYTES = 32 };
static const char Topic[] = "bench/client/receive-reference";
static const char Identity[] = "bench-client-receive-reference";

typedef struct receiver {
    unsigned Qos, Count, Received;
    uint64* Latencies;
    uint64 LastReliable;
    xevent* Done;
    bool DefaultTcp;
    const char* CaFile;
} receiver;

static void require(bool Ok, const char* Message)
{
    if (!Ok) {
        fprintf(stderr, "MQTT receiver reference: %s\n", Message);
        exit(EXIT_FAILURE);
    }
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

static uint64 load64(const unsigned char* Data)
{
    uint64 Result = 0;
    for (unsigned I = 0; I < 8; ++I) Result = (Result << 8u) | Data[I];
    return Result;
}

static bool observe(receiver* State, const char* Name, size_t NameSize,
    const unsigned char* Data, size_t Size, unsigned Qos)
{
    uint64 Observed = xbenchNowNs();
    unsigned Index = State->Received, Total = WARMUP + State->Count;
    require(Index <= Total && Qos == (Index == Total ? 0u : State->Qos) &&
        NameSize == sizeof(Topic) - 1 && !memcmp(Name, Topic, NameSize) &&
        Size == PAYLOAD_BYTES, "delivery envelope");
    uint64 Sequence = load64(Data);
    if (Sequence != Index) {
        fprintf(stderr, "MQTT receiver reference: delivered sequence %llu, expected %u\n",
            (unsigned long long)Sequence, Index);
        exit(EXIT_FAILURE);
    }
    uint64 Due = load64(Data + 8);
    require(Due && Due <= Observed, "peer timestamp");
    for (unsigned I = 16; I < PAYLOAD_BYTES; ++I)
        require(Data[I] == (unsigned char)(Index * 17u + I * 131u), "binary payload");
    if (Index >= WARMUP && Index < Total) {
        State->Latencies[Index - WARMUP] = Observed - Due;
        if (Index + 1 == Total) State->LastReliable = Observed;
    }
    ++State->Received;
    return Index == Total;
}

#if defined(XMQTT_BENCH_PAHO)
typedef MQTTAsync connection;
static const char Implementation[] = "paho";
typedef struct control { xatomic32 Done; int Reason; } control;

static void success(void* Context, MQTTAsync_successData5* Response)
{
    control* State = Context;
    State->Reason = Response ? (int)Response->reasonCode : 0;
    xrtAtomic32Store(&State->Done, 1, XMEMORY_RELEASE);
}

static void failure(void* Context, MQTTAsync_failureData5* Response)
{
    (void)Response;
    xrtAtomic32Store(&((control*)Context)->Done, 2, XMEMORY_RELEASE);
}

static void await(control* Control, int Expected)
{
    double End = xrtTimer() + 5.0;
    uint32 Done;
    while (!(Done = xrtAtomic32Load(&Control->Done, XMEMORY_ACQUIRE))) {
        require(xrtTimer() < End, "Paho operation deadline");
        xrtSleep(1);
    }
    require(Done == 1 && Control->Reason == Expected, "Paho operation outcome");
}

static int received(void* Context, char* Name, int Size, MQTTAsync_message* Message)
{
    receiver* State = Context;
    require(Name && Size >= 0 && Message && Message->payloadlen >= 0 && !Message->retained,
        "Paho message ownership or metadata");
    bool Finished = observe(State, Name, Size ? (size_t)Size : strlen(Name),
        Message->payload, (size_t)Message->payloadlen, (unsigned)Message->qos);
    MQTTAsync_freeMessage(&Message);
    MQTTAsync_free(Name);
    if (Finished) require(xrtEventSet(State->Done), "Paho completion event");
    return 1;
}

static connection openReceiver(unsigned Port, unsigned Window, receiver* State)
{
    const MQTTAsync_nameValue* Version = MQTTAsync_getVersionInfo();
    for (; Version->name && strcmp(Version->name, "Version"); ++Version) {}
    require(Version->name && !strcmp(Version->value, "1.3.15"), "Paho 1.3.15 identity");
    char Address[64];
    snprintf(Address, sizeof(Address), "%s:%u", State->CaFile ? "ssl://localhost" : "tcp://127.0.0.1", Port);
    connection Client;
    MQTTAsync_createOptions Creation = MQTTAsync_createOptions_initializer5;
    require(MQTTAsync_createWithOptions(&Client, Address, Identity, MQTTCLIENT_PERSISTENCE_NONE,
        NULL, &Creation) == MQTTASYNC_SUCCESS, "Paho creation");
    require(MQTTAsync_setCallbacks(Client, State, NULL, received, NULL) == MQTTASYNC_SUCCESS, "Paho callback");
    control Control = {0};
    xrtAtomic32Init(&Control.Done, 0);
    MQTTProperties Properties = MQTTProperties_initializer;
    MQTTProperty Maximum = {.identifier = MQTTPROPERTY_CODE_RECEIVE_MAXIMUM};
    Maximum.value.integer2 = (unsigned short)Window;
    require(MQTTProperties_add(&Properties, &Maximum) == 0, "Paho Receive Maximum");
    MQTTAsync_connectOptions Connect = MQTTAsync_connectOptions_initializer5;
    Connect.cleanstart = 1;
    Connect.connectTimeout = 5;
    Connect.connectProperties = &Properties;
    Connect.context = &Control;
    Connect.onSuccess5 = success;
    Connect.onFailure5 = failure;
#if defined(XMQTT_BENCH_RECEIVER_TLS)
    MQTTAsync_SSLOptions Tls = MQTTAsync_SSLOptions_initializer;
    if (State->CaFile) {
        Tls.trustStore = State->CaFile;
        Tls.enableServerCertAuth = 1;
        Tls.verify = 1;
        Tls.disableDefaultTrustStore = 1;
        Connect.ssl = &Tls;
    }
#endif
    require(MQTTAsync_connect(Client, &Connect) == MQTTASYNC_SUCCESS, "Paho CONNECT");
    await(&Control, 0);
    MQTTProperties_free(&Properties);
    xrtAtomic32Store(&Control.Done, 0, XMEMORY_RELAXED);
    MQTTAsync_responseOptions Subscribe = MQTTAsync_responseOptions_initializer;
    Subscribe.context = &Control;
    Subscribe.onSuccess5 = success;
    Subscribe.onFailure5 = failure;
    require(MQTTAsync_subscribe(Client, Topic, (int)State->Qos, &Subscribe) == MQTTASYNC_SUCCESS, "Paho SUBSCRIBE");
    await(&Control, (int)State->Qos);
    return Client;
}

static void closeReceiver(connection Client)
{
    control Control = {0};
    xrtAtomic32Init(&Control.Done, 0);
    MQTTAsync_disconnectOptions Disconnect = MQTTAsync_disconnectOptions_initializer5;
    Disconnect.timeout = 5000;
    Disconnect.context = &Control;
    Disconnect.onSuccess5 = success;
    Disconnect.onFailure5 = failure;
    require(MQTTAsync_disconnect(Client, &Disconnect) == MQTTASYNC_SUCCESS, "Paho DISCONNECT");
    await(&Control, 0);
    MQTTAsync_destroy(&Client);
}
#else
typedef xmqttclient* connection;
static xnetengine* Engine;
static const char Implementation[] = "xmqtt";

static void notified(xmqttclient* Client, ptr Context)
{
    receiver* State = Context;
    xmqttmessage* Message;
    while (xmqttClientTakeMessage(Client, &Message)) {
        const xmqttpacket* Packet = xmqttMessagePacket(Message);
        require(Packet && Packet->Type == XMQTT_PACKET_PUBLISH && !(Packet->Flags & XMQTT_PUBLISH_RETAIN),
            "xmqtt message metadata");
        xstrview Name = Packet->Body.Publish.Topic;
        xbytesview Payload = Packet->Body.Publish.Payload;
        bool Finished = observe(State, Name.Data, Name.Size, Payload.Data, Payload.Size, (Packet->Flags >> 1u) & 3u);
        xmqttMessageRelease(Message);
        if (Finished) require(xrtEventSet(State->Done), "xmqtt completion event");
    }
}

#if defined(XMQTT_BENCH_RECEIVER_TLS)
static bool connectTls(const char* CaFile, const xnetaddr* Address, const xmqttclientconfig* Config,
    const xmqttpacket* Connect, connection* Client)
{
    size_t Size;
    bytes Pem = xmqttExampleTlsRead(CaFile, 65536, &Size);
    xx509store* Store = xrtX509StoreCreate();
    bool Trusted = Pem && Store && xrtX509StoreAddData(Store, (xbytesview){Pem, Size}, NULL);
    xrtFree(Pem);
    require(Trusted, "xmqtt trust store");
    xtlsverifylimits Limits;
    xrtTlsVerifyLimitsInit(&Limits);
    xtlsverifierconfig Verify;
    xrtTlsVerifierConfigInit(&Verify);
    Verify.Store = Store;
    xtlsverifier* Verifier = xrtTlsVerifierCreateLimited(&Verify, &Limits);
    xrtX509StoreFree(Store);
    require(Verifier != NULL, "xmqtt verifier");
    const xtlsversion Versions[] = {XTLS_VERSION_13};
    const xtlscipher Ciphers[] = {XTLS_AES_128_GCM_SHA256};
    const uint16 Groups[] = {XTLS_GROUP_X25519};
    const xtlssignature Signatures[] = {XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256};
    xtlspolicy Policy;
    xrtTlsPolicyInit(&Policy);
    Policy.Versions = Versions;
    Policy.VersionCount = 1;
    Policy.Ciphers = Ciphers;
    Policy.CipherCount = 1;
    Policy.Groups = Groups;
    Policy.GroupCount = 1;
    Policy.Signatures = Signatures;
    Policy.SignatureCount = 1;
    xtlscontextconfig Options;
    xrtTlsContextConfigInit(&Options);
    Options.Policy = &Policy;
    xtlscontext* Context = xrtTlsContextCreate(&Options);
    xmqttclienttlsconfig Tls;
    require(Context && xmqttClientTlsConfigInit(&Tls), "xmqtt TLS configuration");
    Tls.Session.Context = Context;
    Tls.Session.Verifier = Verifier;
    Tls.Session.ServerName = Tls.Session.VerifyName = XRT_STR_LITERAL("localhost");
    Tls.Session.ResumeLimit = 0;
    bool Started = xmqttClientTlsConnect(Engine, Address, 0, Config, &Tls, Connect, Client);
    xrtTlsContextRelease(Context);
    xrtTlsVerifierRelease(Verifier);
    return Started;
}
#endif

static connection openReceiver(unsigned Port, unsigned Window, receiver* State)
{
    xnetengineconfig EngineConfig;
    xrtNetEngineConfigInit(&EngineConfig);
    EngineConfig.Workers = 1;
    Engine = xrtNetEngineCreate(&EngineConfig);
    require(Engine && xrtNetEngineStart(Engine), "xmqtt engine");
    xnetaddr Address;
    require(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, (uint16)Port), "xmqtt peer address");
    xmqttclientconfig Config;
    require(xmqttClientConfigInit(&Config), "xmqtt client config");
    if (!State->DefaultTcp) Config.Stream.NoDelay = false;
    Config.Notify = notified;
    Config.User = State;
    xmqttpacket Connect;
    xmqttPacketInit(XMQTT_PACKET_CONNECT, &Connect);
    Connect.Body.Connect.CleanStart = true;
    Connect.Body.Connect.KeepAlive = 60;
    Connect.Body.Connect.ClientId = (xstrview){Identity, sizeof(Identity) - 1};
    xmqttproperty Maximum = {.Id = XMQTT_PROPERTY_RECEIVE_MAXIMUM,
        .Type = XMQTT_PROPERTY_UINT16, .Value.Number = Window};
    Connect.Properties = (xmqttpropertylist){&Maximum, 1};
    connection Client;
#if defined(XMQTT_BENCH_RECEIVER_TLS)
    if (State->CaFile)
        require(connectTls(State->CaFile, &Address, &Config, &Connect, &Client), "xmqtt TLS CONNECT");
    else
#endif
        require(xmqttClientConnect(Engine, &Address, 0, &Config, &Connect, &Client), "xmqtt CONNECT");
    xfuture* Ready = xmqttClientReady(Client);
    const xmqttreadyresult* Outcome = Ready && xrtFutureWaitFor(Ready, 5000) == XWAIT_OK ? xrtFutureValue(Ready) : NULL;
    require(Outcome && Outcome->Status == XMQTT_STATUS_OK, "xmqtt CONNACK");
    xrtFutureDestroy(Ready);
    xmqttsubscription Filter = {{Topic, sizeof(Topic) - 1}, (uint8)State->Qos};
    xmqttpacket Subscribe;
    xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE, &Subscribe);
    Subscribe.Body.Topics = (xmqttsubscriptionlist){&Filter, 1};
    xfuture* Subscribed;
    require(xmqttClientSubscribe(Client, &Subscribe, 5000, &Subscribed) == XMQTT_ADMISSION_ACCEPTED, "xmqtt SUBSCRIBE");
    const xmqttsubscriptionresult* Result = xrtFutureValue(Subscribed);
    require(Result && Result->Status == XMQTT_STATUS_OK && Result->Acknowledged && Result->Granted == 1 &&
        Result->Ack && xmqttMessagePacket(Result->Ack)->Body.Reasons.Size == 1 &&
        xmqttMessagePacket(Result->Ack)->Body.Reasons.Data[0] == State->Qos, "xmqtt SUBACK");
    xrtFutureDestroy(Subscribed);
    return Client;
}

static void closeReceiver(connection Client)
{
    xfuture* Closed;
    require(xmqttClientDisconnect(Client, NULL, 5000, &Closed) == XMQTT_ADMISSION_ACCEPTED, "xmqtt DISCONNECT");
    const xmqttwriteresult* Result = xrtFutureValue(Closed);
    require(Result && Result->Status == XMQTT_STATUS_OK, "xmqtt DISCONNECT outcome");
    xrtFutureDestroy(Closed);
    require(xmqttClientWaitFor(Client, 5000) == XWAIT_OK && xmqttClientDestroy(Client) &&
        xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "xmqtt physical teardown");
}
#endif

static int byTime(const void* A, const void* B)
{
    uint64 Left = *(const uint64*)A, Right = *(const uint64*)B;
    return (Left > Right) - (Left < Right);
}

static double quantileUs(const receiver* State, size_t Rank)
{
    return (double)State->Latencies[((size_t)State->Count * Rank + 999) / 1000 - 1] / 1000.0;
}

int main(int Argc, char** Argv)
{
    unsigned Port = xbenchArgU32(Argc, Argv, 1, 0), Count = xbenchArgU32(Argc, Argv, 2, 0);
    unsigned Window = xbenchArgU32(Argc, Argv, 3, 0), Qos = xbenchArgU32(Argc, Argv, 4, 0);
    receiver State = {.Qos = Qos, .Count = Count};
    int Next = 5;
#if defined(XMQTT_BENCH_RECEIVER_TLS)
    if (Argc > Next + 1 && !strcmp(Argv[Next], "--tls")) {
        State.CaFile = Argv[Next + 1];
        Next += 2;
    }
#endif
#if !defined(XMQTT_BENCH_PAHO)
    if (Argc > Next && !strcmp(Argv[Next], "--default-tcp")) {
        State.DefaultTcp = true;
        ++Next;
    }
#endif
    if (Argc != Next || !Port || Port > 65535 || !Count || Count > 60000 || !Window || Window > 64 ||
        (Qos != 1 && Qos != 2)) return 2;
    (void)xbenchNowNs();
    State.Latencies = malloc(Count * sizeof(*State.Latencies));
    State.Done = xrtEventCreate(false, false);
    require(State.Latencies && State.Done, "observer storage");
    connection Client = openReceiver(Port, Window, &State);
    double Cpu = cpuSeconds();
    require(xrtEventWaitFor(State.Done, 22000) == XWAIT_OK, "verified delivery deadline");
    Cpu = cpuSeconds() - Cpu;
    closeReceiver(Client);
    require(State.Received == WARMUP + Count + 1 && xrtEventDestroy(State.Done), "retired application ledger");
    qsort(State.Latencies, Count, sizeof(*State.Latencies), byTime);
    printf("{\"client\":\"%s\",\"qos\":%u,\"window\":%u,\"measured\":%u,\"warmup\":%u,"
        "\"application_p50_us\":%.3f,\"application_p95_us\":%.3f,"
        "\"application_p99_us\":%.3f,\"application_p999_us\":%.3f,"
        "\"cpu_seconds\":%.6f,\"last_reliable_ns\":%llu,\"tls\":%s,\"retired\":true}\n",
        Implementation, Qos, Window, Count, WARMUP, quantileUs(&State, 500),
        quantileUs(&State, 950), quantileUs(&State, 990), quantileUs(&State, 999), Cpu,
        (unsigned long long)State.LastReliable, State.CaFile ? "true" : "false");
    free(State.Latencies);
    return 0;
}
