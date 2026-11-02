#include <xrpc/server.h>
#include "../internal/xrpc_server_internal.h"
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_SERVER)
typedef struct xrpcserverquota {
    xatomic32 References;
    xmutex Gate;
    xcond Idle;
    xrpcserverinfo Info;
} xrpcserverquota;
typedef struct xrpcserverslot {size_t Next;xrpcmessage* Scratch;} xrpcserverslot;
typedef struct xrpcserverbox {
    xrpcserverresult Value; /* Public prefix, one allocation with output. */
    xrpcserverquota* Quota;
    size_t Capacity;
    unsigned char Data[];
} xrpcserverbox;
typedef struct xrpcserverjob {
    xatomic32 References;
    xrpcserver* Server;
    xpromise* Promise;
    xcancel* Cancel;
    xrpcserverbox* Box;
    size_t InputBytes,OwnedBytes,OutputBytes,InitialBoxBytes;
    double Deadline;
    bool Finished,ResultOwned,Entered;
    xrpcserverstatus OutputFailure;
    xrpcdispatchinfo Dispatch;
    unsigned char Input[];
} xrpcserverjob;
struct xrpcserver {
    xrpcserverconfig Config;
    xrpcserverquota* Quota;
    xexecutor* Executor;
    xcancel* Cancel;
    size_t OwnedBytes,FreeSlot;
    xrpcserverslot Slots[];
};
static xbytesview xrpcServerContextView(const xrpcserverjob* J)
{
    size_t At=sizeof(*J)+J->InputBytes;
    if(J->OwnedBytes==At)return (xbytesview){0};
    size_t Align=_Alignof(max_align_t);At+=(Align-At%Align)%Align;
    return (xbytesview){(const unsigned char*)J+At,J->OwnedBytes-At};
}
static bool xrpcServerReport(xerrkind Kind,xrpcservererror Code,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return false;}
static double xrpcServerDeadline(double Entered,int64 Timeout)
{return Timeout==XRT_WAIT_FOREVER?INFINITY:Timeout==0?Entered:nextafter(Entered+(double)(Timeout/1000)+(double)(Timeout%1000)*.001,INFINITY);}
static int64 xrpcServerRemaining(double Deadline)
{if(Deadline==INFINITY)return XRT_WAIT_FOREVER;double N=ceil((Deadline-xrtTimer())*1000.);return N<=0?0:N>=0x1p63?INT64_MAX:(int64)N;}
static void xrpcServerQuotaDrop(xrpcserverquota* Q)
{if(xrtAtomic32FetchSub(&Q->References,1,XMEMORY_ACQ_REL)!=1)return;xrtCondUnit(&Q->Idle);xrtMutexUnit(&Q->Gate);xrtFree(Q);}
static void xrpcServerResultRefund(xrpcserverquota* Q,size_t Bytes)
{xrtMutexLock(&Q->Gate);--Q->Info.Results;Q->Info.ResultBytes-=Bytes;xrtMutexUnlock(&Q->Gate);xrpcServerQuotaDrop(Q);}
static void xrpcServerResultDrop(ptr Value,ptr User)
{(void)User;xrpcserverbox* B=Value;xrpcserverquota* Q=B->Quota;size_t Bytes=sizeof(*B)+B->Capacity;xrtErrorFree((xerror*)B->Value.Cause);xrtFree(B);xrpcServerResultRefund(Q,Bytes);}
static void xrpcServerOperationDrop(xrpcserver* S,size_t Bytes,size_t ContextBytes)
{xrpcserverquota* Q=S->Quota;xrtMutexLock(&Q->Gate);--Q->Info.Operations;Q->Info.RequestBytes-=Bytes;Q->Info.ContextBytes-=ContextBytes;xrtCondBroadcast(&Q->Idle);xrtMutexUnlock(&Q->Gate);}
static void xrpcServerJobDropRef(xrpcserverjob* J)
{
    if(xrtAtomic32FetchSub(&J->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrpcserver* S=J->Server;size_t Bytes=J->OwnedBytes;xbytesview Context=xrpcServerContextView(J);
    if(J->ResultOwned){if(J->Box)xrpcServerResultDrop(J->Box,NULL);else xrpcServerResultRefund(S->Quota,J->InitialBoxBytes);}
    xrtCancelDestroy(J->Cancel);if(Context.Size)xrtSecureZero((ptr)Context.Data,Context.Size);
    xrtFree(J);xrpcServerOperationDrop(S,Bytes,Context.Size);
}
static void xrpcServerProducerDrop(const void* Data){xrpcServerJobDropRef((xrpcserverjob*)Data);}
static bool xrpcServerJobCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load(&((const xrpcserverjob*)Data)->References,XMEMORY_RELAXED);return true;}
static bool xrpcServerJobTrace(const void* Data,xrtownershipvisitor Visit,ptr Context)
{(void)Data;(void)Visit;(void)Context;return false;}
static const xrtownershipops xrpcServerJobOps={xrpcServerJobCount,xrpcServerJobTrace};
static const xfutureproducerownershipv1 xrpcServerProducer={sizeof(xfutureproducerownershipv1),xrpcServerProducerDrop};
static xrpcserverstatus xrpcServerStop(xrpcserverjob* J)
{if(xrtTimer()>=J->Deadline)return XRPC_SERVER_TIMEOUT;if(xrtCancelRequested(J->Cancel))return XRPC_SERVER_CANCELLED;return XRPC_SERVER_DONE;}
static bool xrpcServerGrow(xrpcserverjob* J,size_t Need)
{
    xrpcserverbox* B=J->Box;if(Need<=B->Capacity)return true;
    size_t Limit=J->Server->Config.Dispatch.MaxOutputBytes,Capacity=B->Capacity;
    while(Capacity<Need){if(Capacity>Limit/2u){Capacity=Limit;break;}Capacity*=2u;}
    /* Core realloc may allocate/copy before freeing the old block. Reserve
     * the entire new block alongside the old one, including its header. */
    size_t OldBytes=sizeof(*B)+B->Capacity,NewBytes=sizeof(*B)+Capacity;xrpcserverquota* Q=B->Quota;
    xrtMutexLock(&Q->Gate);bool Fits=NewBytes<=J->Server->Config.MaxResultBytes-Q->Info.ResultBytes;
    if(Fits)Q->Info.ResultBytes+=NewBytes;
    xrtMutexUnlock(&Q->Gate);
    if(!Fits){J->OutputFailure=XRPC_SERVER_LIMIT;return xrpcServerReport(XERR_RANGE,XRPC_SERVER_RESOURCE,"RPC retained result budget exhausted");}
    xrpcserverbox* New=xrtRealloc(B,NewBytes);
    if(!New){xrtMutexLock(&Q->Gate);Q->Info.ResultBytes-=NewBytes;xrtMutexUnlock(&Q->Gate);J->OutputFailure=XRPC_SERVER_MEMORY;return false;}
    New->Capacity=Capacity;J->Box=New;xrtMutexLock(&Q->Gate);Q->Info.ResultBytes-=OldBytes;xrtMutexUnlock(&Q->Gate);return true;
}
static bool xrpcServerEmit(const xrpcoutgoing* Response,bool Batch,size_t Bytes,ptr Data)
{
    xrpcserverjob* J=Data;if(xrpcServerStop(J)!=XRPC_SERVER_DONE)return false;
    size_t Extra=Batch?(J->OutputBytes?1u:2u):0u,Limit=J->Server->Config.Dispatch.MaxOutputBytes;
    if(Extra>Limit-J->OutputBytes || Bytes>Limit-J->OutputBytes-Extra){J->OutputFailure=XRPC_SERVER_LIMIT;return false;}
    size_t Need=J->OutputBytes+Extra+Bytes;if(!xrpcServerGrow(J,Need))return false;
    unsigned char* P=J->Box->Data;size_t At=J->OutputBytes;
    if(Batch){if(At)P[At-1]=',';else P[At++]='[';if(J->OutputBytes)At=J->OutputBytes;}
    size_t Written=0;if(!xrpcEncodeWrite(P+At,Bytes,Response,1,false,&J->Server->Config.Dispatch.Reply,&Written) || Written!=Bytes)return false;
    At+=Bytes;if(Batch)P[At++]=']';J->OutputBytes=At;return true;
}
static bool xrpcServerInvoke(const xrpcmessage* Request,xrpcreply* Reply,ptr Data)
{
    xrpcserverjob* J=Data;if(xrpcServerStop(J)!=XRPC_SERVER_DONE){(void)xrpcReplyAbort(Reply,NULL);return true;}
    xrpcservercontext Context={J->Cancel,J->Deadline,xrpcServerContextView(J)};J->Entered=true;
    bool Handled=J->Server->Config.Call(Request,Reply,&Context,J->Server->Config.User);
    if(xrpcServerStop(J)!=XRPC_SERVER_DONE)(void)xrpcReplyAbort(Reply,NULL);
    return Handled;
}
static void xrpcServerFinish(xrpcserverjob* J,xrpcserverstatus Status)
{
    J->Finished=true;xrpcserverbox* B=J->Box;
    B->Value.Status=Status;B->Value.MayHaveExecuted=J->Entered;B->Value.Dispatch=J->Dispatch;
    if(Status==XRPC_SERVER_DONE)B->Value.Wire=(xstrview){(cstr)B->Data,J->OutputBytes};
    else{B->Value.Wire=(xstrview){0};if(Status!=XRPC_SERVER_TIMEOUT && Status!=XRPC_SERVER_CANCELLED)B->Value.Cause=xrtTakeError();}
    xrpcserverquota* Q=J->Server->Quota;xrtMutexLock(&Q->Gate);++Q->Info.Completed;xrtMutexUnlock(&Q->Gate);
    J->Box=NULL;J->ResultOwned=false;
    if(!xrtPromiseResolveOwned(J->Promise,B,xrpcServerResultDrop,NULL)){
        J->Box=B;J->ResultOwned=true;xrtMutexLock(&Q->Gate);++Q->Info.PublicationFailures;xrtMutexUnlock(&Q->Gate);
    }
}
static void xrpcServerRun(ptr Data)
{
    xrpcserverjob* J=Data;xrpcserver* S=J->Server;xrpcserverquota* Q=S->Quota;
    xrtMutexLock(&Q->Gate);size_t Slot=S->FreeSlot;S->FreeSlot=S->Slots[Slot].Next;++Q->Info.Running;xrtMutexUnlock(&Q->Gate);
    xrpcserverstatus Status=xrpcServerStop(J);
    if(Status==XRPC_SERVER_DONE){
        xrtClearError();bool OK=xrpcDispatch((xstrview){(cstr)J->Input,J->InputBytes},&S->Config.Dispatch,S->Slots[Slot].Scratch,S->Config.Dispatch.Decode.MaxMessages,
            S->Config.Call?xrpcServerInvoke:NULL,J,xrpcServerEmit,J,&J->Dispatch);
        Status=xrpcServerStop(J);
        if(Status==XRPC_SERVER_DONE && !OK){
            if(J->OutputFailure!=XRPC_SERVER_DONE)Status=J->OutputFailure;
            else if(xrtErrorKind(xrtGetError())==XERR_MEMORY)Status=XRPC_SERVER_MEMORY;
            else if(xrtErrorCode(xrtGetError())==XRPC_DISPATCH_LIMIT || xrtErrorCode(xrtGetError())==XRPC_CODEC_LIMIT || xrtErrorCode(xrtGetError())==XRPC_CODEC_CAPACITY)Status=XRPC_SERVER_LIMIT;
            else Status=XRPC_SERVER_LOCAL_ERROR;
        }
    }
    xrtMutexLock(&Q->Gate);S->Slots[Slot].Next=S->FreeSlot;S->FreeSlot=Slot;--Q->Info.Running;xrtMutexUnlock(&Q->Gate);
    xrpcServerFinish(J,Status);
}
static void xrpcServerJobDrop(ptr Data,ptr Context)
{
    (void)Context;xrpcserverjob* J=Data;
    if(!J->Finished)xrpcServerFinish(J,xrpcServerStop(J)==XRPC_SERVER_TIMEOUT?XRPC_SERVER_TIMEOUT:XRPC_SERVER_CANCELLED);
    xpromise* P=J->Promise;J->Promise=NULL;xrtPromiseDestroy(P);xrpcServerJobDropRef(J);
}
XRPC_API bool xrpcServerConfigInit(xrpcserverconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"invalid server config output");
    *C=(xrpcserverconfig){0};C->Size=sizeof(*C);C->Executor=(xexecutorconfig){2,64,0};xrpcDispatchConfigInit(&C->Dispatch);
    C->MaxOperations=128;C->MaxRequestBytesTotal=8u*1024u*1024u;C->MaxResults=1024;C->MaxResultBytes=8u*1024u*1024u;
    C->MaxWorkspaceBytes=4u*1024u*1024u;C->InitialOutputBytes=256;C->MaxContextBytes=XRPC_SERVER_CONTEXT_BYTES_DEFAULT;return true;
}
XRPC_API bool xrpcServerCreate(const xrpcserverconfig* Config,xrpcserver** Result)
{
    if(!Result || !xrtMemRangeValid(Result,sizeof(*Result)) || (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Result,sizeof(*Result)))))goto Argument;
    xrpcserverconfig C;if(Config)C=*Config;else xrpcServerConfigInit(&C);
    const xrpcdecodeconfig* D=&C.Dispatch.Decode;const xrpcencodeconfig* E=&C.Dispatch.Reply;
    if(C.Size!=sizeof(C) || !C.Executor.Threads || C.Executor.Threads>XRT_EXECUTOR_THREAD_LIMIT || !C.Executor.QueueLimit || !C.MaxOperations ||
        !C.MaxRequestBytesTotal || !C.MaxResults || !C.MaxResultBytes || !C.MaxWorkspaceBytes || !C.InitialOutputBytes || C.MaxContextBytes>XRPC_SERVER_CONTEXT_BYTES_MAX ||
        C.Dispatch.Size!=sizeof(C.Dispatch) || !C.Dispatch.MaxReplies || !C.Dispatch.MaxOutputBytes || C.Dispatch.MaxOutputBytes>SIZE_MAX-sizeof(xrpcserverbox) ||
        D->Size!=sizeof(*D) || D->Mode!=XRPC_DECODE_REQUESTS || !D->MaxInputBytes || !D->MaxDepth || D->MaxDepth>XJSON_DEPTH_DEFAULT ||
        !D->MaxStringBytes || !D->MaxValues || !D->MaxContainerItems || !D->MaxMessages || !D->MaxIdBytes || !D->MaxMethodBytes ||
        E->Size!=sizeof(*E) || !E->MaxOutputBytes || !E->MaxDepth || E->MaxDepth>XJSON_DEPTH_DEFAULT || !E->MaxStringBytes ||
        !E->MaxValues || !E->MaxContainerItems || !E->MaxMessages || !E->MaxIdBytes || !E->MaxMethodBytes)goto Argument;
    size_t Base=sizeof(xrpcserver)+(size_t)C.Executor.Threads*sizeof(xrpcserverslot);
    if(D->MaxMessages>(SIZE_MAX-Base)/(sizeof(xrpcmessage)*(size_t)C.Executor.Threads))goto Argument;
    size_t Bytes=Base+D->MaxMessages*sizeof(xrpcmessage)*(size_t)C.Executor.Threads;
    if(Bytes>C.MaxWorkspaceBytes || sizeof(xrpcserverquota)>C.MaxWorkspaceBytes-Bytes)goto Argument;
    xrpcserver* S=xrtCalloc(1,Bytes);if(!S)return false;S->Config=C;S->OwnedBytes=Bytes;S->FreeSlot=0;
    xrpcserverquota* Q=xrtCalloc(1,sizeof(*Q));if(!Q){xrtFree(S);return false;}S->Quota=Q;xrtAtomic32Init(&Q->References,1);
    if(!xrtMutexInit(&Q->Gate)){xrtFree(Q);xrtFree(S);return false;}
    if(!xrtCondInit(&Q->Idle)){xrtMutexUnit(&Q->Gate);xrtFree(Q);xrtFree(S);return false;}
    Q->Info.Size=sizeof(Q->Info);Q->Info.Accepting=true;Q->Info.WorkspaceBytes=Bytes+sizeof(*Q);
    for(size_t K=0;K<C.Executor.Threads;++K){S->Slots[K].Next=K+1;S->Slots[K].Scratch=(xrpcmessage*)((unsigned char*)S+Base)+K*D->MaxMessages;}
    S->Cancel=xrtCancelCreate();S->Executor=S->Cancel?xrtExecutorCreate(&C.Executor):NULL;
    if(!S->Executor){xerror* X=xrtTakeError();xrtCancelDestroy(S->Cancel);xrpcServerQuotaDrop(Q);xrtFree(S);xrtClearError();xrtSetErrorTake(X);return false;}
    *Result=S;return true;
