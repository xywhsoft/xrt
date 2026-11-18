#define XREDIS_MODULE_XREDIS_ROLE_CLIENT
#define XREDIS_MODULE_XREDIS_POOL_SHARED_CONNECT
#include <xredis.h>
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

static void checkRead(xfuture* Future)
{
    xredisreplyvalue Value = root(Future, XREDIS_REQUEST_OK);
    require(Value.Type == XREDIS_REPLY_BULK_STRING && Value.Data.Size == Expected.Size &&
        memcmp(Value.Data.Data, Expected.Data, Expected.Size) == 0, "exact binary replica value");
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

static void checkPool(xnetengine* Engine, xnetaddr* Address)
{
    xredispoolconnectconfig Connect;
    xredisPoolConnectConfigInit(&Connect);
    Connect.Connect = poolConnect;
    Connect.Data = Address;
    Connect.Protocol = XREDIS_RESP2;
    xredispool* Pool = NULL;
    require(xredisSharedPoolConnect(Engine, 0, 2, NULL, &Connect, &Pool) == XREDIS_ADMISSION_ACCEPTED, "maintained shared pool");
    xredispoolinfo Info = {.Size = sizeof(Info)};
    double Deadline = xrtTimer() + 5;
    for (;;) {
        require(xredisPoolInfo(Pool, &Info) && xrtTimer() < Deadline, "pool ready members");
        if (Info.Idle == 2) break;
        xrtSleep(1);
    }
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

int main(int argc, char** argv)
{
    if (argc != 4 || strlen(argv[2]) != 40) return 2;
    long Port = strtol(argv[1], NULL, 10);
    if (Port <= 0 || Port > 65535) return 2;
    xnetaddr Address;
    require(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, (uint16)Port), "loopback address");
    xnetengine* Engine = xrtNetEngineCreate(NULL);
    require(Engine && xrtNetEngineStart(Engine), "engine start");
    checkDirect(Engine, &Address, XREDIS_INIT_RESP2, false, argv[2], argv[3]);
    checkDirect(Engine, &Address, XREDIS_INIT_RESP2, true, argv[2], argv[3]);
    checkDirect(Engine, &Address, XREDIS_INIT_RESP3, false, argv[2], argv[3]);
    checkDirect(Engine, &Address, XREDIS_INIT_RESP3, true, argv[2], argv[3]);
    checkPool(Engine, &Address);
    require(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "engine physical teardown");
    puts("[PASS] real Cluster READONLY: RESP2/RESP3 MOVED controls, binary reads and shared pool");
    return 0;
}
