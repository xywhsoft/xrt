#include <xcron/executor.h>
#if defined(XCRON_FEATURE_XCRON_EXECUTOR_SELECTED)
#include <xcron/executor_selected.h>
#endif
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <xrt/memory.h>
#include <string.h>
#include <stddef.h>
#if defined(XCRON_FEATURE_XCRON_EXECUTOR)
#define XCRON_EXECUTOR_MAGIC UINT32_C(0x58434531)
#define XCRON_EXECUTOR_NONE UINT32_MAX
struct xcronexecution {xatomic32 Requested;};
typedef struct xcronexecutorslot {
    xcroncompletion Done;
    xcronexecutor* Owner;
    xcronexecution Execution;
    uint32 Index,Next;
    bool Used;
} xcronexecutorslot;
struct xcronexecutor {
    uint32 Magic,NotifyDepth;
    uint64 Owner;
    xcronexecutorconfig Config;
    xcronexecutorstats Stats;
    xatomic32 Head,Producers;
    xcronexecutorslot* Slots;
    uint32* Table;
#if defined(XCRON_FEATURE_XCRON_EXECUTOR_SELECTED)
    xcronexecutionlabel* Labels;
#endif
    uint32 Free,Pending,Buckets,Mask;
    _Alignas(xcronexecutorslot) unsigned char Storage[];
};
static bool xcronExecutorMode(const xcronexecutor* B,bool Selected)
{
#if defined(XCRON_FEATURE_XCRON_EXECUTOR_SELECTED)
    return (B->Labels!=NULL)==Selected;
#else
    (void)B;return !Selected;
#endif
}
static bool xcronExecutorSpan(const void* P,size_t N)
{return (!N || P) && N<=UINTPTR_MAX-(uintptr_t)P;}
static bool xcronExecutorOverlap(const void* A,size_t AN,const void* B,size_t BN)
{return AN && BN && (uintptr_t)A<(uintptr_t)B+BN && (uintptr_t)B<(uintptr_t)A+AN;}
static bool xcronExecutorOutputs(const xcronexecutor* B,const void* Input,size_t InputSize,void* const* O,const size_t* N,size_t Count)
{
    if(B && (!xcronExecutorSpan(B,sizeof(*B)) || B->Magic!=XCRON_EXECUTOR_MAGIC || !xcronExecutorSpan(B,B->Stats.Bytes)))return false;
    if(!xcronExecutorSpan(Input,InputSize))return false;
    for(size_t I=0;I<Count;++I){
        if(!xcronExecutorSpan(O[I],N[I]) || (B && xcronExecutorOverlap(B,B->Stats.Bytes,O[I],N[I])) || xcronExecutorOverlap(Input,InputSize,O[I],N[I]))return false;
        for(size_t J=0;J<I;++J)if(xcronExecutorOverlap(O[J],N[J],O[I],N[I]))return false;
    }
    return true;
}
static bool xcronExecutorArgs(const xcronexecutor* B,void* Out,size_t Size,xcronexecutorissue* Issue)
{void* O[]={Out,Issue};size_t N[]={Size,Issue?sizeof(*Issue):0};return B && xcronExecutorOutputs(B,NULL,0,O,N,2);}
static void xcronExecutorOK(xcronexecutorissue* Issue)
{if(Issue)*Issue=(xcronexecutorissue){XCRON_EXECUTOR_OK,XERR_NONE};}
static bool xcronExecutorFail(xcronexecutorerror Code,xerrkind Cause,xcronexecutorissue* Issue)
{
    if(Issue)*Issue=(xcronexecutorissue){Code,Cause};
    cstr Text="invalid cron executor arguments/spans/output aliases";xerrkind Kind=XERR_ARGUMENT;
    switch(Code){
    case XCRON_EXECUTOR_LIMIT:Text="cron executor slot or byte capacity exceeded";Kind=XERR_RANGE;break;
    case XCRON_EXECUTOR_MEMORY:Text="cron executor allocation failed";Kind=XERR_MEMORY;break;
    case XCRON_EXECUTOR_CLOSED:Text="cron executor adapter is closed";Kind=XERR_CLOSED;break;
    case XCRON_EXECUTOR_STALE:Text="cron execution lease is not active";Kind=XERR_NOT_FOUND;break;
    case XCRON_EXECUTOR_BUSY:Text="cron execution receipts or publication tails remain";Kind=XERR_STATE;break;
    case XCRON_EXECUTOR_THREAD:Text="cron executor operation requires its owner thread";Kind=XERR_STATE;break;
    case XCRON_EXECUTOR_REENTRY:Text="cron executor notification cannot reenter its owner";Kind=XERR_STATE;break;
    case XCRON_EXECUTOR_DUPLICATE:Text="cron execution lease is already active";Kind=XERR_EXISTS;break;
    case XCRON_EXECUTOR_REJECTED:Text="borrowed XRT executor rejected the lease";Kind=Cause==XERR_NONE?XERR_AGAIN:Cause;break;
    default:break;
    }
    if(Cause!=XERR_NONE){xerror* Original=xrtTakeError();xerror* Wrapped=xrtErrorWrap(Original,Kind,"xrt.cron.executor",Code,Text);xrtErrorFree(Original);
        if(Wrapped)xrtSetErrorTake(Wrapped);else xrtSetErrorKind(XERR_MEMORY);}
    else xrtSetErrorInfo(Kind,"xrt.cron.executor",Code,Text);
    return false;
}
static bool xcronExecutorOwner(const xcronexecutor* B,xcronexecutorissue* Issue)
{
    if(B->Owner!=xrtThreadCurrentId())return xcronExecutorFail(XCRON_EXECUTOR_THREAD,XERR_NONE,Issue);
    if(B->NotifyDepth)return xcronExecutorFail(XCRON_EXECUTOR_REENTRY,XERR_NONE,Issue);
    return true;
}
static void xcronExecutorIncrement(uint64* N)
{if(*N!=UINT64_MAX)++*N;}
static uint32 xcronExecutorHash(xcronrunid Id,uint32 Mask)
{Id^=Id>>33;Id*=UINT64_C(0xff51afd7ed558ccd);Id^=Id>>33;Id*=UINT64_C(0xc4ceb9fe1a85ec53);Id^=Id>>33;return (uint32)Id&Mask;}
static uint32 xcronExecutorFind(const xcronexecutor* B,xcronrunid Id)
{
    uint32 P=xcronExecutorHash(Id,B->Mask);
    while(B->Table[P]!=XCRON_EXECUTOR_NONE){if(B->Slots[B->Table[P]].Done.Run.Id==Id)return P;P=(P+1)&B->Mask;}
    return P;
}
static void xcronExecutorRemove(xcronexecutor* B,uint32 P)
{
    uint32 Next=(P+1)&B->Mask;
    while(B->Table[Next]!=XCRON_EXECUTOR_NONE){
        uint32 Home=xcronExecutorHash(B->Slots[B->Table[Next]].Done.Run.Id,B->Mask);
        if(((Next-Home)&B->Mask)>=((Next-P)&B->Mask)){B->Table[P]=B->Table[Next];P=Next;}
        Next=(Next+1)&B->Mask;
    }
    B->Table[P]=XCRON_EXECUTOR_NONE;
}
static void xcronExecutorWork(ptr Data)
{
    xcronexecutorslot* S=Data;xcronexecutor* B=S->Owner;
    if(xrtThreadCurrentId()==B->Owner){S->Done.Result=XCRON_RUN_FAILED;S->Done.Code=XCRON_EXECUTOR_OWNER_CONFLICT;return;}
    if(xcronExecutionCancelled(&S->Execution))return;
    xcronrunresult Result=B->Config.Proc(&S->Done.Run,&S->Execution,B->Config.Context);
    if(Result<XCRON_RUN_SUCCEEDED || Result>XCRON_RUN_CANCELLED){Result=XCRON_RUN_FAILED;S->Done.Code=XCRON_EXECUTOR_RESULT;}
    S->Done.Result=Result;
    if(Result==XCRON_RUN_FAILED)S->Done.Error=xrtTakeError();
}
static void xcronExecutorDrop(ptr Data,ptr Context)
{
    (void)Context;xcronexecutorslot* S=Data;xcronexecutor* B=S->Owner;uint32 Index=S->Index;
    uint32 Previous=xrtAtomic32Load(&B->Head,XMEMORY_RELAXED);
    do{S->Next=Previous;}while(!xrtAtomic32CompareExchange(&B->Head,&Previous,Index,XMEMORY_RELEASE,XMEMORY_RELAXED));
    /* No slot/data/error access after publication: owner may drain/reuse it. */
    if(B->Config.Notify){bool Owner=xrtThreadCurrentId()==B->Owner;if(Owner)++B->NotifyDepth;
        B->Config.Notify(B->Config.NotifyContext);if(Owner)--B->NotifyDepth;}
    /* Last adapter access: Destroy also waits for these notification tails. */
    xrtAtomic32FetchSub(&B->Producers,1,XMEMORY_RELEASE);
}
static bool xcronExecutorCreateMode(const xcronexecutorconfig* Config,xcronexecutor** Out,xcronexecutorissue* Issue,bool Selected)
{
    void* O[]={Out,Issue};size_t N[]={sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!Config || !Out || !xcronExecutorOutputs(NULL,Config,sizeof(*Config),O,N,2) || !Config->Executor || !Config->Proc || !Config->RunLimit || Config->RunLimit>UINT32_C(1073741824) || Config->Reserved)
        return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    uint32 Buckets=2;while(Buckets/2<Config->RunLimit)Buckets*=2;
    size_t Bytes=sizeof(xcronexecutor),Limit=Config->RunLimit,Count=Buckets;
    if(Limit>(SIZE_MAX-Bytes)/sizeof(xcronexecutorslot))return xcronExecutorFail(XCRON_EXECUTOR_LIMIT,XERR_NONE,Issue);
    size_t SlotBytes=Limit*sizeof(xcronexecutorslot);Bytes+=SlotBytes;
    size_t LabelBytes=0;
#if defined(XCRON_FEATURE_XCRON_EXECUTOR_SELECTED)
    typedef char xcron_executor_label_alignment_check[(_Alignof(xcronexecutionlabel)<=_Alignof(xcronexecutorslot) && sizeof(xcronexecutorslot)%_Alignof(xcronexecutionlabel)==0)?1:-1];
    (void)sizeof(xcron_executor_label_alignment_check);
    if(Selected){if(Limit>(SIZE_MAX-Bytes)/sizeof(xcronexecutionlabel))return xcronExecutorFail(XCRON_EXECUTOR_LIMIT,XERR_NONE,Issue);LabelBytes=Limit*sizeof(xcronexecutionlabel);Bytes+=LabelBytes;}
#else
    (void)Selected;
#endif
    if(Count>(SIZE_MAX-Bytes)/sizeof(uint32) || (Bytes+=Count*sizeof(uint32))>Config->BytesLimit)return xcronExecutorFail(XCRON_EXECUTOR_LIMIT,XERR_NONE,Issue);
    xcronexecutor* B=xrtMalloc(Bytes);if(!B)return xcronExecutorFail(XCRON_EXECUTOR_MEMORY,XERR_NONE,Issue);
    memset(B,0,Bytes);B->Magic=XCRON_EXECUTOR_MAGIC;B->Owner=xrtThreadCurrentId();B->Config=*Config;B->Stats.Bytes=Bytes;B->Stats.Limit=Config->RunLimit;
    B->Slots=(xcronexecutorslot*)B->Storage;B->Table=(uint32*)(B->Storage+SlotBytes+LabelBytes);B->Buckets=Buckets;B->Mask=Buckets-1;B->Free=0;B->Pending=XCRON_EXECUTOR_NONE;
#if defined(XCRON_FEATURE_XCRON_EXECUTOR_SELECTED)
    if(Selected)B->Labels=(xcronexecutionlabel*)(B->Storage+SlotBytes);
#endif
    xrtAtomic32Init(&B->Head,XCRON_EXECUTOR_NONE);xrtAtomic32Init(&B->Producers,0);
    for(uint32 I=0;I<Buckets;++I)B->Table[I]=XCRON_EXECUTOR_NONE;
    for(uint32 I=0;I<Config->RunLimit;++I){B->Slots[I].Owner=B;B->Slots[I].Index=I;B->Slots[I].Next=I+1<Config->RunLimit?I+1:XCRON_EXECUTOR_NONE;xrtAtomic32Init(&B->Slots[I].Execution.Requested,0);}
    *Out=B;xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorCreate(const xcronexecutorconfig* Config,xcronexecutor** Out,xcronexecutorissue* Issue)
{return xcronExecutorCreateMode(Config,Out,Issue,false);}
static bool xcronExecutorRun(const xcronrun* R)
{return R->Id && R->Job && !(R->Flags&~XCRON_RUN_COALESCED) && R->Through>=R->ScheduledAt;}
static bool xcronExecutorSubmitValue(xcronexecutor* B,const xcronrun* Run,const xdatetime* Local,size_t Candidates,xcronexecutorissue* Issue)
{
    if(!xcronExecutorOwner(B,Issue))return false;
    if(B->Stats.Closed){xcronExecutorIncrement(&B->Stats.Rejected);return xcronExecutorFail(XCRON_EXECUTOR_CLOSED,XERR_NONE,Issue);}
    uint32 P=xcronExecutorFind(B,Run->Id);
    if(B->Table[P]!=XCRON_EXECUTOR_NONE){xcronExecutorIncrement(&B->Stats.Rejected);return xcronExecutorFail(XCRON_EXECUTOR_DUPLICATE,XERR_NONE,Issue);}
    if(B->Free==XCRON_EXECUTOR_NONE){xcronExecutorIncrement(&B->Stats.Rejected);return xcronExecutorFail(XCRON_EXECUTOR_LIMIT,XERR_NONE,Issue);}
    uint32 I=B->Free;xcronexecutorslot* S=&B->Slots[I];B->Free=S->Next;S->Used=true;S->Done=(xcroncompletion){.Run=*Run,.Result=XCRON_RUN_CANCELLED};xrtAtomic32Store(&S->Execution.Requested,0,XMEMORY_RELAXED);B->Table[P]=I;++B->Stats.Active;
#if defined(XCRON_FEATURE_XCRON_EXECUTOR_SELECTED)
    if(B->Labels){B->Labels[I].Local=*Local;B->Labels[I].Candidates=Candidates;}
#else
    (void)Local;(void)Candidates;
#endif
    xrtAtomic32FetchAdd(&B->Producers,1,XMEMORY_RELAXED);
    if(!xrtExecutorSubmit(B->Config.Executor,xcronExecutorWork,S,xcronExecutorDrop,NULL)){
        xerrkind Cause=xrtErrorKind(xrtGetError());xrtAtomic32FetchSub(&B->Producers,1,XMEMORY_RELAXED);--B->Stats.Active;xcronExecutorRemove(B,P);S->Used=false;S->Next=B->Free;B->Free=I;xcronExecutorIncrement(&B->Stats.Rejected);
        return xcronExecutorFail(XCRON_EXECUTOR_REJECTED,Cause,Issue);
    }
    xcronExecutorIncrement(&B->Stats.Submitted);if(B->Stats.Active>B->Stats.Peak)B->Stats.Peak=B->Stats.Active;xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorSubmit(xcronexecutor* B,const xcronrun* Run,xcronexecutorissue* Issue)
{
    void* O[]={Issue};size_t N[]={Issue?sizeof(*Issue):0};
    if(!B || !Run || !xcronExecutorOutputs(B,Run,sizeof(*Run),O,N,1) || !xcronExecutorMode(B,false) || !xcronExecutorRun(Run))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    return xcronExecutorSubmitValue(B,Run,NULL,0,Issue);
}
static bool xcronExecutorRequest(xcronexecutor* B,xcronexecutorslot* S)
{if(xrtAtomic32Exchange(&S->Execution.Requested,1,XMEMORY_ACQ_REL))return false;xcronExecutorIncrement(&B->Stats.CancelRequests);return true;}
XCRON_API bool xcronExecutorCancel(xcronexecutor* B,xcronrunid Id,xcronexecutorissue* Issue)
{
    if(!xcronExecutorArgs(B,NULL,0,Issue))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    uint32 P=xcronExecutorFind(B,Id);if(!Id || B->Table[P]==XCRON_EXECUTOR_NONE)return xcronExecutorFail(XCRON_EXECUTOR_STALE,XERR_NONE,Issue);
    xcronExecutorRequest(B,&B->Slots[B->Table[P]]);xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorCancelJob(xcronexecutor* B,xcronjobid Id,uint32* Requested,xcronexecutorissue* Issue)
{
    if(!Id || !Requested || !xcronExecutorArgs(B,Requested,sizeof(*Requested),Issue))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    uint32 Count=0;for(uint32 I=0;I<B->Config.RunLimit;++I)if(B->Slots[I].Used && B->Slots[I].Done.Run.Job==Id && xcronExecutorRequest(B,&B->Slots[I]))++Count;
    *Requested=Count;xcronExecutorOK(Issue);return true;
}
static void xcronExecutorRelease(xcronexecutor* B,uint32 I)
{
    xcronexecutorslot* S=&B->Slots[I];S->Done.Error=NULL;xcronExecutorRemove(B,xcronExecutorFind(B,S->Done.Run.Id));S->Used=false;S->Next=B->Free;B->Free=I;--B->Stats.Active;xcronExecutorIncrement(&B->Stats.Drained);
}
XCRON_API bool xcronExecutorDrain(xcronexecutor* B,xcroncompletion* Completed,size_t Capacity,size_t* Count,xcronexecutorissue* Issue)
{
    void* O[]={Completed,Count,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Completed)?Capacity*sizeof(*Completed):SIZE_MAX,sizeof(*Count),Issue?sizeof(*Issue):0};
    if(!B || !Completed || !Count || !Capacity || Capacity>SIZE_MAX/sizeof(*Completed) || !xcronExecutorOutputs(B,NULL,0,O,N,3) || !xcronExecutorMode(B,false))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    if(B->Pending==XCRON_EXECUTOR_NONE)B->Pending=xrtAtomic32Exchange(&B->Head,XCRON_EXECUTOR_NONE,XMEMORY_ACQUIRE);
    size_t Written=0;
    while(Written<Capacity && B->Pending!=XCRON_EXECUTOR_NONE){
        uint32 I=B->Pending;xcronexecutorslot* S=&B->Slots[I];B->Pending=S->Next;Completed[Written++]=S->Done;xcronExecutorRelease(B,I);
    }
    *Count=Written;xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorClose(xcronexecutor* B,bool CancelRuns,xcronexecutorissue* Issue)
{
    if(!xcronExecutorArgs(B,NULL,0,Issue))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    B->Stats.Closed=true;if(CancelRuns)for(uint32 I=0;I<B->Config.RunLimit;++I)if(B->Slots[I].Used)xcronExecutorRequest(B,&B->Slots[I]);
    xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorInspect(const xcronexecutor* B,xcronexecutorstats* Stats,xcronexecutorissue* Issue)
{
    if(!Stats || !xcronExecutorArgs(B,Stats,sizeof(*Stats),Issue))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    *Stats=B->Stats;Stats->Producers=xrtAtomic32Load(&B->Producers,XMEMORY_ACQUIRE);xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorDestroy(xcronexecutor* B,xcronexecutorissue* Issue)
{
    if(!xcronExecutorArgs(B,NULL,0,Issue))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    if(B->Stats.Active || xrtAtomic32Load(&B->Producers,XMEMORY_ACQUIRE))return xcronExecutorFail(XCRON_EXECUTOR_BUSY,XERR_NONE,Issue);
    B->Magic=0;xrtFree(B);xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutionCancelled(const xcronexecution* Execution)
{return Execution && xrtAtomic32Load(&Execution->Requested,XMEMORY_ACQUIRE)!=0;}
#if defined(XCRON_FEATURE_XCRON_EXECUTOR_SELECTED)
XCRON_API bool xcronExecutorCreateSelected(const xcronexecutorconfig* Config,xcronexecutor** Out,xcronexecutorissue* Issue)
{return xcronExecutorCreateMode(Config,Out,Issue,true);}
XCRON_API bool xcronExecutorSubmitSelected(xcronexecutor* B,const xcronselectedrun* Run,xcronexecutorissue* Issue)
{
    void* O[]={Issue};size_t N[]={Issue?sizeof(*Issue):0};
    if(!B || !Run || !xcronExecutorOutputs(B,Run,sizeof(*Run),O,N,1) || !xcronExecutorMode(B,true) || !xcronExecutorRun(&Run->Run) || !Run->Candidates || Run->Run.ScheduledAt%1000 || Run->Local.Offset || Run->Local.IsDST!=-1 || Run->Local.Millisecond)
        return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    return xcronExecutorSubmitValue(B,&Run->Run,&Run->Local,Run->Candidates,Issue);
}
XCRON_API bool xcronExecutorDrainSelected(xcronexecutor* B,xcronselectedcompletion* Completed,size_t Capacity,size_t* Count,xcronexecutorissue* Issue)
{
    void* O[]={Completed,Count,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Completed)?Capacity*sizeof(*Completed):SIZE_MAX,sizeof(*Count),Issue?sizeof(*Issue):0};
    if(!B || !Completed || !Count || !Capacity || Capacity>SIZE_MAX/sizeof(*Completed) || !xcronExecutorOutputs(B,NULL,0,O,N,3) || !xcronExecutorMode(B,true))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    if(B->Pending==XCRON_EXECUTOR_NONE)B->Pending=xrtAtomic32Exchange(&B->Head,XCRON_EXECUTOR_NONE,XMEMORY_ACQUIRE);
    size_t Written=0;
    while(Written<Capacity && B->Pending!=XCRON_EXECUTOR_NONE){
        uint32 I=B->Pending;xcronexecutorslot* S=&B->Slots[I];B->Pending=S->Next;xcronselectedcompletion* D=&Completed[Written++];
        D->Run.Run=S->Done.Run;D->Run.Local=B->Labels[I].Local;D->Run.Candidates=B->Labels[I].Candidates;D->Result=S->Done.Result;D->Code=S->Done.Code;D->Error=S->Done.Error;xcronExecutorRelease(B,I);
    }
    *Count=Written;xcronExecutorOK(Issue);return true;
}
XCRON_API const xcronexecutionlabel* xcronExecutionSelected(const xcronexecution* Execution)
{
    if(!Execution)return NULL;
    const xcronexecutorslot* S=(const xcronexecutorslot*)((const unsigned char*)Execution - offsetof(xcronexecutorslot,Execution));
    return S->Owner->Labels?&S->Owner->Labels[S->Index]:NULL;
}
#endif
#endif
