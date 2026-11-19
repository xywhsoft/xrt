#include "../internal/xrpc_duplex_internal.h"
#include "../internal/xrpc_call_internal.h"
#include "../internal/xrpc_pending_internal.h"
#include "../internal/xrpc_result_quota.h"
#include "../internal/xrpc_server_internal.h"
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
typedef struct xrpcduplexjob {
    struct xrpcduplexjob *Next,*Prev,*ReadyNext,*FallbackNext;
    xrpcduplex* Duplex;
    xfuture* Future;
    xfuturewatch Watch;
    bool CancelIssued,Tracked,Abandoned;
} xrpcduplexjob;
typedef struct xrpcduplextrackedjob {
    xrpcduplexjob Job;
    xrpcpendingtoken Token;
} xrpcduplextrackedjob;
struct xrpcduplexoutput {
    xatomic32 References;
    struct xrpcduplexoutput *Next,*Prev;
    xrpcduplexrequest* Request; /* Owner-only; cleared before request retirement. */
    xfuture* Response;
    xstrview Wire;
    bool Linked,Started,CancelAck,DeadlineAck,DeadlineEnvelope;
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
    bool Batch,Started,Sent,Finishing,Finished,TrackCancel;
    uint8 Extension; /* Optional single capability flag. */
    xrpccallstatus Status;
    uint32 DeadlineSlot;
    xrpcduplexentry Entries[];
};
struct xrpcduplex {
    xrpcduplexconfig Config;
    xmutex Gate;
    xatomic32 References;
    xrpcresultquota *Control,*Wire,*Results;
    xrpcpending *Pending,*CancelJobs;
    xfuture* Offers[2]; /* A reserved bit covers construction and the live Future. */
    xrpcduplexrequest *SubmitHead,*SubmitTail,*Head,*Tail,*Scan,*ScanEnd;
    xrpcduplexrequest** Deadlines;
    xrpcduplexjob *Jobs,*ReadyHead,*ReadyTail,*CancelScan,*FallbackHead,*FallbackTail;
    xrpcduplexoutput *OutputHead,*OutputTail,*Selection;
    size_t Operations,PendingCalls,Incoming,DeadlineCount,Finishing,OwnedBytes,FixedBytes;
    size_t Constructing,TemporaryBytes,PeakTemporaryBytes;
    uint64 Serial,SelectionSerial,SelectedToken;
    uint64 Accepted,Rejected,Completed,Received,UnknownResponses,ProtocolFailures;
    bool Closing,Busy,More,CancelStarted,PreferScan,InputEnded;
    bool ReceiveCancel,ReceiveDeadline;
    unsigned Turn;
    xrpccallstatus Reason;
    xrpcmessage* Scratch;
    xrpcpendingresolution* Resolutions;
    xstrview* CorrelationIds;
    char* CorrelationText;
    xatomic32 EnabledExtensions;
    uint32 PendingOffers,AgreedExtensions; /* Gate. */
    size_t ExtensionWireLimit; /* Gate. */
};
static const char xrpcDeadlinePrefix[]="{\"jsonrpc\":\"2.0\",\"method\":\"rpc.xrt.deadline\",\"params\":[\"";
enum {
    XRPC_DUPLEX_ALL_EXTENSIONS=XRPC_DUPLEX_EXTENSION_CANCELLATION|XRPC_DUPLEX_EXTENSION_DEADLINE,
    XRPC_DEADLINE_DIGITS=19,
    XRPC_DEADLINE_PREFIX_BYTES=sizeof(xrpcDeadlinePrefix)-1u,
    XRPC_DEADLINE_INNER_OFFSET=XRPC_DEADLINE_PREFIX_BYTES+XRPC_DEADLINE_DIGITS+2u,
    XRPC_DEADLINE_OVERHEAD=XRPC_DEADLINE_INNER_OFFSET+2u
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
    C->MaxFixedBytes=16u*1024u*1024u;C->MaxTemporaryBytes=1024u*1024u;C->MaxWork=64;C->IncomingTimeout=5000;return true;
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
       !C.MaxWork || C.MaxWork>65536 || C.IncomingTimeout<XRT_WAIT_FOREVER || C.Encode.Size!=sizeof(C.Encode) ||
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
    xrtAtomic32Init(&D->EnabledExtensions,0);
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
{if(O && xrtAtomic32FetchSub(&O->References,1,XMEMORY_ACQ_REL)==1){xrtFutureDestroy(O->Response);xrpcResultQuotaFree(O);}}
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
/* Gate held. Drop the detached ref outside the gate. Future publication does
 * not hold its native lock while invoking completion watches. */
static xfuture* xrpcDuplexTakeDoneOffer(xrpcduplex* D,xrpcduplexextension Extension)
{
    unsigned Slot=Extension==XRPC_DUPLEX_EXTENSION_DEADLINE;
    xfuture* F=D->Offers[Slot];
    xfutureresult Outcome;
    if(!F || !xrtFutureDone(F) || !xrtFutureResult(F,&Outcome))return NULL;
    const xrpccallresult* R=Outcome.State==XFUTURE_RESOLVED?Outcome.Value:NULL;
    xrpcresponseinfo I={.Size=sizeof(I)};
    if(R && R->Status==XRPC_CALL_OK && xrpcResponsesInfo(R->Responses,&I) && I.Count==1){
        const xrpcvalue* V=&I.Messages[0].Result;
        if(V->Kind==XRPC_VALUE_BOOLEAN && V->Raw.Size==4 && !memcmp(V->Raw.Data,"true",4))
            D->AgreedExtensions|=Extension;
    }
    D->Offers[Slot]=NULL;
    D->PendingOffers&=~(uint32)Extension;
    return F;
}

static xrpcduplexadmission xrpcDuplexSubmit(xrpcduplex* D,const xrpcduplexitem* Items,
    size_t Count,bool Batch,int64 Timeout,xfuture** Future,double Entered,xrpcduplexextension Extension)
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
    if(D->Closing || D->InputEnded){xrtMutexUnlock(&D->Gate);return XRPC_DUPLEX_CLOSED;}
    if(D->Operations==D->Config.MaxOperations || Replies>D->Config.MaxPendingCalls-D->PendingCalls ||
       Replies>UINT64_MAX-D->Serial || TempBytes>D->Config.MaxTemporaryBytes-D->TemporaryBytes){
        ++D->Rejected;xrtMutexUnlock(&D->Gate);return XRPC_DUPLEX_OVERLOADED;}
    xfuture* Observed=NULL;
    unsigned Slot=Extension==XRPC_DUPLEX_EXTENSION_DEADLINE;
    if(Extension){
        Observed=xrpcDuplexTakeDoneOffer(D,Extension);
        if(!(xrtAtomic32Load(&D->EnabledExtensions,XMEMORY_RELAXED)&Extension) ||
           ((D->PendingOffers|D->AgreedExtensions)&Extension)){
            xrtMutexUnlock(&D->Gate);
            if(Observed)xrtFutureDestroy(Observed);
            xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC capability offer is disabled, pending or agreed");
            return XRPC_DUPLEX_ERROR;
        }
        D->PendingOffers|=Extension;
    }else if(Timeout!=XRT_WAIT_FOREVER && D->Offers[1]){
        /* A caller awakened by the acknowledged Future can submit before the
         * owner's next turn. Observe the same result at admission. */
        Observed=xrpcDuplexTakeDoneOffer(D,XRPC_DUPLEX_EXTENSION_DEADLINE);
    }
    bool Wrap=!Extension && Timeout!=XRT_WAIT_FOREVER &&
        (D->AgreedExtensions&XRPC_DUPLEX_EXTENSION_DEADLINE);
    size_t WireLimit=D->ExtensionWireLimit;
    uint64 Serial=D->Serial;D->Serial+=Replies; /* Gaps on rollback; never reuse. */
    ++D->Operations;D->PendingCalls+=Replies;++D->Constructing;D->TemporaryBytes+=TempBytes;
    if(D->TemporaryBytes>D->PeakTemporaryBytes)D->PeakTemporaryBytes=D->TemporaryBytes;
    xrtAtomic32FetchAdd(&D->References,1,XMEMORY_RELAXED); /* Constructor pin. */
    xrtMutexUnlock(&D->Gate);
    if(Observed)xrtFutureDestroy(Observed);
    bool Limited=false;xrpcduplexadmission Result=XRPC_DUPLEX_ERROR;
    xrpcduplexrequest* R=xrpcResultQuotaAlloc(D->Control,Bytes,&Limited);
    xrpcduplexoutput* O=NULL;xrpcoutgoing* V=NULL;
    if(!R){if(Limited)Result=XRPC_DUPLEX_OVERLOADED;else goto Memory;goto Rollback;}
    memset(R,0,Bytes);R->Duplex=D;R->Count=Count;R->Replies=Replies;R->Batch=Batch;
    R->Extension=(uint8)Extension;xrtAtomic32Init(&R->References,1);
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
    size_t Extra=Wrap?XRPC_DEADLINE_OVERHEAD:0;
    if((Wrap || Extension) && (WireBytes>WireLimit || Extra>WireLimit-WireBytes)){
        xrpcDuplexReport(XERR_RANGE,XRPC_DUPLEX_LIMIT,"RPC extension wire exceeds the output limit");
        goto Rollback;
    }
    O=xrpcDuplexOutputCreate(D,WireBytes+Extra,WireBytes+Extra,&Limited);
    if(!O){if(Limited){Result=XRPC_DUPLEX_OVERLOADED;goto Rollback;}goto Memory;}
    char* Data=(char*)(O+1);
    if(!xrpcEncodeWrite(Data+(Wrap?XRPC_DEADLINE_INNER_OFFSET:0),WireBytes,V,Count,Batch,&D->Config.Encode,NULL))goto Rollback;
    if(Wrap){
        memcpy(Data,xrpcDeadlinePrefix,XRPC_DEADLINE_PREFIX_BYTES);
        memcpy(Data+XRPC_DEADLINE_INNER_OFFSET-2u,"\",",2);
        memcpy(Data+WireBytes+XRPC_DEADLINE_INNER_OFFSET,"]}",2);
        O->DeadlineEnvelope=true;
    }
    xrtFree(V);V=NULL;O->Wire=(xstrview){Data,WireBytes+Extra};O->Request=R;R->Output=O;
    xrpccallconfig C;xrpcCallConfigInit(&C);C.Timeout=Timeout;C.Wake=xrpcDuplexCallWake;C.Drop=xrpcDuplexDrop;C.User=R;
    xfuture* F=NULL;
    if(!xrpcCallCreateQuota(&C,Batch?Count:0,&R->Call,&F,Entered,D->Results,&Limited)){
        if(Limited)Result=XRPC_DUPLEX_OVERLOADED;
        goto Rollback;}
    xrtAtomic32FetchAdd(&R->References,1,XMEMORY_RELAXED); /* Native context owns one R reference. */
    if(Extension && !xrtFutureRef(F)){
        xerror* Error=xrtTakeError();
        (void)xrpcCallDestroy(R->Call);
        xrtFutureDestroy(F);
        xrtClearError();xrtSetErrorTake(Error);
        goto Rollback;
    }
    xrtMutexLock(&D->Gate);
    if(Extension)D->Offers[Slot]=F;
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
    D->PendingOffers&=~(uint32)Extension;
    if(Result==XRPC_DUPLEX_OVERLOADED)++D->Rejected;
    xrtMutexUnlock(&D->Gate);xrpcDuplexConstructionReturn(D);return Result;
Argument:
    xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex admission spans/count/timeout");return XRPC_DUPLEX_ERROR;
}
XRPC_API xrpcduplexadmission xrpcDuplexCall(xrpcduplex* D,xstrview Method,xstrview Params,int64 Timeout,xfuture** F)
{double Entered=xrtTimer();xrpcduplexitem Item={false,Method,Params};return xrpcDuplexSubmit(D,&Item,1,false,Timeout,F,Entered,0);}
XRPC_API xrpcduplexadmission xrpcDuplexNotify(xrpcduplex* D,xstrview Method,xstrview Params,int64 Timeout,xfuture** F)
{double Entered=xrtTimer();xrpcduplexitem Item={true,Method,Params};return xrpcDuplexSubmit(D,&Item,1,false,Timeout,F,Entered,0);}
XRPC_API xrpcduplexadmission xrpcDuplexBatch(xrpcduplex* D,const xrpcduplexitem* Items,size_t Count,int64 Timeout,xfuture** F)
{return xrpcDuplexSubmit(D,Items,Count,true,Timeout,F,xrtTimer(),0);}
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
static const char xrpcCancelOfferMethod[]="rpc.xrt.cancel.v1";
static const char xrpcCancelNoticeMethod[]="rpc.xrt.cancel";
static const char xrpcDeadlineOfferMethod[]="rpc.xrt.deadline.v1";
static const char xrpcDeadlineNoticeMethod[]="rpc.xrt.deadline";

