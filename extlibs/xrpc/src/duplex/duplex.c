#include <xrpc/duplex.h>
#include "../internal/xrpc_call_internal.h"
#include "../internal/xrpc_pending_internal.h"
#include "../internal/xrpc_result_quota.h"
#include "../internal/xrpc_string.h"
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_DUPLEX)
typedef struct xrpcduplexrequest xrpcduplexrequest;
typedef struct xrpcduplexentry {
    xrpcduplexrequest* Request;
    xrpcpendingtoken Token;
    uint64 Id;
    size_t Index;
} xrpcduplexentry;
typedef struct xrpcduplexhostjob {
    struct xrpcduplexhostjob* Next;
    uint64 Token;
    size_t Count;
    bool Batch;
    xstrview Ids[];
} xrpcduplexhostjob;

struct xrpcduplexoutput {
    xatomic32 References;
    struct xrpcduplexoutput *Next,*Prev;
    xrpcduplexrequest* Request; /* Owner-only; cleared before request retirement. */
    xstrview Wire;
    bool Linked,Started;
};
struct xrpcduplexrequest {
    xatomic32 References;
    xrpcduplex* Duplex;
    xrpcduplexrequest *Next,*Prev,*SubmitNext;
    xrpccall* Call;
    xrpcduplexoutput* Output;
    xrpcresponses* Responses;
    size_t Count,Replies;
    double Deadline;
    bool Batch,Started,Sent,Finishing,Finished;
    xrpccallstatus Status;
    uint32 DeadlineSlot;
    xrpcduplexentry Entries[];
};
struct xrpcduplex {
    xrpcduplexconfig Config;
    xmutex Gate;
    xatomic32 References;
    xrpcresultquota *Control,*Wire,*Results;
    xrpcpending* Pending;
    xrpcduplexrequest *SubmitHead,*SubmitTail,*Head,*Tail,*Scan,*ScanEnd;
    xrpcduplexrequest** Deadlines;
    xrpcduplexoutput *OutputHead,*OutputTail,*Selection;
    xrpcduplexhostjob* HostJobs;
    uint64 HostSerial;
    size_t Operations,PendingCalls,Incoming,DeadlineCount,Finishing,OwnedBytes,FixedBytes;
    size_t Constructing,TemporaryBytes,PeakTemporaryBytes;
    uint64 Serial,SelectionSerial,SelectedToken;
    uint64 Accepted,Rejected,Completed,Received,UnknownResponses,ProtocolFailures;
    bool Closing,Busy,More,PreferScan;
    unsigned Turn;
    xrpccallstatus Reason;
    xrpcmessage* Scratch;
    xrpcpendingresolution* Resolutions;
    xstrview* CorrelationIds;
    char* CorrelationText;
};

static bool xrpcDuplexReport(xerrkind Kind,xrpcduplexerror Code,cstr Text)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Text);return false;}
static bool xrpcDuplexSpan(xrpcduplex* D,const void* P,size_t N)
{return xrtMemRangeValid(P,N) && !xrtMemRangesOverlap(D,D->OwnedBytes,P,N);}
static bool xrpcDuplexEnter(xrpcduplex* D)
{
    if(!D || D->Busy)return xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC duplex missing owner or mutating reentry");
    D->Busy=true;return true;
}
static void xrpcDuplexWake(xrpcduplex* D)
{if(D->Config.Wake)D->Config.Wake(D->Config.User);}
static void xrpcDuplexDrop(ptr Data)
{xrpcduplexrequest* R=Data;if(xrtAtomic32FetchSub(&R->References,1,XMEMORY_ACQ_REL)!=1)return;
 xrpcduplex* D=R->Duplex;xrpcResultQuotaFree(R);xrtAtomic32FetchSub(&D->References,1,XMEMORY_RELEASE);}
