#include <xmqtt/store_write.h>
#include "../internal/xmqtt_store_checkpoint_online.h"
#include "../internal/xmqtt_journal_worker.h"
#include "../internal/xmqtt_store_write.h"
#include "../internal/xmqtt_client_store.h"
#include <xrt/memory.h>
#include <string.h>
#include <xrt/sync.h>
#include <xrt/time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
struct xmqttstorewrite {
    xmqttstorewriteconfig Config;xmqttstoredeltaconfig Delta;xmqttstorestate* State;xmqttjournalworker* Worker;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
    const void* Driver;size_t Admissions;xtaskpool* Pool;bool Unusable;
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_STORE)
    const void* ReconnectOwner;xmqttsession* ReconnectSession;
#endif
#endif
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
XMQTT_API bool xmqttStoreWriteCreate(xtaskpool* Pool,xmqttjournal* Journal,xmqttstorestate* State,
    const xmqttstorewriteconfig* C,xmqttstorewrite** Out)
{
    if(!Pool||!Journal||!State||!xmqttSWSpan(C,sizeof(*C))||C->Size!=sizeof(*C)||!xmqttSWSpan(Out,sizeof(*Out))||!xmqttSWSeparate(Out,sizeof(*Out),C,sizeof(*C)))return xmqttSWError(XERR_ARGUMENT,"invalid store-writer constructor inputs");
    if(C->MaxScratchBytes<sizeof(xmqttstorewrite))return xmqttSWError(XERR_ARGUMENT,"store-writer fixed budget is too small");
    xmqttstorewrite* W=xrtCalloc(1,sizeof(*W));if(!W)return false;
    W->Config=*C;W->State=State;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
    W->Pool=Pool;
#endif
    if(!xrtMutexInit(&W->Gate)){xrtFree(W);return false;}
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
    xmqttInternalStoreWriteIndexPending(State,W);
    *Out=W;return true;
Failure:xmqttInternalStoreWriteRelease(State,W,false);xrtFree(W->Scratch);xrtMutexUnit(&W->Gate);xrtFree(W);return false;
}
XMQTT_API xmqttjournaladmission xmqttStoreWriteRetireAsync(xmqttstorewrite* W,uint64 Epoch,
    uint64 Key,int64 Timeout,xfuture** Future)
{
    double Deadline=Timeout<0?INFINITY:xrtTimer()+(double)Timeout/1000.0;
    if(!W||!Epoch||!Key||!Future||Timeout<XRT_WAIT_FOREVER){
        xmqttSWError(XERR_ARGUMENT,"invalid committed record retirement arguments");
        return XMQTT_JWORK_ERROR;
    }
    if(!xrtMutexLock(&W->Gate))return XMQTT_JWORK_ERROR;
    bool Valid=xmqttSWOutput(W,Future,sizeof(*Future));
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
    bool Closed=W->Unusable;
#endif
    xrtMutexUnlock(&W->Gate);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
    if(Closed)return XMQTT_JWORK_CLOSED;
#endif
    if(!Valid){
        xmqttSWError(XERR_ARGUMENT,"retirement Future overlaps owned state");
        return XMQTT_JWORK_ERROR;
    }
    xmqttstoredelta Op={.Type=XMQTT_STORE_INBOX_ACK,.Epoch=Epoch,.Key=Key};
    unsigned char Batch[128];
    size_t Bytes=0;
    xmqttstoredeltastatus Status=xmqttStoreDeltaWrite(&W->Delta,&Op,1,NULL,0,Batch,sizeof(Batch),&Bytes);
    if(Status!=XMQTT_STORE_DELTA_DONE)
        return Status==XMQTT_STORE_DELTA_LIMIT?XMQTT_JWORK_OVERLOADED:XMQTT_JWORK_ERROR;
    return xmqttInternalJournalWorkerAppendAt(W->Worker,(xbytesview){Batch,Bytes},Deadline,Future);
}
static xmqttjournaladmission xmqttSWAppend(xmqttstorewrite* W,const void* Owner,xbytesview Data,int64 Timeout,double Deadline,xfuture** Future)
{
    if(!W||Timeout<XRT_WAIT_FOREVER){xmqttSWError(XERR_ARGUMENT,"invalid store writer/deadline");return XMQTT_JWORK_ERROR;}
    if(!xrtMutexLock(&W->Gate))return XMQTT_JWORK_ERROR;
    bool OK=xmqttSWOutput(W,Future,sizeof(*Future));
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
    OK=OK&&W->Driver==Owner;
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_STORE)
    if(W->ReconnectOwner&&!Owner)OK=false;
