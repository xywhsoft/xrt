#include <xmqtt/store_write.h>
#include "../internal/xmqtt_store_write.h"
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <xrt/time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
struct xmqttstorewrite {
    xmqttstorewriteconfig Config;xmqttstoredeltaconfig Delta;xmqttstorestate* State;xmqttjournalworker* Worker;
    xmutex Gate;ptr Scratch;size_t ScratchBytes,PlanLimit;xmqttstoredelta* Operations;xmqttproperty* Properties;xmqttstoreplan* Pending;
};
static bool xmqttSWError(xerrkind Kind,cstr Text)
{xrtSetErrorInfo(Kind,"xrt.mqtt.store.write",6700+(int32)Kind,Text);return false;}
static bool xmqttSWSpan(const void* P,size_t N){return (P||!N)&&xrtMemRangeValid(P,N);}
static bool xmqttSWSeparate(const void* P,size_t N,const void* Q,size_t M){return !N||!M||!xrtMemRangesOverlap(P,N,Q,M);}
static bool xmqttSWOutput(xmqttstorewrite* W,void* P,size_t N)
{return xmqttSWSpan(P,N)&&xmqttSWSeparate(P,N,W,sizeof(*W))&&xmqttSWSeparate(P,N,W->Scratch,W->ScratchBytes)&&xmqttInternalStoreWriteOutput(W->State,W,P,N);}
static xmqttcommitstatus xmqttSWPrepare(ptr Context,xbytesview Data)
{
    xmqttstorewrite* W=Context;if(W->Pending)abort();xmqttstoredeltadocument D;
    xmqttstoredeltastatus DS=xmqttStoreDeltaRead(&W->Delta,Data,W->Operations,W->Delta.MaxOperations,W->Properties,W->Delta.Codec.MaxProperties,&D);
    if(DS!=XMQTT_STORE_DELTA_DONE)return DS==XMQTT_STORE_DELTA_LIMIT?XMQTT_COMMIT_LIMIT:DS==XMQTT_STORE_DELTA_ERROR?XMQTT_COMMIT_INITIALIZATION:XMQTT_COMMIT_INVALID;
    xmqttstorestatus S=xmqttInternalStoreWritePrepare(W->State,W,&D,W->Config.MaxModelWork,W->PlanLimit,&W->Pending);
    return S==XMQTT_STORE_DONE?XMQTT_COMMIT_OK:S==XMQTT_STORE_LIMIT?XMQTT_COMMIT_LIMIT:S==XMQTT_STORE_ERROR?XMQTT_COMMIT_INITIALIZATION:XMQTT_COMMIT_INVALID;
}
static void xmqttSWFinish(ptr Context,const xmqttcommitresult* Result)
{
    xmqttstorewrite* W=Context;if(!xrtMutexLock(&W->Gate))abort();xmqttstoreplan* P=W->Pending;
    xmqttInternalStoreWriteFinish(W->State,W,P,Result->Effect==XMQTT_JOURNAL_COMMITTED,Result->Sequence);if(!xrtMutexUnlock(&W->Gate))abort();
    /* Public backing allocator Free callbacks may inspect this live owner.
     * Publish complete metadata before invoking them,without either owner gate.
     * Keep payload/plan credits through actual destruction and refund last. */
    size_t Bytes=xmqttInternalStoreWriteDispose(W->State,W,P);
    if(!xrtMutexLock(&W->Gate))abort();
    W->Pending=NULL;xmqttInternalStoreWriteRefund(W->State,W,Bytes);if(!xrtMutexUnlock(&W->Gate))abort();
}
XMQTT_API bool xmqttStoreWriteConfigInit(xmqttstorewriteconfig* C)
{
    if(!xmqttSWSpan(C,sizeof(*C)))return xmqttSWError(XERR_ARGUMENT,"invalid store-writer config output");
    *C=(xmqttstorewriteconfig){.Size=sizeof(*C),.MaxScratchBytes=1048576,.MaxModelWork=65536};return xmqttJournalWorkerConfigInit(&C->Worker);
}
XMQTT_API bool xmqttStoreWriteCreate(xtaskpool* Pool,xmqttjournal* Journal,xmqttstorestate* State,const xmqttstorewriteconfig* C,xmqttstorewrite** Out)
{
    if(!Pool||!Journal||!State||!xmqttSWSpan(C,sizeof(*C))||C->Size!=sizeof(*C)||!xmqttSWSpan(Out,sizeof(*Out))||!xmqttSWSeparate(Out,sizeof(*Out),C,sizeof(*C)))return xmqttSWError(XERR_ARGUMENT,"invalid store-writer constructor inputs");
    if(C->MaxScratchBytes<sizeof(xmqttstorewrite))return xmqttSWError(XERR_ARGUMENT,"store-writer fixed budget is too small");
    xmqttstorewrite* W=xrtCalloc(1,sizeof(*W));if(!W)return false;
    W->Config=*C;W->State=State;if(!xrtMutexInit(&W->Gate)){xrtFree(W);return false;}
    size_t MinimumPlan=0;
    if(!xmqttInternalStoreWriteClaim(State,Journal,W,Out,sizeof(*Out),&W->Delta,&MinimumPlan)){xrtMutexUnit(&W->Gate);xrtFree(W);return false;}
    size_t Ops=W->Delta.MaxOperations,Props=W->Delta.Codec.MaxProperties;
    if(Ops>SIZE_MAX/sizeof(*W->Operations)||Props>SIZE_MAX/sizeof(*W->Properties)||Ops*sizeof(*W->Operations)>SIZE_MAX-Props*sizeof(*W->Properties)){
        xmqttSWError(XERR_ARGUMENT,"store-writer scratch size overflows");goto Failure;}
    W->ScratchBytes=Ops*sizeof(*W->Operations)+Props*sizeof(*W->Properties);
    if(W->ScratchBytes>C->MaxScratchBytes-sizeof(*W)){xmqttSWError(XERR_ARGUMENT,"store-writer scratch exceeds fixed budget");goto Failure;}
    W->PlanLimit=C->MaxScratchBytes-sizeof(*W)-W->ScratchBytes;
    if(MinimumPlan>W->PlanLimit){xmqttSWError(XERR_ARGUMENT,"scratch cannot reserve the largest configured reset plan");goto Failure;}
    if(W->ScratchBytes){W->Scratch=xrtMalloc(W->ScratchBytes);if(!W->Scratch)goto Failure;}
    W->Operations=W->Scratch;W->Properties=Props?(xmqttproperty*)((unsigned char*)W->Scratch+Ops*sizeof(*W->Operations)):NULL;
    if(!xmqttJournalWorkerCreate(Pool,Journal,&C->Worker,&W->Worker))goto Failure;
    if(!xmqttInternalJournalWorkerBind(W->Worker,xmqttSWPrepare,xmqttSWFinish,W))abort();
    *Out=W;return true;
Failure:xmqttInternalStoreWriteRelease(State,W,false);xrtFree(W->Scratch);xrtMutexUnit(&W->Gate);xrtFree(W);return false;
}
XMQTT_API xmqttjournaladmission xmqttStoreWriteAppendAsync(xmqttstorewrite* W,xbytesview Data,int64 Timeout,xfuture** Future)
{
    double Deadline=Timeout<0?INFINITY:xrtTimer()+(double)Timeout/1000.0;
    if(!W||Timeout<XRT_WAIT_FOREVER){xmqttSWError(XERR_ARGUMENT,"invalid store writer/deadline");return XMQTT_JWORK_ERROR;}
    if(!xrtMutexLock(&W->Gate))return XMQTT_JWORK_ERROR;
    bool OK=xmqttSWOutput(W,Future,sizeof(*Future));xrtMutexUnlock(&W->Gate);
    if(!OK){xmqttSWError(XERR_ARGUMENT,"append Future overlaps store-writer ownership");return XMQTT_JWORK_ERROR;}
    return xmqttInternalJournalWorkerAppendAt(W->Worker,Data,Deadline,Future);
}
XMQTT_API bool xmqttStoreWriteInfo(xmqttstorewrite* W,xmqttstorewriteinfo* Info)
{
    if(!W)return xmqttSWError(XERR_ARGUMENT,"NULL store writer");
    xmqttstorewriteinfo I={.Size=sizeof(I),.ScratchBytes=sizeof(*W)+W->ScratchBytes};
    if(!xmqttJournalWorkerInfo(W->Worker,&I.Worker))return false;
    if(!xrtMutexLock(&W->Gate))return false;
    bool OK=xmqttSWOutput(W,Info,sizeof(*Info));if(OK){xmqttInternalStoreWriteInfo(W->State,W,&I.State);*Info=I;}xrtMutexUnlock(&W->Gate);
    return OK?true:xmqttSWError(XERR_ARGUMENT,"invalid/overlapping store-writer Info output");
}
XMQTT_API xmqttstorestatus xmqttStoreWriteRead(xmqttstorewrite* W,uint64 Key,ptr Buffer,size_t Capacity,xmqttstoreview* View,size_t* Required)
{
    if(!W){xmqttSWError(XERR_ARGUMENT,"NULL store writer");return XMQTT_STORE_ERROR;}
    if(!xrtMutexLock(&W->Gate))return XMQTT_STORE_ERROR;
    xmqttstorestatus S;
    if(!xmqttSWOutput(W,Buffer,Capacity)||!xmqttSWOutput(W,View,sizeof(*View))||!xmqttSWOutput(W,Required,sizeof(*Required)))S=XMQTT_STORE_ERROR;
    else S=xmqttInternalStoreWriteRead(W->State,W,Key,Buffer,Capacity,View,Required);
    xrtMutexUnlock(&W->Gate);if(S==XMQTT_STORE_ERROR)xmqttSWError(XERR_ARGUMENT,"invalid/overlapping store-writer read output/key");return S;
}
XMQTT_API bool xmqttStoreWriteClose(xmqttstorewrite* W)
{return W?xmqttJournalWorkerClose(W->Worker):xmqttSWError(XERR_ARGUMENT,"NULL store writer");}
XMQTT_API xwaitresult xmqttStoreWriteWaitFor(xmqttstorewrite* W,int64 Timeout)
{if(!W){xmqttSWError(XERR_ARGUMENT,"NULL store writer");return XWAIT_ERROR;}return xmqttJournalWorkerWaitFor(W->Worker,Timeout);}
XMQTT_API bool xmqttStoreWriteDestroy(xmqttstorewrite* W)
{
    if(!W)return true;
    if(!xmqttJournalWorkerDestroy(W->Worker))return false;
    if(W->Pending)abort();
    xmqttInternalStoreWriteRelease(W->State,W,true);xrtFree(W->Scratch);xrtMutexUnit(&W->Gate);xrtFree(W);return true;
}
#endif