static void xrpcDuplexCallWake(ptr Data)
{xrpcduplexrequest* R=Data;xrpcDuplexWake(R->Duplex);}
static size_t* xrpcDuplexMap(xrpcduplexrequest* R)
{return (size_t*)(R->Entries+R->Count);}
static xstrview xrpcDuplexId(uint64 Id,char* Buffer)
{int N=snprintf(Buffer,24,"\"%llu\"",(unsigned long long)Id);return (xstrview){Buffer,(size_t)N};}
XRPC_API bool xrpcDuplexConfigInit(xrpcduplexconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex config output");
    *C=(xrpcduplexconfig){0};C->Size=sizeof(*C);xrpcEncodeConfigInit(&C->Encode);xrpcResponseConfigInit(&C->Response);
    C->MaxOperations=128;C->MaxPendingCalls=1024;C->MaxIncoming=64;C->MaxControlBytes=2u*1024u*1024u;
    C->MaxOutputs=256;C->MaxOutputBytes=8u*1024u*1024u;C->MaxResults=1024;C->MaxResultBytes=8u*1024u*1024u;
    C->MaxFixedBytes=16u*1024u*1024u;C->MaxTemporaryBytes=1024u*1024u;C->MaxWork=64;return true;
}
XRPC_API bool xrpcDuplexCreate(const xrpcduplexconfig* Config,xrpcduplex** Result)
{
    if(!Result || !xrtMemRangeValid(Result,sizeof(*Result)) || (Config &&
       (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Result,sizeof(*Result)))))
        return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex constructor spans");
    xrpcduplexconfig C;if(Config)C=*Config;else xrpcDuplexConfigInit(&C);
    if(C.Size!=sizeof(C) || !C.MaxOperations || C.MaxOperations>65536 || !C.MaxIncoming || C.MaxIncoming>C.MaxOperations ||
       !C.MaxPendingCalls || C.MaxPendingCalls>65536 || !C.MaxControlBytes || !C.MaxFixedBytes || !C.MaxOutputs || C.MaxOutputs>65536 ||
       !C.MaxOutputBytes || !C.MaxResults || C.MaxResults>65536 || !C.MaxResultBytes || !C.MaxTemporaryBytes ||
       !C.MaxWork || C.MaxWork>65536 || C.Encode.Size!=sizeof(C.Encode) ||
       C.Response.Size!=sizeof(C.Response) || C.Response.Decode.Size!=sizeof(C.Response.Decode) ||
       C.Response.Decode.Mode!=XRPC_DECODE_RESPONSES || !C.Response.MaxOwnedBytes ||
       !C.Response.Decode.MaxMessages || C.Response.Decode.MaxMessages>65536 ||
       !C.Encode.MaxMessages || C.Encode.MaxMessages>65536)
        return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex configuration");
    /* Exercise complete nested configs with allocation-free public validators. */
    xrpcoutgoing Check={0};Check.Kind=XRPC_MESSAGE_NOTIFICATION;Check.Method=XRT_STR_LITERAL("x");size_t N;
    xrpcmessage M;xrpcdocument Doc;
    if(!xrpcEncodeSize(&Check,1,false,&C.Encode,&N) ||
       xrpcDecode(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":null}"),&C.Response.Decode,&M,1,&Doc)!=XRPC_DECODE_DONE)return false;
    size_t Count=C.Response.Decode.MaxMessages;
    size_t Fixed=sizeof(xrpcduplex)+C.MaxOperations*sizeof(xrpcduplexrequest*);
    size_t PerMessage=sizeof(xrpcmessage)+sizeof(xrpcpendingresolution)+sizeof(xstrview)+24u;
    if(Count>(SIZE_MAX-Fixed)/PerMessage)
        return xrpcDuplexReport(XERR_RANGE,XRPC_DUPLEX_LIMIT,"RPC duplex storage overflow");
    size_t Bytes=Fixed+Count*PerMessage;
    xrpcduplex* D=xrtCalloc(1,Bytes);if(!D)goto Memory;
    D->Config=C;D->OwnedBytes=Bytes;D->Scratch=(xrpcmessage*)(D+1);D->Resolutions=(xrpcpendingresolution*)(D->Scratch+Count);
    D->CorrelationIds=(xstrview*)(D->Resolutions+Count);D->CorrelationText=(char*)(D->CorrelationIds+Count);
    D->Deadlines=(xrpcduplexrequest**)(D->CorrelationText+Count*24u);
    if(!xrtMutexInit(&D->Gate)){xrtFree(D);return false;}
    xrtAtomic32Init(&D->References,1);
    xrpcpendingconfig P;xrpcPendingConfigInit(&P);P.MaxCalls=C.MaxPendingCalls;
    /* Only our canonical nonzero uint64 decimal string IDs enter this table.
     * Peer IDs are fully validated by the response codec, then filtered below.
     * The comparison key is one type byte plus at most twenty digits. */
    P.Id.MaxInputBytes=22;P.Id.MaxStringBytes=20;P.Id.MaxKeyBytes=21;
    if(Bytes>=C.MaxFixedBytes){xrpcDuplexReport(XERR_RANGE,XRPC_DUPLEX_LIMIT,"RPC duplex fixed-storage limit");goto Failed;}
    P.MaxOwnedBytes=C.MaxFixedBytes-Bytes;
    if(!xrpcPendingCreate(&P,&D->Pending))goto Failed;
    if(!xrpcResultQuotaCreate(C.MaxOperations,C.MaxControlBytes,&D->Control) ||
       !xrpcResultQuotaCreate(C.MaxOutputs,C.MaxOutputBytes,&D->Wire) ||
       !xrpcResultQuotaCreate(C.MaxResults,C.MaxResultBytes,&D->Results)){
        xrpcDuplexReport(XERR_MEMORY,XRPC_DUPLEX_MEMORY,"RPC duplex ledger construction failed");goto Failed;}
    xrpcpendinginfo PI={0};PI.Size=sizeof(PI);xrpcPendingInfo(D->Pending,&PI);D->FixedBytes=Bytes+PI.OwnedBytes;
    *Result=D;return true;
Failed:
    xrpcPendingDestroy(D->Pending);xrpcResultQuotaRelease(D->Control);xrpcResultQuotaRelease(D->Wire);
    xrpcResultQuotaRelease(D->Results);xrtMutexUnit(&D->Gate);xrtFree(D);return false;