#endif
    if(OK)++W->Admissions;
#else
    (void)Owner;
#endif
    xrtMutexUnlock(&W->Gate);
    if(!OK){xmqttSWError(XERR_ARGUMENT,"append Future overlaps store-writer ownership");return XMQTT_JWORK_ERROR;}
    xmqttjournaladmission A=xmqttInternalJournalWorkerAppendAt(W->Worker,Data,Deadline,Future);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
    if(!xrtMutexLock(&W->Gate))abort();
    --W->Admissions;if(!xrtMutexUnlock(&W->Gate))abort();
#endif
    return A;
}
XMQTT_API xmqttjournaladmission xmqttStoreWriteAppendAsync(xmqttstorewrite* W,xbytesview Data,int64 Timeout,xfuture** Future)
{return xmqttSWAppend(W,NULL,Data,Timeout,Timeout<0?INFINITY:xrtTimer()+(double)Timeout/1000.0,Future);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_STORE)
XMQTT_INTERNAL void xmqttInternalStoreWriteReleaseReconnect(xmqttstorewrite* W,const void* Owner)
{
    if(!xrtMutexLock(&W->Gate))abort();
    if(W->ReconnectOwner!=Owner||W->Driver||W->Admissions)abort();
    W->ReconnectOwner=NULL;W->ReconnectSession=NULL;
    if(!xrtMutexUnlock(&W->Gate))abort();
}
XMQTT_INTERNAL bool xmqttInternalStoreWriteReserveReconnect(xmqttstorewrite* W,const void* Owner,xmqttsession* Session,
    xtaskpool* Cleanup,xtaskpool** StoragePool)
{
    if(Cleanup==W->Pool)return xmqttSWError(XERR_ARGUMENT,"durable reconnect requires a separate cleanup pool");
    if(!xrtMutexLock(&W->Gate))return false;
    bool OK=!W->Driver&&!W->ReconnectOwner&&!W->Admissions&&!W->Unusable;
    if(OK){W->ReconnectOwner=Owner;W->ReconnectSession=Session;}
    xrtMutexUnlock(&W->Gate);
    if(!OK)return xmqttSWError(XERR_STATE,"durable reconnect requires an unclaimed store writer");
    xmqttjournalworkerinfo Info;
    if(!xmqttJournalWorkerInfo(W->Worker,&Info)||Info.Failed||Info.Closed||Info.Requests||Info.Tasks){
        xmqttInternalStoreWriteReleaseReconnect(W,Owner);
        return xmqttSWError(XERR_STATE,"durable reconnect requires a drained and usable writer");
    }
    *StoragePool=W->Pool;
    return true;
}
XMQTT_INTERNAL bool xmqttInternalStoreWriteReconnectUsable(xmqttstorewrite* W,const void* Owner)
{
    if(!xrtMutexLock(&W->Gate))return false;
    bool OK=W->ReconnectOwner==Owner&&!W->Driver&&!W->Unusable;
    xrtMutexUnlock(&W->Gate);
    xmqttjournalworkerinfo Info;
    return OK&&xmqttJournalWorkerInfo(W->Worker,&Info)&&!Info.Failed&&!Info.Closed;
}
#endif
XMQTT_INTERNAL bool xmqttInternalStoreWriteDriverClaim(xmqttstorewrite* W,const void* Owner,xmqttsession* Session,
    xmqttstoredeltaconfig* Delta,xmqttstorestateinfo* Info,bool* Opened,bool* ProtocolOnly,xtaskpool** Pool)
{
    if(!W||!Owner||!xrtMutexLock(&W->Gate))return false;
    /* Inspect the native owner before reading the pump-owned Pending slot. */
    bool OK=!W->Driver&&!W->Admissions&&!W->Unusable;
    bool Reserved=false;
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_STORE)
    Reserved=W->ReconnectOwner!=NULL;
    if(Reserved&&W->ReconnectSession!=Session)OK=false;
#else
    (void)Session;
