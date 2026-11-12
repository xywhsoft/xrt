#include <xredis/cluster_client.h>
#include "../internal/cluster_route_internal.h"
#include "../internal/cluster_refresh_internal.h"
#include <xrt/memory.h>
#include <math.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT)
typedef struct xredisclusteroperation xredisclusteroperation;
typedef struct xredisclusterwatch {
    xfuturewatch Watch;xfuture* Future;xredisclusterclient* Client;
    xredisclusteroperation* Operation;bool Active;
} xredisclusterwatch;
struct xredisclusteroperation {
    xredisclusterclientresult Result;xredisclusterclient* Client;
    xredisclusteroperation *Next,*Previous,*ReadyNext,*ReadyPrevious,*MapNext,*MapPrevious;
    xredisclusterroute* Route;xredisclusterwatch Watch;
    xfuture* Future;xpromise* Promise;xcancel* Token;xcancelwatch* Cancel;
    xbytesview* Arguments;size_t Count,Wire,Reservation,Heap;
    int64 Timeout;uint16 Slot;double Entered,Deadline,Due,Started;
    size_t Moved;bool Queued,WaitingMap,MissingTried,Stopping,Publishing;
};
#define XREDIS_CLUSTER_OPERATION_PREFIX ((sizeof(xredisclusteroperation)+_Alignof(max_align_t)-1)/_Alignof(max_align_t)*_Alignof(max_align_t))
struct xredisclusterclient {
    xatomic32 References;xmutex Gate;xnetengine* Engine;xnetworker* Worker;xnetpost Post;
    xredisclusterclientconfig Config;xredisclusterclientinfo Info;
    xredisclusternodes* Nodes;xredisclusterrefresh* Refresh;xredisclustermap* Map;
    xredisclusteroperation *All,*ReadyHead,*ReadyTail,*MapHead,**Heap;size_t HeapCount,Bytes;
    xredisclusterwatch Metadata;xfuture* RefreshFuture;
    xpromise *ReadyPromise,*ClosePromise;xfuture *ReadyFuture,*ClosedFuture;
    xredisclusterclientready *ReadyValue,*CloseValue;
    uint64 Timer,InvalidatedGeneration;double TimerDue,RefreshDue;int64 Backoff;
    bool Posted,Finished,Invalidation;
};
static void xredisClusterWork(xnetworker*,ptr);
static void xredisClusterStop(xredisclusterclient*,xredisclusterclientstatus);
static bool xredisClusterClientError(xerrkind K,cstr T)
{xrtSetErrorInfo(K,"xrt.redis",K==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,T);return false;}
static double xredisClusterDeadline(double E,int64 T)
{return T==0?E:nextafter(E+(double)(T/1000)+(double)(T%1000)*.001,INFINITY);}
XREDIS_API bool xredisClusterClientRetain(xredisclusterclient* C)
{if(!C)return xredisClusterClientError(XERR_ARGUMENT,"Cluster client is NULL");xrtAtomic32FetchAdd(&C->References,1,XMEMORY_RELAXED);return true;}
XREDIS_API void xredisClusterClientRelease(xredisclusterclient* C)
{
    if(!C || xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrtPromiseDestroy(C->ReadyPromise);xrtPromiseDestroy(C->ClosePromise);xrtFutureDestroy(C->ReadyFuture);xrtFutureDestroy(C->ClosedFuture);
    xrtFree(C->ReadyValue);xrtFree(C->CloseValue);xrtMutexUnit(&C->Gate);xrtFree(C);
}
static void xredisClusterWakeLocked(xredisclusterclient* C)
{
    if(C->Finished || C->Posted)return;
    C->Posted=true;xredisClusterClientRetain(C);
    if(!xrtNetPost(C->Worker,&C->Post,xredisClusterWork,C)){C->Posted=false;xredisClusterClientRelease(C);}
}
static void xredisClusterWake(xredisclusterclient* C)
{xrtMutexLock(&C->Gate);xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);}
static void xredisClusterQueueLocked(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(O->Queued || O->Publishing)return;
    O->Queued=true;O->ReadyPrevious=C->ReadyTail;O->ReadyNext=NULL;
    if(C->ReadyTail)C->ReadyTail->ReadyNext=O;else C->ReadyHead=O;C->ReadyTail=O;xredisClusterWakeLocked(C);
}
static void xredisClusterUnqueueLocked(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(!O->Queued)return;
    if(O->ReadyPrevious)O->ReadyPrevious->ReadyNext=O->ReadyNext;else C->ReadyHead=O->ReadyNext;
    if(O->ReadyNext)O->ReadyNext->ReadyPrevious=O->ReadyPrevious;else C->ReadyTail=O->ReadyPrevious;
    O->Queued=false;O->ReadyNext=O->ReadyPrevious=NULL;
}
static void xredisClusterWatchNotify(ptr Data){(void)Data;}
static void xredisClusterWatchDrop(ptr Data)
{
    xredisclusterwatch* W=Data;xredisclusterclient* C=W->Client;
    xrtMutexLock(&C->Gate);W->Active=false;if(W->Operation)xredisClusterQueueLocked(C,W->Operation);else xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);
    xredisClusterClientRelease(C);
}
static bool xredisClusterWatchActive(xredisclusterwatch* W)
{xredisclusterclient* C=W->Client;xrtMutexLock(&C->Gate);bool Active=W->Active;xrtMutexUnlock(&C->Gate);return Active;}
static bool xredisClusterWatchStop(xredisclusterwatch* W)
{
    if(!W->Future)return true;
    if(xredisClusterWatchActive(W)){
        (void)xrtFutureWatchDetach(W->Future,&W->Watch);
        if(xredisClusterWatchActive(W))return false;
    }
    xrtFutureDestroy(W->Future);W->Future=NULL;return true;
}
/* Supplied Future reference always transfers, including failed registration. */
static bool xredisClusterWatchStart(xredisclusterwatch* W,xfuture* F)
{
    xredisclusterclient* C=W->Client;W->Future=F;
    if(!F || !xrtFutureWatchInit(&W->Watch,xredisClusterWatchNotify,xredisClusterWatchDrop,W))return false;
    xredisClusterClientRetain(C);xrtMutexLock(&C->Gate);W->Active=true;xrtMutexUnlock(&C->Gate);
    xfuturewatchresult R=xrtFutureWatchAdd(F,&W->Watch);
    if(R!=XFUTURE_WATCH_PENDING){xrtMutexLock(&C->Gate);W->Active=false;if(R==XFUTURE_WATCH_READY){if(W->Operation)xredisClusterQueueLocked(C,W->Operation);else xredisClusterWakeLocked(C);}xrtMutexUnlock(&C->Gate);xredisClusterClientRelease(C);}
    return R!=XFUTURE_WATCH_ERROR;
}
static void xredisClusterHeapSwap(xredisclusterclient* C,size_t A,size_t B)
{xredisclusteroperation* O=C->Heap[A];C->Heap[A]=C->Heap[B];C->Heap[B]=O;C->Heap[A]->Heap=A;O->Heap=B;}
static void xredisClusterHeapFix(xredisclusterclient* C,size_t I)
{
    while(I && C->Heap[I]->Due<C->Heap[(I-1)/2]->Due){size_t P=(I-1)/2;xredisClusterHeapSwap(C,I,P);I=P;}
    for(;;){size_t L=I*2+1;if(L>=C->HeapCount)break;size_t R=L+1,J=R<C->HeapCount && C->Heap[R]->Due<C->Heap[L]->Due?R:L;if(C->Heap[I]->Due<=C->Heap[J]->Due)break;xredisClusterHeapSwap(C,I,J);I=J;}
}
static void xredisClusterHeapRemove(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(O->Heap==SIZE_MAX)return;
    size_t I=O->Heap;O->Heap=SIZE_MAX;--C->HeapCount;
    if(I<C->HeapCount){C->Heap[I]=C->Heap[C->HeapCount];C->Heap[I]->Heap=I;xredisClusterHeapFix(C,I);}
}
static void xredisClusterHeapSet(xredisclusterclient* C,xredisclusteroperation* O,double Due)
{O->Due=Due;if(O->Heap==SIZE_MAX){O->Heap=C->HeapCount;C->Heap[C->HeapCount++]=O;}xredisClusterHeapFix(C,O->Heap);}
static void xredisClusterMapWaitRemove(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(!O->WaitingMap)return;
    if(O->MapPrevious)O->MapPrevious->MapNext=O->MapNext;else C->MapHead=O->MapNext;
    if(O->MapNext)O->MapNext->MapPrevious=O->MapPrevious;
    O->MapNext=O->MapPrevious=NULL;O->WaitingMap=false;
}
static void xredisClusterMapWait(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(!O->WaitingMap){O->MapNext=C->MapHead;if(C->MapHead)C->MapHead->MapPrevious=O;C->MapHead=O;O->WaitingMap=true;}
    xredisClusterHeapSet(C,O,O->Deadline);
}
static bool xredisClusterClosing(xredisclusterclient* C)
{xrtMutexLock(&C->Gate);bool V=C->Info.Closing;xrtMutexUnlock(&C->Gate);return V;}
static xredisclusterpoolstatus xredisClusterProvider(const xredisclusteraddress* A,int64 T,ptr Data,xredispool** Out)
{
    xredisclusteroperation* O=Data;xredisclusterclient* C=O->Client;xredispool* P=NULL;
    if(xredisClusterClosing(C))return XREDIS_CLUSTER_POOL_CLOSED;
    xredisclusterpoolstatus S=xredisClusterNodesGetPool(A,T,C->Nodes,&P);
    if(xredisClusterClosing(C)){if(P)xredisPoolRelease(P);return XREDIS_CLUSTER_POOL_CLOSED;}
    if(S==XREDIS_CLUSTER_POOL_READY)*Out=P;
    return S;
}
static xredisclusterrouteconfig xredisClusterRouteConfigFor(xredisclusterclient* C,xredisclusteroperation* O,bool Tls)
{xredisclusterrouteconfig K=C->Config.Route;K.GetPool=xredisClusterProvider;K.Data=O;K.Tls=Tls;if(O)K.Timeout=O->Timeout;return K;}
static void xredisClusterInvalidateGeneration(xredisclusterclient* C,uint64 Generation)
{
    xrtMutexLock(&C->Gate);bool Needed=Generation==C->Info.Generation && Generation!=C->InvalidatedGeneration;
    if(Needed){C->InvalidatedGeneration=Generation;C->Invalidation=true;C->Info.Dirty=true;xredisClusterWakeLocked(C);}xrtMutexUnlock(&C->Gate);
}
static void xredisClusterOperationFree(ptr Value,ptr Data)
{
    (void)Data;xredisclusteroperation* O=Value;
    if(O->Route==(xredisclusterroute*)((unsigned char*)O+XREDIS_CLUSTER_OPERATION_PREFIX))__xredisClusterRouteUnit(O->Route);else xredisClusterRouteDestroy(O->Route);
    xrtFutureDestroy(O->Result.Native);xrtFree(O->Arguments);xrtFree(O);
}
static void xredisClusterCancelled(ptr Data)
{xredisclusteroperation* O=Data;xredisclusterclient* C=O->Client;xrtMutexLock(&C->Gate);xredisClusterQueueLocked(C,O);xrtMutexUnlock(&C->Gate);}
static void xredisClusterComplete(xredisclusterclient* C,xredisclusteroperation* O)
{
    if(!xredisClusterWatchStop(&O->Watch)){xredisClusterHeapRemove(C,O);return;}
    xredisClusterMapWaitRemove(C,O);xredisClusterHeapRemove(C,O);
    if(O->Route){O->Result.Route.Size=sizeof(O->Result.Route);O->Result.RouteObserved=xredisClusterRouteResult(O->Route,&O->Result.Route);O->Result.Native=xredisClusterRouteResultFuture(O->Route);}
    O->Result.TotalSeconds=fmax(0,xrtTimer()-O->Entered);O->Result.QueueSeconds=fmax(0,(O->Started?O->Started:xrtTimer())-O->Entered);
    xrtMutexLock(&C->Gate);O->Publishing=true;xredisClusterUnqueueLocked(C,O);
    if(O->Previous)O->Previous->Next=O->Next;else C->All=O->Next;if(O->Next)O->Next->Previous=O->Previous;
    --C->Info.Requests;C->Info.CommandBytes-=O->Wire;C->Info.StorageBytes-=O->Reservation;++C->Info.Completed;O->Result.RefreshStatus=C->Info.LastRefresh;O->Result.RefreshObserved=C->Info.RefreshCompleted!=0;xrtMutexUnlock(&C->Gate);
    xrtCancelUnwatch(O->Cancel);O->Cancel=NULL;xrtCancelDestroy(O->Token);O->Token=NULL;
    xrtFutureDestroy(O->Future);O->Future=NULL;xpromise* P=O->Promise;O->Promise=NULL;O->Client=NULL;O->Watch.Client=NULL;xrtFree(O->Arguments);O->Arguments=NULL;
    if(!xrtPromiseResolveOwned(P,O,xredisClusterOperationFree,NULL))xredisClusterOperationFree(O,NULL);
    xrtPromiseDestroy(P);
}
static void xredisClusterStopOperation(xredisclusterclient* C,xredisclusteroperation* O,xredisclusterclientstatus Status)
{if(!O->Stopping){O->Stopping=true;O->Result.Status=Status;if(O->Route)xredisClusterRouteCancel(O->Route);}xredisClusterComplete(C,O);}
static void xredisClusterOperationStep(xredisclusterclient* C,xredisclusteroperation* O)
{
    xredisClusterHeapRemove(C,O);
    if(O->Stopping){xredisClusterComplete(C,O);return;}
    if(xredisClusterClosing(C)){xredisClusterStopOperation(C,O,C->Info.StopStatus);return;}
    if(xrtCancelRequested(O->Token)){xredisClusterStopOperation(C,O,XREDIS_CLUSTER_CLIENT_CANCELLED);return;}
    if(xrtTimer()>=O->Deadline){xredisClusterStopOperation(C,O,XREDIS_CLUSTER_CLIENT_TIMEOUT);return;}
    if(O->Watch.Future){if(xredisClusterWatchActive(&O->Watch) && xrtFutureState(O->Watch.Future)==XFUTURE_PENDING){xredisClusterHeapSet(C,O,O->Deadline);return;}if(!xredisClusterWatchStop(&O->Watch))return;}
    if(!O->Started)O->Started=xrtTimer();
    if(!O->Route || O->WaitingMap){
        if(!C->Map){xredisClusterMapWait(C,O);return;}
        xredisclustermapinfo M={0};M.Size=sizeof(M);xredisClusterMapInfo(C->Map,&M);
        if(O->WaitingMap && O->Route && M.Generation<=O->Result.Generation){xredisClusterMapWait(C,O);return;}
        xredisclusterrouteconfig K=xredisClusterRouteConfigFor(C,O,M.Origin.Tls);xredisclusterroute* R=NULL;
        xredisclusterroutestatus Failure=XREDIS_CLUSTER_ROUTE_MEMORY;
        xredisadmission A=O->Route?__xredisClusterRouteRetryMissing(O->Route,M.Topology,M.Origin.Host,&K,&R,&Failure):__xredisClusterRouteCreate(M.Topology,M.Origin.Host,O->Slot,O->Arguments,O->Count,&K,&R,O->Entered,&Failure);
        if(A!=XREDIS_ADMISSION_ACCEPTED){xredisClusterStopOperation(C,O,Failure==XREDIS_CLUSTER_ROUTE_LIMIT?XREDIS_CLUSTER_CLIENT_LIMIT:XREDIS_CLUSTER_CLIENT_MEMORY);return;}
        xredisClusterMapWaitRemove(C,O);
        if(O->Route==(xredisclusterroute*)((unsigned char*)O+XREDIS_CLUSTER_OPERATION_PREFIX))__xredisClusterRouteUnit(O->Route);else xredisClusterRouteDestroy(O->Route);
        O->Route=R;O->Result.Generation=M.Generation;xrtFree(O->Arguments);O->Arguments=NULL;
    }
    xredisclusterroutestep S=xredisClusterRouteStep(O->Route);xredisclusterrouteresult R={0};R.Size=sizeof(R);xredisClusterRouteResult(O->Route,&R);
    if(R.Moved>O->Moved){xrtMutexLock(&C->Gate);C->Info.Moved+=R.Moved-O->Moved;xrtMutexUnlock(&C->Gate);O->Moved=R.Moved;xredisClusterInvalidateGeneration(C,O->Result.Generation);}
    if(xredisClusterClosing(C)){xredisClusterStopOperation(C,O,C->Info.StopStatus);return;}
    if(xrtTimer()>=O->Deadline && (S!=XREDIS_CLUSTER_ROUTE_DONE || R.Status==XREDIS_CLUSTER_ROUTE_COMPLETE)){xredisClusterStopOperation(C,O,XREDIS_CLUSTER_CLIENT_TIMEOUT);return;}
    if(S==XREDIS_CLUSTER_ROUTE_DONE){
        if(R.Status==XREDIS_CLUSTER_ROUTE_NO_ROUTE && !O->MissingTried){O->MissingTried=true;xredisClusterInvalidateGeneration(C,O->Result.Generation);xredisClusterMapWait(C,O);return;}
        O->Stopping=true;O->Result.Status=R.Status==XREDIS_CLUSTER_ROUTE_TIMEOUT?XREDIS_CLUSTER_CLIENT_TIMEOUT:XREDIS_CLUSTER_CLIENT_COMPLETE;xredisClusterComplete(C,O);return;
    }
    if(S==XREDIS_CLUSTER_ROUTE_STEP_ERROR){xredisClusterStopOperation(C,O,XREDIS_CLUSTER_CLIENT_INTERNAL);return;}
    double Due=xredisClusterRouteNextWake(O->Route);xredisClusterHeapSet(C,O,Due);
    if(S==XREDIS_CLUSTER_ROUTE_MORE){xrtMutexLock(&C->Gate);xredisClusterQueueLocked(C,O);xrtMutexUnlock(&C->Gate);}
    else {xfuture* F=xredisClusterRoutePending(O->Route);if(F && !xredisClusterWatchStart(&O->Watch,F))xredisClusterStopOperation(C,O,XREDIS_CLUSTER_CLIENT_INTERNAL);}
}
static void xredisClusterValueFree(ptr V,ptr Data){(void)Data;xrtFree(V);}
static void xredisClusterReady(xredisclusterclient* C,xredisclusterclientstatus S,uint64 G,uint64 E)
{
    if(!C->ReadyPromise)return;
    xpromise* P=C->ReadyPromise;C->ReadyPromise=NULL;xredisclusterclientready* V=C->ReadyValue;C->ReadyValue=NULL;*V=(xredisclusterclientready){S,G,E};
    if(!xrtPromiseResolveOwned(P,V,xredisClusterValueFree,NULL))xrtFree(V);
    xrtPromiseDestroy(P);
}
static void xredisClusterDriveRefresh(xredisclusterclient* C)
{
    xrtMutexLock(&C->Gate);bool Invalidate=C->Invalidation;C->Invalidation=false;xrtMutexUnlock(&C->Gate);
    if(Invalidate)xredisClusterRefreshInvalidate(C->Refresh);
    xredisclusterrefreshinfo I={0};I.Size=sizeof(I);xredisClusterRefreshInfo(C->Refresh,&I);
    if(!C->RefreshFuture && I.Dirty && xrtTimer()>=C->RefreshDue){
        if(xredisClusterRefreshStart(C->Refresh,false,&C->RefreshFuture)!=XREDIS_ADMISSION_ACCEPTED){xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_MEMORY);return;}
        xrtMutexLock(&C->Gate);++C->Info.Refreshes;xrtMutexUnlock(&C->Gate);
    }
    if(!C->RefreshFuture)return;
    if(C->Metadata.Future){if(xredisClusterWatchActive(&C->Metadata) && xrtFutureState(C->Metadata.Future)==XFUTURE_PENDING && xrtTimer()<xredisClusterRefreshNextWake(C->Refresh))return;if(!xredisClusterWatchStop(&C->Metadata))return;}
    xredisclusterrefreshstep S=xredisClusterRefreshStep(C->Refresh);
    if(S==XREDIS_REFRESH_DONE){
        const xredisclusterrefreshresult* R=xrtFutureValue(C->RefreshFuture);if(!R){xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_INTERNAL);return;}
        xredisclustermap* Map=NULL;bool Dirty=true;if(R->Published)xredisClusterRefreshSnapshot(C->Refresh,&Map,&Dirty);
        if(Map)(void)xredisClusterRefreshDiscover(C->Refresh,Map);
        xredisclustermap* Old=NULL;xrtMutexLock(&C->Gate);C->Info.LastRefresh=R->Status;++C->Info.RefreshCompleted;
        if(Map){Old=C->Map;C->Map=Map;C->Info.Generation=R->Generation;C->Info.Epoch=R->Epoch;C->Info.Dirty=Dirty || C->Invalidation;C->Backoff=C->Config.RefreshMin;}
        else {++C->Info.RefreshFailures;C->Info.Dirty=true;C->Backoff=C->Backoff>C->Config.RefreshRetryMax/2?C->Config.RefreshRetryMax:C->Backoff*2;}
        C->RefreshDue=xredisClusterDeadline(xrtTimer(),C->Backoff);C->Info.NextRefresh=C->Info.Dirty?C->RefreshDue:0;xrtMutexUnlock(&C->Gate);xredisClusterMapRelease(Old);
        if(Map){xredisClusterReady(C,XREDIS_CLUSTER_CLIENT_COMPLETE,R->Generation,R->Epoch);xrtMutexLock(&C->Gate);for(xredisclusteroperation* O=C->MapHead;O;O=O->MapNext)xredisClusterQueueLocked(C,O);xrtMutexUnlock(&C->Gate);}
        xrtFutureDestroy(C->RefreshFuture);C->RefreshFuture=NULL;
    }else if(S==XREDIS_REFRESH_MORE)xredisClusterWake(C);
    else if(S==XREDIS_REFRESH_WAIT){xfuture* F=xredisClusterRefreshPending(C->Refresh);if(F && !xredisClusterWatchStart(&C->Metadata,F))xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_INTERNAL);}
    else if(S==XREDIS_REFRESH_STEP_ERROR || S==XREDIS_REFRESH_BUSY)xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_INTERNAL);
}
static void xredisClusterTimer(xnetworker* W,uint64 Id,xnetresult R,ptr Data)
{(void)W;xredisclusterclient* C=Data;if(C->Timer==Id)C->Timer=0;if(R!=XNET_RESULT_OK && R!=XNET_RESULT_CANCELLED)xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_RESOURCE);xredisClusterWake(C);xredisClusterClientRelease(C);}
static bool xredisClusterArm(xredisclusterclient* C,double Due)
{
    if(C->Timer && C->TimerDue==Due)return true;
    if(C->Timer)xrtNetEngineTimerCancelCurrent(C->Engine,C->Timer);
    if(C->Timer)return false;
    if(Due==INFINITY)return true;
    double Ms=ceil(fmax(0,Due-xrtTimer())*1000);int64 Delay=Ms>=86400000?86400000:(int64)Ms;C->TimerDue=Due;xredisClusterClientRetain(C);
    C->Timer=xrtNetEngineAfter(C->Engine,xrtNetWorkerIndex(C->Worker),Delay,xredisClusterTimer,C);
    if(C->Timer)return true;
    xredisClusterClientRelease(C);return false;
}
static void xredisClusterFinish(xredisclusterclient* C)
{
    if(C->Timer || !xredisClusterWatchStop(&C->Metadata))return;
    xrtMutexLock(&C->Gate);if(C->Info.Requests || C->Info.APIs || C->Posted){xrtMutexUnlock(&C->Gate);return;}C->Finished=true;C->Info.Closed=true;xredisclustermap* M=C->Map;C->Map=NULL;xrtMutexUnlock(&C->Gate);
    xredisClusterReady(C,C->Info.StopStatus,0,0);xrtFutureDestroy(C->RefreshFuture);C->RefreshFuture=NULL;xredisClusterRefreshRelease(C->Refresh);C->Refresh=NULL;xredisClusterMapRelease(M);xredisClusterNodesRelease(C->Nodes);C->Nodes=NULL;
    xnetengine* E=C->Engine;C->Engine=NULL;C->Worker=NULL;xrtNetEngineUnpin(E);
    xpromise* P=C->ClosePromise;C->ClosePromise=NULL;xredisclusterclientready* V=C->CloseValue;C->CloseValue=NULL;*V=(xredisclusterclientready){C->Info.StopStatus,C->Info.Generation,C->Info.Epoch};
    if(!xrtPromiseResolveOwned(P,V,xredisClusterValueFree,NULL))xrtFree(V);
    xrtPromiseDestroy(P);xredisClusterClientRelease(C);
}
static void xredisClusterWork(xnetworker* W,ptr Data)
{
    (void)W;xredisclusterclient* C=Data;xrtMutexLock(&C->Gate);C->Posted=false;bool Closing=C->Info.Closing;xrtMutexUnlock(&C->Gate);
    if(Closing){xredisClusterRefreshClose(C->Refresh);(void)xredisClusterWatchStop(&C->Metadata);}else xredisClusterDriveRefresh(C);
    for(size_t N=0;N<C->Config.MaxSteps;++N){
        xrtMutexLock(&C->Gate);xredisclusteroperation* O=C->ReadyHead;if(O)xredisClusterUnqueueLocked(C,O);xrtMutexUnlock(&C->Gate);
        if(!O && C->HeapCount && C->Heap[0]->Due<=xrtTimer()){O=C->Heap[0];xredisClusterHeapRemove(C,O);}if(!O)break;xredisClusterOperationStep(C,O);
    }
    xrtMutexLock(&C->Gate);C->Info.HeapEntries=C->HeapCount;Closing=C->Info.Closing;bool Ready=C->ReadyHead!=NULL;xrtMutexUnlock(&C->Gate);
    if(Ready)xredisClusterWake(C);
    if(Closing){xredisClusterRefreshClose(C->Refresh);if(C->Timer)xrtNetEngineTimerCancelCurrent(C->Engine,C->Timer);xredisClusterFinish(C);}
    else {double Due=C->HeapCount?C->Heap[0]->Due:INFINITY;
        if(C->RefreshFuture)Due=fmin(Due,xredisClusterRefreshNextWake(C->Refresh));else {xrtMutexLock(&C->Gate);bool Dirty=C->Info.Dirty;xrtMutexUnlock(&C->Gate);if(Dirty)Due=fmin(Due,C->RefreshDue);}
        if(!xredisClusterArm(C,Due)){xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_RESOURCE);}}
    xredisClusterClientRelease(C);
}
XREDIS_API bool xredisClusterClientConfigInit(xredisclusterclientconfig* K)
{
    if(!xrtMemRangeValid(K,sizeof(*K)))return xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client config");
    *K=(xredisclusterclientconfig){0};K->Size=sizeof(*K);K->MaxRequests=1024;K->MaxCommandBytes=16u*1024u*1024u;K->MaxStorageBytes=64u*1024u*1024u;K->MaxSteps=64;K->RefreshMin=100;K->RefreshRetryMax=10000;
    return xredisClusterRefreshConfigInit(&K->Refresh) && xredisClusterRouteConfigInit(&K->Route);
}
XREDIS_API xredisadmission xredisClusterClientCreate(xnetengine* E,uint64 Affinity,xredisclusternodes* Nodes,const xredisclusteraddress* Seeds,size_t Count,const xredisclusterclientconfig* Config,xredisclusterclient** Out)
{
    if(!E || !Nodes || !xrtMemRangeValid(Config,sizeof(*Config)) || !xrtMemRangeValid(Out,sizeof(*Out)) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client create");return XREDIS_ADMISSION_ERROR;}
    xredisclusterclientconfig K=*Config;xredisclusterrouteconfig R=K.Route;R.GetPool=xredisClusterProvider;
    if(K.Size!=sizeof(K) || !K.MaxRequests || K.MaxRequests>65536 || !K.MaxCommandBytes || K.MaxCommandBytes>1024u*1024u*1024u || !K.MaxStorageBytes || K.MaxStorageBytes>1024u*1024u*1024u || !K.MaxSteps || K.MaxSteps>4096 || K.RefreshMin<=0 || K.RefreshRetryMax<K.RefreshMin || K.RefreshRetryMax>60000 || K.Route.GetPool || K.Route.Data || K.Route.Tls || !__xredisClusterRouteConfig(&R)){
        xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client bounds/profile");return XREDIS_ADMISSION_ERROR;}
    if(!Count || Count>1024 || !xrtMemRangeValid(Seeds,Count*sizeof(*Seeds)) || xrtMemRangesOverlap(Seeds,Count*sizeof(*Seeds),Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client seed/output");return XREDIS_ADMISSION_ERROR;}
    for(size_t I=0;I<Count;++I)if(xrtMemRangesOverlap(Seeds[I].Host.Data,Seeds[I].Host.Size,Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"Cluster client output aliases seed");return XREDIS_ADMISSION_ERROR;}
    if(!xrtNetEnginePin(E))return XREDIS_ADMISSION_ERROR;
    size_t Bytes=sizeof(xredisclusterclient)+K.MaxRequests*sizeof(xredisclusteroperation*);xredisclusterclient* C=xrtCalloc(1,Bytes);
    if(!C || !xrtMutexInit(&C->Gate)){xrtFree(C);xrtNetEngineUnpin(E);return XREDIS_ADMISSION_ERROR;}xrtAtomic32Init(&C->References,1);C->Bytes=Bytes;C->Heap=(xredisclusteroperation**)(C+1);C->Config=K;
    C->ReadyValue=xrtMalloc(sizeof(*C->ReadyValue));C->CloseValue=xrtMalloc(sizeof(*C->CloseValue));C->ReadyPromise=xrtPromiseCreate(&C->ReadyFuture,NULL);C->ClosePromise=xrtPromiseCreate(&C->ClosedFuture,NULL);
    if(!C->ReadyValue || !C->CloseValue || !C->ReadyPromise || !C->ClosePromise || !xrtNetPostInit(&C->Post) || xredisClusterRefreshCreate(Nodes,Seeds,Count,&K.Refresh,&C->Refresh)!=XREDIS_ADMISSION_ACCEPTED){xrtNetEngineUnpin(E);xredisClusterClientRelease(C);return XREDIS_ADMISSION_ERROR;}
    C->Engine=E;C->Worker=xrtNetEngineWorker(E,(uint32)(Affinity%xrtNetEngineWorkerCount(E)));C->Nodes=Nodes;xredisClusterNodesRetain(Nodes);C->Metadata.Client=C;C->Info.Size=sizeof(C->Info);C->Info.Dirty=true;C->Backoff=K.RefreshMin;
    xredisClusterClientRetain(C);xrtMutexLock(&C->Gate);*Out=C;xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;
}
static bool xredisClusterClientOutput(xredisclusterclient* C,const void* P,size_t N)
{return C && xrtMemRangeValid(P,N) && !xrtMemRangesOverlap(C,C->Bytes,P,N);}
XREDIS_API xredisadmission xredisClusterClientCall(xredisclusterclient* C,uint16 Slot,const xbytesview* A,size_t Count,int64 Timeout,xfuture** Out)
{
    double E=xrtTimer();if(!xredisClusterClientOutput(C,Out,sizeof(*Out)) || Slot>=XREDIS_CLUSTER_SLOTS || Timeout< -1 || Timeout>86400000 || !Count || Count>C->Config.Route.MaxArguments || !xrtMemRangeValid(A,Count*sizeof(*A)) || xrtMemRangesOverlap(A,Count*sizeof(*A),Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster call");return XREDIS_ADMISSION_ERROR;}
    if(Timeout==-1)Timeout=C->Config.Route.Timeout;
    size_t Wire=0,Payload=0;if(!xredisCommandSize(A,Count,&Wire) || Wire>C->Config.Route.MaxCommandBytes){xredisClusterClientError(XERR_RANGE,"Cluster call command limit");return XREDIS_ADMISSION_ERROR;}
    for(size_t I=0;I<Count;++I){if(xrtMemRangesOverlap(A[I].Data,A[I].Size,Out,sizeof(*Out))){xredisClusterClientError(XERR_ARGUMENT,"Cluster output aliases command");return XREDIS_ADMISSION_ERROR;}Payload+=A[I].Size;}
    size_t Inputs=Count*sizeof(*A)+Payload,Reservation=XREDIS_CLUSTER_OPERATION_PREFIX+2*(__xredisClusterRouteBaseBytes()+Inputs+4*C->Config.Route.MaxHostBytes);
    xrtMutexLock(&C->Gate);if(!__xredisClusterMapOutput(C->Map,Out,sizeof(*Out))){xrtMutexUnlock(&C->Gate);xredisClusterClientError(XERR_ARGUMENT,"Cluster output aliases published map");return XREDIS_ADMISSION_ERROR;}if(C->Info.Closing){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_CLOSED;}
    if(C->Info.Requests>=C->Config.MaxRequests || Wire>C->Config.MaxCommandBytes-C->Info.CommandBytes || Reservation>C->Config.MaxStorageBytes-C->Info.StorageBytes){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_OVERLOADED;}
    ++C->Info.Requests;++C->Info.APIs;C->Info.CommandBytes+=Wire;C->Info.StorageBytes+=Reservation;xredisclustermap* Map=C->Map;if(Map)xredisClusterMapRetain(Map);xrtMutexUnlock(&C->Gate);
    xredisclusteroperation* O=NULL;xredisclusterroute* Route=NULL;uint64 Generation=0;
    if(Map){xredisclustermapinfo M={0};M.Size=sizeof(M);xredisClusterMapInfo(Map,&M);xredisclusterrouteconfig K=xredisClusterRouteConfigFor(C,NULL,M.Origin.Tls);K.Timeout=Timeout;ptr Context=NULL;
        if(__xredisClusterRouteCreateContext(M.Topology,M.Origin.Host,Slot,A,Count,&K,XREDIS_CLUSTER_OPERATION_PREFIX,&Context,&Route,E)==XREDIS_ADMISSION_ACCEPTED){O=Context;Generation=M.Generation;}}
    else O=xrtCalloc(1,sizeof(*O));
    bool OK=O!=NULL;
    if(O){O->Client=C;O->Watch.Client=C;O->Watch.Operation=O;O->Heap=SIZE_MAX;O->Entered=E;O->Deadline=xredisClusterDeadline(E,Timeout);O->Slot=Slot;O->Timeout=Timeout;O->Count=Count;O->Wire=Wire;O->Reservation=Reservation;O->Result.Size=sizeof(O->Result);
        O->Route=Route;O->Result.Generation=Generation;
        O->Promise=xrtPromiseCreate(&O->Future,NULL);if(O->Promise)O->Token=xrtPromiseCancelToken(O->Promise);if(O->Token)O->Cancel=xrtCancelWatch(O->Token,xredisClusterCancelled,O);OK=O->Promise && O->Token && O->Cancel;
        if(OK && !Map){O->Arguments=xrtMalloc(Inputs);OK=O->Arguments!=NULL;if(OK){unsigned char* P=(unsigned char*)(O->Arguments+Count);for(size_t I=0;I<Count;++I){O->Arguments[I]=(xbytesview){P,A[I].Size};if(A[I].Size)memcpy(P,A[I].Data,A[I].Size);P+=A[I].Size;}}}
    }
    xredisClusterMapRelease(Map);xrtMutexLock(&C->Gate);--C->Info.APIs;bool Closing=C->Info.Closing;
    if(OK && !Closing){O->Next=C->All;if(C->All)C->All->Previous=O;C->All=O;++C->Info.Accepted;*Out=xrtFutureRef(O->Future);xredisClusterQueueLocked(C,O);xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;}
    --C->Info.Requests;C->Info.CommandBytes-=Wire;C->Info.StorageBytes-=Reservation;if(Closing)xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);
    if(O){xrtCancelUnwatch(O->Cancel);xrtCancelDestroy(O->Token);xrtPromiseDestroy(O->Promise);xrtFutureDestroy(O->Future);xredisClusterOperationFree(O,NULL);}return Closing?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ERROR;
}
XREDIS_API xfuture* xredisClusterClientReady(xredisclusterclient* C)
{if(!C){xredisClusterClientError(XERR_ARGUMENT,"Cluster client is NULL");return NULL;}return xrtFutureRef(C->ReadyFuture);}
XREDIS_API xfuture* xredisClusterClientClosed(xredisclusterclient* C)
{if(!C){xredisClusterClientError(XERR_ARGUMENT,"Cluster client is NULL");return NULL;}return xrtFutureRef(C->ClosedFuture);}
XREDIS_API bool xredisClusterClientInfo(xredisclusterclient* C,xredisclusterclientinfo* Out)
{if(!xredisClusterClientOutput(C,Out,sizeof(*Out)))return xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client info");xrtMutexLock(&C->Gate);if(!__xredisClusterMapOutput(C->Map,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out)){xrtMutexUnlock(&C->Gate);return xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client info output");}*Out=C->Info;xrtMutexUnlock(&C->Gate);return true;}
XREDIS_API xredisclusterstatus xredisClusterClientSnapshot(xredisclusterclient* C,xredisclustermap** Out,bool* Stale)
{
    if(!xredisClusterClientOutput(C,Out,sizeof(*Out)) || !xredisClusterClientOutput(C,Stale,sizeof(*Stale)) || xrtMemRangesOverlap(Out,sizeof(*Out),Stale,sizeof(*Stale)))return xredisClusterClientError(XERR_ARGUMENT,"invalid Cluster client snapshot"),XREDIS_CLUSTER_ERROR;
    xrtMutexLock(&C->Gate);if(!__xredisClusterMapOutput(C->Map,Out,sizeof(*Out)) || !__xredisClusterMapOutput(C->Map,Stale,sizeof(*Stale))){xrtMutexUnlock(&C->Gate);xredisClusterClientError(XERR_ARGUMENT,"Cluster snapshot output aliases map");return XREDIS_CLUSTER_ERROR;}if(!C->Map){xrtMutexUnlock(&C->Gate);return XREDIS_CLUSTER_ABSENT;}xredisClusterMapRetain(C->Map);*Out=C->Map;*Stale=C->Info.Dirty;xrtMutexUnlock(&C->Gate);return XREDIS_CLUSTER_VALUE;
}
XREDIS_API bool xredisClusterClientInvalidate(xredisclusterclient* C)
{if(!C)return xredisClusterClientError(XERR_ARGUMENT,"Cluster client is NULL");xrtMutexLock(&C->Gate);if(C->Info.Closing){xrtMutexUnlock(&C->Gate);return false;}C->Invalidation=true;C->Info.Dirty=true;xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);return true;}
static void xredisClusterStop(xredisclusterclient* C,xredisclusterclientstatus Status)
{xrtMutexLock(&C->Gate);if(!C->Info.Closing){C->Info.Closing=true;C->Info.StopStatus=Status;}for(xredisclusteroperation* O=C->All;O;O=O->Next)xredisClusterQueueLocked(C,O);xredisClusterWakeLocked(C);xrtMutexUnlock(&C->Gate);}
XREDIS_API bool xredisClusterClientClose(xredisclusterclient* C)
{if(!C)return xredisClusterClientError(XERR_ARGUMENT,"Cluster client is NULL");xredisClusterStop(C,XREDIS_CLUSTER_CLIENT_CLOSED);return true;}
#endif