#if !defined(XRPC_FEATURE_XRPC_TCP) && !defined(XRPC_FEATURE_XRPC_TLS) && !defined(XRPC_FEATURE_XRPC_WS)
static
#endif
bool xrpcDuplexEnableExtensions(xrpcduplex* D,uint32 Extensions,size_t WireLimit)
{
    if(!D || !Extensions || (Extensions&~(uint32)XRPC_DUPLEX_ALL_EXTENSIONS) || !WireLimit)
        return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC extensions or wire limit");
    bool OK=false;
    xrtMutexLock(&D->Gate);
    if(D->InputEnded || D->Closing){
        xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"extension enable on closed RPC duplex");
        goto Done;
    }
    uint32 Enabled=xrtAtomic32Load(&D->EnabledExtensions,XMEMORY_RELAXED);
    uint32 Added=Extensions&~Enabled;
    if((Added&XRPC_DUPLEX_EXTENSION_CANCELLATION) && D->Config.Server){
        xrpcpendingconfig C;
        xrpcPendingConfigInit(&C);
        C.MaxCalls=D->Config.MaxIncoming;
        C.MaxOwnedBytes=D->Config.MaxFixedBytes-D->FixedBytes;
        C.Id.MaxInputBytes=22;C.Id.MaxStringBytes=20;C.Id.MaxKeyBytes=21;
        if(!C.MaxOwnedBytes){
            xrpcDuplexReport(XERR_RANGE,XRPC_DUPLEX_LIMIT,"no fixed storage for RPC cancellation index");
            goto Done;
        }
        if(!xrpcPendingCreate(&C,&D->CancelJobs))goto Done;
        xrpcpendinginfo I={.Size=sizeof(I)};
        xrpcPendingInfo(D->CancelJobs,&I);
        D->FixedBytes+=I.OwnedBytes;
    }
    if(!Enabled)D->ExtensionWireLimit=WireLimit<D->Config.Encode.MaxOutputBytes?WireLimit:D->Config.Encode.MaxOutputBytes;
    xrtAtomic32Store(&D->EnabledExtensions,Enabled|Added,XMEMORY_RELEASE);
    OK=true;
