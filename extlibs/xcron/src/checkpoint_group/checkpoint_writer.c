#include <xcron/checkpoint_writer.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <xrt/time.h>
#include <math.h>

#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
typedef enum xcronwriterstate {XCRON_WRITER_IDLE,XCRON_WRITER_PENDING,XCRON_WRITER_READY} xcronwriterstate;

struct xcroncheckpointwriter {
    xcroncheckpointgroup* Store;
    xexecutor* Executor;
    xmutex Mutex;
    xcond Changed;
    const xcroncheckpoint* Records;
    size_t RecordBytes;
    uint64 Expected;
    uint32 Count;
    xcronwriterstate State;
    xcroncheckpointwriteticket Ticket;
    xcroncheckpointwriterreply Reply;
};

static bool xcronWriterFail(xcroncheckpointstorestatus Code,xerrkind Kind,cstr Text)
{
    xrtSetErrorInfo(Kind,"xrt.cron.checkpoint_writer",Code,Text);
    return false;
}

static bool xcronWriterOutput(const xcroncheckpointwriter* W,const void* Out,size_t Size)
{
    return xrtMemRangeValid(W,sizeof(*W)) && xrtMemRangeValid(Out,Size) &&
        !xrtMemRangesOverlap(W,sizeof(*W),Out,Size);
}

static void xcronWriterRun(ptr Raw)
{
    xcroncheckpointwriter* W=Raw;
    xrtClearError();
    xcroncheckpointstorestatus Code=xcronCheckpointGroupCommit(W->Store,W->Expected,W->Records,W->Count);
    W->Reply=(xcroncheckpointwriterreply){Code,Code==XCRON_CHECKPOINT_STORE_OK?W->Expected+1:0,
        Code==XCRON_CHECKPOINT_STORE_OK?NULL:xrtTakeError()};
}

/* The executor calls cleanup for both a completed body and a discarded queue
 * entry. Cleanup releases all worker access before a collector can reuse W. */
static void xcronWriterReady(ptr Raw,ptr Context)
{
    (void)Context;
    xcroncheckpointwriter* W=Raw;
    xrtMutexLock(&W->Mutex);
    W->State=XCRON_WRITER_READY;
    xrtCondBroadcast(&W->Changed);
    xrtMutexUnlock(&W->Mutex);
}

XCRON_API bool xcronCheckpointWriterCreate(xcroncheckpointgroup* Store,xexecutor* Executor,xcroncheckpointwriter** Out)
{
    if(!Executor || !xrtMemRangeValid(Out,sizeof(*Out)))
        return xcronWriterFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint writer configuration");
    xcroncheckpointgroupview Current;
    xcroncheckpointstorestatus Code=xcronCheckpointGroupRead(Store,&Current);
    if(Code!=XCRON_CHECKPOINT_STORE_OK && Code!=XCRON_CHECKPOINT_STORE_EMPTY)return false;
    xcroncheckpointwriter* W=xrtCalloc(1,sizeof(*W));
    if(!W)return false;
    if(!xrtMutexInit(&W->Mutex)){
        xrtFree(W);
        return false;
    }
    if(!xrtCondInit(&W->Changed)){
        xerror* Error=xrtTakeError();
        xrtMutexUnit(&W->Mutex);
        xrtFree(W);
        xrtSetErrorTake(Error);
        return false;
    }
    W->Store=Store;
    W->Executor=Executor;
    *Out=W;
    return true;
}

XCRON_API bool xcronCheckpointWriterSubmit(xcroncheckpointwriter* W,uint64 Expected,
    const xcroncheckpoint* Records,uint32 Count,xcroncheckpointwriteticket* Ticket)
{
    size_t Members=Count;
    if(Members>SIZE_MAX/sizeof(*Records))
        return xcronWriterFail(XCRON_CHECKPOINT_STORE_LIMIT,XERR_RANGE,"checkpoint writer input exceeds address space");
    size_t Bytes=Members*sizeof(*Records);
    if(!xcronWriterOutput(W,Ticket,sizeof(*Ticket)) || !xrtMemRangeValid(Records,Bytes) ||
       xrtMemRangesOverlap(Records,Bytes,W,sizeof(*W)) || xrtMemRangesOverlap(Records,Bytes,Ticket,sizeof(*Ticket)))
        return xcronWriterFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint writer input or ticket");
    if(!xrtMutexLock(&W->Mutex))return false;
    if(W->State!=XCRON_WRITER_IDLE || W->Ticket==UINT64_MAX){
        bool Busy=W->State!=XCRON_WRITER_IDLE;
        xrtMutexUnlock(&W->Mutex);
        return Busy?xcronWriterFail(XCRON_CHECKPOINT_STORE_BUSY,XERR_AGAIN,"collect the outstanding checkpoint commit"):
            xcronWriterFail(XCRON_CHECKPOINT_STORE_LIMIT,XERR_RANGE,"checkpoint writer tickets exhausted");
    }
    W->Records=Records;
    W->RecordBytes=Bytes;
    W->Expected=Expected;
    W->Count=Count;
    W->Reply=(xcroncheckpointwriterreply){.Status=XCRON_CHECKPOINT_STORE_CANCELLED};
    W->State=XCRON_WRITER_PENDING;
    bool OK=xrtExecutorSubmit(W->Executor,xcronWriterRun,W,xcronWriterReady,NULL);
    if(OK)*Ticket=++W->Ticket;
    else{
        W->State=XCRON_WRITER_IDLE;
        W->Records=NULL;
        W->RecordBytes=0;
    }
    xrtMutexUnlock(&W->Mutex);
    return OK;
}