Argument:
    return xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"invalid server configuration/output span");
}
xrpcserveradmission xrpcServerSubmitContextEntered(xrpcserver* S,xstrview Input,xbytesview Context,int64 Timeout,xfuture** Future,double Entered)
{
    if(!S || !Future || Timeout<XRT_WAIT_FOREVER || !xrtMemRangeValid(Future,sizeof(*Future)) || !xrtMemRangeValid(Input.Data,Input.Size) ||
        !xrtMemRangeValid(Context.Data,Context.Size) || xrtMemRangesOverlap(Future,sizeof(*Future),Context.Data,Context.Size) ||
        xrtMemRangesOverlap(Future,sizeof(*Future),Input.Data,Input.Size) || xrtMemRangesOverlap(Future,sizeof(*Future),S,S->OwnedBytes)){
        xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"invalid server submission span/timeout");return XRPC_SERVER_ADMISSION_ERROR;}
    if(Input.Size>SIZE_MAX-sizeof(xrpcserverjob)){xrpcServerReport(XERR_RANGE,XRPC_SERVER_RESOURCE,"server request size overflows");return XRPC_SERVER_ADMISSION_ERROR;}
    size_t JobBytes=sizeof(xrpcserverjob)+Input.Size,Capacity=S->Config.InitialOutputBytes;
    size_t ContextAt=JobBytes;
    if(Context.Size){size_t Align=_Alignof(max_align_t),Padding=(Align-JobBytes%Align)%Align;
        if(Padding>SIZE_MAX-JobBytes || Context.Size>SIZE_MAX-JobBytes-Padding){xrpcServerReport(XERR_RANGE,XRPC_SERVER_RESOURCE,"server context size overflows");return XRPC_SERVER_ADMISSION_ERROR;}
        ContextAt+=Padding;JobBytes=ContextAt+Context.Size;}
    if(Capacity>S->Config.Dispatch.MaxOutputBytes)Capacity=S->Config.Dispatch.MaxOutputBytes;
    size_t BoxBytes=sizeof(xrpcserverbox)+Capacity;xrpcserverquota* Q=S->Quota;xerror* Ambient=xrtTakeError();
    xrtMutexLock(&Q->Gate);xrpcserveradmission A=XRPC_SERVER_ACCEPTED;
    if(!Q->Info.Accepting)A=XRPC_SERVER_CLOSED;
    else if(Q->Info.Operations>=S->Config.MaxOperations || Q->Info.Results>=S->Config.MaxResults || Input.Size>S->Config.Dispatch.Decode.MaxInputBytes || Context.Size>S->Config.MaxContextBytes ||
        JobBytes>S->Config.MaxRequestBytesTotal-Q->Info.RequestBytes || BoxBytes>S->Config.MaxResultBytes-Q->Info.ResultBytes ||
        xrtAtomic32Load(&Q->References,XMEMORY_RELAXED)>=UINT32_MAX/2u)A=XRPC_SERVER_OVERLOADED;
    if(A!=XRPC_SERVER_ACCEPTED){++Q->Info.Rejected;xrtMutexUnlock(&Q->Gate);xrtSetErrorTake(Ambient);return A;}
    ++Q->Info.Operations;Q->Info.RequestBytes+=JobBytes;Q->Info.ContextBytes+=Context.Size;++Q->Info.Results;Q->Info.ResultBytes+=BoxBytes;
    xrtAtomic32FetchAdd(&Q->References,1,XMEMORY_RELAXED);xrtMutexUnlock(&Q->Gate);
    xrpcserverjob* J=xrtMalloc(JobBytes);xfuture* F=NULL;
    if(!J){xrpcServerResultRefund(Q,BoxBytes);xrpcServerOperationDrop(S,JobBytes,Context.Size);goto NoJob;}
    *J=(xrpcserverjob){0};J->Server=S;J->OwnedBytes=JobBytes;J->InputBytes=Input.Size;J->InitialBoxBytes=BoxBytes;J->ResultOwned=true;
    J->Deadline=xrpcServerDeadline(Entered,Timeout);J->Dispatch.Size=sizeof(J->Dispatch);xrtAtomic32Init(&J->References,1);
    if(Input.Size)memcpy(J->Input,Input.Data,Input.Size);
    if(Context.Size)memcpy((unsigned char*)J+ContextAt,Context.Data,Context.Size);
    J->Box=xrtCalloc(1,BoxBytes);if(!J->Box)goto Error;J->Box->Quota=Q;J->Box->Capacity=Capacity;
    J->Promise=xrtPromiseCreate(&F,S->Cancel);if(!J->Promise)goto Error;J->Cancel=xrtPromiseCancelToken(J->Promise);if(!J->Cancel)goto Error;
    xrtAtomic32FetchAdd(&J->References,1,XMEMORY_RELAXED);
    if(!xrtPromiseProducerBindTakeV1(J->Promise,(xrtownershipref){J,&xrpcServerJobOps},&xrpcServerProducer)){xrpcServerJobDropRef(J);goto Error;}
    xrtMutexLock(&Q->Gate);
    if(!Q->Info.Accepting)A=XRPC_SERVER_CLOSED;
    else if(!xrtExecutorSubmit(S->Executor,xrpcServerRun,J,xrpcServerJobDrop,NULL)){
        xerrkind Kind=xrtErrorKind(xrtGetError());A=Kind==XERR_AGAIN?XRPC_SERVER_OVERLOADED:Kind==XERR_CLOSED?XRPC_SERVER_CLOSED:XRPC_SERVER_ADMISSION_ERROR;}
    if(A==XRPC_SERVER_ACCEPTED)++Q->Info.Accepted;else if(A!=XRPC_SERVER_ADMISSION_ERROR)++Q->Info.Rejected;
    xrtMutexUnlock(&Q->Gate);
    if(A==XRPC_SERVER_ACCEPTED){*Future=F;xrtClearError();xrtSetErrorTake(Ambient);return A;}
