#include <xmqtt/journal.h>
#include "../internal/xmqtt_store_checkpoint_online.h"
#include "../internal/xmqtt_journal_replay.h"
#include "../internal/xmqtt_journal_worker.h"
#include <xrt/crc32.h>
#include <xrt/memory.h>
#include <stdlib.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL)
enum {XMQTT_J_PAGE=4096,XMQTT_J_START=12288,XMQTT_J_HEAD=48,XMQTT_J_OVERHEAD=64};
struct xmqttjournal {
    xfile File;xmqttjournalconfig Config;xmqttjournalinfo Info;
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_WORKER)
    const void* Worker;
#endif
    uint64 Cursor,ReadSequence,FileBytes;uint32 ReadChain;bool Busy;
    xmqttjournalprefix Prefix;
    unsigned char* Page;unsigned char* Buffer;
};
static bool xmqttJOwner(const xmqttjournal* J,const void* Owner)
{
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_WORKER)
    return J->Worker==Owner;
#else
    (void)J;return !Owner;
#endif
}
typedef struct xmqttjwitness {uint64 Sequence,End;uint32 Chain;} xmqttjwitness;
static xmqttjournalstatus xmqttJError(xmqttjournalstatus S,cstr Message)
{
    xerrkind K=S==XMQTT_JOURNAL_CORRUPT?XERR_PROTOCOL:S==XMQTT_JOURNAL_STATE?XERR_STATE:
        (S==XMQTT_JOURNAL_IO||S==XMQTT_JOURNAL_UNCERTAIN)?XERR_IO:XERR_ARGUMENT;
    xrtSetErrorInfo(K,"xrt.mqtt.journal",6200-(int32)S,Message);return S;
}
static uint64 xmqttJGet(const unsigned char* P,unsigned N)
{uint64 V=0;for(unsigned I=0;I<N;++I)V=(V<<8)|P[I];return V;}
static void xmqttJPut(unsigned char* P,uint64 V,unsigned N)
{for(unsigned I=N;I;--I){P[I-1]=(unsigned char)V;V>>=8;}}
static bool xmqttJSpan(const void* P,size_t N)
{return (P||!N)&&xrtMemRangeValid(P,N);}
static bool xmqttJSeparate(const void* A,size_t N,const void* B,size_t M)
{return !N||!M||!xrtMemRangesOverlap(A,N,B,M);}
static bool xmqttJOutside(xmqttjournal* J,const void* P,size_t N)
{return xmqttJSpan(P,N)&&xmqttJSeparate(P,N,J,J->Info.MemoryBytes);}
static bool xmqttJZero(const unsigned char* P,size_t N)
{for(size_t I=0;I<N;++I)if(P[I])return false;return true;}
static void xmqttJSeal(unsigned char* P)
{xmqttJPut(P+4092,xrtCrc32(P,4092),4);}
static bool xmqttJCheck(const unsigned char* P)
{return xmqttJGet(P+4092,4)==xrtCrc32(P,4092);}
/* Isolate stale caller errors from callbacks that fail without setting one. */
static xmqttjournalstatus xmqttJFinish(xmqttjournal* J,xerror* Saved,xmqttjournalstatus S)
{
    if(S>=0){xerror* Discard=NULL;
        if(xrtTrySwapErrorTake(Saved,&Discard))xrtErrorFree(Discard);
        else {xrtErrorFree(Saved);if(J)J->Info.State=XMQTT_JOURNAL_POISONED;S=XMQTT_JOURNAL_ERROR;}}
    else {xrtErrorFree(Saved);if(!xrtGetError())xrtSetErrorKind(XERR_IO);}
    if(J)J->Busy=false;
    return S;
}
static xmqttjournalstatus xmqttJPoison(xmqttjournal* J,xmqttjournalstatus S,cstr Message)
{J->Info.State=XMQTT_JOURNAL_POISONED;if(!xrtGetError()||S==XMQTT_JOURNAL_CORRUPT)xmqttJError(S,Message);return S;}
static bool xmqttJRead(xmqttjournal* J,uint64 At,void* P,size_t N)
{size_t Done=0;return xrtReadAtFull(J->File,At,P,N,&Done);}
static bool xmqttJWrite(xmqttjournal* J,uint64 At,const void* P,size_t N)
{size_t Done=0;return xrtWriteAtFull(J->File,At,P,N,&Done);}
static bool xmqttJConfig(const xmqttjournalconfig* C)
{
    return xmqttJSpan(C,sizeof(*C))&&C->Size==sizeof(*C)&&
        (C->Version==XMQTT_VERSION_5||C->Version==XMQTT_VERSION_311)&&
        C->Binding.Size&&C->Binding.Size<=1024&&xmqttJSpan(C->Binding.Data,C->Binding.Size)&&
        C->Generation&&C->PayloadSchema&&C->MaxTransactionBytes<=SIZE_MAX-sizeof(xmqttjournal)-XMQTT_J_PAGE-1024&&
        C->MaxFileBytes>=XMQTT_J_START&&C->MaxFileBytes<=INT64_MAX;
}
XMQTT_API bool xmqttJournalConfigInit(xmqttjournalconfig* C)
{
    if(!xmqttJSpan(C,sizeof(*C))){xmqttJError(XMQTT_JOURNAL_ERROR,"invalid journal config");return false;}
    *C=(xmqttjournalconfig){0};C->Size=sizeof(*C);C->Version=XMQTT_VERSION_5;C->PayloadSchema=1;
    C->MaxTransactionBytes=1024u*1024u;C->MaxMemoryBytes=2u*1024u*1024u;C->MaxFileBytes=UINT64_C(1073741824);return true;
}
static void xmqttJWitness(xmqttjournal* J,const xmqttjwitness* W)
{
    unsigned char* P=J->Page;memset(P,0,XMQTT_J_PAGE);memcpy(P,"XMQWCOM",7);
    xmqttJPut(P+8,J->Config.Generation,8);xmqttJPut(P+16,W->Sequence,8);xmqttJPut(P+24,W->End,8);xmqttJPut(P+32,W->Chain,4);xmqttJSeal(P);
}
static bool xmqttJParseWitness(xmqttjournal* J,xmqttjwitness* W)
{
    const unsigned char* P=J->Page;
    if(memcmp(P,"XMQWCOM\0",8)||!xmqttJCheck(P)||xmqttJGet(P+8,8)!=J->Config.Generation||!xmqttJZero(P+36,4092-36))return false;
    W->Sequence=xmqttJGet(P+16,8);W->End=xmqttJGet(P+24,8);W->Chain=(uint32)xmqttJGet(P+32,4);
    return W->End>=XMQTT_J_START&&W->End<=INT64_MAX&&W->Sequence<=(W->End-XMQTT_J_START)/XMQTT_J_OVERHEAD&&
        (W->Sequence||(W->End==XMQTT_J_START&&!W->Chain));
}
static xmqttjournalstatus xmqttJLoad(xmqttjournal* J)
{
    const xmqttjournalconfig* C=&J->Config;unsigned char* P=J->Page;
    if(J->FileBytes<XMQTT_J_START)return xmqttJError(XMQTT_JOURNAL_CORRUPT,"truncated journal descriptor/witnesses");
    if(!xmqttJRead(J,0,P,XMQTT_J_PAGE))return XMQTT_JOURNAL_IO;
    if(memcmp(P,"XMQWAL\0\0",8)||xmqttJGet(P+8,2)!=1||P[10]!=C->Version||P[11]||
        xmqttJGet(P+12,4)!=C->Binding.Size||xmqttJGet(P+16,8)!=C->Generation||xmqttJGet(P+24,8)!=XMQTT_J_START||
        xmqttJGet(P+32,4)!=C->PayloadSchema||!xmqttJZero(P+36,28)||!xmqttJCheck(P)||
        memcmp(P+64,C->Binding.Data,C->Binding.Size)||!xmqttJZero(P+64+C->Binding.Size,4092-64-C->Binding.Size))
        return xmqttJError(XMQTT_JOURNAL_CORRUPT,"journal descriptor/version/binding/generation/schema mismatch");
    xmqttjwitness A={0},B={0};
    if(!xmqttJRead(J,4096,P,XMQTT_J_PAGE))return XMQTT_JOURNAL_IO;
    bool AV=xmqttJParseWitness(J,&A);
    if(!xmqttJRead(J,8192,P,XMQTT_J_PAGE))return XMQTT_JOURNAL_IO;
    bool BV=xmqttJParseWitness(J,&B);
    if(!AV&&!BV)return xmqttJError(XMQTT_JOURNAL_CORRUPT,"both commit witnesses invalid");
    if(AV&&BV){
        if(A.Sequence==B.Sequence){if(A.End!=B.End||A.Chain!=B.Chain)return xmqttJError(XMQTT_JOURNAL_CORRUPT,"conflicting equal-sequence witnesses");}
        else {xmqttjwitness H=A.Sequence>B.Sequence?A:B,L=A.Sequence>B.Sequence?B:A;
            if(H.Sequence-L.Sequence!=1||H.End<L.End||H.End-L.End<XMQTT_J_OVERHEAD)return xmqttJError(XMQTT_JOURNAL_CORRUPT,"nonadjacent commit witnesses");}}
    xmqttjwitness W=!AV?B:!BV?A:A.Sequence>=B.Sequence?A:B;
    if(W.End>J->FileBytes)return xmqttJError(XMQTT_JOURNAL_CORRUPT,"committed data is missing; no older fallback");
    if(J->FileBytes>C->MaxFileBytes)return XMQTT_JOURNAL_LIMIT;
    J->Info.Sequence=W.Sequence;J->Info.CommittedEnd=W.End;J->Info.Chain=W.Chain;J->Cursor=XMQTT_J_START;return XMQTT_JOURNAL_DONE;
}
static bool xmqttJConstructorArgs(const xmqttjournalconfig* C,xmqttjournal** Out)
{
    return xmqttJConfig(C)&&xmqttJSpan(Out,sizeof(*Out))&&xmqttJSeparate(Out,sizeof(*Out),C,sizeof(*C))&&
        xmqttJSeparate(Out,sizeof(*Out),C->Binding.Data,C->Binding.Size);
}
static bool xmqttJFileUsable(xfile File,uint64 Required)
{
    return (xrtFileCapabilities(File)&Required)==Required&&
        !(xrtFileFlags(File)&(XFILE_APPEND|XFILE_ASYNC));
}
static xmqttjournalstatus xmqttJConstruct(xfile File,const xmqttjournalconfig* C,
    const xmqttjournalprefix* Prefix,xmqttjournal** Out,bool Create)
{
    if(!File||!xmqttJConstructorArgs(C,Out)||(Prefix&&
       (!xmqttJSpan(Prefix,sizeof(*Prefix))||!xmqttJSeparate(Out,sizeof(*Out),Prefix,sizeof(*Prefix)))))
        return xmqttJError(XMQTT_JOURNAL_ERROR,"invalid journal file/config/output");
    size_t Bytes=sizeof(xmqttjournal)+XMQTT_J_PAGE+C->Binding.Size+C->MaxTransactionBytes;
    if(Bytes>C->MaxMemoryBytes)return XMQTT_JOURNAL_LIMIT;
    xerror* Saved=NULL;if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_JOURNAL_ERROR;
    uint64 Required=XFILE_CAP_READ_AT|XFILE_CAP_WRITE_AT|XFILE_CAP_STAT|XFILE_CAP_RESIZE|XFILE_CAP_FLUSH;
    if(!xmqttJFileUsable(File,Required)){xmqttJError(XMQTT_JOURNAL_ERROR,"journal requires synchronous absolute read/write/stat/resize/flush");return xmqttJFinish(NULL,Saved,XMQTT_JOURNAL_ERROR);}
    uint64 FileBytes=0;if(!xrtFileSize(File,&FileBytes))return xmqttJFinish(NULL,Saved,XMQTT_JOURNAL_IO);
    if(Create&&FileBytes){xmqttJError(XMQTT_JOURNAL_STATE,"journal creation requires an empty file");return xmqttJFinish(NULL,Saved,XMQTT_JOURNAL_STATE);}
    xmqttjournal* J=xrtCalloc(1,Bytes);if(!J)return xmqttJFinish(NULL,Saved,XMQTT_JOURNAL_ERROR);
    J->File=File;J->Config=*C;J->Info.MemoryBytes=Bytes;J->Info.CommittedEnd=XMQTT_J_START;J->FileBytes=FileBytes;
    J->Page=(unsigned char*)(J+1);J->Buffer=J->Page+XMQTT_J_PAGE;
    unsigned char* Binding=J->Buffer+C->MaxTransactionBytes;memcpy(Binding,C->Binding.Data,C->Binding.Size);J->Config.Binding.Data=Binding;
    xmqttjournalstatus S=XMQTT_JOURNAL_DONE;
    if(Create){unsigned char* P=J->Page;memcpy(P,"XMQWAL",6);xmqttJPut(P+8,1,2);P[10]=(unsigned char)C->Version;
        xmqttJPut(P+12,C->Binding.Size,4);xmqttJPut(P+16,C->Generation,8);xmqttJPut(P+24,XMQTT_J_START,8);xmqttJPut(P+32,C->PayloadSchema,4);memcpy(P+64,Binding,C->Binding.Size);xmqttJSeal(P);
        bool OK=xmqttJWrite(J,0,P,XMQTT_J_PAGE);xmqttjwitness W={0,XMQTT_J_START,0};xmqttJWitness(J,&W);
        if(OK)OK=xmqttJWrite(J,4096,P,XMQTT_J_PAGE)&&xmqttJWrite(J,8192,P,XMQTT_J_PAGE)&&xrtFileResize(File,XMQTT_J_START)&&xrtFlush(File);
        if(!OK)S=XMQTT_JOURNAL_IO;else {J->Info.State=XMQTT_JOURNAL_READY;J->FileBytes=XMQTT_J_START;J->Cursor=XMQTT_J_START;}
    }else{
        S=xmqttJLoad(J);
        if(S==XMQTT_JOURNAL_DONE&&Prefix){
            if(Prefix->End<XMQTT_J_START||(!Prefix->Sequence&&(Prefix->End!=XMQTT_J_START||Prefix->Chain))||
               Prefix->Sequence>J->Info.Sequence||Prefix->End>J->Info.CommittedEnd||
               (Prefix->Sequence==J->Info.Sequence&&
                (Prefix->End!=J->Info.CommittedEnd||Prefix->Chain!=J->Info.Chain)))
                S=xmqttJError(XMQTT_JOURNAL_CORRUPT,"Journal lost the published prefix");
            else J->Prefix=*Prefix;
        }
    }
    if(S!=XMQTT_JOURNAL_DONE){xrtFree(J);return xmqttJFinish(NULL,Saved,S);}
    S=xmqttJFinish(NULL,Saved,S);if(S!=XMQTT_JOURNAL_DONE){xrtFree(J);return S;}
    *Out=J;return S;
}
XMQTT_API xmqttjournalstatus xmqttJournalCreate(xfile F,const xmqttjournalconfig* C,xmqttjournal** J)
{return xmqttJConstruct(F,C,NULL,J,true);}
XMQTT_API xmqttjournalstatus xmqttJournalOpen(xfile F,const xmqttjournalconfig* C,xmqttjournal** J)
{return xmqttJConstruct(F,C,NULL,J,false);}
XMQTT_API xmqttjournalstatus xmqttJournalOpenAtPrefix(xfile F,const xmqttjournalconfig* C,
    const xmqttjournalprefix* Prefix,xmqttjournal** Out)
{
    if(!Prefix)return xmqttJError(XMQTT_JOURNAL_ERROR,"missing expected Journal prefix");
    return xmqttJConstruct(F,C,Prefix,Out,false);
}
#if defined(XMQTT_FEATURE_XMQTT_STORE_CHECKPOINT_ONLINE)
XMQTT_INTERNAL bool xmqttInternalJournalCheckpointCurrent(const xmqttjournal* J,
    const void* Owner,xmqttstorecheckpointresult* Result,uint64* DataBytes)
{
    if(!xmqttJOwner(J,Owner)||J->Busy||J->Info.State!=XMQTT_JOURNAL_READY){
        xmqttJError(XMQTT_JOURNAL_STATE,"checkpoint requires an owned READY Journal");
        return false;
    }
    Result->PreviousGeneration=Result->Generation=J->Config.Generation;
    Result->PreviousBytes=Result->Bytes=J->Info.CommittedEnd;
    Result->Sequence=J->Info.Sequence;
    *DataBytes=J->Info.CommittedEnd-XMQTT_J_START;
    return true;
}

