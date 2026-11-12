#include <xredis/cluster_refresh.h>
#include "../internal/pool_admit.h"
#include "../internal/cluster_topology_internal.h"
#include "../internal/cluster_refresh_internal.h"
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <math.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
enum {XRF_RESOLVE,XRF_NATIVE,XRF_BACKOFF};
struct xredisclustermap {xatomic32 References;xredisclustermapinfo Info;size_t Bytes;};
typedef struct xredisrefreshwork {
    xredisclusterrefreshresult Result;xfuture* Future;xpromise* Promise;
    double Entered,Deadline,Due;size_t Seed,AttemptSeed,ProviderSeed;unsigned Phase;bool Done;
} xredisrefreshwork;
struct xredisclusterrefresh {
    xatomic32 References;xmutex Gate;xredisclusterrefreshconfig Config;
    xredisclusternodes* Nodes;xredisclusteraddress *Seeds,*Bootstrap;size_t Count,BootCount,Cursor,Bytes,Buckets;
    uint32* Plan;uint16* Hash;unsigned char* SeedHosts;
    xredisclusterrefreshinfo Info;xredisrefreshwork* Work;xredisclustermap* Current;
    xfuture *Last,*Closed;xpromise* Promise;bool Finishing;
};
static bool xredisRefreshError(xerrkind Kind,cstr Text)
{xrtSetErrorInfo(Kind,"xrt.redis",Kind==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,Text);return false;}
static double xredisRefreshDeadline(double E,int64 T)
{return T==0?E:nextafter(E+(double)(T/1000)+(double)(T%1000)*.001,INFINITY);}
static bool xredisRefreshHost(xbytesview H)
{if(!H.Size || !xrtMemRangeValid(H.Data,H.Size) || (H.Size==1 && H.Data[0]=='?'))return false;for(size_t I=0;I<H.Size;++I){unsigned X=H.Data[I];if(X<=32 || X==127 || X=='/' || X=='\\' || X=='@' || X=='#')return false;}return true;}
XREDIS_API bool xredisClusterMapRetain(xredisclustermap* M)
{if(!M)return xredisRefreshError(XERR_ARGUMENT,"Cluster map is NULL");xrtAtomic32FetchAdd(&M->References,1,XMEMORY_RELAXED);return true;}
XREDIS_API void xredisClusterMapRelease(xredisclustermap* M)
{if(!M || xrtAtomic32FetchSub(&M->References,1,XMEMORY_ACQ_REL)!=1)return;xredisClusterTopologyRelease(M->Info.Topology);xrtFree(M);}
bool __xredisClusterMapOutput(const xredisclustermap* M,const void* P,size_t N)
{return xrtMemRangeValid(P,N) && (!M || (!xrtMemRangesOverlap(M,M->Bytes,P,N) && __xredisClusterTopologyOutput(M->Info.Topology,P,N)));}
XREDIS_API bool xredisClusterMapInfo(const xredisclustermap* M,xredisclustermapinfo* Out)
{if(!M || !__xredisClusterMapOutput(M,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out))return xredisRefreshError(XERR_ARGUMENT,"invalid Cluster map info output");*Out=M->Info;return true;}
static bool xredisRefreshOutput(xredisclusterrefresh* C,const void* P,size_t N)
{return C && xrtMemRangeValid(P,N) && !xrtMemRangesOverlap(C,C->Bytes,P,N) && __xredisClusterMapOutput(C->Current,P,N);}
XREDIS_API bool xredisClusterRefreshRetain(xredisclusterrefresh* C)
{if(!C)return xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");xrtAtomic32FetchAdd(&C->References,1,XMEMORY_RELAXED);return true;}
XREDIS_API void xredisClusterRefreshRelease(xredisclusterrefresh* C)
{if(!C || xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)!=1)return;xrtPromiseDestroy(C->Promise);xrtFutureDestroy(C->Closed);xrtMutexUnit(&C->Gate);xrtFree(C);}
static void xredisRefreshPayloadFree(ptr Value,ptr Data)
{(void)Data;xredisrefreshwork* W=Value;xrtFutureDestroy(W->Result.Native);xredisClusterMapRelease(W->Result.Map);xrtFree(W);}
static void xredisRefreshMaybeClose(xredisclusterrefresh* C)
{
    xrtMutexLock(&C->Gate);if(!C->Info.Closing || C->Info.Busy || C->Work || C->Finishing){xrtMutexUnlock(&C->Gate);return;}
    C->Finishing=true;xredisclusternodes* N=C->Nodes;C->Nodes=NULL;xredisclustermap* M=C->Current;C->Current=NULL;xfuture* Last=C->Last;C->Last=NULL;xpromise* P=C->Promise;C->Promise=NULL;xrtMutexUnlock(&C->Gate);
    xredisClusterNodesRelease(N);xredisClusterMapRelease(M);xrtFutureDestroy(Last);
    xrtMutexLock(&C->Gate);C->Info.Closed=true;xrtMutexUnlock(&C->Gate);xrtPromiseResolve(P,NULL);xrtPromiseDestroy(P);xredisClusterRefreshRelease(C);
}
static void xredisRefreshStop(xredisrefreshwork* W,xredisclusterrefreshstatus S)
{W->Result.Status=S;W->Done=true;}
/* Busy keeps Work/cache/seeds stable while policy/native calls run outside Gate. */
static void xredisRefreshPublish(xredisclusterrefresh* C,xredisrefreshwork* W)
{
    if(W->Result.Native && xrtFutureState(W->Result.Native)==XFUTURE_PENDING)xrtFutureCancel(W->Result.Native);
    if(W->Result.Native && xrtFutureState(W->Result.Native)==XFUTURE_RESOLVED){const xredisrequestresult* Q=xrtFutureValue(W->Result.Native);if(Q){W->Result.Query=*Q;W->Result.QueryObserved=true;}}
    xredisclustermap* OldMap=NULL;xfuture* OldLast=NULL;xredisclustermap* Unpublished=NULL;
    xrtMutexLock(&C->Gate);
    if(C->Info.Closing)W->Result.Status=XREDIS_REFRESH_CLOSED;
    if(W->Result.Status==XREDIS_REFRESH_OK){
        if(C->Info.Generation==UINT64_MAX)W->Result.Status=XREDIS_REFRESH_LIMIT;
        else {xredisclustermap* M=W->Result.Map;M->Info.Generation=++C->Info.Generation;M->Info.Epoch=W->Result.Epoch;W->Result.Generation=M->Info.Generation;W->Result.Published=true;
            W->Result.StaleAtPublish=W->Result.Epoch!=C->Info.DesiredEpoch;if(W->Result.StaleAtPublish)W->Result.Status=XREDIS_REFRESH_STALE;
            OldMap=C->Current;C->Current=M;xredisClusterMapRetain(M);C->Info.PublishedEpoch=W->Result.Epoch;C->Info.Dirty=C->Info.PublishedEpoch!=C->Info.DesiredEpoch;}
    }
    if(!W->Result.Published){Unpublished=W->Result.Map;W->Result.Map=NULL;}
    size_t Last=W->Result.ProviderCalls?W->ProviderSeed:W->Seed;
    xredisclusteraddress A=C->Seeds[W->Result.Attempts?W->AttemptSeed:Last];unsigned char* Host=(unsigned char*)(W+1);memcpy(Host,A.Host.Data,A.Host.Size);W->Result.Origin=A;W->Result.Origin.Host=(xbytesview){Host,A.Host.Size};
    C->Cursor=W->Result.Published?W->AttemptSeed:(Last+1)%C->Count;
    C->Work=NULL;C->Info.Active=false;++C->Info.Completed;C->Info.LastStatus=W->Result.Status;OldLast=C->Last;C->Last=W->Future;W->Future=NULL;xpromise* P=W->Promise;W->Promise=NULL;xrtMutexUnlock(&C->Gate);
    xredisClusterMapRelease(Unpublished);xredisClusterMapRelease(OldMap);xrtFutureDestroy(OldLast);xrtPromiseResolveOwned(P,W,xredisRefreshPayloadFree,NULL);xrtPromiseDestroy(P);
    xrtMutexLock(&C->Gate);C->Info.Busy=false;xrtMutexUnlock(&C->Gate);xredisRefreshMaybeClose(C);
}
XREDIS_API bool xredisClusterRefreshConfigInit(xredisclusterrefreshconfig* K)
{
    if(!xrtMemRangeValid(K,sizeof(*K)))return xredisRefreshError(XERR_ARGUMENT,"invalid Cluster refresh config output");
    *K=(xredisclusterrefreshconfig){sizeof(*K),16,1024,1024u*1024u,32u*1024u*1024u,8,1024,5000,5,XREDIS_CLUSTER_SHARDS,{0}};
    if(!xredisClusterLimitsInit(&K->Limits))return false;
    K->Limits.RequireFullCoverage=true;return true;
}
XREDIS_API xredisadmission xredisClusterRefreshCreate(xredisclusternodes* Nodes,const xredisclusteraddress* A,size_t Count,const xredisclusterrefreshconfig* Config,xredisclusterrefresh** Out)
{
    if(!Nodes || !xrtMemRangeValid(Config,sizeof(*Config)) || !xrtMemRangeValid(Out,sizeof(*Out)) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out))){xredisRefreshError(XERR_ARGUMENT,"invalid Cluster refresh config/output");return XREDIS_ADMISSION_ERROR;}
    xredisclusterrefreshconfig K=*Config;size_t Overhead=2*(sizeof(xredisrefreshwork)+K.MaxHostBytes);
    if(K.Size!=sizeof(K) || !Count || !K.MaxSeeds || K.MaxSeeds>1024 || Count>K.MaxSeeds || !K.MaxHostBytes || K.MaxHostBytes>65536 || !K.MaxControlBytes || K.MaxControlBytes>128u*1024u*1024u ||
        K.MaxSnapshotBytes<sizeof(xredisclustermap) || K.MaxSnapshotBytes>128u*1024u*1024u || !K.MaxAttempts || K.MaxAttempts>1024 || K.MaxWaits>65536 || K.Timeout<0 || K.Timeout>86400000 || K.RetryDelay<=0 || K.RetryDelay>60000 ||
        (K.Format!=XREDIS_CLUSTER_SHARDS && K.Format!=XREDIS_CLUSTER_LEGACY_SLOTS) || K.Limits.Size!=sizeof(K.Limits) || K.Limits.MaxNodes>65535 || K.Limits.MaxShards>XREDIS_CLUSTER_SLOTS || K.Limits.MaxRanges>XREDIS_CLUSTER_SLOTS || !xrtMemRangeValid(A,Count*sizeof(*A)) || xrtMemRangesOverlap(A,Count*sizeof(*A),Out,sizeof(*Out))){xredisRefreshError(XERR_ARGUMENT,"invalid Cluster refresh bounds");return XREDIS_ADMISSION_ERROR;}
    size_t Buckets=4;while(Buckets<4*K.MaxSeeds)Buckets*=2;
    size_t Base=sizeof(xredisclusterrefresh)+(Count+2*K.MaxSeeds+1)*sizeof(*A)+(2*K.MaxSeeds+1)*sizeof(uint32)+Buckets*sizeof(uint16)+(K.MaxSeeds+1)*K.MaxHostBytes,Bytes=Base;
    if(Base>K.MaxControlBytes || Overhead>K.MaxControlBytes-Base){xredisRefreshError(XERR_RANGE,"Cluster refresh control limit");return XREDIS_ADMISSION_ERROR;}
    for(size_t I=0;I<Count;++I){if(!A[I].Port || A[I].Slot || A[I].Reason || A[I].Host.Size>K.MaxHostBytes || !xredisRefreshHost(A[I].Host) || xrtMemRangesOverlap(A[I].Host.Data,A[I].Host.Size,Out,sizeof(*Out)) || A[I].Host.Size>K.MaxControlBytes-Bytes-Overhead){xredisRefreshError(XERR_ARGUMENT,"invalid Cluster refresh seed/storage");return XREDIS_ADMISSION_ERROR;}Bytes+=A[I].Host.Size;}
    xredisclusterrefresh* C=xrtCalloc(1,Bytes);if(!C || !xrtMutexInit(&C->Gate)){xrtFree(C);return XREDIS_ADMISSION_ERROR;}xrtAtomic32Init(&C->References,1);C->Config=K;C->Bytes=Bytes;C->Count=C->BootCount=Count;C->Buckets=Buckets;C->Bootstrap=(xredisclusteraddress*)(C+1);C->Seeds=C->Bootstrap+Count;C->Plan=(uint32*)(C->Seeds+2*K.MaxSeeds+1);C->Hash=(uint16*)(C->Plan+2*K.MaxSeeds+1);C->SeedHosts=(unsigned char*)(C->Hash+Buckets);
    if(!(C->Promise=xrtPromiseCreate(&C->Closed,NULL))){xredisClusterRefreshRelease(C);return XREDIS_ADMISSION_ERROR;}
    unsigned char* Host=C->SeedHosts+(K.MaxSeeds+1)*K.MaxHostBytes;for(size_t I=0;I<Count;++I){C->Bootstrap[I]=A[I];memcpy(Host,A[I].Host.Data,A[I].Host.Size);C->Bootstrap[I].Host.Data=Host;C->Seeds[I]=C->Bootstrap[I];Host+=A[I].Host.Size;}
    C->Nodes=Nodes;xredisClusterNodesRetain(Nodes);C->Info.Size=sizeof(C->Info);C->Info.DesiredEpoch=1;C->Info.Dirty=true;C->Info.Seeds=Count;C->Info.ControlBytes=Bytes+Overhead;xredisClusterRefreshRetain(C);*Out=C;return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisadmission xredisClusterRefreshStart(xredisclusterrefresh* C,bool Force,xfuture** Out)
{
    double E=xrtTimer();if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return XREDIS_ADMISSION_ERROR;}xrtMutexLock(&C->Gate);
    if(!xredisRefreshOutput(C,Out,sizeof(*Out))){xrtMutexUnlock(&C->Gate);xredisRefreshError(XERR_ARGUMENT,"invalid refresh Future output");return XREDIS_ADMISSION_ERROR;}
    if(C->Info.Closing){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_CLOSED;}
    if(C->Work){*Out=xrtFutureRef(C->Work->Future);++C->Info.Coalesced;xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;}
    if(C->Info.Busy){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_OVERLOADED;}
    if(!Force && !C->Info.Dirty && C->Last){*Out=xrtFutureRef(C->Last);++C->Info.CacheHits;xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;}
    C->Info.Busy=true;xrtMutexUnlock(&C->Gate);
    xredisrefreshwork* W=xrtCalloc(1,sizeof(*W)+C->Config.MaxHostBytes);
    if(W)W->Promise=xrtPromiseCreate(&W->Future,NULL);
    xrtMutexLock(&C->Gate);bool Closing=C->Info.Closing,Advance=Force && !C->Info.Dirty,Limit=Advance && C->Info.DesiredEpoch==UINT64_MAX;
    if(Closing || Limit || !W || !W->Promise){xrtMutexUnlock(&C->Gate);if(W){xrtPromiseDestroy(W->Promise);xrtFutureDestroy(W->Future);xrtFree(W);}xrtMutexLock(&C->Gate);C->Info.Busy=false;xrtMutexUnlock(&C->Gate);xredisRefreshMaybeClose(C);if(Limit)xredisRefreshError(XERR_RANGE,"Cluster refresh epoch limit");return Closing?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ERROR;}
    if(Advance)++C->Info.DesiredEpoch;
    C->Info.Dirty=true;W->Entered=E;W->Deadline=xredisRefreshDeadline(E,C->Config.Timeout);W->Due=W->Deadline;W->Seed=C->Cursor;W->Result.Size=sizeof(W->Result);W->Result.Epoch=C->Info.DesiredEpoch;C->Work=W;C->Info.Active=true;C->Info.Busy=false;++C->Info.Started;*Out=xrtFutureRef(W->Future);xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;
}
static bool xredisRefreshSeedEqual(const xredisclusteraddress* A,const xredisclusteraddress* B)
{return A->Port==B->Port && A->Tls==B->Tls && A->Host.Size==B->Host.Size && !memcmp(A->Host.Data,B->Host.Data,A->Host.Size);}
static bool xredisRefreshCandidate(const xredisclustermapinfo* M,size_t Index,size_t Max,xredisclusteraddress* A)
{
    xredisclusternode N={0};if(!xredisClusterTopologyNode(M->Topology,Index,&N))return false;
    xredisclustertopologyinfo T={0};T.Size=sizeof(T);if(!xredisClusterTopologyInfo(M->Topology,&T))return false;
    *A=(xredisclusteraddress){N.Endpoint==XREDIS_CLUSTER_ENDPOINT_SOURCE?M->Origin.Host:N.Host,T.Format==XREDIS_CLUSTER_LEGACY_SLOTS?N.Port:M->Origin.Tls?N.TlsPort:N.Port,0,M->Origin.Tls,0};
    return N.Role==XREDIS_CLUSTER_MASTER && (N.Health==XREDIS_CLUSTER_ONLINE || N.Health==XREDIS_CLUSTER_UNREPORTED) && N.Endpoint!=XREDIS_CLUSTER_ENDPOINT_UNKNOWN && A->Port && A->Host.Size<=Max && xredisRefreshHost(A->Host);
}
static xredisclusteraddress xredisRefreshPlanned(xredisclusterrefresh* C,const xredisclustermapinfo* M,uint32 Code)
{xredisclusteraddress A={0};if(Code&0x80000000u)A=C->Bootstrap[Code&0x7fffffffu];else if(Code&0x40000000u){A=M->Origin;A.Slot=0;A.Reason=0;}else (void)xredisRefreshCandidate(M,Code,C->Config.MaxHostBytes,&A);return A;}
static bool xredisRefreshPlanAdd(xredisclusterrefresh* C,const xredisclustermapinfo* M,uint32 Code,const xredisclusteraddress* A,size_t* Count)
{
    uint64 H=14695981039346656037ull;for(size_t I=0;I<A->Host.Size;++I){H^=A->Host.Data[I];H*=1099511628211ull;}H^=A->Port;H*=1099511628211ull;H^=A->Tls;size_t Bucket=(size_t)H&(C->Buckets-1);
    while(C->Hash[Bucket]){xredisclusteraddress B=xredisRefreshPlanned(C,M,C->Plan[C->Hash[Bucket]-1]);if(xredisRefreshSeedEqual(A,&B))return false;Bucket=(Bucket+1)&(C->Buckets-1);}
    C->Plan[*Count]=Code;C->Hash[Bucket]=(uint16)(*Count+1);++*Count;return true;
}
XREDIS_API xredisadmission xredisClusterRefreshDiscover(xredisclusterrefresh* C,const xredisclustermap* Map)
{
    if(!C || !Map){xredisRefreshError(XERR_ARGUMENT,"invalid Cluster seed discovery");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);if(C->Info.Closing){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_CLOSED;}if(C->Info.Busy || C->Work){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_OVERLOADED;}C->Info.Busy=true;xrtMutexUnlock(&C->Gate);
    xredisclustermapinfo M={0};M.Size=sizeof(M);xredisClusterMapInfo(Map,&M);xredisclustertopologyinfo T={0};T.Size=sizeof(T);xredisClusterTopologyInfo(M.Topology,&T);memset(C->Hash,0,C->Buckets*sizeof(*C->Hash));size_t Count=0;
    for(size_t I=0;I<T.Nodes;++I){xredisclusteraddress A;if(xredisRefreshCandidate(&M,I,C->Config.MaxHostBytes,&A) && xredisRefreshSeedEqual(&A,&M.Origin)){xredisRefreshPlanAdd(C,&M,(uint32)I,&A,&Count);break;}}
    for(size_t I=0;I<T.Nodes && Count<C->Config.MaxSeeds;++I){xredisclusteraddress A;if(xredisRefreshCandidate(&M,I,C->Config.MaxHostBytes,&A))xredisRefreshPlanAdd(C,&M,(uint32)I,&A,&Count);}
    size_t Discovered=Count;if(M.Origin.Port && M.Origin.Host.Size<=C->Config.MaxHostBytes && xredisRefreshHost(M.Origin.Host))xredisRefreshPlanAdd(C,&M,0x40000000u,&M.Origin,&Count);for(size_t I=0;I<C->BootCount;++I)xredisRefreshPlanAdd(C,&M,0x80000000u|(uint32)I,&C->Bootstrap[I],&Count);
    xrtMutexLock(&C->Gate);bool Closing=C->Info.Closing;xrtMutexUnlock(&C->Gate);
    if(!Closing){unsigned char* Host=C->SeedHosts;for(size_t I=0;I<Count;++I){xredisclusteraddress A=xredisRefreshPlanned(C,&M,C->Plan[I]);C->Seeds[I]=A;if(I<Discovered || C->Plan[I]==0x40000000u){memcpy(Host,A.Host.Data,A.Host.Size);C->Seeds[I].Host.Data=Host;Host+=A.Host.Size;}}}
    xrtMutexLock(&C->Gate);Closing=C->Info.Closing;if(!Closing){C->Count=Count;C->Cursor=0;C->Info.Seeds=Count;}C->Info.Busy=false;xrtMutexUnlock(&C->Gate);xredisRefreshMaybeClose(C);return Closing?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API bool xredisClusterRefreshInvalidate(xredisclusterrefresh* C)
{
    if(!C)return xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");
    xrtMutexLock(&C->Gate);
    if(C->Info.Closing || C->Info.DesiredEpoch==UINT64_MAX){xrtMutexUnlock(&C->Gate);return xredisRefreshError(XERR_STATE,"Cluster refresh is sealed or epoch exhausted");}
    ++C->Info.DesiredEpoch;C->Info.Dirty=true;xrtMutexUnlock(&C->Gate);return true;
}
static void xredisRefreshWait(xredisclusterrefresh* C,xredisrefreshwork* W)
{if(W->Result.Waits>=C->Config.MaxWaits){xredisRefreshStop(W,XREDIS_REFRESH_OVERLOADED);return;}++W->Result.Waits;W->Due=fmin(W->Deadline,xredisRefreshDeadline(xrtTimer(),C->Config.RetryDelay));W->Phase=XRF_BACKOFF;}
static void xredisRefreshResolve(xredisclusterrefresh* C,xredisrefreshwork* W)
{
    double Ms=(W->Deadline-xrtTimer())*1000.;int64 Remaining=Ms<=0?0:Ms>=86400000.?86400000:(int64)ceil(Ms);xredispool* P=NULL;W->ProviderSeed=W->Seed;++W->Result.ProviderCalls;
    W->Result.Provider=xredisClusterNodesGetPool(&C->Seeds[W->Seed],Remaining,C->Nodes,&P);
    xrtMutexLock(&C->Gate);bool Closing=C->Info.Closing;xrtMutexUnlock(&C->Gate);
    if(Closing || xrtTimer()>=W->Deadline){if(P)xredisPoolRelease(P);xredisRefreshStop(W,Closing?XREDIS_REFRESH_CLOSED:XREDIS_REFRESH_TIMEOUT);return;}
    if(W->Result.Provider!=XREDIS_CLUSTER_POOL_READY){
        if(W->Result.Provider==XREDIS_CLUSTER_POOL_WAIT){W->Seed=(W->Seed+1)%C->Count;xredisRefreshWait(C,W);}
        else xredisRefreshStop(W,W->Result.Provider==XREDIS_CLUSTER_POOL_DENIED?XREDIS_REFRESH_POLICY:W->Result.Provider==XREDIS_CLUSTER_POOL_CLOSED?XREDIS_REFRESH_CLOSED:W->Result.Provider==XREDIS_CLUSTER_POOL_MEMORY?XREDIS_REFRESH_MEMORY:XREDIS_REFRESH_PROTOCOL);
        return;
    }
    xbytesview Args[]={XRT_BYTES_LITERAL("CLUSTER"),C->Config.Format==XREDIS_CLUSTER_SHARDS?(xbytesview)XRT_BYTES_LITERAL("SHARDS"):(xbytesview)XRT_BYTES_LITERAL("SLOTS")};xfuture* F=NULL;
    W->Result.Admission=__xredisSharedPoolCall(P,Args,2,C->Config.Timeout,&F,W->Entered,false);xredisPoolRelease(P);
    if(W->Result.Admission==XREDIS_ADMISSION_ACCEPTED){xrtFutureDestroy(W->Result.Native);W->Result.Native=F;W->AttemptSeed=W->Seed;++W->Result.Attempts;W->Phase=XRF_NATIVE;W->Due=W->Deadline;}
    else if(W->Result.Admission==XREDIS_ADMISSION_OVERLOADED){W->Seed=(W->Seed+1)%C->Count;xredisRefreshWait(C,W);}
    else xredisRefreshStop(W,W->Result.Admission==XREDIS_ADMISSION_CLOSED?XREDIS_REFRESH_CLOSED:XREDIS_REFRESH_MEMORY);
}
static void xredisRefreshRead(xredisclusterrefresh* C,xredisrefreshwork* W)
{
    const xredisrequestresult* Q=xrtFutureValue(W->Result.Native);if(!Q){xredisRefreshStop(W,XREDIS_REFRESH_PROTOCOL);return;}
    if(Q->Status!=XREDIS_REQUEST_OK){
        if(Q->Status==XREDIS_REQUEST_SERVER_ERROR){xredisRefreshStop(W,XREDIS_REFRESH_SERVER);return;}
        if(W->Result.Attempts>=C->Config.MaxAttempts){xredisRefreshStop(W,XREDIS_REFRESH_TRANSPORT);return;}
        W->Seed=(W->Seed+1)%C->Count;xredisRefreshWait(C,W);return;
    }
    xredisclusteraddress A=C->Seeds[W->Seed];size_t Bytes=sizeof(xredisclustermap)+A.Host.Size;
    if(Bytes>C->Config.MaxSnapshotBytes){xredisRefreshStop(W,XREDIS_REFRESH_LIMIT);return;}
    xredisclusterlimits K=C->Config.Limits;if(K.MaxOwnedBytes>C->Config.MaxSnapshotBytes-Bytes)K.MaxOwnedBytes=C->Config.MaxSnapshotBytes-Bytes;xredisclustertopology* T=NULL;
    W->Result.Parse=xredisClusterTopologyCreate(Q->Reply,C->Config.Format,&K,&T);
    if(W->Result.Parse!=XREDIS_CLUSTER_VALUE){xredisRefreshStop(W,W->Result.Parse==XREDIS_CLUSTER_MEMORY?XREDIS_REFRESH_MEMORY:W->Result.Parse==XREDIS_CLUSTER_LIMIT?XREDIS_REFRESH_LIMIT:XREDIS_REFRESH_PROTOCOL);return;}
    xredisclustermap* M=xrtCalloc(1,Bytes);if(!M){xredisClusterTopologyRelease(T);xredisRefreshStop(W,XREDIS_REFRESH_MEMORY);return;}
    xrtAtomic32Init(&M->References,1);M->Bytes=Bytes;M->Info.Size=sizeof(M->Info);M->Info.Topology=T;M->Info.Origin=A;unsigned char* Host=(unsigned char*)(M+1);memcpy(Host,A.Host.Data,A.Host.Size);M->Info.Origin.Host=(xbytesview){Host,A.Host.Size};
    xredisclustertopologyinfo Info={0};Info.Size=sizeof(Info);xredisClusterTopologyInfo(T,&Info);M->Info.StorageBytes=Bytes+Info.StorageBytes+Info.RetainedReplyBytes;W->Result.Map=M;xredisRefreshStop(W,XREDIS_REFRESH_OK);
}
XREDIS_API xredisclusterrefreshstep xredisClusterRefreshStep(xredisclusterrefresh* C)
{
    if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return XREDIS_REFRESH_STEP_ERROR;}xrtMutexLock(&C->Gate);
    if(C->Info.Busy){xrtMutexUnlock(&C->Gate);return XREDIS_REFRESH_BUSY;}xredisrefreshwork* W=C->Work;
    if(!W){xrtMutexUnlock(&C->Gate);return XREDIS_REFRESH_IDLE;}C->Info.Busy=true;bool Closing=C->Info.Closing;xrtMutexUnlock(&C->Gate);
    if(Closing)xredisRefreshStop(W,XREDIS_REFRESH_CLOSED);
    else if(xrtTimer()>=W->Deadline)xredisRefreshStop(W,XREDIS_REFRESH_TIMEOUT);
    else if(W->Phase==XRF_BACKOFF){if(xrtTimer()>=W->Due)W->Phase=XRF_RESOLVE;}
    else if(W->Phase==XRF_NATIVE){if(xrtFutureState(W->Result.Native)!=XFUTURE_PENDING)xredisRefreshRead(C,W);}
    else xredisRefreshResolve(C,W);
    if((!W->Done || W->Result.Status==XREDIS_REFRESH_OK) && xrtTimer()>=W->Deadline)xredisRefreshStop(W,XREDIS_REFRESH_TIMEOUT);
    xrtMutexLock(&C->Gate);Closing=C->Info.Closing;if(Closing)xredisRefreshStop(W,XREDIS_REFRESH_CLOSED);
    if(W->Done){xrtMutexUnlock(&C->Gate);xredisRefreshPublish(C,W);return XREDIS_REFRESH_DONE;}
    xredisclusterrefreshstep Result=W->Phase==XRF_BACKOFF || (W->Phase==XRF_NATIVE && xrtFutureState(W->Result.Native)==XFUTURE_PENDING)?XREDIS_REFRESH_WAIT:XREDIS_REFRESH_MORE;
    C->Info.Busy=false;xrtMutexUnlock(&C->Gate);return Result;
}
XREDIS_API xfuture* xredisClusterRefreshPending(xredisclusterrefresh* C)
{if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return NULL;}xrtMutexLock(&C->Gate);xredisrefreshwork* W=C->Work;xfuture* F=!C->Info.Busy && W && W->Phase==XRF_NATIVE?xrtFutureRef(W->Result.Native):NULL;xrtMutexUnlock(&C->Gate);return F;}
XREDIS_API double xredisClusterRefreshNextWake(xredisclusterrefresh* C)
{if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return 0;}xrtMutexLock(&C->Gate);xredisrefreshwork* W=C->Work;double D=!W?0:C->Info.Busy?xrtTimer():W->Phase==XRF_BACKOFF?W->Due:W->Phase==XRF_RESOLVE || (W->Result.Native && xrtFutureState(W->Result.Native)!=XFUTURE_PENDING)?fmin(W->Deadline,xrtTimer()):W->Deadline;xrtMutexUnlock(&C->Gate);return D;}
XREDIS_API xredisclusterstatus xredisClusterRefreshSnapshot(xredisclusterrefresh* C,xredisclustermap** Out,bool* Stale)
{
    if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return XREDIS_CLUSTER_ERROR;}xrtMutexLock(&C->Gate);
    if(!xredisRefreshOutput(C,Out,sizeof(*Out)) || !xredisRefreshOutput(C,Stale,sizeof(*Stale)) || xrtMemRangesOverlap(Out,sizeof(*Out),Stale,sizeof(*Stale))){xrtMutexUnlock(&C->Gate);xredisRefreshError(XERR_ARGUMENT,"invalid Cluster snapshot outputs");return XREDIS_CLUSTER_ERROR;}
    if(!C->Current){xrtMutexUnlock(&C->Gate);return XREDIS_CLUSTER_ABSENT;}xredisClusterMapRetain(C->Current);*Out=C->Current;*Stale=C->Info.Dirty;xrtMutexUnlock(&C->Gate);return XREDIS_CLUSTER_VALUE;
}
XREDIS_API bool xredisClusterRefreshInfo(xredisclusterrefresh* C,xredisclusterrefreshinfo* Out)
{if(!C)return xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");
    xrtMutexLock(&C->Gate);if(!xredisRefreshOutput(C,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out)){xrtMutexUnlock(&C->Gate);return xredisRefreshError(XERR_ARGUMENT,"invalid refresh info output");}*Out=C->Info;xrtMutexUnlock(&C->Gate);return true;}
XREDIS_API bool xredisClusterRefreshClose(xredisclusterrefresh* C)
{
    if(!C)return xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");
    xrtMutexLock(&C->Gate);C->Info.Closing=true;
    if(C->Info.Busy){xrtMutexUnlock(&C->Gate);return true;}xredisrefreshwork* W=C->Work;if(W)C->Info.Busy=true;xrtMutexUnlock(&C->Gate);
    if(W){xredisRefreshStop(W,XREDIS_REFRESH_CLOSED);xredisRefreshPublish(C,W);}else xredisRefreshMaybeClose(C);return true;
}
XREDIS_API xfuture* xredisClusterRefreshClosed(xredisclusterrefresh* C)
{if(!C){xredisRefreshError(XERR_ARGUMENT,"Cluster refresh is NULL");return NULL;}return xrtFutureRef(C->Closed);}
#endif