Done:
    xrtMutexUnlock(&D->Gate);
    return OK;
}

static bool xrpcDuplexOwnerEnable(xrpcduplex* D,xrpcduplexextension Extension)
{
    if(!xrpcDuplexEnter(D))return false;
    bool Enabled=xrpcDuplexEnableExtensions(D,Extension,D->Config.Encode.MaxOutputBytes);
    D->Busy=false;
    return Enabled;
}

XRPC_API bool xrpcDuplexEnableCancellation(xrpcduplex* D)
{return xrpcDuplexOwnerEnable(D,XRPC_DUPLEX_EXTENSION_CANCELLATION);}

XRPC_API bool xrpcDuplexEnableDeadline(xrpcduplex* D)
{return xrpcDuplexOwnerEnable(D,XRPC_DUPLEX_EXTENSION_DEADLINE);}

#if !defined(XRPC_FEATURE_XRPC_TCP) && !defined(XRPC_FEATURE_XRPC_TLS) && !defined(XRPC_FEATURE_XRPC_WS)
static
#endif
xrpcduplexadmission xrpcDuplexNegotiateExtensionEntered(xrpcduplex* D,xrpcduplexextension Extension,
    int64 Timeout,xfuture** Future,double Entered)
{
    xstrview Method;
    if(Extension==XRPC_DUPLEX_EXTENSION_DEADLINE)Method=XRT_STR_LITERAL(xrpcDeadlineOfferMethod);
    else if(Extension==XRPC_DUPLEX_EXTENSION_CANCELLATION)Method=XRT_STR_LITERAL(xrpcCancelOfferMethod);
    else{
        xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"unknown RPC capability offer");
        return XRPC_DUPLEX_ERROR;
    }
    xrpcduplexitem Item={false,Method,{0}};
    return xrpcDuplexSubmit(D,&Item,1,false,Timeout,Future,Entered,Extension);
}

static xrpcduplexadmission xrpcDuplexOwnerOffer(xrpcduplex* D,xrpcduplexextension Extension,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!xrpcDuplexEnter(D))return XRPC_DUPLEX_ERROR;
    xrpcduplexadmission Result=xrpcDuplexNegotiateExtensionEntered(D,Extension,Timeout,Future,Entered);
    D->Busy=false;
    return Result;
}

XRPC_API xrpcduplexadmission xrpcDuplexNegotiateCancellation(xrpcduplex* D,int64 Timeout,xfuture** Future)
{return xrpcDuplexOwnerOffer(D,XRPC_DUPLEX_EXTENSION_CANCELLATION,Timeout,Future);}

XRPC_API xrpcduplexadmission xrpcDuplexNegotiateDeadline(xrpcduplex* D,int64 Timeout,xfuture** Future)
{return xrpcDuplexOwnerOffer(D,XRPC_DUPLEX_EXTENSION_DEADLINE,Timeout,Future);}

