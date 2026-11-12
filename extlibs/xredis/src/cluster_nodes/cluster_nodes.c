#include <xredis/cluster_nodes.h>
#include "../internal/cluster_nodes_internal.h"
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES)
typedef struct xredisclusternodepool xredisclusternodepool;
struct xredisclusternodepool {
    xredisclusternodes* Cache;xredisclusternodepool *Next,*FreeNext;
    xredisclusteraddress Address;uint64 Hash;
    xredispool* Pool;xfuture* Closed;xfuturewatch Watch;
    bool Opening,Retiring,ObservedClosed;
};
struct xredisclusternodes {
    xatomic32 References,Admission;xmutex Gate;xnetengine* Engine;uint64 Affinity;
    xredisclusternodesconfig Config;xredisclusternodesinfo Info;
    xpromise* Promise;xfuture* Closed;
    xredisclusternodepool **Buckets,*Slots,*Free;
    size_t BucketCount,StorageBytes;bool Finishing,Scanning;
};
static bool xredisNodesError(xerrkind K,cstr S)
{xrtSetErrorInfo(K,"xrt.redis",K==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,S);return false;}
static void xredisNodesRef(xredisclusternodes* C){xrtAtomic32FetchAdd(&C->References,1,XMEMORY_RELAXED);}
XREDIS_API bool xredisClusterNodesRetain(xredisclusternodes* C)
{if(!C)return xredisNodesError(XERR_ARGUMENT,"Cluster node cache is NULL");xredisNodesRef(C);return true;}
XREDIS_API void xredisClusterNodesRelease(xredisclusternodes* C)
{if(!C || xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)!=1)return;xrtPromiseDestroy(C->Promise);xrtFutureDestroy(C->Closed);xrtMutexUnit(&C->Gate);xrtFree(C);}
static bool xredisNodesOutput(xredisclusternodes* C,const void* P,size_t N)
{return C && xrtMemRangeValid(P,N) && !xrtMemRangesOverlap(C,C->StorageBytes,P,N);}
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT_SYNC)
bool __xredisClusterNodesCurrent(xredisclusternodes* C)
{
    xrtMutexLock(&C->Gate);bool Current=C->Engine && xrtNetEngineCurrent(C->Engine);
    xrtMutexUnlock(&C->Gate);return Current;
}
#endif
static bool xredisNodesAddress(const xredisclusteraddress* A,size_t Max)
{
    if(!xrtMemRangeValid(A,sizeof(*A)) || !A->Host.Size || A->Host.Size>Max || !xrtMemRangeValid(A->Host.Data,A->Host.Size) || !A->Port || A->Slot>=XREDIS_CLUSTER_SLOTS ||
        (A->Reason!=0 && A->Reason!=XREDIS_CLUSTER_MOVED && A->Reason!=XREDIS_CLUSTER_ASK) || (A->Host.Size==1 && A->Host.Data[0]=='?'))return false;
    for(size_t I=0;I<A->Host.Size;++I){unsigned X=A->Host.Data[I];if(X<=32 || X==127 || X=='/' || X=='\\' || X=='@' || X=='#')return false;}return true;
}
static uint64 xredisNodesHash(const xredisclusteraddress* A)
{uint64 H=14695981039346656037ull;for(size_t I=0;I<A->Host.Size;++I){H^=A->Host.Data[I];H*=1099511628211ull;}H^=A->Port;H*=1099511628211ull;H^=A->Tls;return H;}
static xredisclusternodepool* xredisNodesFind(xredisclusternodes* C,const xredisclusteraddress* A,uint64 H)
{for(xredisclusternodepool* N=C->Buckets[H&(C->BucketCount-1)];N;N=N->Next)if(N->Hash==H && N->Address.Port==A->Port && N->Address.Tls==A->Tls && N->Address.Host.Size==A->Host.Size && !memcmp(N->Address.Host.Data,A->Host.Data,A->Host.Size))return N;return NULL;}
#define XREDIS_NODES_SEALED 0x80000000u
#define XREDIS_NODES_USERS 0x7fffffffu
/* The seal bit and admitted caller count share one modification order. A
 * successful increment protects policy/data through End; Close atomically
 * prevents every later admission. Saturation/contended retries transfer no
 * lifetime and never call policy. MaxLookups<=65536 leaves the seal bit free. */
