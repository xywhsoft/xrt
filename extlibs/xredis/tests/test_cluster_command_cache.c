#include <xredis/cluster_command_cache.h>
#include "support/cluster_command_peer.h"

typedef enum cache_reply {CACHE_NORMAL,CACHE_REPLACE,CACHE_MALFORMED,CACHE_DENIED,CACHE_HOLD} cache_reply;
typedef struct cache_peer {
    rfnode* Node;
    xatomic32 Mode,Queries,Installed;
    xatomicptr HeldStream;
} cache_peer;

static bool cachePeerCommand(redispeer* Peer,xredisreply* Command,ptr Data)
{
    cache_peer* State=Data;
    xredisreplyvalue Root={0},Word={0};
    redisPeerRequire(xredisReplyGet(Command,0,&Root)&&xredisReplyGet(Command,Root.FirstChild,&Word),"command cache peer frame");
    if(!redisPeerName(Word,"COMMAND"))return qpCommand(Peer,Command,State->Node);
    redisPeerRequire(Root.ChildCount==1,"only fixed COMMAND requests fetch the catalog");
    static const char Replacement[]="*1\r\n*6\r\n$3\r\nget\r\n:2\r\n*1\r\n$8\r\nreadonly\r\n:1\r\n:1\r\n:1\r\n";
    static const char Malformed[]="*1\r\n:42\r\n";
    static const char Denied[]="-NOPERM command metadata\r\n";
    switch(xrtAtomic32Load(&State->Mode,XMEMORY_ACQUIRE)){
    case CACHE_NORMAL:redisPeerSend(Peer,CK_METADATA3,sizeof(CK_METADATA3)-1);break;
    case CACHE_REPLACE:redisPeerSend(Peer,Replacement,sizeof(Replacement)-1);break;
    case CACHE_MALFORMED:redisPeerSend(Peer,Malformed,sizeof(Malformed)-1);break;
    case CACHE_DENIED:redisPeerSend(Peer,Denied,sizeof(Denied)-1);break;
    case CACHE_HOLD:
        redisPeerRequire(xrtNetStreamRef(Peer->Stream)!=NULL,"retain pending metadata stream");
        redisPeerRequire(!xrtAtomicPtrExchange(&State->HeldStream,Peer->Stream,XMEMORY_ACQ_REL),"only one refresh sends an outstanding COMMAND");
        break;
    default:redisPeerRequire(false,"unknown cache fixture mode");break;
    }
    xrtAtomic32FetchAdd(&State->Queries,1,XMEMORY_RELEASE);
    return true;
}

static void cachePeerInstall(xnetworker* Worker,ptr Data)
{
    (void)Worker;
    cache_peer* State=Data;
    State->Node->Owner->Base->Peers[State->Node->Index].Hook=(redispeerhook){cachePeerCommand,State};
    xrtAtomic32Store(&State->Installed,1,XMEMORY_RELEASE);
}

static void cachePeerUnhold(cache_peer* State)
{
    xnetstream* Stream=xrtAtomicPtrExchange(&State->HeldStream,NULL,XMEMORY_ACQ_REL);
    redisPeerRequire(Stream!=NULL,"refresh actually sent before release");
    redisPeerRequire(xrtNetStreamSend(Stream,CK_METADATA3,sizeof(CK_METADATA3)-1)==XNET_RESULT_OK,
        "release held metadata response");
    xrtNetStreamDestroy(Stream);
}

static void cacheRefresh(xredisclusterclient* Client,cache_peer* Peer,cache_reply Mode,
    const xrediscommandkeylimits* Limits,xredisclustercatalogstatus Expected)
{
    xrtAtomic32Store(&Peer->Mode,Mode,XMEMORY_RELEASE);
    xredisclusterwait Native={0};
    xredisclustercatalogstatus Status=xredisClusterClientRefreshCommandCatalog(Client,1,Limits,5000,&Native);
    if(Status!=Expected)fprintf(stderr,"catalog refresh status=%d expected=%d\n",(int)Status,(int)Expected);
    redisPeerRequire(Status==Expected&&Native.Future&&Native.Wait==XWAIT_OK,"refresh produced native outcome");
    const xredisclusterclientresult* Result=cfWait(Native.Future);
    redisPeerRequire(Result->Status==XREDIS_CLUSTER_CLIENT_COMPLETE&&Result->RouteObserved&&Result->Route.RequestObserved,
        "refresh retains the actual authorized primary route");
    if(Mode==CACHE_DENIED)redisPeerRequire(Result->Route.Query.Status==XREDIS_REQUEST_SERVER_ERROR,
        "ACL denial retains the server error");
    xrtFutureDestroy(Native.Future);
}