Memory:
    return xrpcDuplexReport(XERR_MEMORY,XRPC_DUPLEX_MEMORY,"RPC duplex construction failed");
}
XRPC_API bool xrpcDuplexOutputRetain(xrpcduplexoutput* O)
{
    if(!O)return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"RPC duplex output is NULL");
    uint32 N=xrtAtomic32Load(&O->References,XMEMORY_RELAXED);
    for(;;){if(N==UINT32_MAX)return xrpcDuplexReport(XERR_RANGE,XRPC_DUPLEX_LIMIT,"RPC duplex output reference overflow");
        if(xrtAtomic32CompareExchange(&O->References,&N,N+1u,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
XRPC_API void xrpcDuplexOutputRelease(xrpcduplexoutput* O)
{if(O && xrtAtomic32FetchSub(&O->References,1,XMEMORY_ACQ_REL)==1){xrpcResultQuotaFree(O);}}
XRPC_API xstrview xrpcDuplexOutputWire(const xrpcduplexoutput* O)
{return O?O->Wire:(xstrview){0};}
static xrpcduplexoutput* xrpcDuplexOutputCreate(xrpcduplex* D,size_t Allocation,size_t Charge,bool* Limited)
{
    if(Allocation>SIZE_MAX-sizeof(xrpcduplexoutput) || Charge>SIZE_MAX-sizeof(xrpcduplexoutput)){*Limited=true;return NULL;}
    xrpcduplexoutput* O=xrpcResultQuotaAllocCharged(D->Wire,sizeof(*O)+Allocation,sizeof(*O)+Charge,Limited);
    if(O){memset(O,0,sizeof(*O));xrtAtomic32Init(&O->References,1);}return O;
}
static void xrpcDuplexConstructionReturn(xrpcduplex* D)
{
    /* Wake can race the owner seeing the final constructor pin. Closing Info
     * supplies a retry hint until the last physical reference has retired. */
    xerror* Error=xrtTakeError();xrpcDuplexWake(D);
    xrtAtomic32FetchSub(&D->References,1,XMEMORY_RELEASE);
    xrtClearError();xrtSetErrorTake(Error);
}
static xrpcduplexadmission xrpcDuplexSubmit(xrpcduplex* D,const xrpcduplexitem* Items,
    size_t Count,bool Batch,int64 Timeout,xfuture** Future,double Entered)
{
    if(!D || !Count || Count>D->Config.Encode.MaxMessages || (!Batch && Count!=1) || Timeout<XRT_WAIT_FOREVER ||
       Count>SIZE_MAX/sizeof(*Items) || !xrpcDuplexSpan(D,Items,Count*sizeof(*Items)) ||
       !Future || !xrpcDuplexSpan(D,Future,sizeof(*Future)) || xrtMemRangesOverlap(Items,Count*sizeof(*Items),Future,sizeof(*Future)))goto Argument;
    size_t Replies=0;
    for(size_t I=0;I<Count;++I){const xrpcduplexitem* V=Items+I;
        if(!xrpcDuplexSpan(D,V->Method.Data,V->Method.Size) || !xrpcDuplexSpan(D,V->Params.Data,V->Params.Size) ||
           xrtMemRangesOverlap(Future,sizeof(*Future),V->Method.Data,V->Method.Size) ||
           xrtMemRangesOverlap(Future,sizeof(*Future),V->Params.Data,V->Params.Size))goto Argument;
        if(!V->Notify)++Replies;
    }
    if(Count>SIZE_MAX/(sizeof(xrpcoutgoing)+24u) || Count>(SIZE_MAX-sizeof(xrpcduplexrequest))/(sizeof(xrpcduplexentry)+sizeof(size_t)))goto Argument;
    size_t TempBytes=Count*(sizeof(xrpcoutgoing)+24u),Bytes=sizeof(xrpcduplexrequest)+Count*(sizeof(xrpcduplexentry)+sizeof(size_t));
    /* Only reservation/publication are serialized. JSON scans, quoting, copies,
     * allocator/quota locks and native Future construction do not hold Gate. */
    xrtMutexLock(&D->Gate);
    if(D->Closing){xrtMutexUnlock(&D->Gate);return XRPC_DUPLEX_CLOSED;}
    if(D->Operations==D->Config.MaxOperations || Replies>D->Config.MaxPendingCalls-D->PendingCalls ||
       Replies>UINT64_MAX-D->Serial || TempBytes>D->Config.MaxTemporaryBytes-D->TemporaryBytes){
        ++D->Rejected;xrtMutexUnlock(&D->Gate);return XRPC_DUPLEX_OVERLOADED;}
    uint64 Serial=D->Serial;D->Serial+=Replies; /* Gaps on rollback; never reuse. */
    ++D->Operations;D->PendingCalls+=Replies;++D->Constructing;D->TemporaryBytes+=TempBytes;
    if(D->TemporaryBytes>D->PeakTemporaryBytes)D->PeakTemporaryBytes=D->TemporaryBytes;
    xrtAtomic32FetchAdd(&D->References,1,XMEMORY_RELAXED); /* Constructor pin. */
    xrtMutexUnlock(&D->Gate);
    bool Limited=false;xrpcduplexadmission Result=XRPC_DUPLEX_ERROR;
    xrpcduplexrequest* R=xrpcResultQuotaAlloc(D->Control,Bytes,&Limited);
    xrpcduplexoutput* O=NULL;xrpcoutgoing* V=NULL;
    if(!R){if(Limited)Result=XRPC_DUPLEX_OVERLOADED;else goto Memory;goto Rollback;}
    memset(R,0,Bytes);R->Duplex=D;R->Count=Count;R->Replies=Replies;R->Batch=Batch;
    xrtAtomic32Init(&R->References,1);
    xrtAtomic32FetchAdd(&D->References,1,XMEMORY_RELAXED); /* Request/context pin. */
    V=xrtMalloc(TempBytes);if(!V)goto Memory;
    memset(V,0,Count*sizeof(*V));char* Ids=(char*)(V+Count);
    for(size_t I=0;I<Count;++I){V[I].Kind=Items[I].Notify?XRPC_MESSAGE_NOTIFICATION:XRPC_MESSAGE_CALL;
        V[I].Method=Items[I].Method;V[I].Params=Items[I].Params;
        R->Entries[I].Request=R;R->Entries[I].Index=I;
        if(!Items[I].Notify){R->Entries[I].Id=++Serial;V[I].Id=xrpcDuplexId(Serial,Ids+I*24u);}
        xrpcDuplexMap(R)[I]=SIZE_MAX;
    }
    size_t WireBytes=0;
    if(!xrpcEncodeSize(V,Count,Batch,&D->Config.Encode,&WireBytes))goto Rollback;
    O=xrpcDuplexOutputCreate(D,WireBytes,WireBytes,&Limited);
    if(!O){if(Limited){Result=XRPC_DUPLEX_OVERLOADED;goto Rollback;}goto Memory;}
    char* Data=(char*)(O+1);
    if(!xrpcEncodeWrite(Data,WireBytes,V,Count,Batch,&D->Config.Encode,NULL))goto Rollback;
    xrtFree(V);V=NULL;O->Wire=(xstrview){Data,WireBytes};O->Request=R;R->Output=O;
    xrpccallconfig C;xrpcCallConfigInit(&C);C.Timeout=Timeout;C.Wake=xrpcDuplexCallWake;C.Drop=xrpcDuplexDrop;C.User=R;
    xfuture* F=NULL;
    if(!xrpcCallCreateQuota(&C,Batch?Count:0,&R->Call,&F,Entered,D->Results,&Limited)){
        if(Limited)Result=XRPC_DUPLEX_OVERLOADED;
        goto Rollback;}
    xrtAtomic32FetchAdd(&R->References,1,XMEMORY_RELAXED); /* Native context owns one R reference. */
    xrtMutexLock(&D->Gate);
    --D->Constructing;D->TemporaryBytes-=TempBytes;++D->Accepted;
    /* A successful reservation precedes a racing Close. Publish even if it has
     * closed meanwhile: owner completes CLOSED without sending/replaying it. */
    if(D->SubmitTail)D->SubmitTail->SubmitNext=R;else D->SubmitHead=R;D->SubmitTail=R;
    *Future=F;xrtMutexUnlock(&D->Gate);xrpcDuplexConstructionReturn(D);return XRPC_DUPLEX_ACCEPTED;
Memory:
    xrpcDuplexReport(XERR_MEMORY,XRPC_DUPLEX_MEMORY,"RPC duplex admission allocation failed");
Rollback:
    xrtFree(V);xrpcDuplexOutputRelease(O);if(R)xrpcDuplexDrop(R);
    xrtMutexLock(&D->Gate);--D->Operations;D->PendingCalls-=Replies;--D->Constructing;D->TemporaryBytes-=TempBytes;
    if(Result==XRPC_DUPLEX_OVERLOADED)++D->Rejected;
    xrtMutexUnlock(&D->Gate);xrpcDuplexConstructionReturn(D);return Result;
Argument:
    xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex admission spans/count/timeout");return XRPC_DUPLEX_ERROR;
}
XRPC_API xrpcduplexadmission xrpcDuplexCall(xrpcduplex* D,xstrview Method,xstrview Params,int64 Timeout,xfuture** F)
{double Entered=xrtTimer();xrpcduplexitem Item={false,Method,Params};return xrpcDuplexSubmit(D,&Item,1,false,Timeout,F,Entered);}
XRPC_API xrpcduplexadmission xrpcDuplexNotify(xrpcduplex* D,xstrview Method,xstrview Params,int64 Timeout,xfuture** F)
{double Entered=xrtTimer();xrpcduplexitem Item={true,Method,Params};return xrpcDuplexSubmit(D,&Item,1,false,Timeout,F,Entered);}
XRPC_API xrpcduplexadmission xrpcDuplexBatch(xrpcduplex* D,const xrpcduplexitem* Items,size_t Count,int64 Timeout,xfuture** F)
{return xrpcDuplexSubmit(D,Items,Count,true,Timeout,F,xrtTimer());}
static bool xrpcDuplexIsClosing(xrpcduplex* D)
{xrtMutexLock(&D->Gate);bool B=D->Closing;xrtMutexUnlock(&D->Gate);return B;}
static void xrpcDuplexSeal(xrpcduplex* D,xrpccallstatus Reason)
{xrtMutexLock(&D->Gate);if(!D->Closing){D->Closing=true;D->Reason=Reason;}xrtMutexUnlock(&D->Gate);}
XRPC_API bool xrpcDuplexClose(xrpcduplex* D,xrpccallstatus Reason)
{
    if(!D || (Reason!=XRPC_CALL_CLOSED && Reason!=XRPC_CALL_TRANSPORT && Reason!=XRPC_CALL_PROTOCOL && Reason!=XRPC_CALL_LIMIT && Reason!=XRPC_CALL_MEMORY && Reason!=XRPC_CALL_TIMEOUT && Reason!=XRPC_CALL_HTTP_ERROR))
        return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex close reason");
    xrpcDuplexSeal(D,Reason);xrpcDuplexWake(D);return true;
}
static void xrpcDuplexQueue(xrpcduplex* D,xrpcduplexoutput* O)
{O->Linked=true;O->Prev=D->OutputTail;if(O->Prev)O->Prev->Next=O;else D->OutputHead=O;D->OutputTail=O;}
/* The two reserved v1 methods describe a connection-local abandonment
 * extension. Legacy sessions never spend allocation or wire credit on it. */

static void xrpcDuplexUnqueue(xrpcduplex* D,xrpcduplexoutput* O)
{
    if(D->Selection==O){D->Selection=NULL;D->SelectedToken=0;}
    if(O->Linked){if(O->Prev)O->Prev->Next=O->Next;else D->OutputHead=O->Next;
        if(O->Next)O->Next->Prev=O->Prev;else D->OutputTail=O->Prev;O->Linked=false;}
    O->Next=O->Prev=NULL;O->Request=NULL;xrpcDuplexOutputRelease(O);
}
/* Owner-only heap with one slot per finite-deadline operation. The request
 * keeps its index so reply/cancel/close can remove it without leaving tombstones. */
static void xrpcDuplexDeadlineSet(xrpcduplex* D,size_t I,xrpcduplexrequest* R)
{
    D->Deadlines[I]=R;
    R->DeadlineSlot=(uint32)(I+1u);
}
static void xrpcDuplexDeadlineUp(xrpcduplex* D,size_t I,xrpcduplexrequest* R)
{
    while(I){
        size_t Parent=(I-1u)/2u;
        if(D->Deadlines[Parent]->Deadline<=R->Deadline)break;
        xrpcDuplexDeadlineSet(D,I,D->Deadlines[Parent]);
        I=Parent;
    }
    xrpcDuplexDeadlineSet(D,I,R);
}
static void xrpcDuplexDeadlineDown(xrpcduplex* D,size_t I,xrpcduplexrequest* R)
{
    while(I<D->DeadlineCount/2u){
        size_t Child=I*2u+1u;
        if(Child+1u<D->DeadlineCount && D->Deadlines[Child+1u]->Deadline<D->Deadlines[Child]->Deadline)++Child;
        if(R->Deadline<=D->Deadlines[Child]->Deadline)break;
        xrpcDuplexDeadlineSet(D,I,D->Deadlines[Child]);
        I=Child;
    }
    xrpcDuplexDeadlineSet(D,I,R);
}
static void xrpcDuplexDeadlineRemove(xrpcduplex* D,xrpcduplexrequest* R)
{
    if(!R->DeadlineSlot)return;
    size_t I=R->DeadlineSlot-1u;
    R->DeadlineSlot=0;
    xrpcduplexrequest* Last=D->Deadlines[--D->DeadlineCount];
    if(I==D->DeadlineCount)return;
    if(I && Last->Deadline<D->Deadlines[(I-1u)/2u]->Deadline)
        xrpcDuplexDeadlineUp(D,I,Last);
    else xrpcDuplexDeadlineDown(D,I,Last);
}
static void xrpcDuplexAdvanceScan(xrpcduplex* D)
{
    if(D->Scan==D->ScanEnd)D->Scan=D->ScanEnd=NULL;
    else D->Scan=D->Scan->Next;
}
static void xrpcDuplexPrepareFinish(xrpcduplex* D,xrpcduplexrequest* R,xrpccallstatus Status)
{
    if(R->Finishing)return;
    R->Finishing=true;R->Status=Status;++D->Finishing;
    for(size_t I=0;I<R->Count;++I)if(R->Entries[I].Token.Serial){xrpcpendingcall Taken;
        xrpcPendingTakeToken(D->Pending,R->Entries[I].Token,&Taken);R->Entries[I].Token.Serial=0;}
    if(R->Output){if(R->Started && !R->Sent)xrpcDuplexSeal(D,XRPC_CALL_TRANSPORT);
        xrpcDuplexUnqueue(D,R->Output);R->Output=NULL;}
}
static bool xrpcDuplexFinish(xrpcduplex* D,xrpcduplexrequest* R)
{
    if(!R->Finished){xrpccallfinish F=R->Responses && R->Batch?
        xrpcBatchCallFinish(R->Call,R->Responses,xrpcDuplexMap(R),R->Count,NULL):
        xrpcCallFinish(R->Call,R->Status,R->Responses,R->Responses?xrpcDuplexMap(R)[0]:0,NULL);
        if(F==XRPC_CALL_FINISH_ERROR)return false;
        R->Finished=true;
    }
    if(!xrpcCallDestroy(R->Call))return false;
    R->Call=NULL;
    xrpcDuplexDeadlineRemove(D,R);
    --D->Finishing;
    if(D->Scan==R)xrpcDuplexAdvanceScan(D);
    else if(D->ScanEnd==R)D->ScanEnd=R->Prev;
    if(R->Prev)R->Prev->Next=R->Next;else D->Head=R->Next;
    if(R->Next)R->Next->Prev=R->Prev;else D->Tail=R->Prev;
    xrtMutexLock(&D->Gate);
    --D->Operations;D->PendingCalls-=R->Replies;++D->Completed;
    xrtMutexUnlock(&D->Gate);
    xrpcResponsesRelease(R->Responses);xrpcDuplexDrop(R);return true;
}
static xrpcduplexadmission xrpcDuplexProtocol(xrpcduplex* D)
{++D->ProtocolFailures;xrpcDuplexSeal(D,XRPC_CALL_PROTOCOL);return XRPC_DUPLEX_PROTOCOL;}
/* Validated JSON string tokens only. Zero means outside our generated ID domain. */
static uint64 xrpcDuplexNumericId(xstrview Raw,size_t* Count)
{
    xrpcstringiterator It={Raw,1,{0},0,0};unsigned char Byte;
    uint64 Value=0;size_t Digits=0;
    while(xrpcStringNext(&It,&Byte)){
        if(Byte<'0' || Byte>'9' || (!Digits && Byte=='0') || Digits==20 || Value>(UINT64_MAX-(Byte-'0'))/10u)
            return 0;
        Value=Value*10u+(Byte-'0');++Digits;
    }
    if(Count)*Count=Digits;
    return Value;
}

static xstrview xrpcDuplexCorrelationId(const xrpcmessage* M,char* Text)
{
    if(M->Id.Kind!=XRPC_VALUE_STRING)return XRT_STR_LITERAL("null");
    xstrview Raw=M->Id.Raw;size_t Digits=0;
    uint64 Value=xrpcDuplexNumericId(Raw,&Digits);
    if(!Value)return XRT_STR_LITERAL("null");
    return Raw.Size==Digits+2u?Raw:xrpcDuplexId(Value,Text);
}

typedef enum xrpcduplexcontrol {
    XRPC_CONTROL_NONE=0,XRPC_CONTROL_CANCEL_OFFER,XRPC_CONTROL_ABANDON,
    XRPC_CONTROL_DEADLINE_OFFER,XRPC_CONTROL_DEADLINE
} xrpcduplexcontrol;

static xrpcduplexadmission xrpcDuplexResponse(xrpcduplex* D,xstrview Input)
{
    xrpcresponses* Responses=NULL;
    xrpcresponsestatus S=xrpcResponsesReadQuota(Input,&D->Config.Response,D->Scratch,D->Config.Response.Decode.MaxMessages,&Responses,D->Results);
    if(S!=XRPC_RESPONSE_DONE)return S==XRPC_RESPONSE_LOCAL_ERROR?XRPC_DUPLEX_ERROR:xrpcDuplexProtocol(D);
    xrpcresponseinfo I={0};I.Size=sizeof(I);xrpcResponsesInfo(Responses,&I);
    for(size_t N=0;N<I.Count;++N)D->CorrelationIds[N]=xrpcDuplexCorrelationId(I.Messages+N,D->CorrelationText+N*24u);
    if(!xrpcPendingProbeBatch(D->Pending,D->CorrelationIds,I.Count,D->Resolutions)){xrpcResponsesRelease(Responses);return XRPC_DUPLEX_ERROR;}
    xrpcduplexrequest* R=NULL;size_t Matches=0;
    for(size_t N=0;N<I.Count;++N)if(D->Resolutions[N].Status==XRPC_PENDING_MATCHED){
        xrpcduplexentry* E=D->Resolutions[N].Call.User;
        if(!R){R=E->Request;for(size_t K=0;K<R->Count;++K)xrpcDuplexMap(R)[K]=SIZE_MAX;}
        if(E->Request!=R || !R->Sent || R->Finishing || I.Batch!=R->Batch || xrpcDuplexMap(R)[E->Index]!=SIZE_MAX)goto Protocol;
        xrpcDuplexMap(R)[E->Index]=N;++Matches;
    }
    if(!R){D->UnknownResponses+=I.Count;xrpcResponsesRelease(Responses);return XRPC_DUPLEX_ACCEPTED;}
    if(Matches!=I.Count || Matches!=R->Replies)goto Protocol;
    if(!xrpcPendingTakeBatch(D->Pending,D->CorrelationIds,I.Count,D->Resolutions)){xrpcResponsesRelease(Responses);return XRPC_DUPLEX_ERROR;}
    for(size_t N=0;N<I.Count;++N){xrpcduplexentry* E=D->Resolutions[N].Call.User;E->Token.Serial=0;}
    R->Responses=Responses;
    size_t At=xrpcDuplexMap(R)[0];xrpccallstatus Status=R->Batch?XRPC_CALL_BATCH_DONE:
        I.Messages[At].Kind==XRPC_MESSAGE_ERROR?XRPC_CALL_REMOTE_ERROR:XRPC_CALL_OK;
    xrpcDuplexPrepareFinish(D,R,Status);if(!xrpcDuplexFinish(D,R))return XRPC_DUPLEX_ERROR;
    return XRPC_DUPLEX_ACCEPTED;
Protocol:
    xrpcResponsesRelease(Responses);return xrpcDuplexProtocol(D);
}
/* The fixed outer grammar lets us validate only the metadata here and run
 * the untouched inner document through the ordinary bounded duplex decoder. */
static xstrview xrpcDuplexHostId(const xrpcmessage* Message)
{
    if (!Message) return XRT_STR_LITERAL("null");
    if (Message->Kind == XRPC_MESSAGE_NOTIFICATION) return (xstrview){0};
    if (Message->Kind == XRPC_MESSAGE_CALL) return Message->Id.Raw;
    return XRT_STR_LITERAL("null");
}

static void xrpcDuplexHostRetire(xrpcduplex* D, xrpcduplexhostjob** Link, bool Completed)
{
    xrpcduplexhostjob* Job = *Link;
    *Link = Job->Next;
    xrpcResultQuotaFree(Job);
    xrtMutexLock(&D->Gate);
    --D->Operations;
    --D->Incoming;
    if (Completed) ++D->Completed;
    xrtMutexUnlock(&D->Gate);
}

static xrpcduplexadmission xrpcDuplexHostReceive(xrpcduplex* D, xstrview Input,
    xbytesview Context, const xrpcdocument* Document)
{
    size_t Messages = Document ? Document->Count : 1;
    size_t Count = 0;
    size_t Bytes = sizeof(xrpcduplexhostjob);
    for (size_t I = 0; I < Messages; ++I) {
        xstrview Id = xrpcDuplexHostId(Document ? D->Scratch + I : NULL);
        if (!Id.Size) continue;
        if (Bytes > SIZE_MAX - sizeof(xstrview) || Id.Size > SIZE_MAX - Bytes - sizeof(xstrview))
            return XRPC_DUPLEX_OVERLOADED;
        Bytes += sizeof(xstrview) + Id.Size;
        ++Count;
    }
    xrtMutexLock(&D->Gate);
    if (D->Closing) {
        xrtMutexUnlock(&D->Gate);
        return XRPC_DUPLEX_CLOSED;
    }
    if (D->Operations == D->Config.MaxOperations || D->Incoming == D->Config.MaxIncoming ||
        D->HostSerial == UINT64_MAX) {
        ++D->Rejected;
        xrtMutexUnlock(&D->Gate);
        return XRPC_DUPLEX_OVERLOADED;
    }
    bool Limited = false;
    xrpcduplexhostjob* Job = xrpcResultQuotaAlloc(D->Control, Bytes, &Limited);
    if (!Job) {
        if (Limited) ++D->Rejected;
        xrtMutexUnlock(&D->Gate);
        if (Limited) return XRPC_DUPLEX_OVERLOADED;
        xrpcDuplexReport(XERR_MEMORY, XRPC_DUPLEX_MEMORY, "RPC APP job allocation failed");
        return XRPC_DUPLEX_ERROR;
    }
    Job->Token = ++D->HostSerial;
    Job->Count = Count;
    Job->Batch = Document && Document->Batch;
    Job->Next = D->HostJobs;
    D->HostJobs = Job;
    ++D->Operations;
    ++D->Incoming;
    xrtMutexUnlock(&D->Gate);

    char* Copy = (char*)(Job->Ids + Count);
    size_t Index = 0;
    for (size_t I = 0; I < Messages; ++I) {
        xstrview Id = xrpcDuplexHostId(Document ? D->Scratch + I : NULL);
        if (!Id.Size) continue;
        memcpy(Copy, Id.Data, Id.Size);
        Job->Ids[Index++] = (xstrview){Copy, Id.Size};
        Copy += Id.Size;
    }
    if (!D->Config.OnRequest(Job->Token, Input, Context, Document,
        Document ? D->Scratch : NULL, D->Config.User)) {
        xrpcDuplexHostRetire(D, &D->HostJobs, false);
        xrtMutexLock(&D->Gate);
        ++D->Rejected;
        xrtMutexUnlock(&D->Gate);
        return XRPC_DUPLEX_OVERLOADED;
    }
    xrtMutexLock(&D->Gate);
    ++D->Accepted;
    ++D->Received;
    xrtMutexUnlock(&D->Gate);
    return XRPC_DUPLEX_ACCEPTED;
}

XRPC_API xrpcduplexadmission xrpcDuplexReply(xrpcduplex* D, uint64 Token, xstrview Reply)
{
    if (!D || !Token || !xrpcDuplexSpan(D, Reply.Data, Reply.Size)) {
        xrpcDuplexReport(XERR_ARGUMENT, XRPC_DUPLEX_ARGUMENT, "invalid RPC APP reply");
        return XRPC_DUPLEX_ERROR;
    }
    if (!xrpcDuplexEnter(D)) return XRPC_DUPLEX_ERROR;
    xrpcduplexadmission Status = XRPC_DUPLEX_ERROR;
    if (xrpcDuplexIsClosing(D)) {
        Status = XRPC_DUPLEX_CLOSED;
        goto Done;
    }
    xrpcduplexhostjob** Link = &D->HostJobs;
    while (*Link && (*Link)->Token != Token) Link = &(*Link)->Next;
    xrpcduplexhostjob* Job = *Link;
    if (!Job || ((Job->Count == 0) != (Reply.Size == 0))) goto Invalid;
    if (Reply.Size) {
        xrpcdocument Document;
        if (xrpcDecode(Reply, &D->Config.Response.Decode, D->Scratch,
            D->Config.Response.Decode.MaxMessages, &Document) != XRPC_DECODE_DONE ||
            Document.InvalidCount || Document.Batch != Job->Batch || Document.Count != Job->Count)
            goto Invalid;
        for (size_t I = 0; I < Job->Count; ++I) {
            xstrview Id = D->Scratch[I].Id.Raw;
            if (Id.Size != Job->Ids[I].Size || memcmp(Id.Data, Job->Ids[I].Data, Id.Size))
                goto Invalid;
        }
        if (Reply.Size > SIZE_MAX - sizeof(xrpcduplexoutput) ||
            !xrpcResultQuotaFits(D->Wire, sizeof(xrpcduplexoutput) + Reply.Size)) {
            xrpcDuplexReport(XERR_RANGE, XRPC_DUPLEX_LIMIT, "RPC APP reply exceeds wire quota");
            goto Done;
        }
        bool Limited = false;
        xrpcduplexoutput* Output = xrpcDuplexOutputCreate(D, Reply.Size, Reply.Size, &Limited);
        if (!Output) {
            if (Limited) Status = XRPC_DUPLEX_OVERLOADED;
            else xrpcDuplexReport(XERR_MEMORY, XRPC_DUPLEX_MEMORY, "RPC APP reply allocation failed");
            goto Done;
        }
        memcpy(Output + 1, Reply.Data, Reply.Size);
        Output->Wire = (xstrview){(cstr)(Output + 1), Reply.Size};
        xrpcDuplexQueue(D, Output);
    }
    xrpcDuplexHostRetire(D, Link, true);
    Status = XRPC_DUPLEX_ACCEPTED;
    goto Done;
Invalid:
    xrpcDuplexReport(XERR_ARGUMENT, XRPC_DUPLEX_ARGUMENT, "RPC APP reply token, IDs or shape do not match");
Done:
    D->Busy = false;
    return Status;
}

XRPC_API xrpcduplexadmission xrpcDuplexReceive(xrpcduplex* D, xstrview Input, xbytesview Context)
{
    if (!D || !xrpcDuplexSpan(D, Input.Data, Input.Size) ||
        !xrpcDuplexSpan(D, Context.Data, Context.Size)) {
        xrpcDuplexReport(XERR_ARGUMENT, XRPC_DUPLEX_ARGUMENT, "invalid RPC duplex receive spans");
        return XRPC_DUPLEX_ERROR;
    }
    if (!xrpcDuplexEnter(D)) return XRPC_DUPLEX_ERROR;
    xrpcduplexadmission Status = XRPC_DUPLEX_CLOSED;
    if (xrpcDuplexIsClosing(D)) goto Done;

    xrpcdecodeconfig Decode = D->Config.Response.Decode;
    Decode.Mode = XRPC_DECODE_DUPLEX;
    xrpcdocument Document;
    xrpcdecodestatus Decoded = xrpcDecode(Input, &Decode, D->Scratch,
        Decode.MaxMessages, &Document);
    Status = XRPC_DUPLEX_ERROR;
    if (Decoded == XRPC_DECODE_LOCAL_ERROR) goto Done;

    bool Requests = false, Responses = false;
    if (Decoded == XRPC_DECODE_DONE) {
        for (size_t I = 0; I < Document.Count; ++I) {
            const xrpcmessage* M = D->Scratch + I;
            bool Reply = M->Kind == XRPC_MESSAGE_RESULT || M->Kind == XRPC_MESSAGE_ERROR ||
                M->Result.Kind != XRPC_VALUE_MISSING || M->Error.Kind != XRPC_VALUE_MISSING;
            Requests |= M->Method.Kind != XRPC_VALUE_MISSING || !Reply;
            Responses |= Reply;
        }
    }
    if (Requests && Responses) {
        Status = xrpcDuplexProtocol(D);
    } else if (Responses) {
        Status = xrpcDuplexResponse(D, Input);
        if (Status == XRPC_DUPLEX_ACCEPTED) ++D->Received;
    } else if (D->Config.OnRequest) {
        Status = xrpcDuplexHostReceive(D, Input, Context,
            Decoded == XRPC_DECODE_DONE ? &Document : NULL);
    } else {
        xrtMutexLock(&D->Gate);
        ++D->Rejected;
        xrtMutexUnlock(&D->Gate);
        Status = XRPC_DUPLEX_OVERLOADED;
    }
Done:
    D->Busy = false;
    return Status;
}

static bool xrpcDuplexWorkSubmit(xrpcduplex* D,bool* OK)
{
    xrtMutexLock(&D->Gate);xrpcduplexrequest* R=D->SubmitHead;
    if(R){D->SubmitHead=R->SubmitNext;if(!D->SubmitHead)D->SubmitTail=NULL;R->SubmitNext=NULL;}
    xrtMutexUnlock(&D->Gate);if(!R)return false;
    R->Prev=D->Tail;if(R->Prev)R->Prev->Next=R;else D->Head=R;D->Tail=R;
    R->Deadline=xrpcCallDeadline(R->Call);
    if(isfinite(R->Deadline)){
        size_t Slot=D->DeadlineCount++;
        xrpcDuplexDeadlineUp(D,Slot,R);
    }
    if(xrpcDuplexIsClosing(D))xrpcDuplexPrepareFinish(D,R,D->Reason);
    else {xrpccallfinish F=xrpcCallPoll(R->Call);
        if(F==XRPC_CALL_FINISH_ERROR){*OK=false;return true;}
        if(F!=XRPC_CALL_PENDING)xrpcDuplexPrepareFinish(D,R,XRPC_CALL_CLOSED);
        else {
            for(size_t I=0;I<R->Count;++I)if(R->Entries[I].Id){char Id[24];
                xrpcpendingstatus P=xrpcPendingInsert(D->Pending,xrpcDuplexId(R->Entries[I].Id,Id),UINT64_MAX,R->Entries+I,&R->Entries[I].Token);
                if(P!=XRPC_PENDING_OK){xrpcDuplexPrepareFinish(D,R,P==XRPC_PENDING_ERROR?XRPC_CALL_PROTOCOL:XRPC_CALL_LIMIT);break;}}
            if(!R->Finishing)xrpcDuplexQueue(D,R->Output);
        }
    }
    if(R->Finishing)*OK=xrpcDuplexFinish(D,R);
    else D->More=true; /* Added outside this turn's bounded scan cohort. */
    return true;
}
static bool xrpcDuplexWorkCall(xrpcduplex* D,bool* OK,double Now)
{
    xrpcduplexrequest* Due=D->DeadlineCount?D->Deadlines[0]:NULL;
    if(Due && Due->Deadline>Now)Due=NULL;
    /* Give an expired call a turn without waiting for its position in the
     * cancellation cohort. Raw token cancellation still needs that cohort. */
    xrpcduplexrequest* R=Due && (!D->PreferScan || !D->Scan)?Due:D->Scan;
    if(!R)return false;
    D->PreferScan=R!=D->Scan && D->Scan!=NULL;
    if(R==D->Scan)xrpcDuplexAdvanceScan(D);
    if(!R->Finishing){if(xrpcDuplexIsClosing(D))xrpcDuplexPrepareFinish(D,R,D->Reason);
        else {xrpccallfinish P=xrpcCallPoll(R->Call);if(P==XRPC_CALL_FINISH_ERROR){*OK=false;return true;}
            if(P!=XRPC_CALL_PENDING)xrpcDuplexPrepareFinish(D,R,XRPC_CALL_CLOSED);}}
    if(R->Finishing)*OK=xrpcDuplexFinish(D,R);
    return true;
}
static bool xrpcDuplexWorkClose(xrpcduplex* D)
{
    if(!xrpcDuplexIsClosing(D))return false;
    if (D->HostJobs) {
        xrpcDuplexHostRetire(D, &D->HostJobs, true);
        return true;
    }
    if(D->OutputHead){if(D->OutputHead->Request)D->OutputHead->Request->Output=NULL;
        xrpcDuplexUnqueue(D,D->OutputHead);return true;}
    return false;
}
XRPC_API bool xrpcDuplexStep(xrpcduplex* D)
{
    if(!xrpcDuplexEnter(D))return false;
    bool OK=true;D->More=false;
    if(!D->Scan){D->Scan=D->Head;D->ScanEnd=D->Tail;}
    double Now=D->DeadlineCount?xrtTimer():-INFINITY;
    /* Rotate between admission, deadlines and close, even with MaxWork=1. */
    for(size_t Work=0;Work<D->Config.MaxWork && OK;++Work){bool Worked=false;
        for(unsigned Attempt=0;Attempt<3 && !Worked;++Attempt){unsigned Turn=D->Turn++%3;
            if(Turn==0)Worked=xrpcDuplexWorkSubmit(D,&OK);
            else if(Turn==1)Worked=xrpcDuplexWorkCall(D,&OK,Now);
            else Worked=xrpcDuplexWorkClose(D);
        }
        if(!Worked)break;
    }
    D->More|=D->Scan!=NULL || (D->DeadlineCount && D->Deadlines[0]->Deadline<=Now);
    xrtMutexLock(&D->Gate);
    D->More|=D->SubmitHead!=NULL || (D->Closing && (D->OutputHead || D->HostJobs));
    xrtMutexUnlock(&D->Gate);
    D->Busy=false;return OK;
}

XRPC_API int xrpcDuplexNext(xrpcduplex* D,xrpcduplexoutput** Output,uint64* Token)
{
    if(!D || !Output || !Token || !xrpcDuplexSpan(D,Output,sizeof(*Output)) || !xrpcDuplexSpan(D,Token,sizeof(*Token)) ||
       xrtMemRangesOverlap(Output,sizeof(*Output),Token,sizeof(*Token))){xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex output selection spans");return -1;}
    if(!xrpcDuplexEnter(D))return -1;
    int Result=0;
    if(D->Selection){xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC duplex already has a selected output");Result=-1;}
    else if(!xrpcDuplexIsClosing(D) && D->OutputHead){
        if(D->SelectionSerial==UINT64_MAX){xrpcDuplexSeal(D,XRPC_CALL_LIMIT);xrpcDuplexReport(XERR_RANGE,XRPC_DUPLEX_LIMIT,"RPC duplex selection serial exhausted");Result=-1;}
        else if(!xrpcDuplexOutputRetain(D->OutputHead))Result=-1;
        else {
            D->Selection=D->OutputHead;
            D->SelectedToken=++D->SelectionSerial;*Output=D->Selection;*Token=D->SelectedToken;Result=1;
        }
    }
    D->Busy=false;return Result;
}
XRPC_API bool xrpcDuplexStarted(xrpcduplex* D,uint64 Token)
{
    if(!xrpcDuplexEnter(D))return false;
    bool OK=false;
    if(Token && Token==D->SelectedToken && D->Selection){
        xrtMutexLock(&D->Gate);
        bool Closing=D->Closing;
        xrtMutexUnlock(&D->Gate);
        xrpcduplexrequest* R=D->Selection->Request;
        if(!Closing){
            if(!R)OK=true;
            else if(!R->Started && xrpcCallPoll(R->Call)==XRPC_CALL_PENDING){
                OK=xrpcCallMarkSent(R->Call);R->Started=OK;
            }
            else OK=R->Started;
        }
    }
    if(OK)D->Selection->Started=true;
    D->Busy=false;return OK;
}
XRPC_API bool xrpcDuplexSent(xrpcduplex* D,uint64 Token)
{
    if(!xrpcDuplexEnter(D))return false;
    bool OK=false;xrpcduplexoutput* O=D->Selection;
    if(Token && Token==D->SelectedToken && O && !xrpcDuplexIsClosing(D)){
        xrpcduplexrequest* R=O->Request;
        if(O->Started && (!R || R->Started)){if(R){R->Sent=true;R->Output=NULL;}
            xrpcDuplexUnqueue(D,O);OK=true;
            if(R && !R->Replies){
                xrpcDuplexPrepareFinish(D,R,XRPC_CALL_NOTIFY_DONE);
                OK=xrpcDuplexFinish(D,R);
            }}
    }
    D->Busy=false;return OK;
}
XRPC_API bool xrpcDuplexInfo(xrpcduplex* D,xrpcduplexinfo* I)
{
    if(!D || !I || !xrpcDuplexSpan(D,I,sizeof(*I)) || I->Size!=sizeof(*I))return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex info output");
    xrpcduplexinfo V={0};V.Size=sizeof(V);V.NextDeadline=INFINITY;
    xrtMutexLock(&D->Gate);V.OwnedBytes=D->FixedBytes;V.Closing=D->Closing;V.Reason=D->Reason;V.Operations=D->Operations;V.PendingCalls=D->PendingCalls;
    V.Incoming=D->Incoming;V.Constructing=D->Constructing;V.TemporaryBytes=D->TemporaryBytes;V.PeakTemporaryBytes=D->PeakTemporaryBytes;V.Accepted=D->Accepted;V.Rejected=D->Rejected;V.Completed=D->Completed;
    bool Submit=D->SubmitHead!=NULL;
    xrtMutexUnlock(&D->Gate);V.Received=D->Received;V.UnknownResponses=D->UnknownResponses;V.ProtocolFailures=D->ProtocolFailures;
    xrpcResultQuotaInfo(D->Control,&V.ControlBlocks,&V.ControlBytes,&V.ControlRefusals);
    xrpcResultQuotaInfo(D->Wire,&V.Outputs,&V.OutputBytes,&V.OutputRefusals);xrpcResultQuotaInfo(D->Results,&V.Results,&V.ResultBytes,&V.ResultRefusals);
    V.Selected=D->Selection!=NULL;V.Started=D->Selection && D->Selection->Started;
    V.Drained=V.Closing && !V.Operations && !D->OutputHead && !D->Selection && xrtAtomic32Load(&D->References,XMEMORY_ACQUIRE)==1;
    V.More=D->More || Submit;
    if(D->DeadlineCount)V.NextDeadline=D->Deadlines[0]->Deadline;
    if(V.More || D->Finishing)V.NextDeadline=xrtTimer();
    else if(V.Closing && !V.Drained && !V.Operations){
        double Retry=xrtTimer()+.001;
        if(Retry<V.NextDeadline)V.NextDeadline=Retry; /* recheck a final constructor pin */
    }
    *I=V;return true;
}
XRPC_API bool xrpcDuplexDestroy(xrpcduplex* D)
{
    if(!D)return true;
    if(D->Busy)return xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC duplex destruction during owner callback");
    xrpcduplexinfo I={0};I.Size=sizeof(I);if(!xrpcDuplexInfo(D,&I))return false;
    if(!I.Drained)return xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC duplex close duties still live");
    xrpcPendingDestroy(D->Pending);xrpcResultQuotaRelease(D->Control);xrpcResultQuotaRelease(D->Wire);xrpcResultQuotaRelease(D->Results);
    xrtMutexUnit(&D->Gate);xrtFree(D);return true;
}
#endif
