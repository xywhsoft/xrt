#define XREDIS_MODULE_XREDIS_ROLE_CLIENT
#define XREDIS_MODULE_XREDIS_POOL_SHARED_CONNECT
#define XREDIS_MODULE_XREDIS_CLUSTER_CLIENT
#include <xredis.h>
#include "../../examples/cluster_client/replica_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const xbytesview Get[] = {XRT_BYTES_LITERAL("GET"), XRT_BYTES_LITERAL("xrt-owned:readonly:{fixed}\0key")};
static const xbytesview Expected = XRT_BYTES_LITERAL("value\0x");

static void require(bool Condition, cstr Message)
{
    if (!Condition) {
        fprintf(stderr, "[FAIL] Cluster READONLY: %s\n", Message);
        exit(1);
    }
}

static const void* waitValue(xfuture* Future)
{
    require(Future && xrtFutureWaitFor(Future, 5000) == XWAIT_OK && xrtFutureValue(Future), "typed Future");
    return xrtFutureValue(Future);
}

static xredisreplyvalue root(xfuture* Future, xredisrequeststatus Status)
{
    const xredisrequestresult* Result = waitValue(Future);
    xredisreplyinfo Info = {.Size = sizeof(Info)};
    xredisreplyvalue Value;
    require(Result->Status == Status && Result->Delivery == XREDIS_REPLIED, "server result");
    require(xredisReplyInfo(Result->Reply, &Info) && xredisReplyGet(Result->Reply, Info.Root, &Value), "reply root");
    return Value;
}

static void checkValue(xredisreplyvalue Value)
{
    require(Value.Type == XREDIS_REPLY_BULK_STRING && Value.Data.Size == Expected.Size &&
        memcmp(Value.Data.Data, Expected.Data, Expected.Size) == 0, "exact binary replica value");
}

static void checkRead(xfuture* Future)
{
    checkValue(root(Future, XREDIS_REQUEST_OK));
    xrtFutureDestroy(Future);
}