#endif
    if(OK)W->Driver=Owner;
    xrtMutexUnlock(&W->Gate);if(!OK)return xmqttSWError(XERR_STATE,"store writer is already active or claimed");
    /* Reap native tails outside the model gate: backing frees can reenter Info. */
    /* A reconnect reservation excludes other protocol mutations. The new
     * generation may use the committed projection while an Inbox ACK settles. */
    xmqttjournalworkerinfo I;OK=xmqttJournalWorkerInfo(W->Worker,&I)&&!I.Failed&&!I.Closed&&
        (Reserved||(!I.Requests&&!I.Tasks));
    if(!xrtMutexLock(&W->Gate))abort();
    if(OK&&(Reserved||!W->Pending)){
        *Delta=W->Delta;xmqttInternalStoreWriteInfo(W->State,W,Info);
        *Opened=xmqttInternalStoreWriteEverOpened(W->State,W);*ProtocolOnly=W->Config.ProtocolOnly;*Pool=W->Pool;
    }
    else {W->Driver=NULL;OK=false;}
    xrtMutexUnlock(&W->Gate);return OK?true:xmqttSWError(XERR_STATE,"store writer has an active native tail");
}
XMQTT_INTERNAL uint64 xmqttInternalStoreWriteDriverIncoming(xmqttstorewrite* W,const void* Owner,uint16 Id)
{
    if(!xrtMutexLock(&W->Gate))abort();
    if(!Owner||W->Driver!=Owner)abort(); /* Reads committed State even during a concurrent Inbox ACK. */
    uint64 Key=xmqttInternalStoreWriteIncomingKey(W->State,W,Id);
    if(!xrtMutexUnlock(&W->Gate))abort();
    return Key;
}
XMQTT_INTERNAL uint64 xmqttInternalStoreWriteDriverOutgoing(xmqttstorewrite* W,const void* Owner,
    uint16 Id,xbytesview Wire,bool Pubrel,bool Sent,uint8 PubrecReason,bool* Attempted)
{
    if(!xrtMutexLock(&W->Gate))abort();
    if(!Owner||W->Driver!=Owner)abort();
    uint64 Key=xmqttInternalStoreWriteOutgoingKey(W->State,W,Id,Wire,Pubrel,Sent,PubrecReason,Attempted);
    if(!xrtMutexUnlock(&W->Gate))abort();
    return Key;
}
XMQTT_INTERNAL void xmqttInternalStoreWriteDriverRelease(xmqttstorewrite* W,const void* Owner,bool Reusable)
{
    if(!W||!xrtMutexLock(&W->Gate))abort();
    if(W->Driver!=Owner||W->Admissions)abort();
    W->Driver=NULL;
    if(!Reusable)W->Unusable=true;
    xrtMutexUnlock(&W->Gate);
    if(!Reusable)(void)xmqttJournalWorkerClose(W->Worker);
}
XMQTT_INTERNAL xmqttjournaladmission xmqttInternalStoreWriteDriverAppend(xmqttstorewrite* W,const void* Owner,xbytesview Data,double Deadline,xfuture** Future)
{return xmqttSWAppend(W,Owner,Data,XRT_WAIT_FOREVER,Deadline,Future);}
static bool xmqttSWUnclaimed(xmqttstorewrite* W)
{
    if(!W||!xrtMutexLock(&W->Gate))return false;
    bool OK=!W->Driver;
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_STORE)
    if(W->ReconnectOwner)OK=false;
#endif
    xrtMutexUnlock(&W->Gate);
    return OK?true:xmqttSWError(XERR_STATE,"Client or controller owns store writer until physical destruction");
}
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_CHECKPOINT_ONLINE)
XMQTT_API xmqttjournaladmission xmqttStoreWriteCheckpointAsync(xmqttstorewrite* W,
    const xmqttstorecheckpointrequest* Request,int64 Timeout,xfuture** Future)
{
    double Deadline=Timeout<0?INFINITY:xrtTimer()+(double)Timeout/1000.0;
    if(!W||Timeout<XRT_WAIT_FOREVER||!xmqttSWSpan(Request,sizeof(*Request))||Request->Size!=sizeof(*Request)||!Request->Apply||
       !xmqttSWSeparate(Request,sizeof(*Request),Future,sizeof(*Future))){
        xmqttSWError(XERR_ARGUMENT,"invalid checkpoint request, timeout or Future");
        return XMQTT_JWORK_ERROR;
    }
    if(!xrtMutexLock(&W->Gate))return XMQTT_JWORK_ERROR;
    bool Valid=xmqttSWOutput(W,Future,sizeof(*Future));
    xrtMutexUnlock(&W->Gate);
    if(!Valid){
        xmqttSWError(XERR_ARGUMENT,"checkpoint Future overlaps owned state");
        return XMQTT_JWORK_ERROR;
    }
    return xmqttInternalJournalCheckpointSubmit(W->Worker,Request,Deadline,Future);
}
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalStoreCheckpointWork(xmqttstorewrite* W,
    xmqttjournal** Journal,const void* Owner,const xmqttstorecheckpointrequest* Request,
    xmqttcheckpointstop Stop,ptr StopContext,xmqttstorecheckpointresult* Result)
{
    if(!W||W->Pending)abort();
    xmqttcheckpointstats Stats={0};
    xmqttjournalstatus Status=xmqttInternalStoreCheckpointModel(W->State,W,Journal,Owner,Request,Stop,StopContext,Result,&Stats);
    if(Result->Effect==XMQTT_JOURNAL_COMMITTED){
        xmqttjournalinfo J;
        if(!xmqttInternalJournalWorkerInfo(*Journal,Owner,&J)||!xrtMutexLock(&W->Gate))abort();
        xmqttInternalStoreCheckpointRebase(W->State,W,&J,Result,&Stats);
        if(!xrtMutexUnlock(&W->Gate))abort();
    }
    return Status;
}
#endif