static xrpcduplexoutput* xrpcDuplexMakeControl(xrpcduplex* D,const xrpcoutgoing* Message,bool* Limited)
{
    size_t Size;
    if(!xrpcEncodeSize(Message,1,false,&D->Config.Encode,&Size))return NULL;
    if(Size>D->ExtensionWireLimit){
        *Limited=true;
        return NULL;
    }
    xrpcduplexoutput* O=xrpcDuplexOutputCreate(D,Size,Size,Limited);
    if(!O){
        if(!*Limited)xrpcDuplexReport(XERR_MEMORY,XRPC_DUPLEX_MEMORY,"RPC control output allocation failed");
        return NULL;
    }
    if(!xrpcEncodeWrite(O+1,Size,Message,1,false,&D->Config.Encode,NULL)){
        xrpcDuplexOutputRelease(O);
        return NULL;
    }
    O->Wire=(xstrview){(cstr)(O+1),Size};
    return O;
}

static void xrpcDuplexAbandonSent(xrpcduplex* D,const xrpcduplexrequest* R)
{
    if(!R->TrackCancel || !R->Sent || !R->Replies || D->InputEnded || xrpcDuplexIsClosing(D))return;
    size_t I=0;
    while(!R->Entries[I].Id)++I; /* A fully sent operation has at least one call. */
    char Arg[26];
    xstrview Id=xrpcDuplexId(R->Entries[I].Id,Arg+1);
    Arg[0]='[';Arg[Id.Size+1]=']';
    xrpcoutgoing Message={.Kind=XRPC_MESSAGE_NOTIFICATION,
        .Method=XRT_STR_LITERAL(xrpcCancelNoticeMethod),.Params={Arg,Id.Size+2}};
    bool Limited=false;
    xrpcduplexoutput* O=xrpcDuplexMakeControl(D,&Message,&Limited);
    if(!O){
        /* The local Future is already terminal. A failed wire obligation seals
         * the transport, which lets the peer retire its connection-local work. */
        xrpcDuplexSeal(D,Limited?XRPC_CALL_LIMIT:
            xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT);
        return;
    }
    /* Control precedes unsent application work without disturbing a selected
     * output or revisiting the physical borrow already held by the transport. */
    O->Linked=true;
    O->Prev=D->Selection;
    O->Next=O->Prev?O->Prev->Next:D->OutputHead;
    if(O->Prev)O->Prev->Next=O;else D->OutputHead=O;
    if(O->Next)O->Next->Prev=O;else D->OutputTail=O;
}

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
    xfuture* Offer=R->Extension?xrpcDuplexTakeDoneOffer(D,(xrpcduplexextension)R->Extension):NULL;
    --D->Operations;D->PendingCalls-=R->Replies;++D->Completed;
    xrtMutexUnlock(&D->Gate);
    if(Offer)xrtFutureDestroy(Offer);
    xrpcResponsesRelease(R->Responses);xrpcDuplexDrop(R);return true;
}
static void xrpcDuplexJobNotify(ptr Data){(void)Data;}
static void xrpcDuplexJobRelease(ptr Data)
{
    xrpcduplexjob* J=Data;xrpcduplex* D=J->Duplex;
    /* Publish only from Release: core no longer accesses the embedded Watch.
     * Cache D before publication; owner may retire J immediately after unlock. */
    xrtMutexLock(&D->Gate);if(D->ReadyTail)D->ReadyTail->ReadyNext=J;else D->ReadyHead=J;D->ReadyTail=J;xrtMutexUnlock(&D->Gate);
    xrpcDuplexWake(D);xrtAtomic32FetchSub(&D->References,1,XMEMORY_RELEASE);
}
static void xrpcDuplexJobRetire(xrpcduplex* D,xrpcduplexjob* J)
{
    if(D->CancelScan==J)D->CancelScan=J->Next;
    if(J->Prev)J->Prev->Next=J->Next;else D->Jobs=J->Next;if(J->Next)J->Next->Prev=J->Prev;
    if(J->Tracked){xrpcpendingcall Removed;
        xrpcPendingTakeToken(D->CancelJobs,((xrpcduplextrackedjob*)J)->Token,&Removed);}
    xrtFutureDestroy(J->Future);xrpcResultQuotaFree(J);
    xrtMutexLock(&D->Gate);--D->Operations;--D->Incoming;++D->Completed;xrtMutexUnlock(&D->Gate);
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

static bool xrpcDuplexMethodIs(xstrview Raw,cstr Method)
{
    xrpcstringiterator It={Raw,1,{0},0,0};unsigned char Byte;
    while(*Method)if(!xrpcStringNext(&It,&Byte) || Byte!=(unsigned char)*Method++)return false;
    return !xrpcStringNext(&It,&Byte);
}

static xrpcduplexcontrol xrpcDuplexControlKind(const xrpcduplex* D,const xrpcmessage* M)
{
    uint32 Enabled=xrtAtomic32Load(&D->EnabledExtensions,XMEMORY_ACQUIRE);
    if(Enabled&XRPC_DUPLEX_EXTENSION_CANCELLATION){
        if(xrpcDuplexMethodIs(M->Method.Raw,xrpcCancelOfferMethod))return XRPC_CONTROL_CANCEL_OFFER;
        if(xrpcDuplexMethodIs(M->Method.Raw,xrpcCancelNoticeMethod))return XRPC_CONTROL_ABANDON;
    }
    if(Enabled&XRPC_DUPLEX_EXTENSION_DEADLINE){
        if(xrpcDuplexMethodIs(M->Method.Raw,xrpcDeadlineOfferMethod))return XRPC_CONTROL_DEADLINE_OFFER;
        if(xrpcDuplexMethodIs(M->Method.Raw,xrpcDeadlineNoticeMethod))return XRPC_CONTROL_DEADLINE;
    }
    return XRPC_CONTROL_NONE;
}

static xjsonvisitaction xrpcDuplexReadCancelId(const xjsonscanevent* E,ptr User)
{
    if(E->Depth==1 && E->Index==0 && E->Type==XJSON_SCAN_STRING)
        *(uint64*)User=xrpcDuplexNumericId(E->Raw,NULL);
    return XJSON_VISIT_NEXT;
}

static xrpcduplexadmission xrpcDuplexReceiveControl(xrpcduplex* D,xrpcduplexcontrol Kind,const xrpcmessage* M)
{
    if(Kind==XRPC_CONTROL_DEADLINE)return xrpcDuplexProtocol(D);
    if(Kind!=XRPC_CONTROL_ABANDON){
        if(M->Kind!=XRPC_MESSAGE_CALL || M->Params.Kind!=XRPC_VALUE_MISSING)return xrpcDuplexProtocol(D);
        bool Deadline=Kind==XRPC_CONTROL_DEADLINE_OFFER;
        bool Supported=Deadline?D->Config.Server!=NULL:D->CancelJobs!=NULL;
        xrpcoutgoing Reply={.Kind=XRPC_MESSAGE_RESULT,.Id=M->Id.Raw,
            .Result=Supported?XRT_STR_LITERAL("true"):XRT_STR_LITERAL("false")};
        bool Limited=false;
        xrpcduplexoutput* O=xrpcDuplexMakeControl(D,&Reply,&Limited);
        if(!O){
            if(!Limited)return XRPC_DUPLEX_ERROR;
            xrtMutexLock(&D->Gate);++D->Rejected;xrtMutexUnlock(&D->Gate);
            return XRPC_DUPLEX_OVERLOADED;
        }
        O->CancelAck=Supported && !Deadline;
        O->DeadlineAck=Supported && Deadline;
        xrpcDuplexQueue(D,O);
        return XRPC_DUPLEX_ACCEPTED;
    }
    if(M->Kind!=XRPC_MESSAGE_NOTIFICATION || M->Params.Kind!=XRPC_VALUE_ARRAY)
        return xrpcDuplexProtocol(D);
    if(!D->ReceiveCancel)return XRPC_DUPLEX_ACCEPTED;
    if(M->Params.Raw.Size>256)return xrpcDuplexProtocol(D);
    xjsonreadconfig C;
    xrtJsonReadConfigInit(&C);
    C.MaxInputBytes=256;C.MaxDepth=2;C.MaxContainerItems=1;C.MaxValues=2;C.MaxStringBytes=20;
    uint64 Id=0;
    if(xrtJsonScan(M->Params.Raw,&C,xrpcDuplexReadCancelId,&Id)!=XJSON_VISIT_DONE || !Id)
        return xrpcDuplexProtocol(D);
    char Raw[24];
    xstrview Key=xrpcDuplexId(Id,Raw);
    xrpcpendingresolution Match;
    if(!xrpcPendingProbeBatch(D->CancelJobs,&Key,1,&Match))return XRPC_DUPLEX_ERROR;
    if(Match.Status==XRPC_PENDING_MATCHED){
        xrpcduplexjob* J=Match.Call.User;
        J->Abandoned=true;
        if(!J->CancelIssued){J->CancelIssued=true;xrtFutureCancel(J->Future);}
    }
    return XRPC_DUPLEX_ACCEPTED;
}

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
static bool xrpcDuplexReadDeadline(xstrview* Input,int64* Timeout)
{
    cstr Data=Input->Data;
    if(Input->Size<=XRPC_DEADLINE_OVERHEAD ||
       memcmp(Data+XRPC_DEADLINE_INNER_OFFSET-2u,"\",",2) ||
       memcmp(Data+Input->Size-2u,"]}",2))return false;
    int64 Budget=0;
    for(size_t I=0;I<XRPC_DEADLINE_DIGITS;++I){
        unsigned Digit=(unsigned char)Data[XRPC_DEADLINE_PREFIX_BYTES+I]-'0';
        if(Digit>9u || Budget>(INT64_MAX-(int64)Digit)/10)return false;
        Budget=Budget*10+(int64)Digit;
    }
    if(*Timeout==XRT_WAIT_FOREVER || Budget<*Timeout)*Timeout=Budget;
    Input->Data+=XRPC_DEADLINE_INNER_OFFSET;
    Input->Size-=XRPC_DEADLINE_OVERHEAD;
    return true;
}

static xrpcduplexadmission xrpcDuplexReceiveImpl(xrpcduplex* D,xstrview Input,xbytesview Context,double Entered)
{
    if(!D || !xrpcDuplexSpan(D,Input.Data,Input.Size) || !xrpcDuplexSpan(D,Context.Data,Context.Size)){
        xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex receive spans");return XRPC_DUPLEX_ERROR;}
    if(!xrpcDuplexEnter(D))return XRPC_DUPLEX_ERROR;
    xrpcduplexadmission Result=XRPC_DUPLEX_ERROR;
    if(D->InputEnded || xrpcDuplexIsClosing(D)){Result=XRPC_DUPLEX_CLOSED;goto Done;}
    xrpcdecodeconfig C=D->Config.Response.Decode;C.Mode=XRPC_DECODE_DUPLEX;xrpcdocument Doc;
    int64 Timeout=D->Config.IncomingTimeout;
    bool WithDeadline=D->ReceiveDeadline && Input.Size>=XRPC_DEADLINE_PREFIX_BYTES &&
        !memcmp(Input.Data,xrpcDeadlinePrefix,XRPC_DEADLINE_PREFIX_BYTES);
    if(WithDeadline && (Input.Size>C.MaxInputBytes || !xrpcDuplexReadDeadline(&Input,&Timeout))){
        Result=xrpcDuplexProtocol(D);
        goto Done;
    }
    xrpcdecodestatus S=xrpcDecode(Input,&C,D->Scratch,C.MaxMessages,&Doc);
    if(S==XRPC_DECODE_LOCAL_ERROR)goto Done;
    if(WithDeadline && (S!=XRPC_DECODE_DONE || Doc.InvalidCount)){
        Result=xrpcDuplexProtocol(D);
        goto Done;
    }
    bool Requests=false,Responses=false;
    if(S==XRPC_DECODE_DONE)for(size_t I=0;I<Doc.Count;++I){xrpcmessage* M=D->Scratch+I;
        xrpcduplexcontrol Control=(M->Flags&XRPC_METHOD_RESERVED)?xrpcDuplexControlKind(D,M):XRPC_CONTROL_NONE;
        if(Control!=XRPC_CONTROL_NONE){
            if(WithDeadline || Doc.Batch || Doc.Count!=1){Result=xrpcDuplexProtocol(D);goto Done;}
            Result=xrpcDuplexReceiveControl(D,Control,M);
            if(Result==XRPC_DUPLEX_ACCEPTED)++D->Received;
            goto Done;
        }
        bool Response=M->Kind==XRPC_MESSAGE_RESULT || M->Kind==XRPC_MESSAGE_ERROR ||
            M->Result.Kind!=XRPC_VALUE_MISSING || M->Error.Kind!=XRPC_VALUE_MISSING;
        bool Request=M->Method.Kind!=XRPC_VALUE_MISSING || !Response;
        Requests|=Request;Responses|=Response;
    }
    if(Responses && (Requests || WithDeadline)){Result=xrpcDuplexProtocol(D);goto Done;}
    if(Responses){
        Result=xrpcDuplexResponse(D,Input);
        if(Result==XRPC_DUPLEX_ACCEPTED)++D->Received;
        goto Done;
    }
    uint64 IncomingId=0;
    if(D->ReceiveCancel && S==XRPC_DECODE_DONE && !Doc.InvalidCount){
        for(size_t I=0;I<Doc.Count;++I){const xrpcmessage* M=D->Scratch+I;
            if(M->Kind!=XRPC_MESSAGE_CALL)continue;
            if(M->Id.Kind==XRPC_VALUE_STRING)IncomingId=xrpcDuplexNumericId(M->Id.Raw,NULL);
            break;
        }
    }
    xrtMutexLock(&D->Gate);
    if(D->Closing){xrtMutexUnlock(&D->Gate);Result=XRPC_DUPLEX_CLOSED;goto Done;}
    if(!D->Config.Server || D->Operations==D->Config.MaxOperations || D->Incoming==D->Config.MaxIncoming){
        ++D->Rejected;xrtMutexUnlock(&D->Gate);Result=XRPC_DUPLEX_OVERLOADED;goto Done;}
    size_t Bytes=IncomingId?sizeof(xrpcduplextrackedjob):sizeof(xrpcduplexjob);
    bool Limited=false;xrpcduplexjob* J=xrpcResultQuotaAlloc(D->Control,Bytes,&Limited);
    if(!J){if(Limited){++D->Rejected;Result=XRPC_DUPLEX_OVERLOADED;}else xrpcDuplexReport(XERR_MEMORY,XRPC_DUPLEX_MEMORY,"RPC duplex incoming job allocation failed");
        xrtMutexUnlock(&D->Gate);goto Done;}
    memset(J,0,sizeof(*J));J->Duplex=D;
    if(IncomingId){
        char Raw[24];
        xrpcpendingstatus P=xrpcPendingInsert(D->CancelJobs,xrpcDuplexId(IncomingId,Raw),
            UINT64_MAX,J,&((xrpcduplextrackedjob*)J)->Token);
        if(P!=XRPC_PENDING_OK){
            xrpcResultQuotaFree(J);xrtMutexUnlock(&D->Gate);
            if(P==XRPC_PENDING_DUPLICATE)Result=xrpcDuplexProtocol(D);
            goto Done;
        }
        J->Tracked=true;
    }
    xrpcserveradmission A=WithDeadline?
        xrpcServerSubmitDeadlineEntered(D->Config.Server,Input,Context,Timeout,&J->Future,Entered):
        xrpcServerSubmitContextEntered(D->Config.Server,Input,Context,Timeout,&J->Future,Entered);
    if(A!=XRPC_SERVER_ACCEPTED){
        if(J->Tracked){xrpcpendingcall Removed;
            xrpcPendingTakeToken(D->CancelJobs,((xrpcduplextrackedjob*)J)->Token,&Removed);}
        xrpcResultQuotaFree(J);
        if(A!=XRPC_SERVER_ADMISSION_ERROR){++D->Rejected;Result=XRPC_DUPLEX_OVERLOADED;}
        xrtMutexUnlock(&D->Gate);goto Done;
    }
    ++D->Operations;++D->Incoming;++D->Accepted;++D->Received;xrtMutexUnlock(&D->Gate);
    J->Next=D->Jobs;if(J->Next)J->Next->Prev=J;D->Jobs=J;
    xrtAtomic32FetchAdd(&D->References,1,XMEMORY_RELAXED);
    xfuturewatchresult W=xrtFutureWatchInit(&J->Watch,xrpcDuplexJobNotify,xrpcDuplexJobRelease,J)?
        xrtFutureWatchAdd(J->Future,&J->Watch):XFUTURE_WATCH_ERROR;
    if(W==XFUTURE_WATCH_READY)xrpcDuplexJobRelease(J);
    else if(W==XFUTURE_WATCH_ERROR){
        /* Registration consumed nothing. Keep the actual accepted Future duty
         * until terminal; never pretend a registration failure completed work. */
        xrpcDuplexSeal(D,XRPC_CALL_MEMORY);
        if(D->FallbackTail)D->FallbackTail->FallbackNext=J;else D->FallbackHead=J;D->FallbackTail=J;
    }
    Result=XRPC_DUPLEX_ACCEPTED;
Done:
    D->Busy=false;return Result;
}
XRPC_API xrpcduplexadmission xrpcDuplexReceive(xrpcduplex* D,xstrview Input,xbytesview Context)
{return xrpcDuplexReceiveImpl(D,Input,Context,xrtTimer());}
#if defined(XRPC_FEATURE_XRPC_STDIO)
/* The owner closes the input direction. A concurrent constructor that
 * already reserved credits still publishes and retires through the owner. */
void xrpcDuplexEndInput(xrpcduplex* D)
{
    xrtMutexLock(&D->Gate);
    D->InputEnded=true;
    xrtMutexUnlock(&D->Gate);
}
#endif
#if defined(XRPC_FEATURE_XRPC_TCP) || defined(XRPC_FEATURE_XRPC_TLS) || defined(XRPC_FEATURE_XRPC_WS) || defined(XRPC_FEATURE_XRPC_STDIO)
xrpcduplexadmission xrpcDuplexSubmitEntered(xrpcduplex* D,const xrpcduplexitem* Items,size_t Count,bool Batch,int64 Timeout,xfuture** Future,double Entered)
{return xrpcDuplexSubmit(D,Items,Count,Batch,Timeout,Future,Entered,0);}
xrpcduplexadmission xrpcDuplexReceiveEntered(xrpcduplex* D,xstrview Input,xbytesview Context,double Entered)
{return xrpcDuplexReceiveImpl(D,Input,Context,Entered);}
#endif
static bool xrpcDuplexWorkJob(xrpcduplex* D,bool* Blocked)
{
    xrtMutexLock(&D->Gate);xrpcduplexjob* J=D->ReadyHead;
    if(J){D->ReadyHead=J->ReadyNext;if(!D->ReadyHead)D->ReadyTail=NULL;J->ReadyNext=NULL;}
    xrtMutexUnlock(&D->Gate);if(!J)return false;
    if(J->Abandoned){xrpcDuplexJobRetire(D,J);return true;}
    bool Closing=xrpcDuplexIsClosing(D);xfutureresult F={0};
    if(!Closing && (!xrtFutureResult(J->Future,&F) || F.State!=XFUTURE_RESOLVED || !F.Value)){
        xrpcDuplexSeal(D,XRPC_CALL_TRANSPORT);Closing=true;}
    const xrpcserverresult* R=F.Value;
    if(!Closing && R->Status!=XRPC_SERVER_DONE){
        xrpcDuplexSeal(D,R->Status==XRPC_SERVER_MEMORY?XRPC_CALL_MEMORY:R->Status==XRPC_SERVER_LIMIT?XRPC_CALL_LIMIT:XRPC_CALL_TRANSPORT);Closing=true;}
    if(!Closing && R->Wire.Size){bool Limited=false;xrpcduplexoutput* O=xrpcDuplexOutputCreate(D,0,R->Wire.Size,&Limited);
        if(!O){if(Limited && R->Wire.Size<=SIZE_MAX-sizeof(xrpcduplexoutput) &&
                xrpcResultQuotaFits(D->Wire,sizeof(xrpcduplexoutput)+R->Wire.Size)){
                xrtMutexLock(&D->Gate);if(D->ReadyTail)D->ReadyTail->ReadyNext=J;else D->ReadyHead=J;D->ReadyTail=J;xrtMutexUnlock(&D->Gate);
                *Blocked=true;return true;}
            xrpcDuplexSeal(D,Limited?XRPC_CALL_LIMIT:XRPC_CALL_MEMORY);}
        else {O->Response=J->Future;J->Future=NULL;O->Wire=R->Wire;xrpcDuplexQueue(D,O);}
    }
    xrpcDuplexJobRetire(D,J);return true;
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
    else if(D->InputEnded)xrpcDuplexPrepareFinish(D,R,XRPC_CALL_TRANSPORT);
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
            if(P!=XRPC_CALL_PENDING){
                if(P==XRPC_CALL_FINISHED)xrpcDuplexAbandonSent(D,R);
                xrpcDuplexPrepareFinish(D,R,XRPC_CALL_CLOSED);
            }
            /* A started frame must reach its boundary or the whole output
             * direction must be discarded. Unstarted requests need no write. */
            else if(D->InputEnded && (!R->Started || R->Sent))
                xrpcDuplexPrepareFinish(D,R,XRPC_CALL_TRANSPORT);}}
    if(R->Finishing)*OK=xrpcDuplexFinish(D,R);
    return true;
}
static bool xrpcDuplexWorkClose(xrpcduplex* D)
{
    if(!xrpcDuplexIsClosing(D))return false;
    if(!D->CancelStarted){D->CancelStarted=true;D->CancelScan=D->Jobs;}
    if(D->CancelScan){xrpcduplexjob* J=D->CancelScan;D->CancelScan=J->Next;
        if(!J->CancelIssued){J->CancelIssued=true;xrtFutureCancel(J->Future);}return true;}
    if(D->OutputHead){if(D->OutputHead->Request)D->OutputHead->Request->Output=NULL;
        xrpcDuplexUnqueue(D,D->OutputHead);return true;}
    return false;
}
static bool xrpcDuplexWorkFallback(xrpcduplex* D,bool* Blocked)
{
    xrpcduplexjob* J=D->FallbackHead;if(!J)return false;
    if(!xrtFutureDone(J->Future)){*Blocked=true;return true;}
    D->FallbackHead=J->FallbackNext;if(!D->FallbackHead)D->FallbackTail=NULL;
    J->FallbackNext=NULL;xrpcDuplexJobRelease(J);return true;
}
XRPC_API bool xrpcDuplexStep(xrpcduplex* D)
{
    if(!xrpcDuplexEnter(D))return false;
    bool OK=true,Blocked=false,FallbackBlocked=false;D->More=false;
    if(!D->Scan){D->Scan=D->Head;D->ScanEnd=D->Tail;}
    double Now=D->DeadlineCount?xrtTimer():-INFINITY;
    /* Round robin persists across turns, including MaxWork=1. Admission and
     * executor floods cannot starve deadline/cancel/close work. */
    for(size_t Work=0;Work<D->Config.MaxWork && OK;++Work){bool Worked=false;
        for(unsigned Attempt=0;Attempt<5 && !Worked;++Attempt){unsigned Turn=D->Turn++%5;
            if(Turn==0 && !Blocked)Worked=xrpcDuplexWorkJob(D,&Blocked);
            else if(Turn==1)Worked=xrpcDuplexWorkSubmit(D,&OK);
            else if(Turn==2)Worked=xrpcDuplexWorkCall(D,&OK,Now);
            else if(Turn==3)Worked=xrpcDuplexWorkClose(D);
            else if(Turn==4 && !FallbackBlocked)Worked=xrpcDuplexWorkFallback(D,&FallbackBlocked);
        }
        if(!Worked)break;
    }
    D->More|=D->Scan!=NULL || (D->DeadlineCount && D->Deadlines[0]->Deadline<=Now);
    xrtMutexLock(&D->Gate);D->More|=D->SubmitHead!=NULL || (!Blocked && D->ReadyHead!=NULL) || (!FallbackBlocked && D->FallbackHead!=NULL);
    if(D->Closing)D->More|=D->OutputHead!=NULL || !D->CancelStarted || D->CancelScan!=NULL;
    xrtMutexUnlock(&D->Gate);
    D->Busy=false;return OK;
}
static void xrpcDuplexStampDeadline(xrpcduplexoutput* O)
{
    double Millis=(O->Request->Deadline-xrtTimer())*1000.;
    uint64 Budget=Millis<=0.?0:Millis>=0x1p63?(uint64)INT64_MAX:(uint64)Millis;
    char* End=(char*)(O+1)+XRPC_DEADLINE_PREFIX_BYTES+XRPC_DEADLINE_DIGITS;
    for(size_t I=0;I<XRPC_DEADLINE_DIGITS;++I){
        *--End=(char)('0'+Budget%10u);
        Budget/=10u;
    }
}