static void checkOwner(xredisclient* Client, cstr RunId)
{
    const xbytesview Args[] = {XRT_BYTES_LITERAL("INFO"), XRT_BYTES_LITERAL("SERVER")};
    xfuture* Future = NULL;
    require(xredisClientCall(Client, Args, 2, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED, "INFO admission");
    xredisreplyvalue Value = root(Future, XREDIS_REQUEST_OK);
    char Identity[64];
    size_t Size = (size_t)snprintf(Identity, sizeof(Identity), "\r\nrun_id:%s\r\n", RunId);
    require(Size < sizeof(Identity), "owned fixture identity length");
    bool Owned = false;
    for (size_t Index = 0; Index + Size <= Value.Data.Size; ++Index)
        if (memcmp(Value.Data.Data + Index, Identity, Size) == 0) { Owned = true; break; }
    require(Owned, "replica identity");
    xrtFutureDestroy(Future);
}

static bool connectReplica(xnetengine* Engine, const xnetaddr* Address, xredisinitmode Mode,
    bool Readonly, int64 Timeout, xredisclient** Client)
{
    xredisclientinitconfig Init;
    xredisClientInitConfigInit(&Init);
    Init.Mode = Mode;
    Init.Timeout = Timeout;
    Init.Flags = XREDIS_INIT_REQUIRE_REPLICA | (Readonly ? XREDIS_INIT_CLUSTER_READONLY : 0u);
    return xredisClientConnectInit(Engine, Address, 0, NULL, &Init, Client);
}

static void closeClient(xredisclient* Client)
{
    xfuture* Closed = xredisClientClosed(Client);
    require(xredisClientClose(Client) && xrtFutureWaitFor(Closed, 5000) == XWAIT_OK, "client physical close");
    xredisclientinfo Info = {.Size = sizeof(Info)};
    require(xredisClientInfo(Client, &Info) && Info.InitBytes == 0 && Info.Requests == 0, "client storage retired");
    xrtFutureDestroy(Closed);
    xredisClientRelease(Client);
}

static void checkDirect(xnetengine* Engine, const xnetaddr* Address, xredisinitmode Mode,
    bool Readonly, cstr RunId, cstr Moved)
{
    xredisclient* Client = NULL;
    require(connectReplica(Engine, Address, Mode, Readonly, 5000, &Client), "direct connect");
    xfuture* Ready = xredisClientReady(Client), *Read = NULL;
    require(xredisClientCallAsync(Client, Get, 2, 5000, &Read) == XREDIS_ADMISSION_ACCEPTED, "direct GET");
    const xredisreadyresult* State = waitValue(Ready);
    require(State->Status == XREDIS_READY_OK && State->Step == XREDIS_INIT_DONE &&
        State->Protocol == (Mode == XREDIS_INIT_RESP2 ? XREDIS_RESP2 : XREDIS_RESP3), "direct ready");
    checkOwner(Client, RunId);
    if (Readonly) {
        checkRead(Read);
        const xbytesview Toggle = XRT_BYTES_LITERAL("READWRITE");
        xfuture* Refused = NULL;
        require(xredisClientCallAsync(Client, &Toggle, 1, 5000, &Refused) == XREDIS_ADMISSION_ERROR && !Refused &&
            xrtErrorCode(xrtGetError()) == XREDIS_CLIENT_COMMAND_MODE, "managed READWRITE admission");
        xrtClearError();
        require(xredisClientCall(Client, Get, 2, 5000, &Read) == XREDIS_ADMISSION_ACCEPTED, "second direct GET");
        checkRead(Read);
    } else {
        xredisreplyvalue Value = root(Read, XREDIS_REQUEST_SERVER_ERROR);
        require(Value.Type == XREDIS_REPLY_SIMPLE_ERROR && Value.Data.Size == strlen(Moved) &&
            memcmp(Value.Data.Data, Moved, Value.Data.Size) == 0, "actual MOVED to primary");
        xrtFutureDestroy(Read);
    }
    xrtFutureDestroy(Ready);
    closeClient(Client);
}

static bool poolConnect(xnetengine* Engine, size_t Slot, uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Client)
{
    (void)Slot;
    (void)Attempt;
    return connectReplica(Engine, Data, XREDIS_INIT_RESP2, true, Timeout, Client);
}

static void waitPool(xredispool* Pool, size_t Members)
{
    xredispoolinfo Info = {.Size = sizeof(Info)};
    double Deadline = xrtTimer() + 5;
    for (;;) {
        require(xredisPoolInfo(Pool, &Info) && xrtTimer() < Deadline, "pool ready members");
        if (Info.Idle == Members) break;
        xrtSleep(1);
    }
}

static void checkPool(xnetengine* Engine, xnetaddr* Address)
{
    xredispoolconnectconfig Connect;
    xredisPoolConnectConfigInit(&Connect);
    Connect.Connect = poolConnect;
    Connect.Data = Address;
    Connect.Protocol = XREDIS_RESP2;
    xredispool* Pool = NULL;
    require(xredisSharedPoolConnect(Engine, 0, 2, NULL, &Connect, &Pool) == XREDIS_ADMISSION_ACCEPTED, "maintained shared pool");
    waitPool(Pool, 2);
    /* Queue an even number of reads so each ready slot is used twice. */
    xfuture* Reads[4] = {0};
    for (size_t Index = 0; Index < 4; ++Index)
        require(xredisSharedPoolCallAsync(Pool, Get, 2, 5000, &Reads[Index]) == XREDIS_ADMISSION_ACCEPTED, "pool GET");
    for (size_t Index = 0; Index < 4; ++Index) checkRead(Reads[Index]);
    xredispoolconnectinfo Counters = {.Size = sizeof(Counters)};
    require(xredisPoolConnectInfo(Pool, &Counters) && Counters.Attempts == 2 && Counters.Connected == 2 &&
        Counters.Failures == 0, "no replacement after valid READONLY");
    xfuture* Closed = xredisPoolClosed(Pool);
    require(xredisPoolClose(Pool) && xrtFutureWaitFor(Closed, 5000) == XWAIT_OK, "pool physical close");
    xrtFutureDestroy(Closed);
    xredisPoolRelease(Pool);
}

typedef struct clusterfixture {
    uint16 Primary, Replica, Witness;
    xredisinitmode Mode;
    xatomic32 DenyReplica;
} clusterfixture;

static bool clusterAuthorize(const xredisclusteraddress* Address, ptr Data)
{
    clusterfixture* Fixture = Data;
    if (Address->Tls || Address->Host.Size != 9 || memcmp(Address->Host.Data, "127.0.0.1", 9)) return false;
    if (Address->Reason == XREDIS_CLUSTER_REPLICA_READ)
        return Address->Port == Fixture->Replica && !xrtAtomic32Load(&Fixture->DenyReplica, XMEMORY_ACQUIRE);
    return Address->Port == Fixture->Primary ||
        (Fixture->Witness && (Address->Port == Fixture->Replica || Address->Port == Fixture->Witness));
}

static bool clusterConnect(const xredisclusteraddress* Endpoint, xnetengine* Engine, size_t Slot, uint64 Attempt,
    int64 Timeout, ptr Data, xredisclient** Client)
{
    (void)Slot;
    (void)Attempt;
    clusterfixture* Fixture = Data;
    xnetaddr Address;
    if (!xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, Endpoint->Port)) return false;
    if (Endpoint->Reason == XREDIS_CLUSTER_REPLICA_READ)
        return connectReplica(Engine, &Address, Fixture->Mode, true, Timeout, Client);
    xredisclientinitconfig Init;
    xredisClientInitConfigInit(&Init);
    Init.Timeout = Timeout;
    Init.Mode = Fixture->Mode;
    return xredisClientConnectInit(Engine, &Address, 0, NULL, &Init, Client);
}