static xredisclusterpoolstatus xredisNodesBegin(xredisclusternodes* C)
{
    uint32 S=xrtAtomic32Load(&C->Admission,XMEMORY_ACQUIRE);
    for(unsigned I=0;I<32;++I){
        if(S&XREDIS_NODES_SEALED)return XREDIS_CLUSTER_POOL_CLOSED;
        if((S&XREDIS_NODES_USERS)>=C->Config.MaxLookups)break;
        if(xrtAtomic32CompareExchange(&C->Admission,&S,S+1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))return XREDIS_CLUSTER_POOL_READY;
    }
    xrtMutexLock(&C->Gate);xredisclusterpoolstatus Result=C->Info.State==XREDIS_CLUSTER_NODES_OPEN?XREDIS_CLUSTER_POOL_WAIT:XREDIS_CLUSTER_POOL_CLOSED;
    if(Result==XREDIS_CLUSTER_POOL_WAIT)++C->Info.Waits;
    xrtMutexUnlock(&C->Gate);return Result;
}
static void xredisNodesFinish(xredisclusternodes* C)
{
    xrtMutexLock(&C->Gate);
    if(C->Info.State!=XREDIS_CLUSTER_NODES_CLOSING || C->Info.Nodes || (xrtAtomic32Load(&C->Admission,XMEMORY_ACQUIRE)&XREDIS_NODES_USERS) || C->Scanning || C->Finishing){xrtMutexUnlock(&C->Gate);return;}
    C->Finishing=true;xnetengine* E=C->Engine;C->Engine=NULL;xredispoolconnectdrop Drop=C->Config.Drop;ptr Data=C->Config.Data;
    C->Config.Drop=NULL;C->Config.Data=NULL;C->Config.Connect=NULL;C->Config.Authorize=NULL;xpromise* P=C->Promise;C->Promise=NULL;xrtMutexUnlock(&C->Gate);
    if(Drop)Drop(Data);
    xrtNetEngineUnpin(E);
    xrtMutexLock(&C->Gate);C->Info.State=XREDIS_CLUSTER_NODES_CLOSED;xrtMutexUnlock(&C->Gate);
    xrtPromiseResolve(P,NULL);xrtPromiseDestroy(P);xredisClusterNodesRelease(C); /* active lifetime */
}
static void xredisNodesEnd(xredisclusternodes* C)
{
    uint32 Previous=xrtAtomic32FetchSub(&C->Admission,1,XMEMORY_ACQ_REL);
    if(Previous==(XREDIS_NODES_SEALED|1u))xredisNodesFinish(C);
}
static void xredisNodesEndLocked(xredisclusternodes* C)
{
    uint32 Previous=xrtAtomic32FetchSub(&C->Admission,1,XMEMORY_ACQ_REL);
    xrtMutexUnlock(&C->Gate);
    if(Previous==(XREDIS_NODES_SEALED|1u))xredisNodesFinish(C);
}
/* Gate held; caller releases the detached pool/Future outside it. The Watch
 * release right is already retired (or was never transferred on READY). */