static size_t cacheCommands(const xrediscommandcatalog* Catalog)
{
    xrediscommandcataloginfo Info={.Size=sizeof(Info)};
    redisPeerRequire(xredisCommandCatalogInfo(Catalog,&Info),"read retained command catalog");
    return Info.Commands;
}

static xfuture* cacheGet(xredisclusterclient* Client,xredisclustercommandadmission Expected)
{
    const xbytesview Args[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("owned\0{22179}")};
    xfuture* Canary=(xfuture*)(uintptr_t)1;
    xfuture* Future=Canary;
    redisPeerRequire(xredisClusterClientCallCachedCommand(Client,Args,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,5000,&Future)==Expected,
        "cached command typed admission");
    if(Expected==XREDIS_CLUSTER_COMMAND_ACCEPTED)return Future;
    redisPeerRequire(Future==Canary,"refused cached command preserves output");
    return NULL;
}

typedef struct cache_attempt {
    xredisclusterclient* Client;
    xredisclustercatalogstatus Status;
    xredisclusterwait Native;
    int64 Timeout;
    xatomic32 Done;
} cache_attempt;
static int32 cacheAttempt(ptr Data)
{
    cache_attempt* Work=Data;
    Work->Status=xredisClusterClientRefreshCommandCatalog(Work->Client,1,NULL,Work->Timeout,&Work->Native);
    xrtAtomic32Store(&Work->Done,1,XMEMORY_RELEASE);
    return 0;
}
static void cacheWorkerRefusal(xnetworker* Worker,ptr Data)
{
    (void)Worker;
    (void)cacheAttempt(Data);
}
static xthread* cacheStartAttempt(cache_attempt* Work,cache_peer* Peer)
{
    uint32 Queries=xrtAtomic32Load(&Peer->Queries,XMEMORY_ACQUIRE);
    xrtAtomic32Store(&Peer->Mode,CACHE_HOLD,XMEMORY_RELEASE);
    xthread* Thread=xrtThreadCreate(cacheAttempt,Work,0);
    redisPeerRequire(Thread!=NULL,"refresh caller thread");
    redisPeerWait(&Peer->Queries,Queries+1);
    return Thread;
}
static void cacheJoin(xthread* Thread,cache_attempt* Work,xredisclustercatalogstatus Status)
{
    redisPeerRequire(xrtThreadWaitFor(Thread,10000)==XWAIT_OK,"refresh caller retires");
    xrtThreadDestroy(Thread);
    redisPeerRequire(Work->Status==Status&&Work->Native.Future&&Work->Native.Wait==XWAIT_OK,
        "refresh keeps its native result across supersede or close");
    xrtFutureDestroy(Work->Native.Future);
}