XMQTT_INTERNAL uint64 xmqttInternalJournalCheckpointBytes(uint64 Transactions,uint64 PayloadBytes)
{
    if(PayloadBytes>UINT64_MAX-XMQTT_J_START||
       Transactions>(UINT64_MAX-XMQTT_J_START-PayloadBytes)/XMQTT_J_OVERHEAD)return UINT64_MAX;
    return XMQTT_J_START+PayloadBytes+Transactions*XMQTT_J_OVERHEAD;
}

XMQTT_INTERNAL xmqttjournalstatus xmqttInternalJournalCheckpointStage(xmqttjournal* Old,
    xmqttstorecheckpointprepare Prepare,ptr Context,xmqttjournal** Out)
{
    if(Old->Config.Generation==UINT64_MAX)return XMQTT_JOURNAL_LIMIT;
    Old->Busy=true;
    xmqttjournal* New=NULL;
    bool Fence=false;
    xmqttjournalstatus Status=Prepare(Context,Old->Config.Generation,&Old->Info,&New,&Fence);
    if(Fence)Old->Info.State=XMQTT_JOURNAL_POISONED;
    if(Status==XMQTT_JOURNAL_DONE){
        if(New&&New!=Old&&xmqttJOwner(New,NULL)&&!New->Busy){
            const xmqttjournalconfig* A=&Old->Config,*B=&New->Config;
            if(!Fence&&New->File!=Old->File&&New->Info.State==XMQTT_JOURNAL_READY&&!New->Info.Sequence&&
               B->Generation==A->Generation+1u&&B->Version==A->Version&&B->PayloadSchema==A->PayloadSchema&&
               B->Binding.Size==A->Binding.Size&&!memcmp(B->Binding.Data,A->Binding.Data,A->Binding.Size)){
                *Out=New;
                return XMQTT_JOURNAL_DONE;
            }
            (void)xmqttJournalDestroy(New);
        }
        Status=xmqttJError(XMQTT_JOURNAL_STATE,"checkpoint requires a separate empty matching next Journal");
    }
    Old->Busy=false;
    return Status;
}