Error:
    {xerror* X=xrtTakeError();xpromise* P=J->Promise;J->Promise=NULL;xrtPromiseDestroy(P);xrtFutureDestroy(F);xrpcServerJobDropRef(J);xrtClearError();xrtSetErrorTake(X);}
NoJob:
    if(A==XRPC_SERVER_OVERLOADED || A==XRPC_SERVER_CLOSED){xrtClearError();xrtSetErrorTake(Ambient);return A;}
    xrtErrorFree(Ambient);return XRPC_SERVER_ADMISSION_ERROR;
}
xrpcserveradmission xrpcServerSubmitEntered(xrpcserver* S,xstrview Input,int64 Timeout,xfuture** Future,double Entered)
{return xrpcServerSubmitContextEntered(S,Input,(xbytesview){0},Timeout,Future,Entered);}
XRPC_API xrpcserveradmission xrpcServerSubmitWithContext(xrpcserver* S,xstrview Input,xbytesview Context,int64 Timeout,xfuture** Future)
{return xrpcServerSubmitContextEntered(S,Input,Context,Timeout,Future,xrtTimer());}
XRPC_API xrpcserveradmission xrpcServerSubmit(xrpcserver* S,xstrview Input,int64 Timeout,xfuture** Future)
{return xrpcServerSubmitEntered(S,Input,Timeout,Future,xrtTimer());}
xcancel* xrpcServerCancelChild(xrpcserver* S){return xrtCancelChild(S->Cancel);}
static xrpcserverstatus xrpcServerWorkStatus(xrpcserverwork* W)
{return xrtTimer()>=W->Deadline?XRPC_SERVER_TIMEOUT:xrtCancelRequested(W->Cancel)?XRPC_SERVER_CANCELLED:XRPC_SERVER_DONE;}
static void xrpcServerWorkRun(ptr Data)
{
    xrpcserverwork* W=Data;xrpcserverquota* Q=W->Server->Quota;
    xrtMutexLock(&Q->Gate);++Q->Info.Running;xrtMutexUnlock(&Q->Gate);
    W->Called=true;W->Call(W,xrpcServerWorkStatus(W));
    xrtMutexLock(&Q->Gate);--Q->Info.Running;xrtMutexUnlock(&Q->Gate);
}
static void xrpcServerWorkDrop(ptr Data,ptr Context)
{
    (void)Context;xrpcserverwork* W=Data;xrpcserverquota* Q=W->Server->Quota;
    if(!W->Called)W->Call(W,xrpcServerWorkStatus(W)==XRPC_SERVER_TIMEOUT?XRPC_SERVER_TIMEOUT:XRPC_SERVER_CANCELLED);
    /* Return queue credit before notifying the transport, so a one-slot
     * executor can admit the now-authorized document. Native drop/join remains
     * the physical barrier and transport-owned storage retains its own quota. */
    xrtMutexLock(&Q->Gate);--Q->Info.Operations;--Q->Info.Auxiliary;Q->Info.RequestBytes-=W->Bytes;++Q->Info.AuxiliaryCompleted;xrtCondBroadcast(&Q->Idle);xrtMutexUnlock(&Q->Gate);
    W->Drop(W);
}
xrpcserveradmission xrpcServerWorkSubmit(xrpcserver* S,xrpcserverwork* W)
{
    xerror* Ambient=xrtTakeError();xrpcserverquota* Q=S->Quota;xrpcserveradmission A=XRPC_SERVER_ACCEPTED;
    xrtMutexLock(&Q->Gate);
    if(!Q->Info.Accepting)A=XRPC_SERVER_CLOSED;
    else if(Q->Info.Operations>=S->Config.MaxOperations || W->Bytes>S->Config.MaxRequestBytesTotal-Q->Info.RequestBytes)A=XRPC_SERVER_OVERLOADED;
    else{W->Server=S;
        if(!xrtExecutorSubmit(S->Executor,xrpcServerWorkRun,W,xrpcServerWorkDrop,NULL)){
            xerrkind K=xrtErrorKind(xrtGetError());A=K==XERR_AGAIN?XRPC_SERVER_OVERLOADED:K==XERR_CLOSED?XRPC_SERVER_CLOSED:XRPC_SERVER_ADMISSION_ERROR;
        }else{++Q->Info.Operations;++Q->Info.Auxiliary;Q->Info.RequestBytes+=W->Bytes;++Q->Info.AuxiliaryAccepted;}}
    xrtMutexUnlock(&Q->Gate);
    if(A!=XRPC_SERVER_ADMISSION_ERROR){xrtClearError();xrtSetErrorTake(Ambient);}else xrtErrorFree(Ambient);return A;
}
XRPC_API bool xrpcServerInfo(xrpcserver* S,xrpcserverinfo* Info)
{
    if(!S || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || Info->Size!=sizeof(*Info) || xrtMemRangesOverlap(Info,sizeof(*Info),S,S->OwnedBytes))
        return xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"invalid server info output");
    xrtMutexLock(&S->Quota->Gate);*Info=S->Quota->Info;xrtMutexUnlock(&S->Quota->Gate);return true;
}
XRPC_API bool xrpcServerClose(xrpcserver* S)
{if(!S)return xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"server is NULL");xrtMutexLock(&S->Quota->Gate);S->Quota->Info.Accepting=false;xrtMutexUnlock(&S->Quota->Gate);return xrtExecutorClose(S->Executor);}
XRPC_API bool xrpcServerCancel(xrpcserver* S)
{
    if(!S)return xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"server is NULL");
    xerror* Ambient=xrtTakeError();
    xrtMutexLock(&S->Quota->Gate);S->Quota->Info.Accepting=false;S->Quota->Info.Cancelling=true;xrtMutexUnlock(&S->Quota->Gate);
    bool OK=(xrtCancelRequest(S->Cancel) || xrtCancelRequested(S->Cancel)) && xrtExecutorCancel(S->Executor);
    if(OK){xrtClearError();xrtSetErrorTake(Ambient);}else xrtErrorFree(Ambient);
    return OK;
}
XRPC_API xwaitresult xrpcServerWaitFor(xrpcserver* S,int64 Timeout)
{
    double Entered=xrtTimer();if(!S || Timeout<XRT_WAIT_FOREVER){xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"invalid server wait");return XWAIT_ERROR;}
    double Deadline=xrpcServerDeadline(Entered,Timeout);if(!xrpcServerClose(S))return XWAIT_ERROR;
    xwaitresult R=xrtExecutorWaitFor(S->Executor,xrpcServerRemaining(Deadline));if(R!=XWAIT_OK)return R;
    xrpcserverquota* Q=S->Quota;xrtMutexLock(&Q->Gate);
    while(Q->Info.Operations){int64 Left=xrpcServerRemaining(Deadline);if(!Left){R=XWAIT_TIMEOUT;break;}
        R=xrtCondWaitFor(&Q->Idle,&Q->Gate,Left);if(R==XWAIT_ERROR)break;if(!Q->Info.Operations){R=XWAIT_OK;break;}if(R==XWAIT_TIMEOUT)break;}
    xrtMutexUnlock(&Q->Gate);return R;
}
XRPC_API bool xrpcServerDestroy(xrpcserver* S)
{
    if(!S)return true;
    if(xrpcServerWaitFor(S,XRT_WAIT_FOREVER)!=XWAIT_OK || !xrtExecutorDestroy(S->Executor))return false;
    xrtCancelDestroy(S->Cancel);xrpcServerQuotaDrop(S->Quota);xrtFree(S);return true;
}
#endif