XCRON_API xwaitresult xcronCheckpointWriterCollect(xcroncheckpointwriter* W,xcroncheckpointwriteticket Ticket,
    int64 Timeout,xcroncheckpointwriterreply* Reply)
{
    if(Timeout< -1 || !Ticket || !xcronWriterOutput(W,Reply,sizeof(*Reply))){
        xcronWriterFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint writer collection");
        return XWAIT_ERROR;
    }
    double Start=Timeout>0?xrtTimer():0;
    if(!xrtMutexLock(&W->Mutex))return XWAIT_ERROR;
    xwaitresult Result=XWAIT_ERROR;
    bool CheckedContext=false;
    for(;;){
        if(Ticket!=W->Ticket || W->State==XCRON_WRITER_IDLE){
            xcronWriterFail(XCRON_CHECKPOINT_STORE_STALE,XERR_NOT_FOUND,"stale checkpoint writer ticket");
            break;
        }
        if(xrtMemRangesOverlap(W->Records,W->RecordBytes,Reply,sizeof(*Reply))){
            xcronWriterFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"reply overlaps borrowed checkpoint records");
            break;
        }
        if(W->State==XCRON_WRITER_READY){
            *Reply=W->Reply;
            W->Reply.Error=NULL;
            W->Records=NULL;
            W->RecordBytes=0;
            W->State=XCRON_WRITER_IDLE;
            Result=XWAIT_OK;
            break;
        }
        if(!Timeout){Result=XWAIT_TIMEOUT;break;}
        if(!CheckedContext && xrtExecutorIsCurrent(W->Executor)){
            xcronWriterFail(XCRON_CHECKPOINT_STORE_BUSY,XERR_STATE,"checkpoint writer wait depends on the current executor");
            break;
        }
        CheckedContext=true;
        xwaitresult Wait;
        if(Timeout<0)Wait=xrtCondWait(&W->Changed,&W->Mutex);
        else{
            double Left=(double)Timeout-(xrtTimer()-Start)*1000.0;
            if(Left<=0){Result=XWAIT_TIMEOUT;break;}
            int64 Milliseconds=Left>1000.0?1000:(int64)ceil(Left);
            Wait=xrtCondWaitFor(&W->Changed,&W->Mutex,Milliseconds);
        }
        if(Wait==XWAIT_ERROR)break;
    }
    xrtMutexUnlock(&W->Mutex);
    return Result;
}

XCRON_API xcroncheckpointstorestatus xcronCheckpointWriterDestroy(xcroncheckpointwriter* W)
{
    if(!xrtMemRangeValid(W,sizeof(*W))){
        xcronWriterFail(XCRON_CHECKPOINT_STORE_ARGUMENT,XERR_ARGUMENT,"invalid checkpoint writer");
        return XCRON_CHECKPOINT_STORE_ARGUMENT;
    }
    if(!xrtMutexLock(&W->Mutex))return XCRON_CHECKPOINT_STORE_IO;
    bool Busy=W->State!=XCRON_WRITER_IDLE;
    xrtMutexUnlock(&W->Mutex);
    if(Busy){
        xcronWriterFail(XCRON_CHECKPOINT_STORE_BUSY,XERR_STATE,"collect the checkpoint commit before destruction");
        return XCRON_CHECKPOINT_STORE_BUSY;
    }
    xcroncheckpointstorestatus Code=xcronCheckpointGroupClose(W->Store);
    xerror* Error=Code==XCRON_CHECKPOINT_STORE_OK?NULL:xrtTakeError();
    xrtCondUnit(&W->Changed);
    xrtMutexUnit(&W->Mutex);
    xrtFree(W);
    if(Error)xrtSetErrorTake(Error);
    return Code;
}
#endif