static void xredisNodesUnlink(xredisclusternodepool* N,xredispool** P,xfuture** F)
{
    xredisclusternodes* C=N->Cache;xredisclusternodepool** Link=&C->Buckets[N->Hash&(C->BucketCount-1)];while(*Link!=N)Link=&(*Link)->Next;*Link=N->Next;
    *P=N->Pool;*F=N->Closed;N->Pool=NULL;N->Closed=NULL;N->Next=NULL;N->FreeNext=C->Free;C->Free=N;--C->Info.Nodes;if(*P)++C->Info.Retired;
}
static void xredisNodesNotify(ptr Data){(void)Data;}
static void xredisNodesReleased(ptr Data)
{
    xredisclusternodepool* N=Data;xredisclusternodes* C=N->Cache;xredispool* P=NULL;xfuture* F=NULL;
    xrtMutexLock(&C->Gate);N->ObservedClosed=true;if(!N->Opening)xredisNodesUnlink(N,&P,&F);xrtMutexUnlock(&C->Gate);
    xrtFutureDestroy(F);xredisPoolRelease(P);xredisNodesFinish(C);xredisClusterNodesRelease(C); /* watch lifetime; no Node access after unlock */
}
static bool xredisNodesConnect(xnetengine* E,size_t Slot,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** Client)
{xredisclusternodepool* N=Data;xredisclusternodes* C=N->Cache;return C->Config.Connect(&N->Address,E,Slot,Attempt,Timeout,C->Config.Data,Client);}
XREDIS_API bool xredisClusterNodesConfigInit(xredisclusternodesconfig* C)
{
    if(!xrtMemRangeValid(C,sizeof(*C)))return xredisNodesError(XERR_ARGUMENT,"invalid Cluster nodes config output");
    *C=(xredisclusternodesconfig){0};C->Size=sizeof(*C);C->MaxNodes=64;C->MaxHostBytes=1024;C->MaxStorageBytes=1024u*1024u;C->ConnectionsPerNode=1;C->MaxConnections=64;C->MaxLookups=1024;
    C->Protocol=XREDIS_RESP3;C->MaxConnecting=4;C->AttemptTimeout=5000;C->RetryMin=100;C->RetryMax=10000;return xredisPoolConfigInit(&C->Pool);
}
static bool xredisNodesConfig(const xredisclusternodesconfig* C)
{
    const xredispoolconfig* P=&C->Pool;
    return C->Size==sizeof(*C) && C->Authorize && C->Connect && C->MaxNodes && C->MaxNodes<=65536 && C->MaxHostBytes && C->MaxHostBytes<=65536 &&
        C->MaxStorageBytes && C->MaxStorageBytes<=128u*1024u*1024u && C->MaxLookups && C->MaxLookups<=65536 && C->ConnectionsPerNode && C->MaxConnections && C->MaxConnections<=65536 &&
        C->ConnectionsPerNode<=C->MaxConnections/C->MaxNodes && (C->Protocol==XREDIS_RESP2 || C->Protocol==XREDIS_RESP3) && C->MaxConnecting && C->MaxConnecting<=65536 &&
        C->AttemptTimeout>0 && C->AttemptTimeout<=60000 && C->RetryMin>0 && C->RetryMin<=C->RetryMax && C->RetryMax<=60000 &&
        P->Size==sizeof(*P) && P->MaxPending && P->MaxPending<=65536 && P->MaxRequests && P->MaxRequests<=UINT64_MAX/C->MaxNodes && P->MaxCommandBytes && P->MaxCommandBytes<=UINT64_MAX/C->MaxNodes &&
        P->LeaseTimeout>0 && P->LeaseTimeout<=86400000 && P->ReturnTimeout>0 && P->ReturnTimeout<=60000 && P->WorkItems && P->WorkItems<=65536;
}
XREDIS_API xredisadmission xredisClusterNodesCreate(xnetengine* E,uint64 Affinity,const xredisclusternodesconfig* Config,xredisclusternodes** Out)
{
    if(!xrtMemRangeValid(Config,sizeof(*Config)) || !xrtMemRangeValid(Out,sizeof(*Out)) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out)) || !xredisNodesConfig(Config)){
        xredisNodesError(XERR_ARGUMENT,"invalid Cluster nodes config/output");return XREDIS_ADMISSION_ERROR;}
    xredisclusternodesconfig K=*Config;size_t Buckets=1;while(Buckets<2*K.MaxNodes)Buckets*=2;
    size_t Base=sizeof(xredisclusternodes)+Buckets*sizeof(xredisclusternodepool*),Each=sizeof(xredisclusternodepool)+K.MaxHostBytes;
    if(Base>K.MaxStorageBytes || K.MaxNodes>(K.MaxStorageBytes-Base)/Each){xredisNodesError(XERR_RANGE,"Cluster nodes storage limit");return XREDIS_ADMISSION_ERROR;}
    size_t Bytes=Base+K.MaxNodes*Each;
    if(!xrtNetEnginePin(E))return XREDIS_ADMISSION_ERROR;
    xredisclusternodes* C=xrtCalloc(1,Bytes);if(!C || !xrtMutexInit(&C->Gate)){xrtFree(C);xrtNetEngineUnpin(E);return XREDIS_ADMISSION_ERROR;}
    xrtAtomic32Init(&C->References,1);xrtAtomic32Init(&C->Admission,0);C->StorageBytes=Bytes;C->Engine=E;C->Affinity=Affinity;C->Config=K;C->BucketCount=Buckets;
    if(!(C->Promise=xrtPromiseCreate(&C->Closed,NULL))){C->Engine=NULL;xrtNetEngineUnpin(E);xredisClusterNodesRelease(C);return XREDIS_ADMISSION_ERROR;}
    C->Buckets=(xredisclusternodepool**)(C+1);C->Slots=(xredisclusternodepool*)(C->Buckets+Buckets);unsigned char* Host=(unsigned char*)(C->Slots+K.MaxNodes);
    for(size_t I=0;I<K.MaxNodes;++I){xredisclusternodepool* N=&C->Slots[I];N->Cache=C;N->Address.Host.Data=Host+I*K.MaxHostBytes;N->FreeNext=C->Free;C->Free=N;}
    C->Info.Size=sizeof(C->Info);C->Info.Capacity=K.MaxNodes;C->Info.StorageBytes=Bytes;C->Info.ConnectionBound=K.MaxNodes*K.ConnectionsPerNode;
    C->Info.LookupBound=K.MaxLookups;C->Info.ConnectingBound=K.MaxNodes*(K.MaxConnecting<K.ConnectionsPerNode?K.MaxConnecting:K.ConnectionsPerNode);C->Info.RequestBound=(uint64)K.MaxNodes*K.Pool.MaxRequests;C->Info.CommandBytesBound=(uint64)K.MaxNodes*K.Pool.MaxCommandBytes;
    xredisNodesRef(C);*Out=C;return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisclusterpoolstatus xredisClusterNodesGetPool(const xredisclusteraddress* A,int64 Remaining,ptr Data,xredispool** Out)
{
    xredisclusternodes* C=Data;double Entered=xrtTimer();
    if(!C || !xredisNodesAddress(A,C->Config.MaxHostBytes) || !xredisNodesOutput(C,Out,sizeof(*Out)) || Remaining<0 || Remaining>86400000 ||
        xrtMemRangesOverlap(A,sizeof(*A),Out,sizeof(*Out)) || xrtMemRangesOverlap(A->Host.Data,A->Host.Size,Out,sizeof(*Out))){xredisNodesError(XERR_ARGUMENT,"invalid Cluster node lookup");return XREDIS_CLUSTER_POOL_ERROR;}
    xredisclusterpoolstatus Began=xredisNodesBegin(C);if(Began!=XREDIS_CLUSTER_POOL_READY)return Began;
    if(!Remaining){xrtMutexLock(&C->Gate);++C->Info.Waits;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_WAIT;}
    bool Allowed=C->Config.Authorize(A,C->Config.Data);uint64 H=xredisNodesHash(A);xrtMutexLock(&C->Gate);
    if(!Allowed){++C->Info.Denied;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_DENIED;}
    if(C->Info.State!=XREDIS_CLUSTER_NODES_OPEN){xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_CLOSED;}
    if(xrtTimer()>=nextafter(Entered+(double)(Remaining/1000)+(double)(Remaining%1000)*.001,INFINITY)){++C->Info.Waits;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_WAIT;}
    xredisclusternodepool* N=xredisNodesFind(C,A,H);
    if(N){if(N->Opening || N->Retiring){++C->Info.Waits;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_WAIT;}
        xredisPoolRetain(N->Pool);*Out=N->Pool;++C->Info.Hits;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_READY;}
    if(!C->Free){++C->Info.Waits;xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_WAIT;}
    N=C->Free;C->Free=N->FreeNext;N->FreeNext=NULL;N->Address.Host.Size=A->Host.Size;memcpy((ptr)N->Address.Host.Data,A->Host.Data,A->Host.Size);N->Address.Port=A->Port;N->Address.Tls=A->Tls;N->Address.Slot=0;N->Address.Reason=0;
    N->Hash=H;size_t Bucket=H&(C->BucketCount-1);N->Next=C->Buckets[Bucket];C->Buckets[Bucket]=N;N->Opening=true;N->Retiring=false;N->ObservedClosed=false;N->Pool=NULL;N->Closed=NULL;++C->Info.Nodes;++C->Info.Opening;++C->Info.Misses;
    xrtMutexUnlock(&C->Gate);
    if(!xrtFutureWatchInit(&N->Watch,xredisNodesNotify,xredisNodesReleased,N)){
        xredispool* P=NULL;xfuture* F=NULL;xrtMutexLock(&C->Gate);--C->Info.Opening;N->Opening=false;++C->Info.Failures;xredisNodesUnlink(N,&P,&F);xredisNodesEndLocked(C);return XREDIS_CLUSTER_POOL_MEMORY;}
    xredispoolconnectconfig Connect={sizeof(Connect),xredisNodesConnect,NULL,N,C->Config.Protocol,C->Config.MaxConnecting,C->Config.AttemptTimeout,C->Config.RetryMin,C->Config.RetryMax};
    xredispool* P=NULL;xredisNodesRef(C);xredisadmission Admission=xredisSharedPoolConnect(C->Engine,C->Affinity,C->Config.ConnectionsPerNode,&C->Config.Pool,&Connect,&P);
    if(Admission!=XREDIS_ADMISSION_ACCEPTED){xredispool* Drop=NULL;xfuture* F=NULL;xrtMutexLock(&C->Gate);--C->Info.Opening;N->Opening=false;++C->Info.Failures;xredisNodesUnlink(N,&Drop,&F);xrtMutexUnlock(&C->Gate);xredisClusterNodesRelease(C);xredisNodesEnd(C);return Admission==XREDIS_ADMISSION_CLOSED?XREDIS_CLUSTER_POOL_CLOSED:XREDIS_CLUSTER_POOL_MEMORY;}
    xfuture* Closed=xredisPoolClosed(P);xrtMutexLock(&C->Gate);N->Pool=P;N->Closed=Closed;++C->Info.Created;xrtMutexUnlock(&C->Gate);
    xfuturewatchresult W=xrtFutureWatchAdd(Closed,&N->Watch);
    /* Fresh private Watch/valid native Future: READY/PENDING are the only
     * possible outcomes. No allocation is performed by registration. */
    if(W==XFUTURE_WATCH_ERROR)abort();
    if(W==XFUTURE_WATCH_READY)xredisNodesReleased(N);
    xredispool* Drop=NULL;xfuture* F=NULL;xredisclusterpoolstatus Result=XREDIS_CLUSTER_POOL_READY;
    xrtMutexLock(&C->Gate);N->Opening=false;--C->Info.Opening;bool Closing=C->Info.State!=XREDIS_CLUSTER_NODES_OPEN || N->Retiring;bool Dead=N->ObservedClosed;
    if(Dead){xredisNodesUnlink(N,&Drop,&F);Result=Closing?XREDIS_CLUSTER_POOL_CLOSED:XREDIS_CLUSTER_POOL_WAIT;}
    else {if(Closing){N->Retiring=true;Result=XREDIS_CLUSTER_POOL_CLOSED;}else {xredisPoolRetain(P);*Out=P;}}
    if(Closing && !Dead)xredisPoolRetain(P);
    xrtMutexUnlock(&C->Gate);
    if(Closing && !Dead){xredisPoolClose(P);xredisPoolRelease(P);}xrtFutureDestroy(F);xredisPoolRelease(Drop);xredisNodesEnd(C);return Result;
}
XREDIS_API bool xredisClusterNodesRetire(xredisclusternodes* C,const xredisclusteraddress* A)
{
    if(!C || !xredisNodesAddress(A,C->Config.MaxHostBytes))return xredisNodesError(XERR_ARGUMENT,"invalid Cluster node retirement");
    uint64 H=xredisNodesHash(A);xredispool* P=NULL;xrtMutexLock(&C->Gate);xredisclusternodepool* N=xredisNodesFind(C,A,H);if(N){N->Retiring=true;if(N->Pool){P=N->Pool;xredisPoolRetain(P);}}xrtMutexUnlock(&C->Gate);
    if(P){xredisPoolClose(P);xredisPoolRelease(P);}return true;
}
XREDIS_API bool xredisClusterNodesInfo(xredisclusternodes* C,xredisclusternodesinfo* Out)
{if(!xredisNodesOutput(C,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out))return xredisNodesError(XERR_ARGUMENT,"invalid Cluster node info output");xrtMutexLock(&C->Gate);*Out=C->Info;Out->APIs=(xrtAtomic32Load(&C->Admission,XMEMORY_ACQUIRE)&XREDIS_NODES_USERS)+(C->Scanning?1u:0u);xrtMutexUnlock(&C->Gate);return true;}
XREDIS_API bool xredisClusterNodesClose(xredisclusternodes* C)
{
    if(!C)return xredisNodesError(XERR_ARGUMENT,"Cluster node cache is NULL");
    xrtMutexLock(&C->Gate);if(C->Info.State!=XREDIS_CLUSTER_NODES_OPEN){xrtMutexUnlock(&C->Gate);return true;}xrtAtomic32FetchOr(&C->Admission,XREDIS_NODES_SEALED,XMEMORY_ACQ_REL);C->Info.State=XREDIS_CLUSTER_NODES_CLOSING;C->Scanning=true;xrtMutexUnlock(&C->Gate);
    for(size_t I=0;I<C->Config.MaxNodes;++I){xredispool* P=NULL;xrtMutexLock(&C->Gate);xredisclusternodepool* N=&C->Slots[I];N->Retiring=true;if(N->Pool){P=N->Pool;xredisPoolRetain(P);}xrtMutexUnlock(&C->Gate);if(P){xredisPoolClose(P);xredisPoolRelease(P);}}
    xrtMutexLock(&C->Gate);C->Scanning=false;xrtMutexUnlock(&C->Gate);xredisNodesFinish(C);return true;
}
XREDIS_API xfuture* xredisClusterNodesClosed(xredisclusternodes* C)
{if(!C){xredisNodesError(XERR_ARGUMENT,"Cluster node cache is NULL");return NULL;}return xrtFutureRef(C->Closed);}
#endif
