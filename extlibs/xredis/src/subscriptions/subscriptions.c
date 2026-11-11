#include <xredis/subscriptions.h>
#include "../internal/subscription_replay.h"
#include <xrt/hash.h>
#include <xrt/memory.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
typedef struct xredissubscriptionentry {
    xatomic32 References;uint16 Family,Slot;uint64 Hash;size_t Bytes;unsigned char Name[];
} xredissubscriptionentry;
struct xredissubscriptionsnapshot {
    xatomic32 References;uint64 Revision;size_t Count,NameBytes,EntryBytes,StorageBytes;
    xredissubscriptionentry* Entries[];
};
struct xredissubscriptions {
    xatomic32 References;xmutex Gate;xredissubscriptionsconfig Config;
    xredissubscriptionsnapshot* Current;bool Preparing;
};
typedef struct xredissubscriptionedit {xbytesview Name;uint64 Hash;uint16 Slot;} xredissubscriptionedit;
typedef struct xredissubscriptionmerge {size_t Count,NameBytes,EntryBytes;bool Changed;} xredissubscriptionmerge;
struct xredissubscriptionreplay {
    xredissubscriptions* Store;xredissubscriptionsnapshot* Snapshot;xredisclient* Client;
    xredissubscriptionreplayconfig Config;xredissubscriptionreplayresult Result;
    xfuture* Future;size_t Offset,InFlight,StorageBytes;uint64 ExpectedRegular,ExpectedShard;double Entered,Deadline;
    xbytesview Names[];
};
static bool xredisSubscriptionsReport(xerrkind Kind,cstr Text)
{xrtSetErrorInfo(Kind,"xrt.redis",Kind==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,Text);return false;}
static bool xredisSubscriptionsRef(xatomic32* Ref)
{
    uint32 N=xrtAtomic32Load(Ref,XMEMORY_RELAXED);
    for(;;){if(N==UINT32_MAX)return xredisSubscriptionsReport(XERR_RANGE,"subscription reference overflow");
        if(xrtAtomic32CompareExchange(Ref,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
static void xredisSubscriptionEntryRelease(xredissubscriptionentry* E)
{if(xrtAtomic32FetchSub(&E->References,1,XMEMORY_ACQ_REL)==1)xrtFree(E);}
XREDIS_API bool xredisSubscriptionSnapshotRetain(xredissubscriptionsnapshot* P)
{return P?xredisSubscriptionsRef(&P->References):xredisSubscriptionsReport(XERR_ARGUMENT,"snapshot is NULL");}
XREDIS_API void xredisSubscriptionSnapshotRelease(xredissubscriptionsnapshot* P)
{
    if(!P || xrtAtomic32FetchSub(&P->References,1,XMEMORY_ACQ_REL)!=1)return;
    for(size_t N=0;N<P->Count;++N){xredisSubscriptionEntryRelease(P->Entries[N]);}
    xrtFree(P);
}
XREDIS_API bool xredisSubscriptionsRetain(xredissubscriptions* S)
{return S?xredisSubscriptionsRef(&S->References):xredisSubscriptionsReport(XERR_ARGUMENT,"subscription store is NULL");}
XREDIS_API void xredisSubscriptionsRelease(xredissubscriptions* S)
{
    if(!S || xrtAtomic32FetchSub(&S->References,1,XMEMORY_ACQ_REL)!=1)return;
    xredisSubscriptionSnapshotRelease(S->Current);xrtMutexUnit(&S->Gate);xrtFree(S);
}
XREDIS_API bool xredisSubscriptionsConfigInit(xredissubscriptionsconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid subscription config");
    *K=(xredissubscriptionsconfig){sizeof(*K),4096,1024u*1024u,1024u*1024u};return true;
}
XREDIS_API bool xredisSubscriptionsCreate(const xredissubscriptionsconfig* Config,xredissubscriptions** Output)
{
    xredissubscriptionsconfig K;
    if(!Output || !xrtMemRangeValid(Output,sizeof(*Output)) || (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output)))))
        return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid subscription creation output");
    if(Config)K=*Config;else xredisSubscriptionsConfigInit(&K);
    if(K.Size!=sizeof(K) || !K.MaxSubscriptions || K.MaxSubscriptions>100000 || !K.MaxNameBytes || !K.MaxChangeBytes ||
        K.MaxNameBytes>SIZE_MAX-sizeof(xredissubscriptionsnapshot)-sizeof(xredissubscriptions)-K.MaxSubscriptions*(sizeof(xredissubscriptionentry)+sizeof(void*)))
        return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid subscription bounds");
    xredissubscriptions* S=xrtCalloc(1,sizeof(*S));if(!S)return false;
    xrtAtomic32Init(&S->References,1);S->Config=K;
    if(!xrtMutexInit(&S->Gate)){xrtFree(S);return false;}
    S->Current=xrtCalloc(1,sizeof(*S->Current));if(!S->Current){xrtMutexUnit(&S->Gate);xrtFree(S);return false;}
    xrtAtomic32Init(&S->Current->References,1);S->Current->StorageBytes=sizeof(*S->Current);*Output=S;return true;
}
/* Redis Cluster normative CRC16/XMODEM polynomial and first-pair hash tags.
 * Compute once for edited shard names; recovery reads the cached slot. */
static uint16 xredisSubscriptionSlot(xbytesview Name)
{
    size_t Begin=0,End=Name.Size,Open=0;while(Open<Name.Size && Name.Data[Open]!='{')++Open;
    if(Open<Name.Size){size_t Close=Open+1;while(Close<Name.Size && Name.Data[Close]!='}')++Close;
        if(Close<Name.Size && Close>Open+1){Begin=Open+1;End=Close;}}
    uint16 Crc=0;
    for(size_t N=Begin;N<End;++N){Crc^=(uint16)((uint16)Name.Data[N]<<8);
        for(unsigned Bit=0;Bit<8;++Bit)Crc=(uint16)((Crc<<1)^((Crc&0x8000u)?0x1021u:0));}
    return (uint16)(Crc&16383u);
}
static int xredisSubscriptionCompareName(uint16 ASlot,uint64 AHash,xbytesview A,uint16 BSlot,uint64 BHash,xbytesview B)
{
    if(ASlot!=BSlot)return ASlot<BSlot?-1:1;
    if(AHash!=BHash)return AHash<BHash?-1:1;
    if(A.Size!=B.Size)return A.Size<B.Size?-1:1;
    return A.Size?memcmp(A.Data,B.Data,A.Size):0;
}
static int xredisSubscriptionCompareEdit(const void* A,const void* B)
{
    const xredissubscriptionedit *X=A,*Y=B;return xredisSubscriptionCompareName(X->Slot,X->Hash,X->Name,Y->Slot,Y->Hash,Y->Name);
}
static bool xredisSubscriptionEmit(xredissubscriptionsnapshot* Dest,xredissubscriptionmerge* M,
    const xredissubscriptionsconfig* K,xredissubscriptionentry* Existing,const xredissubscriptionedit* Added,unsigned Family)
{
    size_t Bytes=Existing?Existing->Bytes:Added->Name.Size;
    if(M->Count==K->MaxSubscriptions || Bytes>K->MaxNameBytes-M->NameBytes)return false;
    ++M->Count;M->NameBytes+=Bytes;M->EntryBytes+=sizeof(xredissubscriptionentry)+Bytes;
    if(Dest){xredissubscriptionentry* E=Existing;
        if(E){if(!xredisSubscriptionsRef(&E->References))return false;}
        else{E=xrtMalloc(sizeof(*E)+Bytes);if(!E)return false;xrtAtomic32Init(&E->References,1);E->Family=(uint16)Family;E->Slot=Added->Slot;E->Hash=Added->Hash;E->Bytes=Bytes;if(Bytes)memcpy(E->Name,Added->Name.Data,Bytes);}
        Dest->Entries[Dest->Count++]=E;
    }
    return true;
}
static bool xredisSubscriptionMerge(const xredissubscriptionsnapshot* Old,const xredissubscriptionedit* Edits,size_t Count,
    bool Add,bool All,unsigned Family,const xredissubscriptionsconfig* K,xredissubscriptionsnapshot* Dest,xredissubscriptionmerge* M)
{
    *M=(xredissubscriptionmerge){0};size_t A=0,B=0;
    while(A<Old->Count || B<Count){
        xredissubscriptionentry* E=A<Old->Count?Old->Entries[A]:NULL;
        if(All){if(E->Family==Family){M->Changed=true;++A;continue;}if(!xredisSubscriptionEmit(Dest,M,K,E,NULL,Family))return false;++A;continue;}
        int Compare=!E?1:B==Count?-1:E->Family!=Family?(E->Family<Family?-1:1):
            xredisSubscriptionCompareName(E->Slot,E->Hash,(xbytesview){E->Name,E->Bytes},Edits[B].Slot,Edits[B].Hash,Edits[B].Name);
        if(Compare<0){if(!xredisSubscriptionEmit(Dest,M,K,E,NULL,Family))return false;++A;}
        else if(!Compare){if(Add){if(!xredisSubscriptionEmit(Dest,M,K,E,NULL,Family))return false;}else M->Changed=true;++A;++B;}
        else {if(Add){if(!xredisSubscriptionEmit(Dest,M,K,NULL,Edits+B,Family))return false;M->Changed=true;}++B;}
    }
    return true;
}
XREDIS_API xredisadmission xredisSubscriptionsApply(xredissubscriptions* S,xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision)
{
    if(!S || (unsigned)Command>XREDIS_PUBSUB_CMD_SUNSUBSCRIBE || !Revision || !xrtMemRangeValid(Revision,sizeof(*Revision)) ||
        xrtMemRangesOverlap(S,S?sizeof(*S):0,Revision,sizeof(*Revision)) || Count>XREDIS_PUBSUB_NAMES_MAX || ((unsigned)Command<3 && !Count) || !xrtMemRangeValid(Names,Count*sizeof(*Names)) ||
        xrtMemRangesOverlap(Names,Count*sizeof(*Names),Revision,sizeof(*Revision))){
        xredisSubscriptionsReport(XERR_ARGUMENT,"invalid desired subscription edit");return XREDIS_ADMISSION_ERROR;}
    size_t InputBytes=0;
    for(size_t N=0;N<Count;++N){if(!xrtMemRangeValid(Names[N].Data,Names[N].Size) || xrtMemRangesOverlap(Names[N].Data,Names[N].Size,Revision,sizeof(*Revision))){xredisSubscriptionsReport(XERR_ARGUMENT,"edit output aliases name");return XREDIS_ADMISSION_ERROR;}
        if(Names[N].Size>S->Config.MaxChangeBytes-InputBytes)return XREDIS_ADMISSION_OVERLOADED;
        InputBytes+=Names[N].Size;}
    xrtMutexLock(&S->Gate);
    if(S->Preparing){xrtMutexUnlock(&S->Gate);return XREDIS_ADMISSION_OVERLOADED;}
    S->Preparing=true;xredissubscriptionsnapshot* Old=S->Current;xrtMutexUnlock(&S->Gate);
    xredisadmission Result=XREDIS_ADMISSION_ERROR;xredissubscriptionsnapshot* Next=NULL;xredissubscriptionedit* Edits=NULL;
    if(Count){Edits=xrtMalloc(Count*sizeof(*Edits));if(!Edits)goto Done;
        for(size_t N=0;N<Count;++N)Edits[N]=(xredissubscriptionedit){Names[N],xrtHash64(Names[N].Data,Names[N].Size),(unsigned)Command%3==2?xredisSubscriptionSlot(Names[N]):0};
        qsort(Edits,Count,sizeof(*Edits),xredisSubscriptionCompareEdit);
        size_t Unique=0;for(size_t N=0;N<Count;++N)if(!Unique || xredisSubscriptionCompareEdit(Edits+N,Edits+Unique-1))Edits[Unique++]=Edits[N];Count=Unique;
    }
    xredissubscriptionmerge M;bool Add=(unsigned)Command<3,All=!Add && !Count;unsigned Family=(unsigned)Command%3;
    if(!xredisSubscriptionMerge(Old,Edits,Count,Add,All,Family,&S->Config,NULL,&M)){Result=XREDIS_ADMISSION_OVERLOADED;goto Done;}
    if(!M.Changed){*Revision=Old->Revision;Result=XREDIS_ADMISSION_ACCEPTED;goto Done;}
    if(Old->Revision==UINT64_MAX){xredisSubscriptionsReport(XERR_RANGE,"subscription revision exhausted");goto Done;}
    size_t Bytes=sizeof(*Next)+M.Count*sizeof(*Next->Entries);Next=xrtCalloc(1,Bytes);if(!Next)goto Done;
    xrtAtomic32Init(&Next->References,1);Next->StorageBytes=Bytes;Next->Revision=Old->Revision+1;
    if(!xredisSubscriptionMerge(Old,Edits,Count,Add,All,Family,&S->Config,Next,&M))goto Done;
    Next->NameBytes=M.NameBytes;Next->EntryBytes=M.EntryBytes;
    xrtMutexLock(&S->Gate);S->Current=Next;*Revision=Next->Revision;Next=NULL;xrtMutexUnlock(&S->Gate);
    xredisSubscriptionSnapshotRelease(Old);Result=XREDIS_ADMISSION_ACCEPTED;
Done:
    xredisSubscriptionSnapshotRelease(Next);xrtFree(Edits);
    xrtMutexLock(&S->Gate);S->Preparing=false;xrtMutexUnlock(&S->Gate);return Result;
}
static xredissubscriptionsinfo xredisSubscriptionInfoValue(const xredissubscriptionsnapshot* P,bool Preparing)
{return (xredissubscriptionsinfo){sizeof(xredissubscriptionsinfo),P->Revision,P->Count,P->NameBytes,P->StorageBytes,P->EntryBytes,Preparing};}
XREDIS_API bool xredisSubscriptionsInfo(xredissubscriptions* S,xredissubscriptionsinfo* Info)
{
    if(!S || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || Info->Size!=sizeof(*Info) || xrtMemRangesOverlap(S,S?sizeof(*S):0,Info,sizeof(*Info)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid desired-state output");
    xrtMutexLock(&S->Gate);*Info=xredisSubscriptionInfoValue(S->Current,S->Preparing);xrtMutexUnlock(&S->Gate);return true;
}
XREDIS_API bool xredisSubscriptionsSnapshot(xredissubscriptions* S,xredissubscriptionsnapshot** Output)
{
    if(!S || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(S,S?sizeof(*S):0,Output,sizeof(*Output)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid snapshot output");
    xrtMutexLock(&S->Gate);bool OK=xredisSubscriptionSnapshotRetain(S->Current);if(OK)*Output=S->Current;xrtMutexUnlock(&S->Gate);return OK;
}
XREDIS_API bool xredisSubscriptionSnapshotInfo(const xredissubscriptionsnapshot* P,xredissubscriptionsinfo* Info)
{
    if(!P || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || Info->Size!=sizeof(*Info) || xrtMemRangesOverlap(P,P?P->StorageBytes:0,Info,sizeof(*Info)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid immutable snapshot output");
    *Info=xredisSubscriptionInfoValue(P,false);return true;
}
XREDIS_API bool xredisSubscriptionSnapshotAt(const xredissubscriptionsnapshot* P,size_t Index,xredissubscriptionview* View)
{
    if(!P || !View || !xrtMemRangeValid(View,sizeof(*View)) || xrtMemRangesOverlap(P,P?P->StorageBytes:0,View,sizeof(*View)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid snapshot view output");
    if(Index>=P->Count)return false;
    xredissubscriptionentry* E=P->Entries[Index];*View=(xredissubscriptionview){(xredispubsubcommand)E->Family,{E->Name,E->Bytes}};return true;
}
XREDIS_API bool xredisSubscriptionReplayConfigInit(xredissubscriptionreplayconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid replay config");
    *K=(xredissubscriptionreplayconfig){0};K->Size=sizeof(*K);xredisSubscriberConfigInit(&K->Registry);K->MaxBatchNames=64;K->MaxBatchBytes=65536;K->Timeout=5000;return true;
}
XREDIS_API xredisadmission xredisSubscriptionReplayCreate(xredissubscriptions* S,xredisclient* C,const xredissubscriptionreplayconfig* Config,xredissubscriptionreplay** Output)
{
    double Entered=xrtTimer();xredissubscriptionreplayconfig K;
    if(!S || !C || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(S,S?sizeof(*S):0,Output,sizeof(*Output)) ||
       (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output))))){xredisSubscriptionsReport(XERR_ARGUMENT,"invalid replay arguments");return XREDIS_ADMISSION_ERROR;}
    if(Config)K=*Config;else xredisSubscriptionReplayConfigInit(&K);
    if(K.Size!=sizeof(K) || !K.MaxBatchNames || K.MaxBatchNames>XREDIS_PUBSUB_NAMES_MAX || !K.MaxBatchBytes || K.Timeout<XRT_WAIT_FOREVER){xredisSubscriptionsReport(XERR_ARGUMENT,"invalid replay work bounds");return XREDIS_ADMISSION_ERROR;}
    xredissubscriptionsnapshot* P=NULL;if(!xredisSubscriptionsSnapshot(S,&P))return XREDIS_ADMISSION_ERROR;
    if(P->Count>K.Registry.MaxSubscriptions || P->NameBytes>K.Registry.MaxNameBytes){xredisSubscriptionSnapshotRelease(P);xredisSubscriptionsReport(XERR_RANGE,"native registry cannot fit desired set");return XREDIS_ADMISSION_ERROR;}
    size_t Slots=P->Count<K.MaxBatchNames?P->Count:K.MaxBatchNames;
    size_t Bytes=sizeof(xredissubscriptionreplay)+Slots*sizeof(xbytesview);xredissubscriptionreplay* R=xrtCalloc(1,Bytes);
    if(!R){xredisSubscriptionSnapshotRelease(P);return XREDIS_ADMISSION_ERROR;}
    if(!xredisSubscriptionsRetain(S)){xredisSubscriptionSnapshotRelease(P);xrtFree(R);return XREDIS_ADMISSION_ERROR;}
    if(!xredisClientRetain(C)){xredisSubscriptionsRelease(S);xredisSubscriptionSnapshotRelease(P);xrtFree(R);return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=__xredisSubscriberReplayStart(C,&K.Registry,R);
    if(A!=XREDIS_ADMISSION_ACCEPTED){xredisClientRelease(C);xredisSubscriptionsRelease(S);xredisSubscriptionSnapshotRelease(P);xrtFree(R);return A;}
    R->Store=S;R->Snapshot=P;R->Client=C;R->Config=K;R->StorageBytes=Bytes;R->Entered=Entered;
    R->Deadline=K.Timeout==XRT_WAIT_FOREVER?INFINITY:K.Timeout?nextafter(Entered+(double)K.Timeout*.001,INFINITY):Entered;
    R->Result.Size=sizeof(R->Result);R->Result.Revision=P->Revision;*Output=R;return XREDIS_ADMISSION_ACCEPTED;
}
static xredissubscriptionstatus xredisSubscriptionNativeStatus(xredisrequeststatus S)
{
    switch(S){case XREDIS_REQUEST_OK:return XREDIS_SUBSCRIPTIONS_OK;case XREDIS_REQUEST_SERVER_ERROR:return XREDIS_SUBSCRIPTIONS_SERVER_ERROR;
    case XREDIS_REQUEST_TIMEOUT:return XREDIS_SUBSCRIPTIONS_TIMEOUT;case XREDIS_REQUEST_CANCELLED:return XREDIS_SUBSCRIPTIONS_CANCELLED;
    case XREDIS_REQUEST_PROTOCOL:return XREDIS_SUBSCRIPTIONS_PROTOCOL;case XREDIS_REQUEST_LIMIT:return XREDIS_SUBSCRIPTIONS_LIMIT;
    case XREDIS_REQUEST_MEMORY:return XREDIS_SUBSCRIPTIONS_MEMORY;default:return XREDIS_SUBSCRIPTIONS_CLOSED;}
}
static xredissubscriptionstep xredisSubscriptionFinish(xredissubscriptionreplay* R,xredissubscriptionstatus Status,xredisrequeststatus Native)
{
    R->Result.Status=Status;R->Result.NativeStatus=Native;if(Status!=XREDIS_SUBSCRIPTIONS_OK)(void)xredisClientClose(R->Client);__xredisSubscriberReplayRelease(R->Client,R);return XREDIS_SUBSCRIPTIONS_DONE;
}
static size_t xredisSubscriptionDigits(size_t N)
{size_t D=1;while(N>=10){N/=10;++D;}return D;}
XREDIS_API xredissubscriptionstep xredisSubscriptionReplayStep(xredissubscriptionreplay* R)
{
    if(!R){xredisSubscriptionsReport(XERR_ARGUMENT,"replay is NULL");return XREDIS_SUBSCRIPTIONS_STEP_ERROR;}
    if(R->Result.Status!=XREDIS_SUBSCRIPTIONS_PENDING)return XREDIS_SUBSCRIPTIONS_DONE;
    bool Observed=false;
    if(R->Future && xrtFutureDone(R->Future)){
        xfutureresult F;if(!xrtFutureResult(R->Future,&F) || F.State!=XFUTURE_RESOLVED || !F.Value)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_PROTOCOL,XREDIS_REQUEST_PROTOCOL);
        const xredissubscriberresult* V=F.Value;R->Result.Acknowledged+=V->Acknowledged;R->Result.Subscriptions=V->Subscriptions;R->Result.ShardSubscriptions=V->ShardSubscriptions;
        if(V->Request.Status!=XREDIS_REQUEST_OK){R->Result.ErrorReply=V->Request.Reply;R->Result.Cause=V->Request.Cause;return xredisSubscriptionFinish(R,xredisSubscriptionNativeStatus(V->Request.Status),V->Request.Status);}
        if(V->Acknowledged!=R->InFlight || V->Subscriptions!=R->ExpectedRegular || V->ShardSubscriptions!=R->ExpectedShard)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_PROTOCOL,XREDIS_REQUEST_PROTOCOL);
        R->Offset+=R->InFlight;++R->Result.Batches;xrtFutureDestroy(R->Future);R->Future=NULL;Observed=true;
    }
    xrtMutexLock(&R->Store->Gate);bool Stale=R->Store->Current->Revision!=R->Snapshot->Revision;xrtMutexUnlock(&R->Store->Gate);
    if(Stale)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_STALE,XREDIS_REQUEST_CANCELLED);
    if(xrtTimer()>=R->Deadline)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_TIMEOUT,XREDIS_REQUEST_TIMEOUT);
    xredisclientinfo I={0};I.Size=sizeof(I);if(!xredisClientInfo(R->Client,&I))return XREDIS_SUBSCRIPTIONS_STEP_ERROR;
    if(I.State>=XREDIS_CLIENT_CLOSING){xredisrequeststatus Closed=I.CloseStatus==XREDIS_REQUEST_OK?XREDIS_REQUEST_CLOSED:I.CloseStatus;return xredisSubscriptionFinish(R,xredisSubscriptionNativeStatus(Closed),Closed);}
    if(R->Offset==R->Snapshot->Count)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_OK,XREDIS_REQUEST_OK);
    if(R->Future)return XREDIS_SUBSCRIPTIONS_WAIT;
    if(Observed || I.Requests)return XREDIS_SUBSCRIPTIONS_MORE;
    unsigned Family=R->Snapshot->Entries[R->Offset]->Family;size_t Count=0,Sum=0,Word=Family?10:9,CommandBytes=Word+5+xredisSubscriptionDigits(Word);
    while(R->Offset+Count<R->Snapshot->Count && Count<R->Config.MaxBatchNames){xredissubscriptionentry* E=R->Snapshot->Entries[R->Offset+Count];if(E->Family!=Family || (Family==2 && E->Slot!=R->Snapshot->Entries[R->Offset]->Slot))break;
        size_t Digits=xredisSubscriptionDigits(E->Bytes),Header=CommandBytes+3+xredisSubscriptionDigits(Count+2);
        if(E->Bytes>SIZE_MAX-5-Digits || Header>R->Config.MaxBatchBytes || Sum>R->Config.MaxBatchBytes-Header || E->Bytes+5+Digits>R->Config.MaxBatchBytes-Header-Sum)break;
        Sum+=E->Bytes+5+Digits;R->Names[Count++]=(xbytesview){E->Name,E->Bytes};
    }
    if(!Count)return xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_LIMIT,XREDIS_REQUEST_LIMIT);
    R->InFlight=Count;R->ExpectedRegular=R->Result.Subscriptions+(Family<2?Count:0);R->ExpectedShard=R->Result.ShardSubscriptions+(Family==2?Count:0);
    xredisadmission A=__xredisSubscriberCommandBudget(R->Client,(xredispubsubcommand)Family,R->Names,Count,R->Config.Timeout,R->Entered,R,&R->Future);
    if(A==XREDIS_ADMISSION_ACCEPTED)return XREDIS_SUBSCRIPTIONS_WAIT;
    return xredisSubscriptionFinish(R,A==XREDIS_ADMISSION_OVERLOADED?XREDIS_SUBSCRIPTIONS_LIMIT:A==XREDIS_ADMISSION_CLOSED?XREDIS_SUBSCRIPTIONS_CLOSED:XREDIS_SUBSCRIPTIONS_MEMORY,
        A==XREDIS_ADMISSION_OVERLOADED?XREDIS_REQUEST_LIMIT:A==XREDIS_ADMISSION_CLOSED?XREDIS_REQUEST_CLOSED:XREDIS_REQUEST_MEMORY);
}
XREDIS_API xfuture* xredisSubscriptionReplayPending(xredissubscriptionreplay* R)
{if(!R){xredisSubscriptionsReport(XERR_ARGUMENT,"replay is NULL");return NULL;}return R->Future && !xrtFutureDone(R->Future)?xrtFutureRef(R->Future):NULL;}
XREDIS_API bool xredisSubscriptionReplayResult(xredissubscriptionreplay* R,xredissubscriptionreplayresult* Output)
{
    if(!R || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || Output->Size!=sizeof(*Output) || xrtMemRangesOverlap(R,R?R->StorageBytes:0,Output,sizeof(*Output)))return xredisSubscriptionsReport(XERR_ARGUMENT,"invalid replay result");
    *Output=R->Result;return true;
}
XREDIS_API bool xredisSubscriptionReplayCancel(xredissubscriptionreplay* R)
{
    if(!R)return xredisSubscriptionsReport(XERR_ARGUMENT,"replay is NULL");
    if(R->Result.Status!=XREDIS_SUBSCRIPTIONS_PENDING)return false;
    (void)xredisSubscriptionFinish(R,XREDIS_SUBSCRIPTIONS_CANCELLED,XREDIS_REQUEST_CANCELLED);return true;
}
XREDIS_API void xredisSubscriptionReplayDestroy(xredissubscriptionreplay* R)
{
    if(!R)return;
    if(R->Result.Status==XREDIS_SUBSCRIPTIONS_PENDING)(void)xredisClientClose(R->Client);
    __xredisSubscriberReplayRelease(R->Client,R);xrtFutureDestroy(R->Future);xredisClientRelease(R->Client);xredisSubscriptionSnapshotRelease(R->Snapshot);xredisSubscriptionsRelease(R->Store);xrtFree(R);
}
#endif