XMQTT_INTERNAL xmqttjournalstatus xmqttInternalJournalCheckpointPublish(xmqttjournal* Candidate,
    xmqttstorecheckpointselect Select,ptr Context,xmqttstorecheckpointresult* Result)
{
    xmqttjournaleffect Effect=XMQTT_JOURNAL_NOT_COMMITTED;
    Candidate->Busy=true;
    xmqttjournalstatus Status=Select(Context,Candidate->Config.Generation,&Candidate->Info,&Effect);
    Candidate->Busy=false;
    if(Effect!=XMQTT_JOURNAL_NOT_COMMITTED&&Effect!=XMQTT_JOURNAL_COMMITTED)
        Effect=XMQTT_JOURNAL_MAY_BE_COMMITTED;
    Result->Effect=Effect;
    if(Effect==XMQTT_JOURNAL_MAY_BE_COMMITTED)return XMQTT_JOURNAL_UNCERTAIN;
    if(Effect==XMQTT_JOURNAL_NOT_COMMITTED&&Status==XMQTT_JOURNAL_DONE)
        return xmqttJError(XMQTT_JOURNAL_STATE,"checkpoint selector returned no committed effect");
    return Status;
}

XMQTT_INTERNAL void xmqttInternalJournalCheckpointFinish(xmqttjournal** Current,const void* Owner,xmqttjournal* Candidate,
    xmqttstorecheckpointresult* Result)
{
    xmqttjournal* Old=*Current;
    Old->Busy=false;
    if(Result->Effect==XMQTT_JOURNAL_COMMITTED){
        Candidate->Worker=Owner;
        *Current=Candidate;
        Result->Generation=Candidate->Config.Generation;
        Result->Sequence=Candidate->Info.Sequence;
        Result->Bytes=Candidate->Info.CommittedEnd;
        if(!xmqttInternalJournalWorkerDestroy(Old,Owner))abort();
    }else{
        if(Result->Effect==XMQTT_JOURNAL_MAY_BE_COMMITTED)
            Old->Info.State=XMQTT_JOURNAL_POISONED;
        if(!xmqttJournalDestroy(Candidate))abort();
    }
}
#endif