#if defined(XRPC_FEATURE_XRPC_TCP) || defined(XRPC_FEATURE_XRPC_TLS) || defined(XRPC_FEATURE_XRPC_WS)
size_t xrpcDuplexNextSize(const xrpcduplex* D)
{return D->OutputHead?D->OutputHead->Wire.Size:0;}
#endif

#if defined(XRPC_FEATURE_XRPC_TCP)
bool xrpcDuplexSelectionIs(const xrpcduplex* D,uint64 Token)
{return Token && D->SelectedToken==Token;}
#endif

XRPC_API int xrpcDuplexNext(xrpcduplex* D,xrpcduplexoutput** Output,uint64* Token)
{
    if(!D || !Output || !Token || !xrpcDuplexSpan(D,Output,sizeof(*Output)) || !xrpcDuplexSpan(D,Token,sizeof(*Token)) ||
       xrtMemRangesOverlap(Output,sizeof(*Output),Token,sizeof(*Token))){xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex output selection spans");return -1;}
    if(!xrpcDuplexEnter(D))return -1;
    int Result=0;
    if(D->Selection){xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC duplex already has a selected output");Result=-1;}
    else if(!xrpcDuplexIsClosing(D) && D->OutputHead &&
            (!D->InputEnded || !D->OutputHead->Request)){
        if(D->SelectionSerial==UINT64_MAX){xrpcDuplexSeal(D,XRPC_CALL_LIMIT);xrpcDuplexReport(XERR_RANGE,XRPC_DUPLEX_LIMIT,"RPC duplex selection serial exhausted");Result=-1;}
        else if(!xrpcDuplexOutputRetain(D->OutputHead))Result=-1;
        else {
            D->Selection=D->OutputHead;
            if(D->Selection->DeadlineEnvelope)xrpcDuplexStampDeadline(D->Selection);
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
        bool Cancel=(D->AgreedExtensions&XRPC_DUPLEX_EXTENSION_CANCELLATION)!=0;
        xrtMutexUnlock(&D->Gate);
        xrpcduplexrequest* R=D->Selection->Request;
        if(!Closing){
            if(!R)OK=true;
            else if(!R->Started && !D->InputEnded && xrpcCallPoll(R->Call)==XRPC_CALL_PENDING){
                OK=xrpcCallMarkSent(R->Call);R->Started=OK;R->TrackCancel=OK && Cancel;
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
            if(O->CancelAck)D->ReceiveCancel=true;
            if(O->DeadlineAck)D->ReceiveDeadline=true;
            xrpcDuplexUnqueue(D,O);OK=true;
            if(R && (!R->Replies || D->InputEnded)){
                xrpcDuplexPrepareFinish(D,R,R->Replies?XRPC_CALL_TRANSPORT:XRPC_CALL_NOTIFY_DONE);
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
    bool Submit=D->SubmitHead!=NULL,Ready=D->ReadyHead!=NULL;
    xrtMutexUnlock(&D->Gate);V.Received=D->Received;V.UnknownResponses=D->UnknownResponses;V.ProtocolFailures=D->ProtocolFailures;
    xrpcResultQuotaInfo(D->Control,&V.ControlBlocks,&V.ControlBytes,&V.ControlRefusals);
    xrpcResultQuotaInfo(D->Wire,&V.Outputs,&V.OutputBytes,&V.OutputRefusals);xrpcResultQuotaInfo(D->Results,&V.Results,&V.ResultBytes,&V.ResultRefusals);
    V.Selected=D->Selection!=NULL;V.Started=D->Selection && D->Selection->Started;
    V.Drained=V.Closing && !V.Operations && !D->OutputHead && !D->Selection && xrtAtomic32Load(&D->References,XMEMORY_ACQUIRE)==1;
    V.More=D->More || Submit;
    if(D->DeadlineCount)V.NextDeadline=D->Deadlines[0]->Deadline;
    if(V.More || D->Finishing)V.NextDeadline=xrtTimer();
    else if(Ready || D->FallbackHead || (V.Closing && !V.Drained && !V.Operations)){
        double Retry=xrtTimer()+.001;
        if(Retry<V.NextDeadline)V.NextDeadline=Retry; /* retry retained-wire credit */
    }
    *I=V;return true;
}
XRPC_API bool xrpcDuplexDestroy(xrpcduplex* D)
{
    if(!D)return true;
    if(D->Busy)return xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC duplex destruction during owner callback");
    xrpcduplexinfo I={0};I.Size=sizeof(I);if(!xrpcDuplexInfo(D,&I))return false;
    if(!I.Drained)return xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC duplex close duties still live");
    xrtFutureDestroy(D->Offers[0]);xrtFutureDestroy(D->Offers[1]);xrpcPendingDestroy(D->CancelJobs);
    xrpcPendingDestroy(D->Pending);xrpcResultQuotaRelease(D->Control);xrpcResultQuotaRelease(D->Wire);xrpcResultQuotaRelease(D->Results);
    xrtMutexUnit(&D->Gate);xrtFree(D);return true;
}
#endif