XMQTT_API bool xmqttStoreWriteInfo(xmqttstorewrite* W,xmqttstorewriteinfo* Info)
{
    if(!W)return xmqttSWError(XERR_ARGUMENT,"NULL store writer");
    xmqttstorewriteinfo I={.Size=sizeof(I),.ScratchBytes=sizeof(*W)+W->ScratchBytes};
    if(!xmqttJournalWorkerInfo(W->Worker,&I.Worker))return false;
    if(!xrtMutexLock(&W->Gate))return false;
    bool OK=xmqttSWOutput(W,Info,sizeof(*Info));if(OK){xmqttInternalStoreWriteInfo(W->State,W,&I.State);*Info=I;}xrtMutexUnlock(&W->Gate);
    return OK?true:xmqttSWError(XERR_ARGUMENT,"invalid/overlapping store-writer Info output");
}
static xmqttstorestatus xmqttSWRead(xmqttstorewrite* W,uint64 Key,ptr Buffer,size_t Capacity,xmqttstoreview* View,size_t* Required)
{
    if(!W){xmqttSWError(XERR_ARGUMENT,"NULL store writer");return XMQTT_STORE_ERROR;}
    if(!xrtMutexLock(&W->Gate))return XMQTT_STORE_ERROR;
    xmqttstorestatus S;
    if(!xmqttSWOutput(W,Buffer,Capacity)||!xmqttSWOutput(W,View,sizeof(*View))||!xmqttSWOutput(W,Required,sizeof(*Required)))S=XMQTT_STORE_ERROR;
    else S=xmqttInternalStoreWriteRead(W->State,W,Key,Buffer,Capacity,View,Required);
    xrtMutexUnlock(&W->Gate);if(S==XMQTT_STORE_ERROR)xmqttSWError(XERR_ARGUMENT,"invalid/overlapping store-writer read output/key");return S;
}
XMQTT_API xmqttstorestatus xmqttStoreWriteRead(xmqttstorewrite* W,uint64 Key,ptr Buffer,size_t Capacity,xmqttstoreview* View,size_t* Required)
{
    if(!Key){xmqttSWError(XERR_ARGUMENT,"invalid committed record key");return XMQTT_STORE_ERROR;}
    return xmqttSWRead(W,Key,Buffer,Capacity,View,Required);
}
XMQTT_API xmqttstorestatus xmqttStoreWriteReadPending(xmqttstorewrite* W,ptr Buffer,size_t Capacity,xmqttstoreview* View,size_t* Required)
{return xmqttSWRead(W,0,Buffer,Capacity,View,Required);}
XMQTT_API bool xmqttStoreWriteClose(xmqttstorewrite* W)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
    if(W&&!xmqttSWUnclaimed(W))return false;
#endif
    if(!W)return xmqttSWError(XERR_ARGUMENT,"NULL store writer");
    return xmqttJournalWorkerClose(W->Worker);
}
XMQTT_API xwaitresult xmqttStoreWriteWaitFor(xmqttstorewrite* W,int64 Timeout)
{if(!W){xmqttSWError(XERR_ARGUMENT,"NULL store writer");return XWAIT_ERROR;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
    if(!xmqttSWUnclaimed(W))return XWAIT_ERROR;
#endif
    return xmqttJournalWorkerWaitFor(W->Worker,Timeout);}
XMQTT_API bool xmqttStoreWriteDestroy(xmqttstorewrite* W)
{
    if(!W)return true;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
    if(!xmqttSWUnclaimed(W))return false;
#endif
    if(!xmqttJournalWorkerDestroy(W->Worker))return false;
    if(W->Pending)abort();
    xmqttInternalStoreWriteRelease(W->State,W,true);xrtFree(W->Scratch);xrtMutexUnit(&W->Gate);xrtFree(W);return true;
}
#endif