XMQTT_API xmqttjournalstatus xmqttJournalNext(xmqttjournal* J,xmqttjournalrecord* Out)
{
    if(!J||!xmqttJOutside(J,Out,sizeof(*Out)))return xmqttJError(XMQTT_JOURNAL_ERROR,"invalid replay output");
    if(!xmqttJOwner(J,NULL)||J->Busy||J->Info.State==XMQTT_JOURNAL_POISONED)return xmqttJError(XMQTT_JOURNAL_STATE,"journal unavailable or reentrant");
    xerror* Saved=NULL;if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_JOURNAL_ERROR;
    J->Busy=true;xmqttjournalstatus S=XMQTT_JOURNAL_DONE;xmqttjournalrecord Record={0};
    if(J->Cursor==J->Info.CommittedEnd){
        if(J->ReadSequence!=J->Info.Sequence||J->ReadChain!=J->Info.Chain)S=xmqttJPoison(J,XMQTT_JOURNAL_CORRUPT,"committed final chain/sequence mismatch");
        else if(J->Info.State==XMQTT_JOURNAL_RECOVERING&&J->FileBytes>J->Info.CommittedEnd&&
            (!xrtFileResize(J->File,J->Info.CommittedEnd)||!xrtFlush(J->File)))S=xmqttJPoison(J,XMQTT_JOURNAL_IO,"uncommitted suffix retirement failed");
        else {J->FileBytes=J->Info.CommittedEnd;J->Info.State=XMQTT_JOURNAL_READY;S=XMQTT_JOURNAL_EMPTY;}
        return xmqttJFinish(J,Saved,S);
    }
    unsigned char H[XMQTT_J_HEAD],T[16];uint64 Remaining=J->Info.CommittedEnd-J->Cursor;
    if(Remaining<XMQTT_J_OVERHEAD){S=xmqttJPoison(J,XMQTT_JOURNAL_CORRUPT,"short committed frame");goto End;}
    if(!xmqttJRead(J,J->Cursor,H,sizeof(H))){S=xmqttJPoison(J,XMQTT_JOURNAL_IO,"frame read failed");goto End;}
    uint64 Length=xmqttJGet(H+24,8),Total=xmqttJGet(H+8,8);
    if(memcmp(H,"XMQWTX1\0",8)||Total>Remaining||Total<XMQTT_J_OVERHEAD||Length!=Total-XMQTT_J_OVERHEAD||
        J->ReadSequence==UINT64_MAX||xmqttJGet(H+16,8)!=J->ReadSequence+1||xmqttJGet(H+32,4)!=J->ReadChain||
        xmqttJGet(H+40,4)||xmqttJGet(H+44,4)!=xrtCrc32(H,44)){
        S=xmqttJPoison(J,XMQTT_JOURNAL_CORRUPT,"invalid committed frame header");goto End;}
    if(Length>J->Config.MaxTransactionBytes){S=XMQTT_JOURNAL_LIMIT;goto End;}
    if((Length&&!xmqttJRead(J,J->Cursor+48,J->Buffer,(size_t)Length))||!xmqttJRead(J,J->Cursor+48+Length,T,sizeof(T))){S=xmqttJPoison(J,XMQTT_JOURNAL_IO,"frame payload read failed");goto End;}
    uint32 Chain=xrtCrc32Update(xrtCrc32(H,sizeof(H)),J->Buffer,(size_t)Length);
    if(memcmp(T,"XMQWEND\0",8)||xmqttJGet(T+12,4)||xmqttJGet(T+8,4)!=Chain||xmqttJGet(H+36,4)!=xrtCrc32(J->Buffer,(size_t)Length)){
        S=xmqttJPoison(J,XMQTT_JOURNAL_CORRUPT,"committed payload/footer checksum mismatch");goto End;}
    if(J->ReadSequence+1==J->Prefix.Sequence&&(J->Cursor+Total!=J->Prefix.End||Chain!=J->Prefix.Chain)){
        S=xmqttJPoison(J,XMQTT_JOURNAL_CORRUPT,"Journal disagrees with its published prefix");
        goto End;
    }
    J->Cursor+=Total;J->ReadChain=Chain;++J->ReadSequence;Record=(xmqttjournalrecord){J->ReadSequence,{J->Buffer,(size_t)Length}};
End:S=xmqttJFinish(J,Saved,S);if(S==XMQTT_JOURNAL_DONE)*Out=Record;return S;
}
static xmqttjournalstatus xmqttJAppendOwned(xmqttjournal* J,const void* Owner,xbytesview Data,uint64* Sequence)
{
    if(!J||!xmqttJOutside(J,Data.Data,Data.Size)||!xmqttJOutside(J,Sequence,sizeof(*Sequence))||
        !xmqttJSeparate(Sequence,sizeof(*Sequence),Data.Data,Data.Size))return xmqttJError(XMQTT_JOURNAL_ERROR,"invalid/overlapping append spans");
    if(!xmqttJOwner(J,Owner)||J->Busy||J->Info.State!=XMQTT_JOURNAL_READY)return xmqttJError(XMQTT_JOURNAL_STATE,"journal not recovered/ready or reentrant");
    if(Data.Size>J->Config.MaxTransactionBytes||J->Info.Sequence==UINT64_MAX||
        J->Info.CommittedEnd>J->Config.MaxFileBytes-XMQTT_J_OVERHEAD||Data.Size>J->Config.MaxFileBytes-J->Info.CommittedEnd-XMQTT_J_OVERHEAD)return XMQTT_JOURNAL_LIMIT;
    unsigned char H[XMQTT_J_HEAD]={0},T[16]={0};xmqttjwitness W={J->Info.Sequence+1,J->Info.CommittedEnd+XMQTT_J_OVERHEAD+Data.Size,0};
    memcpy(H,"XMQWTX1",7);xmqttJPut(H+8,XMQTT_J_OVERHEAD+Data.Size,8);xmqttJPut(H+16,W.Sequence,8);xmqttJPut(H+24,Data.Size,8);
    xmqttJPut(H+32,J->Info.Chain,4);xmqttJPut(H+36,xrtCrc32(Data.Data,Data.Size),4);xmqttJPut(H+44,xrtCrc32(H,44),4);
    W.Chain=xrtCrc32Update(xrtCrc32(H,sizeof(H)),Data.Data,Data.Size);memcpy(T,"XMQWEND",7);xmqttJPut(T+8,W.Chain,4);
    xerror* Saved=NULL;if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_JOURNAL_ERROR;
    J->Busy=true;J->Info.Effect=XMQTT_JOURNAL_NOT_COMMITTED;J->Info.AttemptSequence=W.Sequence;J->Info.AttemptEnd=W.End;
    uint64 At=J->Info.CommittedEnd;xmqttjournalstatus S=XMQTT_JOURNAL_DONE;
    if(!xmqttJWrite(J,At,H,sizeof(H))||(Data.Size&&!xmqttJWrite(J,At+48,Data.Data,Data.Size))||!xmqttJWrite(J,W.End-16,T,sizeof(T))||!xrtFlush(J->File)){
        S=xmqttJPoison(J,XMQTT_JOURNAL_IO,"transaction data write/sync failed");goto End;}
    xmqttJWitness(J,&W);J->Info.Effect=XMQTT_JOURNAL_MAY_BE_COMMITTED;
    if(!xmqttJWrite(J,4096,J->Page,XMQTT_J_PAGE)||!xmqttJWrite(J,8192,J->Page,XMQTT_J_PAGE)||!xrtFlush(J->File)){
        S=xmqttJPoison(J,XMQTT_JOURNAL_UNCERTAIN,"commit witness write/sync failed");goto End;}
    J->Info.Sequence=W.Sequence;J->Info.CommittedEnd=W.End;J->Info.Chain=W.Chain;J->Info.Effect=XMQTT_JOURNAL_COMMITTED;J->FileBytes=W.End;
    /* Appended records are already applied by the owner, not replayed again. */
    J->Cursor=W.End;J->ReadSequence=W.Sequence;J->ReadChain=W.Chain;
End:S=xmqttJFinish(J,Saved,S);if(S==XMQTT_JOURNAL_DONE)*Sequence=W.Sequence;return S;
}
static bool xmqttJInfoOwned(xmqttjournal* J,const void* Owner,xmqttjournalinfo* Out)
{
    if(!J||!xmqttJOutside(J,Out,sizeof(*Out))){xmqttJError(XMQTT_JOURNAL_ERROR,"invalid journal info output");return false;}
    if(!xmqttJOwner(J,Owner)||J->Busy){xmqttJError(XMQTT_JOURNAL_STATE,"claimed or reentrant journal info");return false;}*Out=J->Info;return true;
}
static bool xmqttJDestroyOwned(xmqttjournal* J,const void* Owner)
{
    if(!J)return true;
    if(!xmqttJOwner(J,Owner)||J->Busy){xmqttJError(XMQTT_JOURNAL_STATE,"claimed or reentrant journal destroy");return false;}
    xrtFree(J);return true;
}
XMQTT_API xmqttjournalstatus xmqttJournalAppend(xmqttjournal* J,xbytesview Data,uint64* Sequence)
{return xmqttJAppendOwned(J,NULL,Data,Sequence);}
XMQTT_API bool xmqttJournalInfo(xmqttjournal* J,xmqttjournalinfo* I){return xmqttJInfoOwned(J,NULL,I);}
XMQTT_API bool xmqttJournalDestroy(xmqttjournal* J){return xmqttJDestroyOwned(J,NULL);}
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_WORKER)
XMQTT_INTERNAL bool xmqttInternalJournalWorkerClaim(xmqttjournal* J,const void* Owner,void* Output,size_t Bytes,xmqttjournalinfo* Info)
{
    if(!J||!Owner||!xmqttJOutside(J,Output,Bytes)||J->Worker||J->Busy||J->Info.State!=XMQTT_JOURNAL_READY){xmqttJError(XMQTT_JOURNAL_STATE,"worker requires exclusive READY Journal and disjoint output");return false;}
    *Info=J->Info;J->Worker=Owner;return true;
}
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalJournalWorkerAppend(xmqttjournal* J,const void* O,xbytesview D,uint64* S){return xmqttJAppendOwned(J,O,D,S);}
XMQTT_INTERNAL bool xmqttInternalJournalWorkerInfo(xmqttjournal* J,const void* O,xmqttjournalinfo* I){return xmqttJInfoOwned(J,O,I);}
XMQTT_INTERNAL bool xmqttInternalJournalWorkerDestroy(xmqttjournal* J,const void* O){return xmqttJDestroyOwned(J,O);}
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_STATE)
#if defined(XMQTT_FEATURE_XMQTT_STORE_CHECKPOINT)
XMQTT_INTERNAL size_t xmqttJournalCheckpointLimit(xmqttjournal* J,xmqttversion Version,xbytesview Binding)
{
    uint64 Transactions=0;
    if(!J||J->Info.State!=XMQTT_JOURNAL_READY||
       !xmqttJournalReplayStart(J,Version,Binding,J->Config.Generation,&Transactions,sizeof(Transactions),&Transactions)||
       Transactions){
        xmqttJError(XMQTT_JOURNAL_STATE,"checkpoint requires an empty matching READY Journal");
        return 0;
    }
    return J->Config.MaxTransactionBytes;
}
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_CHECKPOINT_ONLINE)
XMQTT_INTERNAL size_t xmqttJournalReplayTransactionLimit(const xmqttjournal* J){return J->Config.MaxTransactionBytes;}
#endif
bool xmqttJournalReplayStart(xmqttjournal* J,xmqttversion Version,xbytesview Binding,
    uint64 Generation,const void* Output,size_t Bytes,uint64* Transactions)
{
    if(!J||!xmqttJOutside(J,Output,Bytes)||!xmqttJOwner(J,NULL)||J->Busy||J->Info.State==XMQTT_JOURNAL_POISONED||
       J->Cursor!=XMQTT_J_START||J->ReadSequence||J->Config.Version!=Version||
       J->Config.PayloadSchema!=UINT32_C(0x584d4401)||J->Config.Generation!=Generation||
       J->Config.Binding.Size!=Binding.Size||memcmp(J->Config.Binding.Data,Binding.Data,Binding.Size)){
        xmqttJError(XMQTT_JOURNAL_STATE,"recovery requires exact typed descriptor and original journal cursor");return false;
    }
    *Transactions=J->Info.Sequence;return true;
}

#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
XMQTT_INTERNAL bool xmqttJournalWriteStart(xmqttjournal* J,xmqttversion Version,xbytesview Binding,uint64 Generation,uint64 Sequence,uint32 Chain,uint64 End,void* Output,size_t Bytes)
{
    if(!J||!xmqttJOutside(J,Output,Bytes)||!xmqttJOwner(J,NULL)||J->Busy||J->Info.State!=XMQTT_JOURNAL_READY||J->Cursor!=J->Info.CommittedEnd||
       J->ReadSequence!=J->Info.Sequence||J->Info.Sequence!=Sequence||J->Info.Chain!=Chain||J->Info.CommittedEnd!=End||J->Config.Version!=Version||
       J->Config.PayloadSchema!=UINT32_C(0x584d4401)||J->Config.Generation!=Generation||J->Config.Binding.Size!=Binding.Size||memcmp(J->Config.Binding.Data,Binding.Data,Binding.Size)){
        xmqttJError(XMQTT_JOURNAL_STATE,"writer State does not match actual complete typed READY Journal");return false;}
    return true;
}
#endif
#endif
#endif
