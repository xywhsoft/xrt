#include "../examples/cluster_command_cache/app_catalog.h"
#include "support/cluster_command_peer.h"

typedef enum cachereply { CACHE_NORMAL, CACHE_REPLACE, CACHE_MALFORMED, CACHE_DENIED } cachereply;
typedef struct cachepeer {
    rfnode* Node;
    xatomic32 Mode, Queries, Installed;
} cachepeer;

static bool cacheCommand(redispeer* Peer, xredisreply* Command, ptr Data)
{
    cachepeer* State = Data;
    xredisreplyvalue Root = {0}, Word = {0};
    redisPeerRequire(xredisReplyGet(Command, 0, &Root) &&
        xredisReplyGet(Command, Root.FirstChild, &Word), "command cache peer frame");
    if (!redisPeerName(Word, "COMMAND")) return qpCommand(Peer, Command, State->Node);
    redisPeerRequire(Root.ChildCount == 1, "APP asks for the full catalogue");
    static const char Replacement[] = "*1\r\n*6\r\n$3\r\nget\r\n:2\r\n*1\r\n$8\r\nreadonly\r\n:1\r\n:1\r\n:1\r\n";
    static const char Malformed[] = "*1\r\n:42\r\n";
    static const char Denied[] = "-NOPERM command metadata\r\n";
    const xstrview Replies[] = {
        XRT_STR_LITERAL(CK_METADATA3), XRT_STR_LITERAL(Replacement),
        XRT_STR_LITERAL(Malformed), XRT_STR_LITERAL(Denied),
    };
    xstrview Reply = Replies[xrtAtomic32Load(&State->Mode, XMEMORY_ACQUIRE)];
    xrtAtomic32FetchAdd(&State->Queries, 1, XMEMORY_RELEASE);
    redisPeerSend(Peer, Reply.Data, Reply.Size);
    return true;
}

static void cacheInstall(xnetworker* Worker, ptr Data)
{
    (void)Worker;
    cachepeer* State = Data;
    State->Node->Owner->Base->Peers[State->Node->Index].Hook = (redispeerhook){cacheCommand, State};
    xrtAtomic32Store(&State->Installed, 1, XMEMORY_RELEASE);
}

static size_t cacheCount(const xrediscommandcatalog* Catalogue)
{
    xrediscommandcataloginfo Info = {.Size = sizeof(Info)};
    redisPeerRequire(xredisCommandCatalogInfo(Catalogue, &Info), "read immutable catalogue");
    return Info.Commands;
}

int main(void)
{
    rffixture* Fixture = qpStart();
    cachepeer Peer = {.Node = &Fixture->Nodes[0]};
    xrtAtomic32Init(&Peer.Mode, CACHE_NORMAL);
    xrtAtomic32Init(&Peer.Queries, 0);
    xrtAtomic32Init(&Peer.Installed, 0);
    xnetengine* Engine = Fixture->Base->Peers[0].Engine;
    redisPeerRequire(xrtNetEnginePost(Engine, 1, cacheInstall, &Peer), "install bounded metadata fixture");
    redisPeerWait(&Peer.Installed, 1);

    xredisclusterclient* Client = cfCreate(Fixture, NULL);
    xrediscommandcatalog* Catalogue = NULL;
    redisPeerRequire(appCatalogRefresh(Client, 1, 5000, NULL, &Catalogue), "first APP snapshot");
    xrediscommandcatalog* Old = Catalogue;
    redisPeerRequire(xredisCommandCatalogRetain(Old) && cacheCount(Old) > 1, "hold immutable old snapshot");
    for (unsigned Mode = CACHE_MALFORMED; Mode <= CACHE_DENIED; ++Mode) {
        xrtAtomic32Store(&Peer.Mode, Mode, XMEMORY_RELEASE);
        redisPeerRequire(!appCatalogRefresh(Client, 1, 5000, NULL, &Catalogue) && Catalogue == Old,
            "invalid or denied refresh leaves prior snapshot available");
    }
    xrtAtomic32Store(&Peer.Mode, CACHE_NORMAL, XMEMORY_RELEASE);
    xrediscommandkeylimits Small;
    xredisCommandKeyLimitsInit(&Small);
    Small.MaxCommands = 0;
    redisPeerRequire(!appCatalogRefresh(Client, 1, 5000, &Small, &Catalogue) && Catalogue == Old,
        "catalogue limit leaves prior snapshot available");

#if defined(XREDIS_TEST_APP_CATALOG_PARSE)
    xallocationbudget* Budget = NULL;
    redisPeerRequire(xrtAllocationBudgetCreate(1, &Budget), "fresh APP catalogue budget");
    AppCatalogBudget = Budget;
    redisPeerRequire(!appCatalogRefresh(Client, 1, 5000, NULL, &Catalogue) && Catalogue == Old &&
        !AppCatalogBudget, "parse allocation failure preserves the previous snapshot");
    xallocationbudgetinfo Stats = {.Size = sizeof(Stats)};
    redisPeerRequire(xrtAllocationBudgetInfo(Budget, &Stats) && Stats.Rejections && !Stats.LiveBytes,
        "failed candidate released all charged bytes");
    xrtAllocationBudgetFree(Budget);
    xrtClearError();
#endif

    xrtAtomic32Store(&Peer.Mode, CACHE_REPLACE, XMEMORY_RELEASE);
    redisPeerRequire(appCatalogRefresh(Client, 1, 5000, NULL, &Catalogue) &&
        Catalogue != Old && cacheCount(Catalogue) == 1, "healthy replacement publishes atomically on APP owner");
    uint32 Calls = xrtAtomic32Load(&Peer.Queries, XMEMORY_ACQUIRE);
    const xbytesview Args[] = {XRT_BYTES_LITERAL("GET"), XRT_BYTES_LITERAL("owned\0{22179}")};
    xrediscommandkeyresult Keys = {.Size = sizeof(Keys)};
    xfuture* Business = NULL;
    redisPeerRequire(xredisCommandCatalogEvaluate(Catalogue, Args, 2, NULL, 0, &Keys) == XREDIS_KEYS_OK &&
        xredisClusterClientCall(Client, Keys.Slot, Args, 2, 5000, &Business) == XREDIS_ADMISSION_ACCEPTED,
        "APP snapshot gives an explicit slot for a binary key");
    qpValue(Business);
    redisPeerRequire(xrtAtomic32Load(&Peer.Queries, XMEMORY_ACQUIRE) == Calls, "warm GET sends no COMMAND");
    cfStop(Fixture, Client);
    qpValue(Business);
    xrtFutureDestroy(Business);
    redisPeerRequire(cacheCount(Old) > 1 && cacheCount(Catalogue) == 1, "APP snapshots outlive the client");
    xredisCommandCatalogRelease(Old);
    xredisCommandCatalogRelease(Catalogue);
    puts("[PASS] APP-owned catalogue replacement, rollback, explicit slot and independent lifetime");
    return 0;
}