static void checkManagedRead(xredisclusterclient* Client, xredisclusternodes* Nodes,
    uint16 Slot, bool PreferReplica, uint16 Port, xredisclusterredirectkind Profile)
{
    xfuture* Future = NULL;
    if (PreferReplica) {
        xredispool* Pool = NULL;
        uint16 SelectedPort = 0;
        xredisclusterpoolstatus Selected = appReplicaPool(Client, Nodes, Slot, 5000, &Pool, &SelectedPort);
        if (Selected == XREDIS_CLUSTER_POOL_READY) {
            require(Profile == XREDIS_CLUSTER_REPLICA_READ && SelectedPort == Port, "APP selected replica");
            require(xredisSharedPoolCallAsync(Pool, Get, 2, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED,
                "explicit replica GET admission");
            xredisPoolRelease(Pool);
            checkValue(root(Future, XREDIS_REQUEST_OK));
            xrtFutureDestroy(Future);
            return;
        }
        require(Selected == XREDIS_CLUSTER_POOL_DENIED, "replica denied before native admission");
    }
    require(!Profile && xredisClusterClientCall(Client, Slot, Get, 2, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED,
        "managed primary GET admission");
    const xredisclusterclientresult* Result = waitValue(Future);
    require(Result->Status == XREDIS_CLUSTER_CLIENT_COMPLETE && Result->RouteObserved &&
        Result->Route.Status == XREDIS_CLUSTER_ROUTE_COMPLETE && Result->Route.Attempts == 1 &&
        Result->Route.AttemptAddress.Port == Port && !Result->Route.AttemptAddress.Reason &&
        Result->Route.ProviderCalls == 1,"primary GET endpoint");
    checkValue(root(Result->Native, XREDIS_REQUEST_OK));
    xrtFutureDestroy(Future);
}

static void closeManaged(xredisclusterclient* Client, xredisclusternodes* Nodes)
{
    xfuture* Closed = xredisClusterClientClosed(Client);
    require(xredisClusterClientClose(Client) && xrtFutureWaitFor(Closed, 5000) == XWAIT_OK, "managed Cluster close");
    xrtFutureDestroy(Closed);
    xredisClusterClientRelease(Client);
    Closed = xredisClusterNodesClosed(Nodes);
    require(xredisClusterNodesClose(Nodes) && xrtFutureWaitFor(Closed, 5000) == XWAIT_OK, "managed cache physical close");
    xrtFutureDestroy(Closed);
    xredisClusterNodesRelease(Nodes);
}

static void checkManaged(xnetengine* Engine, uint16 Primary, uint16 Replica, xredisinitmode Mode, xredisclusterformat Format)
{
    printf("managed replica case mode=%d format=%d\n", (int)Mode, (int)Format);
    clusterfixture Fixture = {.Primary = Primary, .Replica = Replica, .Mode = Mode};
    xrtAtomic32Init(&Fixture.DenyReplica, 0);
    xredisclusternodesconfig NodesConfig;
    xredisClusterNodesConfigInit(&NodesConfig);
    NodesConfig.Protocol = Mode == XREDIS_INIT_RESP2 ? XREDIS_RESP2 : XREDIS_RESP3;
    NodesConfig.Authorize = clusterAuthorize;
    NodesConfig.Connect = clusterConnect;
    NodesConfig.Data = &Fixture;
    xredisclusternodes* Nodes = NULL;
    require(xredisClusterNodesCreate(Engine, 0, &NodesConfig, &Nodes) == XREDIS_ADMISSION_ACCEPTED, "managed node cache");
    xredisclusteraddress Seed = {.Host = XRT_BYTES_LITERAL("127.0.0.1"), .Port = Primary};
    xredisclusteraddress ReadNode = {.Host = XRT_BYTES_LITERAL("127.0.0.1"), .Port = Replica, .Reason = XREDIS_CLUSTER_REPLICA_READ};
    xredispool* ReadPool = NULL;
    require(xredisClusterNodesGetPool(&ReadNode, 5000, Nodes, &ReadPool) == XREDIS_CLUSTER_POOL_READY, "prewarm managed read pool");
    waitPool(ReadPool, 1);
    xredisclusterclientconfig Config;
    xredisClusterClientConfigInit(&Config);
    Config.Refresh.Format = Format;
    xredisclusterclient* Client = NULL;
    require(xredisClusterClientCreate(Engine, 0, Nodes, &Seed, 1, &Config, &Client) == XREDIS_ADMISSION_ACCEPTED, "managed Cluster startup");
    uint16 Slot;
    require(xredisClusterKeySlot(Get[1], &Slot), "local binary key slot");
    checkManagedRead(Client, Nodes, Slot, false, Primary, 0);
    checkManagedRead(Client, Nodes, Slot, true, Replica, XREDIS_CLUSTER_REPLICA_READ);
    xrtAtomic32Store(&Fixture.DenyReplica, 1, XMEMORY_RELEASE);
    checkManagedRead(Client, Nodes, Slot, true, Primary, 0);
    closeManaged(Client, Nodes);
    xredisPoolRelease(ReadPool);
}

static xredisclusterclientinfo clientInfo(xredisclusterclient* Client)
{
    xredisclusterclientinfo Info = {.Size = sizeof(Info)};
    require(xredisClusterClientInfo(Client, &Info), "promotion client info");
    return Info;
}

static void checkDisconnectedPool(xredispool* Pool)
{
    double Deadline = xrtTimer() + 5;
    for (;;) {
        xredispoolinfo Info = {.Size = sizeof(Info)};
        xredispoolconnectinfo Connect = {.Size = sizeof(Connect)};
        require(xredisPoolInfo(Pool, &Info) && xredisPoolConnectInfo(Pool, &Connect) &&
            xrtTimer() < Deadline, "old primary pool observed disconnect");
        if (!Info.Idle && Connect.Failures) return;
        xrtSleep(2);
    }
}

static void waitSignal(cstr Path)
{
    double Deadline = xrtTimer() + 40;
    for (;;) {
        FILE* Signal = fopen(Path, "rb");
        if (Signal) { fclose(Signal); return; }
        require(xrtTimer() < Deadline, "primary promotion signal");
        xrtSleep(5);
    }
}

static int checkPromotion(int argc, char** argv)
{
    if (argc != 8 || (strcmp(argv[5], "inflight") && strcmp(argv[5], "unsent") && strcmp(argv[5], "election")))
        return 2;
    long Primary = strtol(argv[2], NULL, 10), Replica = strtol(argv[3], NULL, 10), Witness = strtol(argv[4], NULL, 10);
    if (Primary <= 0 || Replica <= 0 || Witness <= 0 || Primary > 65535 || Replica > 65535 || Witness > 65535 ||
        Primary == Replica || Primary == Witness || Replica == Witness) return 2;
    bool Inflight = !strcmp(argv[5], "inflight");
    bool Election = !strcmp(argv[5], "election");
    bool Shards = Inflight || Election;
    char Counter[96];
    int Size = snprintf(Counter, sizeof(Counter), "xrt-owned:failover:{fixed}:%s", argv[6]);
    if (Size <= 0 || (size_t)Size >= sizeof(Counter)) return 2;
    xnetengine* Engine = xrtNetEngineCreate(NULL);
    require(Engine && xrtNetEngineStart(Engine), "promotion engine start");
    clusterfixture Fixture = {
        .Primary = (uint16)Primary, .Replica = (uint16)Replica, .Witness = (uint16)Witness,
        .Mode = Shards ? XREDIS_INIT_RESP3 : XREDIS_INIT_RESP2,
    };
    xrtAtomic32Init(&Fixture.DenyReplica, 0);
    xredisclusternodesconfig NC;
    xredisClusterNodesConfigInit(&NC);
    NC.Protocol = Shards ? XREDIS_RESP3 : XREDIS_RESP2;
    NC.Authorize = clusterAuthorize;
    NC.Connect = clusterConnect;
    NC.Data = &Fixture;
    NC.AttemptTimeout = 500;
    NC.RetryMin = 20;
    NC.RetryMax = 100;
    xredisclusternodes* Nodes = NULL;
    require(xredisClusterNodesCreate(Engine, 0, &NC, &Nodes) == XREDIS_ADMISSION_ACCEPTED, "promotion node cache");
    xredisclusteraddress Seeds[] = {
        {.Host = XRT_BYTES_LITERAL("127.0.0.1"), .Port = Fixture.Primary},
        {.Host = XRT_BYTES_LITERAL("127.0.0.1"), .Port = Fixture.Witness},
    };
    xredisclusteraddress ReadNode = Seeds[0];
    ReadNode.Port = Fixture.Replica;
    ReadNode.Reason = XREDIS_CLUSTER_REPLICA_READ;
    xredispool *Old = NULL, *Read = NULL, *Other = NULL;
    require(xredisClusterNodesGetPool(&Seeds[0], 5000, Nodes, &Old) == XREDIS_CLUSTER_POOL_READY &&
        xredisClusterNodesGetPool(&ReadNode, 5000, Nodes, &Read) == XREDIS_CLUSTER_POOL_READY &&
        xredisClusterNodesGetPool(&Seeds[1], 5000, Nodes, &Other) == XREDIS_CLUSTER_POOL_READY, "promotion pool prewarm");
    waitPool(Old, 1);
    waitPool(Read, 1);
    waitPool(Other, 1);
    xredisPoolRelease(Other);
    xredisclusterclientconfig Config;
    xredisClusterClientConfigInit(&Config);
    Config.Refresh.Format = Shards ? XREDIS_CLUSTER_SHARDS : XREDIS_CLUSTER_LEGACY_SLOTS;
    Config.RefreshMin = Election ? 100 : 10;
    Config.RefreshRetryMax = Election ? 300 : 100;
    xredisclusterclient* Client = NULL;
    require(xredisClusterClientCreate(Engine, 0, Nodes, Seeds, 2, &Config, &Client) == XREDIS_ADMISSION_ACCEPTED,
        "promotion client start");
    uint16 Slot, CounterSlot;
    const xbytesview Increment[] = {XRT_BYTES_LITERAL("INCR"), {(cbytes)Counter, (size_t)Size}};
    require(xredisClusterKeySlot(Get[1], &Slot) && xredisClusterKeySlot(Increment[1], &CounterSlot) && Slot == CounterSlot,
        "promotion slot");
    checkManagedRead(Client, Nodes, Slot, false, Fixture.Primary, 0);
    checkManagedRead(Client, Nodes, Slot, true, Fixture.Replica, XREDIS_CLUSTER_REPLICA_READ);
    xredisclusterclientinfo Before = clientInfo(Client);
    xfuture* Blocked = NULL;
    if (Inflight) {
        const xbytesview Block[] = {
            XRT_BYTES_LITERAL("BLPOP"), XRT_BYTES_LITERAL("xrt-owned:failover:{fixed}\0blocked"), XRT_BYTES_LITERAL("0"),
        };
        require(xredisClusterClientCall(Client, Slot, Block, 3, 30000, &Blocked) == XREDIS_ADMISSION_ACCEPTED,
            "in-flight blocking call admitted");
    }
    printf("READY %s %s\n", argv[5], argv[6]);
    fflush(stdout);
    if (Blocked) {
        require(xrtFutureWaitFor(Blocked, 30000) == XWAIT_OK && xrtFutureValue(Blocked), "in-flight disconnect resolves");
        const xredisclusterclientresult* Failure = xrtFutureValue(Blocked);
        require(Failure->Status == XREDIS_CLUSTER_CLIENT_COMPLETE && Failure->RouteObserved &&
            Failure->Route.Status == XREDIS_CLUSTER_ROUTE_COMPLETE && Failure->Route.Attempts == 1 && Failure->Route.Moved == 0 &&
            Failure->Route.Query.Status == XREDIS_REQUEST_DISCONNECTED && Failure->Route.Query.Delivery == XREDIS_MAY_HAVE_EXECUTED,
            "accepted disconnect keeps uncertainty and is never replayed");
        xrtFutureDestroy(Blocked);
        double Deadline = xrtTimer() + 5;
        while (clientInfo(Client).Refreshes == Before.Refreshes) {
            require(xrtTimer() < Deadline, "accepted disconnect schedules control-plane refresh");
            xrtSleep(2);
        }
        puts("DISCONNECTED MAY_HAVE_EXECUTED attempts=1");
    }
    waitSignal(argv[7]);
    checkDisconnectedPool(Old);
    if (!Inflight) {
        xredisclusterclientinfo Unsent = clientInfo(Client);
        require(Unsent.Generation == Before.Generation && Unsent.Refreshes == Before.Refreshes && !Unsent.Moved,
            "unsent call still uses the original snapshot");
    }
    double Entered = xrtTimer();
    xfuture* Write = NULL;
    require(xredisClusterClientCall(Client, Slot, Increment, 2, Election ? 10000 : 4000, &Write) ==
        XREDIS_ADMISSION_ACCEPTED, "recovery INCR");
    if (Election) {
        printf("OFFERED %s\n", argv[6]);
        fflush(stdout);
    }
    require(xrtFutureWaitFor(Write, 11000) == XWAIT_OK && xrtFutureValue(Write), "typed recovery result");
    const xredisclusterclientresult* Result = xrtFutureValue(Write);
    xredisclusterclientinfo After = clientInfo(Client);
    bool Recovered = Result->Status == XREDIS_CLUSTER_CLIENT_COMPLETE && Result->RouteObserved &&
        Result->Route.Status == XREDIS_CLUSTER_ROUTE_COMPLETE && Result->Generation > Before.Generation &&
        Result->Route.Attempts == 1 && Result->Route.Moved == 0 && Result->Route.AttemptAddress.Port == Fixture.Replica &&
        !Result->Route.AttemptAddress.Reason && !After.Moved &&
        (!Election || After.Refreshes > Before.Refreshes + 1u);
    if (!Recovered)
        fprintf(stderr, "recovery result status=%d route=%d query=%d delivery=%d attempts=%zu port=%u candidate=%u provider=%d admission=%d generation=%llu current=%llu refreshes=%llu refresh_status=%d moved=%llu\n",
            (int)Result->Status, (int)Result->Route.Status, (int)Result->Route.Query.Status, (int)Result->Route.Query.Delivery,
            Result->Route.Attempts, (unsigned)Result->Route.AttemptAddress.Port, (unsigned)Result->Route.Address.Port,
            (int)Result->Route.Provider, (int)Result->Route.Admission, (unsigned long long)Result->Generation,
            (unsigned long long)After.Generation, (unsigned long long)After.Refreshes, (int)After.LastRefresh,
            (unsigned long long)After.Moved);
    require(Recovered, "one write on the promoted primary without MOVED");
    xredisreplyvalue Count = root(Result->Native, XREDIS_REQUEST_OK);
    require(Count.Type == XREDIS_REPLY_INTEGER && Count.Integer == 1, "post-promotion counter increments once");
    xrtFutureDestroy(Write);
    printf("recovery_call_ms=%.3f\n", (xrtTimer() - Entered) * 1000.);
    if (Election)
        printf("election_refreshes=%llu\n", (unsigned long long)(After.Refreshes - Before.Refreshes));
    xredisclusteraddress Promoted = Seeds[0];
    Promoted.Port = Fixture.Replica;
    xredispool* New = NULL;
    require(xredisClusterNodesGetPool(&Promoted, 5000, Nodes, &New) == XREDIS_CLUSTER_POOL_READY && New != Read,
        "promoted primary receives a distinct ordinary pool");
    waitPool(New, 1);
    xredisPoolRelease(New);
    closeManaged(Client, Nodes);
    xredisPoolRelease(Old);
    xredisPoolRelease(Read);
    require(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "promotion engine physical close");
    printf("[PASS] real Cluster primary loss: %s %s\n", argv[5], argv[6]);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc > 1 && !strcmp(argv[1], "promotion")) return checkPromotion(argc, argv);
    if (argc != 5 || strlen(argv[2]) != 40) return 2;
    long Port = strtol(argv[1], NULL, 10);
    long Primary = strtol(argv[4], NULL, 10);
    if (Port <= 0 || Port > 65535 || Primary <= 0 || Primary > 65535 || Primary == Port) return 2;
    xnetaddr Address;
    require(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, (uint16)Port), "loopback address");
    xnetengine* Engine = xrtNetEngineCreate(NULL);
    require(Engine && xrtNetEngineStart(Engine), "engine start");
    checkDirect(Engine, &Address, XREDIS_INIT_RESP2, false, argv[2], argv[3]);
    checkDirect(Engine, &Address, XREDIS_INIT_RESP2, true, argv[2], argv[3]);
    checkDirect(Engine, &Address, XREDIS_INIT_RESP3, false, argv[2], argv[3]);
    checkDirect(Engine, &Address, XREDIS_INIT_RESP3, true, argv[2], argv[3]);
    checkPool(Engine, &Address);
    checkManaged(Engine, (uint16)Primary, (uint16)Port, XREDIS_INIT_RESP2, XREDIS_CLUSTER_SHARDS);
    checkManaged(Engine, (uint16)Primary, (uint16)Port, XREDIS_INIT_RESP3, XREDIS_CLUSTER_SHARDS);
    checkManaged(Engine, (uint16)Primary, (uint16)Port, XREDIS_INIT_RESP2, XREDIS_CLUSTER_LEGACY_SLOTS);
    checkManaged(Engine, (uint16)Primary, (uint16)Port, XREDIS_INIT_RESP3, XREDIS_CLUSTER_LEGACY_SLOTS);
    require(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "engine physical teardown");
    puts("[PASS] real Cluster READONLY: RESP2/RESP3 MOVED controls, binary reads and shared pool");
    puts("[PASS] real Cluster replica routing: SHARDS/SLOTS, RESP2/RESP3, selection, ordinary and denied-replica fallback");
    return 0;
}
