#include <xredis/cluster_route.h>
#include "../internal/pool_admit.h"
#include "../internal/cluster_reply.h"
#include "../internal/cluster_topology_internal.h"
#include "../internal/cluster_route_internal.h"
#include <xrt/memory.h>
#include <math.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE)
enum {XCR_RESOLVE, XCR_NATIVE, XCR_BACKOFF, XCR_DONE};
struct xredisclusterroute {
    xredisclusterrouteconfig Config;
    xredisclusterrouteresult Result;
    xfuture* Future; xbytesview* Arguments; unsigned char *Host, *AttemptHost, *HostHeap;
    size_t Count, StorageBytes, HostCapacity; double Entered, Deadline, Due;
    unsigned Phase; bool NativeAsking;
};
static bool xredisRouteError(xerrkind Kind, cstr Text)
{xrtSetErrorInfo(Kind,"xrt.redis",Kind==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,Text);return false;}
static double xredisRouteDue(double Entered,int64 Timeout)
{return Timeout==0?Entered:nextafter(Entered+(double)(Timeout/1000)+(double)(Timeout%1000)*.001,INFINITY);}
static bool xredisRouteHost(xbytesview H)
{
    if(!H.Size || !xrtMemRangeValid(H.Data,H.Size) || (H.Size==1 && H.Data[0]=='?'))return false;
    for(size_t I=0;I<H.Size;++I){unsigned C=H.Data[I];if(C<=32 || C==127 || C=='/' || C=='\\' || C=='@' || C=='#')return false;}
    return true;
}
static bool xredisRouteObserve(xredisclusterroute* R)
{
    if(!R->Future || xrtFutureState(R->Future)!=XFUTURE_RESOLVED)return false;
    const xredisrequestresult* Q=xrtFutureValue(R->Future);if(!Q)return false;
    R->Result.Query=*Q;R->Result.RequestObserved=true;R->Result.UsedAsking=R->NativeAsking;
    if(R->NativeAsking){const xredisaskingresult* A=(const xredisaskingresult*)Q;R->Result.ControlReceived=A->ControlReceived;R->Result.Control=A->Control;}
    return true;
}
static void xredisRouteFinish(xredisclusterroute* R,xredisclusterroutestatus Status)
{
    if(R->Phase==XCR_DONE)return;
    R->Result.Status=Status;
    if(R->Future && xrtFutureState(R->Future)==XFUTURE_PENDING)xrtFutureCancel(R->Future);
    (void)xredisRouteObserve(R);R->Phase=XCR_DONE;R->Due=0;R->Config.GetPool=NULL;R->Config.Data=NULL;
}
static bool xredisRouteOutput(xredisclusterroute* R,const void* P,size_t Bytes)
{
    if(!R || !xrtMemRangeValid(P,Bytes) || xrtMemRangesOverlap(R,R?R->StorageBytes:0,P,Bytes) ||
        (R->HostHeap && xrtMemRangesOverlap(R->HostHeap,2*R->HostCapacity,P,Bytes)))return false;
    if(R->Future && xrtFutureState(R->Future)==XFUTURE_RESOLVED){
        const xredisrequestresult* Q=xrtFutureValue(R->Future);
        if(Q && (xrtMemRangesOverlap(Q,R->NativeAsking?sizeof(xredisaskingresult):sizeof(*Q),P,Bytes) ||
            (Q->Reply && !__xredisClusterReplyOutput(Q->Reply,P,Bytes))))return false;
        if(Q && R->NativeAsking){const xredisaskingresult* A=(const xredisaskingresult*)Q;
            if(A->Control.Reply && !__xredisClusterReplyOutput(A->Control.Reply,P,Bytes))return false;}
    }
    return true;
}
XREDIS_API bool xredisClusterRouteConfigInit(xredisclusterrouteconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xredisRouteError(XERR_ARGUMENT,"invalid Cluster route config output");
    *C=(xredisclusterrouteconfig){sizeof(*C),8,1024,1024,1024u*1024u,1024,2u*1024u*1024u,5000,5,false,NULL,NULL};return true;
}
bool __xredisClusterRouteConfig(const xredisclusterrouteconfig* C)
{
    return C->Size==sizeof(*C) && C->GetPool && C->MaxRedirects<=1024 && C->MaxWaits<=65536 && C->MaxArguments && C->MaxArguments<=65536 &&
        C->MaxCommandBytes && C->MaxCommandBytes<=64u*1024u*1024u && C->MaxHostBytes && C->MaxHostBytes<=65536 &&
        C->MaxStorageBytes && C->MaxStorageBytes<=128u*1024u*1024u && C->Timeout>=0 && C->Timeout<=86400000 && C->RetryDelay>0 && C->RetryDelay<=60000;
}
size_t __xredisClusterRouteBaseBytes(void){return sizeof(xredisclusterroute);}
xredisadmission __xredisClusterRouteCreate(const xredisclustertopology* T,xbytesview Origin,uint16 Slot,
    const xbytesview* A,size_t Count,const xredisclusterrouteconfig* Config,xredisclusterroute** Out,double Entered,xredisclusterroutestatus* Failure)
{
    if(Failure)*Failure=XREDIS_CLUSTER_ROUTE_MEMORY;
    xredisclusterrouteconfig C;
    if(!T || !Out || !__xredisClusterTopologyOutput(T,Out,sizeof(*Out)) || Slot>=XREDIS_CLUSTER_SLOTS ||
        !Config || !xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out))){
        xredisRouteError(XERR_ARGUMENT,"invalid Cluster route topology/config/output");return XREDIS_ADMISSION_ERROR;}
    C=*Config;
    if(!__xredisClusterRouteConfig(&C) || !Count || Count>C.MaxArguments || !xrtMemRangeValid(A,Count*sizeof(*A)) ||
        xrtMemRangesOverlap(A,Count*sizeof(*A),Out,sizeof(*Out)) || !xrtMemRangeValid(Origin.Data,Origin.Size) ||
        xrtMemRangesOverlap(Origin.Data,Origin.Size,Out,sizeof(*Out))){xredisRouteError(XERR_ARGUMENT,"invalid Cluster route bounds/input");return XREDIS_ADMISSION_ERROR;}
    size_t Wire=0,Payload=0;if(!xredisCommandSize(A,Count,&Wire) || Wire>C.MaxCommandBytes){xredisRouteError(XERR_RANGE,"Cluster route command limit");return XREDIS_ADMISSION_ERROR;}
    for(size_t I=0;I<Count;++I){
        if(xrtMemRangesOverlap(A[I].Data,A[I].Size,Out,sizeof(*Out)) || A[I].Size>C.MaxCommandBytes-Payload){xredisRouteError(XERR_ARGUMENT,"Cluster route payload/output overlap");return XREDIS_ADMISSION_ERROR;}
        Payload+=A[I].Size;
    }
    xredisclusternode N={0};xredisclusterstatus Found=xredisClusterTopologyLookup(T,Slot,&N);
    if(Found<0)return XREDIS_ADMISSION_ERROR;
    xredisclustertopologyinfo TI={0};TI.Size=sizeof(TI);if(!xredisClusterTopologyInfo(T,&TI))return XREDIS_ADMISSION_ERROR;
    xbytesview Host=N.Endpoint==XREDIS_CLUSTER_ENDPOINT_SOURCE?Origin:N.Host;
    uint16 Port=TI.Format==XREDIS_CLUSTER_LEGACY_SLOTS?N.Port:C.Tls?N.TlsPort:N.Port;
    bool Eligible=Found==XREDIS_CLUSTER_VALUE && N.Role==XREDIS_CLUSTER_MASTER && (N.Health==XREDIS_CLUSTER_ONLINE || N.Health==XREDIS_CLUSTER_UNREPORTED) &&
        N.Endpoint!=XREDIS_CLUSTER_ENDPOINT_UNKNOWN && Port && Host.Size<=C.MaxHostBytes && xredisRouteHost(Host);
    size_t HostCapacity=Eligible?Host.Size:1;
    size_t Bytes=sizeof(xredisclusterroute)+Count*sizeof(*A)+Payload+2*HostCapacity;
    if(Bytes>C.MaxStorageBytes){if(Failure)*Failure=XREDIS_CLUSTER_ROUTE_LIMIT;xredisRouteError(XERR_RANGE,"Cluster route storage limit");return XREDIS_ADMISSION_ERROR;}
    xredisclusterroute* R=xrtCalloc(1,Bytes);if(!R)return XREDIS_ADMISSION_ERROR;
    R->Config=C;R->Entered=Entered;R->Deadline=xredisRouteDue(Entered,C.Timeout);R->StorageBytes=Bytes;R->Count=Count;R->Arguments=(xbytesview*)(R+1);
    unsigned char* P=(unsigned char*)(R->Arguments+Count);
    for(size_t I=0;I<Count;++I){R->Arguments[I]=(xbytesview){P,A[I].Size};if(A[I].Size)memcpy(P,A[I].Data,A[I].Size);P+=A[I].Size;}
    R->Host=P;R->HostCapacity=HostCapacity;R->AttemptHost=P+HostCapacity;R->Result.Size=sizeof(R->Result);R->Result.StorageBytes=Bytes;R->Result.Address.Slot=Slot;R->Result.Address.Tls=C.Tls;R->Result.Admission=XREDIS_ADMISSION_ACCEPTED;
    if(!Eligible)xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_NO_ROUTE);
    else {memcpy(R->Host,Host.Data,Host.Size);R->Result.Address.Host=(xbytesview){R->Host,Host.Size};R->Result.Address.Port=Port;R->Due=R->Deadline;}
    if(R->Phase!=XCR_DONE && xrtTimer()>=R->Deadline)xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_TIMEOUT);
    *Out=R;return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisadmission xredisClusterRouteCreate(const xredisclustertopology* T,xbytesview Origin,uint16 Slot,
    const xbytesview* A,size_t Count,const xredisclusterrouteconfig* Config,xredisclusterroute** Out)
{return __xredisClusterRouteCreate(T,Origin,Slot,A,Count,Config,Out,xrtTimer(),NULL);}
xredisadmission __xredisClusterRouteRetryMissing(xredisclusterroute* R,const xredisclustertopology* T,
    xbytesview Origin,const xredisclusterrouteconfig* C,xredisclusterroute** Out,xredisclusterroutestatus* Failure)
{
    if(!R || R->Result.Status!=XREDIS_CLUSTER_ROUTE_NO_ROUTE || R->Result.Attempts){xredisRouteError(XERR_STATE,"only unsubmitted missing routes can restart");return XREDIS_ADMISSION_ERROR;}
    return __xredisClusterRouteCreate(T,Origin,R->Result.Address.Slot,R->Arguments,R->Count,C,Out,R->Entered,Failure);
}
static void xredisRouteWait(xredisclusterroute* R)
{
    if(R->Result.Waits>=R->Config.MaxWaits){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_OVERLOADED);return;}
    ++R->Result.Waits;R->Due=fmin(R->Deadline,xredisRouteDue(xrtTimer(),R->Config.RetryDelay));R->Phase=XCR_BACKOFF;
}
static void xredisRouteResolve(xredisclusterroute* R)
{
    double Remaining=(R->Deadline-xrtTimer())*1000.;int64 Timeout=Remaining<=0?0:Remaining>=86400000.?86400000:(int64)ceil(Remaining);
    xredispool* Pool=NULL;++R->Result.ProviderCalls;
    R->Result.Provider=R->Config.GetPool(&R->Result.Address,Timeout,R->Config.Data,&Pool);
    if(xrtTimer()>=R->Deadline){if(R->Result.Provider==XREDIS_CLUSTER_POOL_READY && Pool)xredisPoolRelease(Pool);xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_TIMEOUT);return;}
    if(R->Result.Provider!=XREDIS_CLUSTER_POOL_READY){
        if(R->Result.Provider==XREDIS_CLUSTER_POOL_WAIT)xredisRouteWait(R);
        else xredisRouteFinish(R,R->Result.Provider==XREDIS_CLUSTER_POOL_DENIED?XREDIS_CLUSTER_ROUTE_POLICY:
            R->Result.Provider==XREDIS_CLUSTER_POOL_CLOSED?XREDIS_CLUSTER_ROUTE_CLOSED:
            R->Result.Provider==XREDIS_CLUSTER_POOL_MEMORY?XREDIS_CLUSTER_ROUTE_MEMORY:XREDIS_CLUSTER_ROUTE_PROVIDER_ERROR);
        return;
    }
    if(!Pool){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_PROVIDER_ERROR);return;}
    bool Asking=R->Result.Address.Reason==XREDIS_CLUSTER_ASK;xfuture* F=NULL;
    R->Result.AdmissionObserved=true;
    R->Result.Admission=Asking?__xredisSharedPoolAsking(Pool,R->Arguments,R->Count,R->Config.Timeout,&F,R->Entered,false):
        __xredisSharedPoolCall(Pool,R->Arguments,R->Count,R->Config.Timeout,&F,R->Entered,false);
    xredisPoolRelease(Pool);
    if(R->Result.Admission==XREDIS_ADMISSION_ACCEPTED){
        xrtFutureDestroy(R->Future);R->Future=F;R->NativeAsking=Asking;R->Result.UsedAsking=Asking;
        R->Result.AttemptAddress=R->Result.Address;
        memcpy(R->AttemptHost,R->Host,R->Result.Address.Host.Size);
        R->Result.AttemptAddress.Host.Data=R->AttemptHost;
        ++R->Result.Attempts;R->Phase=XCR_NATIVE;R->Due=R->Deadline;
        if(xrtTimer()>=R->Deadline)xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_TIMEOUT);
    }else if(xrtTimer()>=R->Deadline)xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_TIMEOUT);
    else if(R->Result.Admission==XREDIS_ADMISSION_OVERLOADED)xredisRouteWait(R);
    else xredisRouteFinish(R,R->Result.Admission==XREDIS_ADMISSION_CLOSED?XREDIS_CLUSTER_ROUTE_CLOSED:XREDIS_CLUSTER_ROUTE_ADMISSION_ERROR);
}
static void xredisRouteReply(xredisclusterroute* R)
{
    if(xrtFutureState(R->Future)!=XFUTURE_RESOLVED || !xrtFutureValue(R->Future)){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_PROTOCOL);return;}
    const xredisrequestresult* Q=xrtFutureValue(R->Future);
    if(R->NativeAsking){const xredisaskingresult* A=(const xredisaskingresult*)Q;
        if(!A->ControlReceived || A->Control.Status!=XREDIS_REQUEST_OK){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_COMPLETE);return;}}
    if(Q->Status!=XREDIS_REQUEST_SERVER_ERROR || Q->Delivery!=XREDIS_REPLIED || !Q->Reply){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_COMPLETE);return;}
    xredisclusterredirect E;xredisclusterstatus S=xredisClusterRedirectRead(Q->Reply,R->Config.MaxHostBytes,&E);
    if(S==XREDIS_CLUSTER_ABSENT){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_COMPLETE);return;}
    if(S!=XREDIS_CLUSTER_VALUE || E.Slot!=R->Result.Address.Slot){xredisRouteFinish(R,S==XREDIS_CLUSTER_LIMIT?XREDIS_CLUSTER_ROUTE_LIMIT:XREDIS_CLUSTER_ROUTE_PROTOCOL);return;}
    if(E.Kind==XREDIS_CLUSTER_MOVED)++R->Result.Moved;else ++R->Result.Asks;
    if(R->Result.Redirects>=R->Config.MaxRedirects){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_LIMIT);return;}
    if(E.Endpoint==XREDIS_CLUSTER_ENDPOINT_UNKNOWN || (E.Endpoint==XREDIS_CLUSTER_ENDPOINT_HOST && !xredisRouteHost(E.Host))){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_POLICY);return;}
    if(E.Endpoint==XREDIS_CLUSTER_ENDPOINT_HOST){
        if(E.Host.Size>R->HostCapacity){
            size_t Bytes=2*R->Config.MaxHostBytes;
            if(Bytes>R->Config.MaxStorageBytes-R->StorageBytes){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_LIMIT);return;}
            unsigned char* Host=xrtMalloc(Bytes);if(!Host){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_MEMORY);return;}
            memcpy(Host,R->Host,R->Result.Address.Host.Size);
            memcpy(Host+R->Config.MaxHostBytes,R->AttemptHost,R->Result.AttemptAddress.Host.Size);
            R->HostHeap=Host;R->Host=Host;R->AttemptHost=Host+R->Config.MaxHostBytes;R->HostCapacity=R->Config.MaxHostBytes;
            R->Result.StorageBytes=R->StorageBytes+Bytes;
            R->Result.Address.Host.Data=R->Host;R->Result.AttemptAddress.Host.Data=R->AttemptHost;
        }
        memmove(R->Host,E.Host.Data,E.Host.Size);R->Result.Address.Host.Size=E.Host.Size;
    }
    R->Result.Address.Port=E.Port;R->Result.Address.Reason=E.Kind;++R->Result.Redirects;R->Phase=XCR_RESOLVE;
}
XREDIS_API xredisclusterroutestep xredisClusterRouteStep(xredisclusterroute* R)
{
    if(!R){xredisRouteError(XERR_ARGUMENT,"Cluster route is NULL");return XREDIS_CLUSTER_ROUTE_STEP_ERROR;}
    if(R->Phase==XCR_DONE)return XREDIS_CLUSTER_ROUTE_DONE;
    if(xrtTimer()>=R->Deadline){xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_TIMEOUT);return XREDIS_CLUSTER_ROUTE_DONE;}
    if(R->Phase==XCR_BACKOFF){if(xrtTimer()<R->Due)return XREDIS_CLUSTER_ROUTE_WAIT;R->Phase=XCR_RESOLVE;}
    else if(R->Phase==XCR_NATIVE){if(xrtFutureState(R->Future)==XFUTURE_PENDING)return XREDIS_CLUSTER_ROUTE_WAIT;xredisRouteReply(R);}
    else xredisRouteResolve(R);
    return R->Phase==XCR_DONE?XREDIS_CLUSTER_ROUTE_DONE:R->Phase==XCR_BACKOFF ||
        (R->Phase==XCR_NATIVE && xrtFutureState(R->Future)==XFUTURE_PENDING)?XREDIS_CLUSTER_ROUTE_WAIT:XREDIS_CLUSTER_ROUTE_MORE;
}
XREDIS_API xfuture* xredisClusterRoutePending(xredisclusterroute* R)
{if(!R){xredisRouteError(XERR_ARGUMENT,"Cluster route is NULL");return NULL;}return R->Phase==XCR_NATIVE?xrtFutureRef(R->Future):NULL;}
XREDIS_API double xredisClusterRouteNextWake(xredisclusterroute* R)
{if(!R){xredisRouteError(XERR_ARGUMENT,"Cluster route is NULL");return 0;}return R->Phase==XCR_DONE?0:(R->Phase==XCR_RESOLVE || (R->Phase==XCR_NATIVE && xrtFutureState(R->Future)!=XFUTURE_PENDING))?fmin(R->Deadline,xrtTimer()):R->Phase==XCR_BACKOFF?R->Due:R->Deadline;}
XREDIS_API bool xredisClusterRouteResult(xredisclusterroute* R,xredisclusterrouteresult* Out)
{if(!xredisRouteOutput(R,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out))return xredisRouteError(XERR_ARGUMENT,"invalid Cluster route result output");*Out=R->Result;return true;}
XREDIS_API xfuture* xredisClusterRouteResultFuture(xredisclusterroute* R)
{if(!R){xredisRouteError(XERR_ARGUMENT,"Cluster route is NULL");return NULL;}return R->Phase==XCR_DONE && R->Future?xrtFutureRef(R->Future):NULL;}
XREDIS_API bool xredisClusterRouteCancel(xredisclusterroute* R)
{if(!R)return xredisRouteError(XERR_ARGUMENT,"Cluster route is NULL");xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_CANCELLED);return true;}
XREDIS_API void xredisClusterRouteDestroy(xredisclusterroute* R)
{if(!R)return;if(R->Phase!=XCR_DONE)xredisRouteFinish(R,XREDIS_CLUSTER_ROUTE_CANCELLED);xrtFutureDestroy(R->Future);xrtFree(R->HostHeap);xrtFree(R);}
#endif