int main(void)
{
    rffixture* Fixture=qpStart();
    cache_peer Peer={.Node=&Fixture->Nodes[0]};
    xrtAtomic32Init(&Peer.Mode,CACHE_NORMAL);
    xrtAtomic32Init(&Peer.Queries,0);
    xrtAtomic32Init(&Peer.Installed,0);
    xrtAtomicPtrInit(&Peer.HeldStream,NULL);
    xnetengine* Engine=Fixture->Base->Peers[0].Engine;
    redisPeerRequire(xrtNetEnginePost(Engine,1,cachePeerInstall,&Peer),"install metadata fault fixture");
    redisPeerWait(&Peer.Installed,1);
    xredisclusterclient* Client=cfCreate(Fixture,NULL);
    xrediscommandcatalog* Canary=(xrediscommandcatalog*)(uintptr_t)1;
    xrediscommandcatalog* Snapshot=Canary;
    redisPeerRequire(xredisClusterClientCommandCatalog(Client,&Snapshot)==XREDIS_CLUSTER_CATALOG_ABSENT&&Snapshot==Canary,
        "cold cache rejects access");
    cacheGet(Client,XREDIS_CLUSTER_COMMAND_CATALOG_UNAVAILABLE);

    cache_attempt Worker={.Client=Client,.Timeout=5000};
    xrtAtomic32Init(&Worker.Done,0);
    redisPeerRequire(xrtNetEnginePost(Engine,1,cacheWorkerRefusal,&Worker),"invoke refresh on progress worker");
    redisPeerWait(&Worker.Done,1);
    redisPeerRequire(Worker.Status==XREDIS_CLUSTER_CATALOG_ERROR&&!Worker.Native.Future&&
        !xrtAtomic32Load(&Peer.Queries,XMEMORY_ACQUIRE),"blocking refresh fails before worker admission");

    cacheRefresh(Client,&Peer,CACHE_NORMAL,NULL,XREDIS_CLUSTER_CATALOG_READY);
    xrediscommandcatalog* Old=NULL;
    redisPeerRequire(xredisClusterClientCommandCatalog(Client,&Old)==XREDIS_CLUSTER_CATALOG_READY&&cacheCommands(Old)>1,
        "publish immutable catalog from the actual wire");
    uint32 Queries=xrtAtomic32Load(&Peer.Queries,XMEMORY_ACQUIRE);
    xfuture* Get=cacheGet(Client,XREDIS_CLUSTER_COMMAND_ACCEPTED);
    qpValue(Get);
    redisPeerRequire(xrtAtomic32Load(&Peer.Queries,XMEMORY_ACQUIRE)==Queries,"warm business does not fetch metadata");

    cacheRefresh(Client,&Peer,CACHE_DENIED,NULL,XREDIS_CLUSTER_CATALOG_REQUEST_FAILED);
    cacheRefresh(Client,&Peer,CACHE_MALFORMED,NULL,XREDIS_CLUSTER_CATALOG_MALFORMED);
    xrediscommandkeylimits Limits;
    xredisCommandKeyLimitsInit(&Limits);
    Limits.MaxCommands=0;
    cacheRefresh(Client,&Peer,CACHE_NORMAL,&Limits,XREDIS_CLUSTER_CATALOG_LIMIT);
    xrediscommandkeylimits Invalid=Limits;
    Invalid.Size=0;
    xredisclusterwait Unchanged={.Future=(xfuture*)(uintptr_t)1};
    redisPeerRequire(xredisClusterClientRefreshCommandCatalog(Client,1,&Invalid,5000,&Unchanged)==XREDIS_CLUSTER_CATALOG_ERROR&&
        Unchanged.Future==(xfuture*)(uintptr_t)1,"invalid limits rejected before admission");
    xrtClearError();

#if defined(XREDIS_TEST_CATALOG_WAIT)
    xallocationbudget* Budget=NULL;
    redisPeerRequire(xrtAllocationBudgetCreate(1,&Budget),"fresh catalog allocation budget");
    CatalogBudget=Budget;
    cacheRefresh(Client,&Peer,CACHE_NORMAL,NULL,XREDIS_CLUSTER_CATALOG_MEMORY);
    redisPeerRequire(!CatalogBudget&&xrtAllocationBudgetLeave(&CatalogScope),"leave parse-only allocation budget");
    xallocationbudgetinfo BudgetInfo={.Size=sizeof(BudgetInfo)};
    redisPeerRequire(xrtAllocationBudgetInfo(Budget,&BudgetInfo)&&BudgetInfo.Rejections&&!BudgetInfo.LiveBytes,"catalog allocation fully rejected");
    xrtAllocationBudgetFree(Budget);
    xrtClearError();
    xredisclusterwait FailedWait={0};
    CatalogWaitError=true;
    redisPeerRequire(xredisClusterClientRefreshCommandCatalog(Client,1,NULL,5000,&FailedWait)==XREDIS_CLUSTER_CATALOG_WAIT_FAILED&&
        FailedWait.Wait==XWAIT_ERROR&&FailedWait.Future,"native wait error transfers the accepted Future");
    CatalogWaitError=false;
    redisPeerRequire(cfWait(FailedWait.Future)->Route.Query.Status==XREDIS_REQUEST_OK,"failed wait retains eventual native outcome");
    xrtFutureDestroy(FailedWait.Future);
#endif
    Snapshot=NULL;
    redisPeerRequire(xredisClusterClientCommandCatalog(Client,&Snapshot)==XREDIS_CLUSTER_CATALOG_READY&&Snapshot==Old,
        "all failures preserve the last usable catalog");
    xredisCommandCatalogRelease(Snapshot);

    cache_attempt Work={.Client=Client,.Timeout=5000};
    xthread* Thread=cacheStartAttempt(&Work,&Peer);
    Queries=xrtAtomic32Load(&Peer.Queries,XMEMORY_ACQUIRE);
    redisPeerRequire(xredisClusterClientRefreshCommandCatalog(Client,1,NULL,5000,&Unchanged)==XREDIS_CLUSTER_CATALOG_BUSY&&
        Unchanged.Future==(xfuture*)(uintptr_t)1&&xrtAtomic32Load(&Peer.Queries,XMEMORY_ACQUIRE)==Queries,
        "competing refresh does not send another COMMAND");
    redisPeerRequire(xredisClusterClientInvalidateCommandCatalog(Client),"invalidate while an actual reply is held");
    cacheGet(Client,XREDIS_CLUSTER_COMMAND_CATALOG_UNAVAILABLE);
    cachePeerUnhold(&Peer);
    cacheJoin(Thread,&Work,XREDIS_CLUSTER_CATALOG_STALE);
    Snapshot=Canary;
    redisPeerRequire(xredisClusterClientCommandCatalog(Client,&Snapshot)==XREDIS_CLUSTER_CATALOG_STALE&&Snapshot==Canary,
        "superseded metadata never restores availability");

    cacheRefresh(Client,&Peer,CACHE_REPLACE,NULL,XREDIS_CLUSTER_CATALOG_READY);
    xrediscommandcatalog* New=NULL;
    redisPeerRequire(xredisClusterClientCommandCatalog(Client,&New)==XREDIS_CLUSTER_CATALOG_READY&&
        cacheCommands(New)==1&&cacheCommands(Old)>1,"replacement has independent immutable lifetime");
    xfuture* NewGet=cacheGet(Client,XREDIS_CLUSTER_COMMAND_ACCEPTED);
    qpValue(NewGet);

    cache_attempt Expiring={.Client=Client,.Timeout=500};
    Thread=cacheStartAttempt(&Expiring,&Peer);
    cacheJoin(Thread,&Expiring,XREDIS_CLUSTER_CATALOG_TIMEOUT);
    cachePeerUnhold(&Peer);
    Snapshot=NULL;
    redisPeerRequire(xredisClusterClientCommandCatalog(Client,&Snapshot)==XREDIS_CLUSTER_CATALOG_READY&&Snapshot==New,
        "native deadline and late response preserve prior catalog");
    xredisCommandCatalogRelease(Snapshot);

    cache_attempt Closing={.Client=Client,.Timeout=5000};
    Thread=cacheStartAttempt(&Closing,&Peer);
    redisPeerRequire(xredisClusterClientClose(Client),"close during pending catalog refresh");
    cacheJoin(Thread,&Closing,XREDIS_CLUSTER_CATALOG_CLOSED);
    xnetstream* Held=xrtAtomicPtrExchange(&Peer.HeldStream,NULL,XMEMORY_ACQ_REL);
    redisPeerRequire(Held!=NULL,"release independently retained native peer");
    xrtNetStreamDestroy(Held);
    cacheGet(Client,XREDIS_CLUSTER_COMMAND_CLOSED);
    cfStop(Fixture,Client);
    qpValue(Get);
    qpValue(NewGet);
    redisPeerRequire(cacheCommands(Old)>1&&cacheCommands(New)==1,"retained snapshots outlive all native owners");
    xredisCommandCatalogRelease(Old);
    xredisCommandCatalogRelease(New);
    xrtFutureDestroy(Get);
    xrtFutureDestroy(NewGet);
    puts("[PASS] authorized COMMAND refresh, cache replacement, malformed/ACL/limit refuses, stale and close fences");
    return 0;
}
