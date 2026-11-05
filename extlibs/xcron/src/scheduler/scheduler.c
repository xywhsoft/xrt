#include "scheduler_internal.h"
#include "../calendar/calendar_internal.h"
#include "../timezone/timezone_internal.h"
#include "../zoned/zoned_internal.h"
#include <xrt/memory.h>
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
#define XCRON_SCHEDULER_MAGIC UINT32_C(0x58435331)
#define XCRON_SLOT_NONE UINT32_MAX
#if defined(_MSC_VER)
#define XCRON_SCHEDULER_INLINE static __forceinline
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
#define XCRON_SCHEDULER_INLINE static inline __attribute__((always_inline))
#else
#define XCRON_SCHEDULER_INLINE static inline
#endif
typedef enum xcronlocation {XCRON_LOC_FREE=0,XCRON_LOC_HEAP,XCRON_LOC_READY,XCRON_LOC_BLOCKED,XCRON_LOC_RETIRED,XCRON_LOC_RUNNING,XCRON_LOC_SEARCH} xcronlocation;
typedef struct xcronjobslot {
    xcronjobconfig Config;
    xcronjobinfo Info;
    uint64 Generation;
    uint32 Next,Previous,Position;
    xcronlocation Location;
    bool Used,Initial;
} xcronjobslot;
typedef struct xcronrunslot {uint64 Generation;uint32 Job,Next;bool Used;} xcronrunslot;
struct xcronscheduler {
    uint32 Magic;
    xcronschedulerconfig Config;
    xcronschedulerstats Stats;
    xcronjobslot* Jobs;
    xcronrunslot* Runs;
    xcronzonedvalue* Searches;
    uint32* Heap;
    uint32 FreeJob,FreeRun,ReadyHead,ReadyTail,RetireHead,RetireTail;
    uint32 SearchHead,SearchTail,Searching;
    uint8 Turn;
    bool Promote;
    uint8 JobBits,RunBits;
    _Alignas(xcronjobslot) unsigned char Storage[];
};
static bool xcronSchedulerSpan(const void* P,size_t N)
{return (!N || P) && N<=UINTPTR_MAX-(uintptr_t)P;}
static bool xcronSchedulerOverlap(const void* A,size_t AN,const void* B,size_t BN)
{return AN && BN && (uintptr_t)A<(uintptr_t)B+BN && (uintptr_t)B<(uintptr_t)A+AN;}
static bool xcronSchedulerOutputs(const xcronscheduler* S,const void* Input,size_t InputSize,void* const* Out,const size_t* Sizes,size_t Count)
{
    if(S && (!xcronSchedulerSpan(S,sizeof(*S)) || S->Magic!=XCRON_SCHEDULER_MAGIC || !xcronSchedulerSpan(S,S->Stats.Bytes)))return false;
    if(!xcronSchedulerSpan(Input,InputSize))return false;
    for(size_t I=0;I<Count;++I){
        if(!xcronSchedulerSpan(Out[I],Sizes[I]) || (S && xcronSchedulerOverlap(S,S->Stats.Bytes,Out[I],Sizes[I])) || xcronSchedulerOverlap(Input,InputSize,Out[I],Sizes[I]))return false;
        for(size_t J=0;J<I;++J)if(xcronSchedulerOverlap(Out[J],Sizes[J],Out[I],Sizes[I]))return false;
    }
    return true;
}
static bool xcronSchedulerArgs(const xcronscheduler* S,void* Output,size_t Size,xcronschedulerissue* Issue)
{void* O[]={Output,Issue};size_t N[]={Size,Issue?sizeof(*Issue):0};return S && xcronSchedulerOutputs(S,NULL,0,O,N,2);}
static void xcronSchedulerOK(xcronschedulerissue* Issue)
{if(Issue){memset(Issue,0,sizeof(*Issue));Issue->Zoned.Field=XCRON_FIELD_NONE;}}
static bool xcronSchedulerFail(xcronschedulererror Code,const xcronzonedissue* Zoned,xcronschedulerissue* Issue)
{
    xcronSchedulerOK(Issue);if(Issue){Issue->Code=Code;if(Zoned)Issue->Zoned=*Zoned;}
    cstr Text="invalid cron scheduler arguments/spans/output aliases";xerrkind Kind=XERR_ARGUMENT;
    switch(Code){
    case XCRON_SCHEDULER_LIMIT:Text="cron scheduler capacity or byte limit exceeded";Kind=XERR_RANGE;break;
    case XCRON_SCHEDULER_MEMORY:Text="cron scheduler allocation failed";Kind=XERR_MEMORY;break;
    case XCRON_SCHEDULER_CLOSED:Text="cron scheduler is closed";Kind=XERR_STATE;break;
    case XCRON_SCHEDULER_STALE:Text="cron job or run ID is stale";Kind=XERR_STATE;break;
    case XCRON_SCHEDULER_BUSY:Text="cron scheduler has unreaped jobs or live leases";Kind=XERR_STATE;break;
    case XCRON_SCHEDULER_CLOCK:Text="cron monotonic clock sample moved backwards";Kind=XERR_STATE;break;
    case XCRON_SCHEDULER_CALCULATION:Text="cron initial occurrence calculation failed";Kind=XERR_VALUE;break;
    default:break;
    }
    xrtSetErrorInfo(Kind,"xrt.cron.scheduler",Code,Text);return false;
}
static void xcronSchedulerIncrement(uint64* Value)
{if(*Value!=UINT64_MAX)++*Value;}
static bool xcronSchedulerArrayBytes(size_t Count,size_t Element,size_t* Total)
{if(Count>(SIZE_MAX-*Total)/Element)return false;*Total+=Count*Element;return true;}
static uint8 xcronSchedulerIndexBits(uint32 Limit)
{uint8 Bits=0;for(uint32 N=Limit-1;N;N>>=1)++Bits;return Bits;}
static uint64 xcronSchedulerId(uint64 Generation,uint32 Index,uint8 Bits)
{return Generation<<Bits | Index;}
static bool xcronSchedulerJobIndex(const xcronscheduler* S,xcronjobid Id,uint32* Index)
{
    uint32 I=(uint32)(Id&((UINT64_C(1)<<S->JobBits)-1));if(!Id || I>=S->Config.JobLimit)return false;
    const xcronjobslot* J=&S->Jobs[I];
    if(!J->Used || J->Generation!=(Id>>S->JobBits))return false;
    *Index=I;return true;
}
static bool xcronSchedulerRunIndex(const xcronscheduler* S,xcronrunid Id,uint32* Index)
{
    uint32 I=(uint32)(Id&((UINT64_C(1)<<S->RunBits)-1));if(!Id || I>=S->Config.RunLimit)return false;
    const xcronrunslot* R=&S->Runs[I];
    if(!R->Used || R->Generation!=(Id>>S->RunBits))return false;
    *Index=I;return true;
}
static bool xcronSchedulerEarlier(const xcronscheduler* S,uint32 A,uint32 B)
{
    const xcronjobslot* X=&S->Jobs[A];const xcronjobslot* Y=&S->Jobs[B];
    return X->Info.Next<Y->Info.Next || (X->Info.Next==Y->Info.Next && xcronSchedulerId(X->Generation,A,S->JobBits)<xcronSchedulerId(Y->Generation,B,S->JobBits));
}
static void xcronSchedulerHeapUp(xcronscheduler* S,uint32 P)
{
    uint32 I=S->Heap[P];while(P){uint32 Parent=(P-1)/2;if(!xcronSchedulerEarlier(S,I,S->Heap[Parent]))break;
        S->Heap[P]=S->Heap[Parent];S->Jobs[S->Heap[P]].Position=P;P=Parent;}
    S->Heap[P]=I;S->Jobs[I].Position=P;
}
static void xcronSchedulerHeapDown(xcronscheduler* S,uint32 P)
{
    uint32 I=S->Heap[P],Count=S->Stats.Heap;
    while(P<Count/2){uint32 C=P*2+1;if(C+1<Count && xcronSchedulerEarlier(S,S->Heap[C+1],S->Heap[C]))++C;
        if(!xcronSchedulerEarlier(S,S->Heap[C],I))break;
        S->Heap[P]=S->Heap[C];S->Jobs[S->Heap[P]].Position=P;P=C;}
    S->Heap[P]=I;S->Jobs[I].Position=P;
}
static void xcronSchedulerHeapAdd(xcronscheduler* S,uint32 I)
{S->Jobs[I].Location=XCRON_LOC_HEAP;uint32 P=S->Stats.Heap++;S->Heap[P]=I;xcronSchedulerHeapUp(S,P);}
static void xcronSchedulerHeapRemove(xcronscheduler* S,uint32 I)
{
    uint32 P=S->Jobs[I].Position,Last=--S->Stats.Heap;
    if(P!=Last){S->Heap[P]=S->Heap[Last];S->Jobs[S->Heap[P]].Position=P;
        if(P && xcronSchedulerEarlier(S,S->Heap[P],S->Heap[(P-1)/2]))xcronSchedulerHeapUp(S,P);else xcronSchedulerHeapDown(S,P);}
    S->Jobs[I].Location=XCRON_LOC_RUNNING;
}
static void xcronSchedulerQueueAdd(xcronscheduler* S,uint32 I,bool Retired)
{
    uint32* Head=Retired?&S->RetireHead:&S->ReadyHead;uint32* Tail=Retired?&S->RetireTail:&S->ReadyTail;
    xcronjobslot* J=&S->Jobs[I];J->Previous=*Tail;J->Next=XCRON_SLOT_NONE;
    if(*Tail!=XCRON_SLOT_NONE)S->Jobs[*Tail].Next=I;else *Head=I;
    *Tail=I;J->Location=Retired?XCRON_LOC_RETIRED:XCRON_LOC_READY;
    if(Retired)++S->Stats.Retired;else ++S->Stats.Ready;
}
static void xcronSchedulerQueueRemove(xcronscheduler* S,uint32 I,bool Retired)
{
    uint32* Head=Retired?&S->RetireHead:&S->ReadyHead;uint32* Tail=Retired?&S->RetireTail:&S->ReadyTail;
    xcronjobslot* J=&S->Jobs[I];
    if(J->Previous!=XCRON_SLOT_NONE)S->Jobs[J->Previous].Next=J->Next;else *Head=J->Next;
    if(J->Next!=XCRON_SLOT_NONE)S->Jobs[J->Next].Previous=J->Previous;else *Tail=J->Previous;
    if(Retired)--S->Stats.Retired;else --S->Stats.Ready;J->Location=XCRON_LOC_RUNNING;
}
static bool xcronSchedulerLive(xcronjobstate State)
{return State==XCRON_JOB_SCHEDULED || State==XCRON_JOB_SEARCHING;}
static void xcronSchedulerSearchAdd(xcronscheduler* S,uint32 I)
{
    xcronjobslot* J=&S->Jobs[I];J->Previous=S->SearchTail;J->Next=XCRON_SLOT_NONE;
    if(S->SearchTail!=XCRON_SLOT_NONE)S->Jobs[S->SearchTail].Next=I;else S->SearchHead=I;
    S->SearchTail=I;J->Location=XCRON_LOC_SEARCH;++S->Searching;
}
static void xcronSchedulerSearchRemove(xcronscheduler* S,uint32 I)
{
    xcronjobslot* J=&S->Jobs[I];
    if(J->Previous!=XCRON_SLOT_NONE)S->Jobs[J->Previous].Next=J->Next;else S->SearchHead=J->Next;
    if(J->Next!=XCRON_SLOT_NONE)S->Jobs[J->Next].Previous=J->Previous;else S->SearchTail=J->Previous;
    --S->Searching;J->Location=XCRON_LOC_RUNNING;
}
static void xcronSchedulerDetach(xcronscheduler* S,uint32 I)
{
    switch(S->Jobs[I].Location){
    case XCRON_LOC_HEAP:xcronSchedulerHeapRemove(S,I);break;
    case XCRON_LOC_READY:xcronSchedulerQueueRemove(S,I,false);break;
    case XCRON_LOC_SEARCH:xcronSchedulerSearchRemove(S,I);break;
    case XCRON_LOC_BLOCKED:--S->Stats.Blocked;S->Jobs[I].Location=XCRON_LOC_RUNNING;break;
    default:break;
    }
}
static void xcronSchedulerPlace(xcronscheduler* S,uint32 I)
{
    xcronjobslot* J=&S->Jobs[I];
    if(J->Info.State==XCRON_JOB_SEARCHING){xcronSchedulerSearchAdd(S,I);return;}
    if(J->Initial){J->Location=XCRON_LOC_RUNNING;return;}
    if(J->Info.State!=XCRON_JOB_SCHEDULED){if(!J->Info.Running)xcronSchedulerQueueAdd(S,I,true);return;}
    if(J->Config.Overlap!=XCRON_OVERLAP_SKIP && J->Info.Running>=J->Config.RunningLimit){J->Location=XCRON_LOC_BLOCKED;++S->Stats.Blocked;}
    else if(S->Stats.HasClock && J->Info.Next<=S->Stats.LastClock.Wall)xcronSchedulerQueueAdd(S,I,false);
    else xcronSchedulerHeapAdd(S,I);
}
static void xcronSchedulerSearch(xcronscheduler* S,uint32 I,xtime Begin,uint32 Flags,xcronpoll* Poll)
{
    xcronjobslot* J=&S->Jobs[I];xcronzonedissue ZI={.Field=XCRON_FIELD_NONE};xtime Next=0;
    xcronzonedstatus Result=Begin>J->Config.End?XCRON_ZONED_NONE:
        xcronZonedNextValue(&J->Config.Expression,J->Config.Zone,Begin,J->Config.End,Flags,&Next,&ZI);
    J->Info.LastIssue=ZI;J->Info.HasNext=Result==XCRON_ZONED_FOUND;
    if(Result==XCRON_ZONED_FOUND)J->Info.Next=Next;
    else if(Result==XCRON_ZONED_NONE)J->Info.State=XCRON_JOB_EXHAUSTED;
    else {J->Info.State=XCRON_JOB_FAULTED;xcronSchedulerIncrement(&S->Stats.Faults);if(Poll)++Poll->Faults;}
}
static void xcronSchedulerSearchStep(xcronscheduler* S,uint32 I,xcronzonedbudget* Remaining,xcronzonedbudget* Used,xcronpoll* Poll,bool Full)
{
    xcronjobslot* J=&S->Jobs[I];xcronzonedvalue* V=&S->Searches[I];xcronzonedcursorissue Before=V->Work;
    xcronzonedbudget B=Full?(xcronzonedbudget){UINT32_MAX,UINT32_MAX,UINT32_MAX}:
        (xcronzonedbudget){Remaining->Segments?1u:0u,Remaining->Months?1u:0u,Remaining->RuleYears?1u:0u};
    xcronzonedstatus Status=xcronZonedValueStep(V,J->Config.Zone,B);
    if(!Full){uint32 Segments=V->Work.SegmentsExamined-Before.SegmentsExamined;
        uint32 Months=(uint32)(V->Work.MonthsExamined-Before.MonthsExamined),Years=(uint32)(V->Work.RuleYearsExamined-Before.RuleYearsExamined);
        Remaining->Segments-=Segments;Remaining->Months-=Months;Remaining->RuleYears-=Years;
        Used->Segments+=Segments;Used->Months+=Months;Used->RuleYears+=Years;
    }
    J->Info.LastIssue=(xcronzonedissue){V->Work.Code,V->Work.Field,V->Work.Zone,V->Work.SegmentsExamined,V->Work.MonthsExamined};
    J->Info.HasNext=Status==XCRON_ZONED_FOUND;
    if(Status==XCRON_ZONED_PENDING)J->Info.State=XCRON_JOB_SEARCHING;
    else if(Status==XCRON_ZONED_FOUND){J->Info.State=XCRON_JOB_SCHEDULED;J->Info.Next=V->Result;}
    else if(Status==XCRON_ZONED_NONE)J->Info.State=XCRON_JOB_EXHAUSTED;
    else {J->Info.State=XCRON_JOB_FAULTED;if(!J->Initial){xcronSchedulerIncrement(&S->Stats.Faults);if(Poll)++Poll->Faults;}}
}
XCRON_SCHEDULER_INLINE void xcronSchedulerSeek(xcronscheduler* S,uint32 I,xtime Begin,uint32 Flags,xcronpoll* Poll,xcronzonedbudget* Remaining,xcronzonedbudget* Used)
{
    if(!Remaining){xcronSchedulerSearch(S,I,Begin,Flags,Poll);return;}
    xcronjobslot* J=&S->Jobs[I];J->Info.HasNext=false;J->Info.LastIssue=(xcronzonedissue){.Field=XCRON_FIELD_NONE};
    if(Begin>J->Config.End){J->Info.State=XCRON_JOB_EXHAUSTED;return;}
    /* Ready probes cannot spend the last segment/month credit ahead of an
     * already queued query. With Capacity=1 that otherwise repeats forever
     * across fresh polls, despite individually correct global counters. */
    if(Remaining->Segments && Remaining->Months && (!S->Searching || (Remaining->Segments>1 && Remaining->Months>1))){
        xtime First;xdatetime Local;int32 Offset;xcronzonedcursorissue Work;
        xcronzonedstatus Status=xcronZonedReadyValue(&J->Config.Expression,J->Config.Zone,Begin,J->Config.End,Flags,&First,&Local,&Offset,&Work);
        Remaining->Segments-=Work.SegmentsExamined;Remaining->Months-=(uint32)Work.MonthsExamined;
        Used->Segments+=Work.SegmentsExamined;Used->Months+=(uint32)Work.MonthsExamined;
        J->Info.LastIssue=(xcronzonedissue){Work.Code,Work.Field,Work.Zone,Work.SegmentsExamined,Work.MonthsExamined};
        if(Status==XCRON_ZONED_FOUND){J->Info.State=XCRON_JOB_SCHEDULED;J->Info.HasNext=true;J->Info.Next=First;return;}
        if(Status==XCRON_ZONED_NONE){J->Info.State=XCRON_JOB_EXHAUSTED;return;}
        if(Status==XCRON_ZONED_ERROR){J->Info.State=XCRON_JOB_FAULTED;xcronSchedulerIncrement(&S->Stats.Faults);if(Poll)++Poll->Faults;return;}
        xcronzonedvalue* V=&S->Searches[I];xcronZonedValueRestart(&J->Config.Expression,J->Config.Zone,Begin,J->Config.End,Flags,V);
        V->Work=Work;V->Calendar.First=Local;V->Offset=Offset;xcronZoneWalkInit(J->Config.Zone,First,&V->Transition);V->Phase=1;
    }else xcronZonedValueRestart(&J->Config.Expression,J->Config.Zone,Begin,J->Config.End,Flags,&S->Searches[I]);
    J->Info.State=S->Searches[I].Status==XCRON_ZONED_NONE?XCRON_JOB_EXHAUSTED:XCRON_JOB_SEARCHING;
}
static void xcronSchedulerSkip(xcronscheduler* S,uint32 I,xtime Begin,uint32 Flags,xcronpoll* Poll,xcronzonedbudget* Remaining,xcronzonedbudget* Used)
{
    xcronSchedulerIncrement(&S->Jobs[I].Info.SkippedRanges);xcronSchedulerIncrement(&S->Stats.SkippedRanges);++Poll->SkippedRanges;
    xcronSchedulerSeek(S,I,Begin,Flags,Poll,Remaining,Used);
}
static bool xcronSchedulerActionable(const xcronscheduler* S,xcronclock Now)
{
    if(S->Searching || (S->Stats.Heap && S->Jobs[S->Heap[0]].Info.Next<=Now.Wall))return true;
    return S->Stats.Ready && S->FreeRun!=XCRON_SLOT_NONE;
}
static bool xcronSchedulerClock(const xcronscheduler* S,xcronclock Now)
{return !S->Stats.HasClock || Now.MonotonicMilliseconds>=S->Stats.LastClock.MonotonicMilliseconds;}
static xtime xcronSchedulerCutoff(xtime Wall,uint64 Grace)
{
    uint64 Distance=(uint64)Wall-(uint64)INT64_MIN;
    if(Grace>=Distance)return INT64_MIN;
    return Grace<=INT64_MAX?Wall-(int64)Grace:INT64_MIN+(int64)(Distance-Grace);
}
XCRON_API bool xcronSchedulerCreate(const xcronschedulerconfig* Config,xcronscheduler** Out,xcronschedulerissue* Issue)
{
    void* O[]={Out,Issue};size_t N[]={sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!Config || !Out || !xcronSchedulerOutputs(NULL,Config,sizeof(*Config),O,N,2) || !Config->JobLimit || !Config->RunLimit || !Config->WorkLimit || Config->Reserved || Config->JobLimit==UINT32_MAX || Config->RunLimit==UINT32_MAX)
        return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    size_t Bytes=sizeof(xcronscheduler);
    if(!xcronSchedulerArrayBytes(Config->JobLimit,sizeof(xcronjobslot),&Bytes) ||
        !xcronSchedulerArrayBytes(Config->RunLimit,sizeof(xcronrunslot),&Bytes) ||
        !xcronSchedulerArrayBytes(Config->JobLimit,sizeof(uint32),&Bytes))return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    size_t Padding=(_Alignof(xcronzonedvalue)-Bytes%_Alignof(xcronzonedvalue))%_Alignof(xcronzonedvalue);
    if(Padding>SIZE_MAX-Bytes)return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    Bytes+=Padding;size_t SearchAt=Bytes;
    if(!xcronSchedulerArrayBytes(Config->JobLimit,sizeof(xcronzonedvalue),&Bytes) || Bytes>Config->BytesLimit)return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    size_t JBytes=(size_t)Config->JobLimit*sizeof(xcronjobslot),RBytes=(size_t)Config->RunLimit*sizeof(xcronrunslot);
    xcronscheduler* S=xrtMalloc(Bytes);if(!S)return xcronSchedulerFail(XCRON_SCHEDULER_MEMORY,NULL,Issue);
    memset(S,0,Bytes);S->Magic=XCRON_SCHEDULER_MAGIC;S->Config=*Config;S->Stats.Bytes=Bytes;
    S->Jobs=(xcronjobslot*)S->Storage;S->Runs=(xcronrunslot*)(S->Storage+JBytes);S->Heap=(uint32*)(S->Storage+JBytes+RBytes);
    S->Searches=(xcronzonedvalue*)((unsigned char*)S+SearchAt);S->SearchHead=S->SearchTail=XCRON_SLOT_NONE;
    S->FreeJob=0;S->FreeRun=0;S->ReadyHead=S->ReadyTail=S->RetireHead=S->RetireTail=XCRON_SLOT_NONE;S->Promote=true;S->JobBits=xcronSchedulerIndexBits(Config->JobLimit);S->RunBits=xcronSchedulerIndexBits(Config->RunLimit);
    for(uint32 I=0;I<Config->JobLimit;++I){S->Jobs[I].Generation=1;S->Jobs[I].Next=I+1<Config->JobLimit?I+1:XCRON_SLOT_NONE;}
    for(uint32 I=0;I<Config->RunLimit;++I){S->Runs[I].Generation=1;S->Runs[I].Next=I+1<Config->RunLimit?I+1:XCRON_SLOT_NONE;}
    *Out=S;xcronSchedulerOK(Issue);return true;
}
static bool xcronSchedulerAdmissionArgs(xcronscheduler* S,const xcronjobconfig* Config,xcronjobid* Id,xcronschedulerissue* Issue)
{
    void* O[]={Id,Issue};size_t N[]={sizeof(*Id),Issue?sizeof(*Issue):0};
    if(!S || !Config || !Id || !xcronSchedulerOutputs(S,Config,sizeof(*Config),O,N,2) ||
        !xcronZoneOutputs(Config->Zone,Config,sizeof(*Config),O,N,2) ||
        (Config->SearchFlags&~XCRON_SEARCH_EXCLUSIVE) || Config->Begin>Config->End ||
        Config->Misfire<XCRON_MISFIRE_CATCH_UP || Config->Misfire>XCRON_MISFIRE_COALESCE || Config->Overlap<XCRON_OVERLAP_WAIT || Config->Overlap>XCRON_OVERLAP_SKIP ||
        !Config->RunningLimit || Config->RunningLimit>S->Config.RunLimit || (Config->Overlap!=XCRON_OVERLAP_ALLOW && Config->RunningLimit!=1))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(S->Stats.Closed)return xcronSchedulerFail(XCRON_SCHEDULER_CLOSED,NULL,Issue);
    if(S->FreeJob==XCRON_SLOT_NONE)return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    xcronfield Field;xcronzonedissue ZI={.Field=XCRON_FIELD_NONE};
    if(!xcronCalendarValidateExpression(&Config->Expression,&Field)){ZI.Code=XCRON_ZONED_EXPRESSION;ZI.Field=Field;return xcronSchedulerFail(XCRON_SCHEDULER_CALCULATION,&ZI,Issue);}
    return true;
}
static uint32 xcronSchedulerAllocateJob(xcronscheduler* S,const xcronjobconfig* Config,bool Initial)
{
    uint32 I=S->FreeJob;xcronjobslot* J=&S->Jobs[I];S->FreeJob=J->Next;J->Used=true;J->Initial=Initial;J->Location=XCRON_LOC_RUNNING;J->Config=*Config;memset(&J->Info,0,sizeof(J->Info));
    ++S->Stats.Jobs;if(S->Stats.Jobs>S->Stats.PeakJobs)S->Stats.PeakJobs=S->Stats.Jobs;return I;
}
static void xcronSchedulerFreeJob(xcronscheduler* S,uint32 I)
{
    xcronjobslot* J=&S->Jobs[I];xcronZoneRelease(J->Config.Zone);J->Config.Zone=NULL;J->Config.Data=NULL;J->Used=false;J->Initial=false;J->Location=XCRON_LOC_FREE;--S->Stats.Jobs;
    if(J->Generation!=(UINT64_MAX>>S->JobBits)){++J->Generation;J->Next=S->FreeJob;S->FreeJob=I;}
}
XCRON_API bool xcronSchedulerAdd(xcronscheduler* S,const xcronjobconfig* Config,xcronjobid* Id,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerAdmissionArgs(S,Config,Id,Issue))return false;
    xcronzonedissue ZI={.Field=XCRON_FIELD_NONE};
    xtime Next=0;xcronzonedstatus Result=xcronZonedNextValue(&Config->Expression,Config->Zone,Config->Begin,Config->End,Config->SearchFlags,&Next,&ZI);
    if(Result==XCRON_ZONED_ERROR)return xcronSchedulerFail(XCRON_SCHEDULER_CALCULATION,&ZI,Issue);
    if(!xcronZoneRetain(Config->Zone))return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    uint32 I=xcronSchedulerAllocateJob(S,Config,false);xcronjobslot* J=&S->Jobs[I];
    J->Info.State=Result==XCRON_ZONED_FOUND?XCRON_JOB_SCHEDULED:XCRON_JOB_EXHAUSTED;J->Info.HasNext=Result==XCRON_ZONED_FOUND;J->Info.Next=Next;J->Info.LastIssue=ZI;
    xcronSchedulerPlace(S,I);*Id=xcronSchedulerId(J->Generation,I,S->JobBits);xcronSchedulerOK(Issue);return true;
}
bool xcronSchedulerBeginAdmission(xcronscheduler* S,const xcronjobconfig* Config,xcronjobid* Id,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerAdmissionArgs(S,Config,Id,Issue))return false;
    if(!xcronZoneRetain(Config->Zone))return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    uint32 I=xcronSchedulerAllocateJob(S,Config,true);xcronjobslot* J=&S->Jobs[I];
    xcronZonedValueInit(&J->Config.Expression,J->Config.Zone,J->Config.Begin,J->Config.End,J->Config.SearchFlags,&S->Searches[I]);
    J->Info.LastIssue=(xcronzonedissue){.Field=XCRON_FIELD_NONE};J->Info.State=S->Searches[I].Status==XCRON_ZONED_PENDING?XCRON_JOB_SEARCHING:XCRON_JOB_EXHAUSTED;
    xcronSchedulerPlace(S,I);*Id=xcronSchedulerId(J->Generation,I,S->JobBits);xcronSchedulerOK(Issue);return true;
}
xcronadmissionstatus xcronSchedulerFinishAdmission(xcronscheduler* S,xcronjobid Id,bool Cancel,xcronschedulerissue* Issue)
{
    uint32 I;if(!xcronSchedulerJobIndex(S,Id,&I) || !S->Jobs[I].Initial){xcronSchedulerFail(XCRON_SCHEDULER_STALE,NULL,Issue);return XCRON_ADMISSION_REJECTED;}
    xcronjobslot* J=&S->Jobs[I];
    if(Cancel || S->Stats.Closed){xcronSchedulerDetach(S,I);xcronSchedulerFreeJob(S,I);xcronSchedulerFail(XCRON_SCHEDULER_CLOSED,NULL,Issue);return XCRON_ADMISSION_REJECTED;}
    if(J->Info.State==XCRON_JOB_SEARCHING)return XCRON_ADMISSION_PENDING;
    if(J->Info.State==XCRON_JOB_FAULTED){xcronzonedissue ZI=J->Info.LastIssue;xcronSchedulerFreeJob(S,I);xcronSchedulerFail(XCRON_SCHEDULER_CALCULATION,&ZI,Issue);return XCRON_ADMISSION_REJECTED;}
    J->Initial=false;xcronSchedulerPlace(S,I);xcronSchedulerOK(Issue);return XCRON_ADMISSION_ACCEPTED;
}
XCRON_SCHEDULER_INLINE void xcronSchedulerReady(xcronscheduler* S,xcronclock Now,xcronrun* Runs,xcronpoll* P,bool* GlobalBlocked,xcronzonedbudget* Remaining,xcronzonedbudget* Used)
{
    uint32 I=S->ReadyHead;xcronjobslot* J=&S->Jobs[I];
    if(J->Info.Next>Now.Wall){xcronSchedulerQueueRemove(S,I,false);xcronSchedulerHeapAdd(S,I);return;}
    if(J->Config.Overlap==XCRON_OVERLAP_SKIP && J->Info.Running){
        xcronSchedulerQueueRemove(S,I,false);xcronSchedulerSkip(S,I,Now.Wall,XCRON_SEARCH_EXCLUSIVE,P,Remaining,Used);xcronSchedulerPlace(S,I);return;
    }
    if(J->Info.Running>=J->Config.RunningLimit){xcronSchedulerQueueRemove(S,I,false);J->Location=XCRON_LOC_BLOCKED;++S->Stats.Blocked;return;}
    if(S->FreeRun==XCRON_SLOT_NONE){*GlobalBlocked=true;return;}
    xcronSchedulerQueueRemove(S,I,false);xtime Cutoff=xcronSchedulerCutoff(Now.Wall,J->Config.GraceMilliseconds);
    if(J->Config.Misfire==XCRON_MISFIRE_SKIP && J->Info.Next<Cutoff){
        xcronSchedulerSkip(S,I,Cutoff,0,P,Remaining,Used);xcronSchedulerPlace(S,I);return;
    }
    bool Coalesced=J->Config.Misfire==XCRON_MISFIRE_COALESCE && J->Info.Next<Cutoff;
    xtime Through=Coalesced?(Now.Wall<J->Config.End?Now.Wall:J->Config.End):J->Info.Next;
    uint32 RIndex=S->FreeRun;xcronrunslot* R=&S->Runs[RIndex];S->FreeRun=R->Next;R->Used=true;R->Job=I;
    Runs[P->Count++]=(xcronrun){xcronSchedulerId(R->Generation,RIndex,S->RunBits),xcronSchedulerId(J->Generation,I,S->JobBits),J->Config.Data,J->Info.Next,Through,Now,Coalesced?XCRON_RUN_COALESCED:0};
    ++J->Info.Running;++S->Stats.Running;if(S->Stats.Running>S->Stats.PeakRunning)S->Stats.PeakRunning=S->Stats.Running;
    xcronSchedulerIncrement(&J->Info.Dispatched);xcronSchedulerIncrement(&S->Stats.Dispatched);
    if(Coalesced){xcronSchedulerIncrement(&J->Info.CoalescedRanges);xcronSchedulerIncrement(&S->Stats.CoalescedRanges);}
    xcronSchedulerSeek(S,I,Through,XCRON_SEARCH_EXCLUSIVE,P,Remaining,Used);xcronSchedulerPlace(S,I);
}
XCRON_API bool xcronSchedulerPoll(xcronscheduler* S,xcronclock Now,xcronrun* Runs,size_t Capacity,xcronpoll* Out,xcronschedulerissue* Issue)
{
    void* O[]={Runs,Out,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Runs)?Capacity*sizeof(*Runs):SIZE_MAX,sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!S || !Out || !Runs || !Capacity || Capacity>SIZE_MAX/sizeof(*Runs) || !xcronSchedulerOutputs(S,NULL,0,O,N,3))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(!xcronSchedulerClock(S,Now))return xcronSchedulerFail(XCRON_SCHEDULER_CLOCK,NULL,Issue);
    if(S->Stats.HasClock && Now.Wall<S->Stats.LastClock.Wall)xcronSchedulerIncrement(&S->Stats.WallRegressions);
    S->Stats.LastClock=Now;S->Stats.HasClock=true;xcronSchedulerIncrement(&S->Stats.ClockSamples);xcronpoll P={0};bool GlobalBlocked=false;
    while(S->Searching && P.Work<S->Config.WorkLimit){uint32 I=S->SearchHead;xcronSchedulerSearchRemove(S,I);++P.Work;xcronSchedulerSearchStep(S,I,NULL,NULL,&P,true);xcronSchedulerPlace(S,I);}
    while(P.Work<S->Config.WorkLimit && P.Count<Capacity){
        bool Due=S->Stats.Heap && S->Jobs[S->Heap[0]].Info.Next<=Now.Wall;
        bool Ready=S->Stats.Ready && !GlobalBlocked;
        if(!Due && !Ready)break;
        ++P.Work;
        if(Due && (!Ready || S->Promote)){
            uint32 I=S->Heap[0];xcronSchedulerHeapRemove(S,I);xcronSchedulerQueueAdd(S,I,false);S->Promote=false;continue;
        }
        S->Promote=true;xcronSchedulerReady(S,Now,Runs,&P,&GlobalBlocked,NULL,NULL);
    }
    P.More=xcronSchedulerActionable(S,Now);*Out=P;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerPollBudgeted(xcronscheduler* S,xcronclock Now,xcronzonedbudget Budget,xcronrun* Runs,size_t Capacity,xcronpollbudgeted* Out,xcronschedulerissue* Issue)
{
    void* O[]={Runs,Out,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Runs)?Capacity*sizeof(*Runs):SIZE_MAX,sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!S || !Out || (!Runs && Capacity) || Capacity>SIZE_MAX/sizeof(*Runs) || !Budget.Segments || !Budget.Months || !Budget.RuleYears || !xcronSchedulerOutputs(S,NULL,0,O,N,3))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(!xcronSchedulerClock(S,Now))return xcronSchedulerFail(XCRON_SCHEDULER_CLOCK,NULL,Issue);
    if(S->Stats.HasClock && Now.Wall<S->Stats.LastClock.Wall)xcronSchedulerIncrement(&S->Stats.WallRegressions);
    S->Stats.LastClock=Now;S->Stats.HasClock=true;xcronSchedulerIncrement(&S->Stats.ClockSamples);
    xcronpollbudgeted Q={0};xcronzonedbudget Remaining=Budget;bool GlobalBlocked=false;uint32 Stalled=0;
    while(Q.Poll.Work<S->Config.WorkLimit){
        /* After filling output, only pending calculations need more actions.
         * Avoid a full class scan (and gratuitous heap promotions) otherwise. */
        if(Q.Poll.Count>=Capacity && !S->Searching)break;
        bool Available[3]={S->Stats.Heap && S->Jobs[S->Heap[0]].Info.Next<=Now.Wall,
            S->Stats.Ready && !GlobalBlocked && Q.Poll.Count<Capacity,
            S->Searching && Stalled<S->Searching && (Remaining.Segments || Remaining.Months || Remaining.RuleYears)};
        if(!Available[0] && !Available[1] && !Available[2])break;
        uint32 Class=3;for(uint32 NClass=0;NClass<3;++NClass){uint32 C=(S->Turn+NClass)%3;if(Available[C]){Class=C;break;}}
        if(Class==3)break;
        S->Turn=(uint8)((Class+1)%3);++Q.Poll.Work;
        if(Class==0){uint32 I=S->Heap[0];xcronSchedulerHeapRemove(S,I);xcronSchedulerQueueAdd(S,I,false);}
        else if(Class==1){uint32 Before=S->Searching;xcronSchedulerReady(S,Now,Runs,&Q.Poll,&GlobalBlocked,&Remaining,&Q.Used);if(S->Searching>Before)Stalled=0;}
        else {uint32 I=S->SearchHead;xcronzonedbudget Before=Q.Used;xcronSchedulerSearchRemove(S,I);xcronSchedulerSearchStep(S,I,&Remaining,&Q.Used,&Q.Poll,false);xcronSchedulerPlace(S,I);
            if(S->Jobs[I].Info.State==XCRON_JOB_SEARCHING && Before.Segments==Q.Used.Segments && Before.Months==Q.Used.Months && Before.RuleYears==Q.Used.RuleYears)++Stalled;else Stalled=0;}
    }
    Q.Searching=S->Searching;Q.Poll.More=xcronSchedulerActionable(S,Now);*Out=Q;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerComplete(xcronscheduler* S,xcronrunid Id,xcronrunresult Result,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerArgs(S,NULL,0,Issue) || Result<XCRON_RUN_SUCCEEDED || Result>XCRON_RUN_CANCELLED)return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    uint32 RIndex;if(!xcronSchedulerRunIndex(S,Id,&RIndex))return xcronSchedulerFail(XCRON_SCHEDULER_STALE,NULL,Issue);
    xcronrunslot* R=&S->Runs[RIndex];uint32 I=R->Job;xcronjobslot* J=&S->Jobs[I];
    --J->Info.Running;--S->Stats.Running;R->Used=false;
    if(R->Generation!=(UINT64_MAX>>S->RunBits)){++R->Generation;R->Next=S->FreeRun;S->FreeRun=RIndex;}
    xcronSchedulerIncrement(&S->Stats.Completed);
    xcronSchedulerIncrement(Result==XCRON_RUN_SUCCEEDED?&J->Info.Succeeded:Result==XCRON_RUN_FAILED?&J->Info.Failed:&J->Info.Cancelled);
    if(!xcronSchedulerLive(J->Info.State)){if(!J->Info.Running)xcronSchedulerQueueAdd(S,I,true);}
    else if(J->Location==XCRON_LOC_BLOCKED && J->Info.Running<J->Config.RunningLimit){--S->Stats.Blocked;xcronSchedulerPlace(S,I);}
    xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerCancel(xcronscheduler* S,xcronjobid Id,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerArgs(S,NULL,0,Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    uint32 I;if(!xcronSchedulerJobIndex(S,Id,&I) || S->Jobs[I].Initial)return xcronSchedulerFail(XCRON_SCHEDULER_STALE,NULL,Issue);
    xcronjobslot* J=&S->Jobs[I];if(xcronSchedulerLive(J->Info.State)){xcronSchedulerDetach(S,I);J->Info.State=XCRON_JOB_CANCELLED;J->Info.HasNext=false;xcronSchedulerPlace(S,I);}
    xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerJob(const xcronscheduler* S,xcronjobid Id,xcronjobinfo* Info,xcronschedulerissue* Issue)
{
    if(!Info || !xcronSchedulerArgs(S,Info,sizeof(*Info),Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    uint32 I;if(!xcronSchedulerJobIndex(S,Id,&I) || S->Jobs[I].Initial)return xcronSchedulerFail(XCRON_SCHEDULER_STALE,NULL,Issue);
    *Info=S->Jobs[I].Info;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerReap(xcronscheduler* S,xcronretired* Retired,bool* Found,xcronschedulerissue* Issue)
{
    void* O[]={Retired,Found,Issue};size_t N[]={sizeof(*Retired),sizeof(*Found),Issue?sizeof(*Issue):0};
    if(!S || !Retired || !Found || !xcronSchedulerOutputs(S,NULL,0,O,N,3))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    uint32 I=S->RetireHead;
    if(I==XCRON_SLOT_NONE){*Found=false;xcronSchedulerOK(Issue);return true;}
    xcronjobslot* J=&S->Jobs[I];xcronretired Value={xcronSchedulerId(J->Generation,I,S->JobBits),J->Config.Data,J->Info};
    xcronSchedulerQueueRemove(S,I,true);xcronSchedulerFreeJob(S,I);
    *Retired=Value;*Found=true;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerClose(xcronscheduler* S,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerArgs(S,NULL,0,Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(!S->Stats.Closed){S->Stats.Closed=true;
        for(uint32 I=0;I<S->Config.JobLimit;++I){xcronjobslot* J=&S->Jobs[I];if(J->Used && xcronSchedulerLive(J->Info.State)){xcronSchedulerDetach(S,I);J->Info.State=XCRON_JOB_CANCELLED;J->Info.HasNext=false;xcronSchedulerPlace(S,I);}}}
    xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerWake(const xcronscheduler* S,xcronclock Now,uint64 MaxWait,xcronwake* Wake,xcronschedulerissue* Issue)
{
    if(!Wake || !MaxWait || !xcronSchedulerArgs(S,Wake,sizeof(*Wake),Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(!xcronSchedulerClock(S,Now))return xcronSchedulerFail(XCRON_SCHEDULER_CLOCK,NULL,Issue);
    uint64 Delay=MaxWait;bool Free=S->FreeRun!=XCRON_SLOT_NONE;
    bool Immediate=S->Searching || (S->Stats.Ready && Free) || (S->Stats.Heap && S->Jobs[S->Heap[0]].Info.Next<=Now.Wall);
    /* A full lease table needs an external completion or bounded resampling,
     * not repeated immediate wakes for a ready job it cannot dispatch. */
    if(!Free && !S->Stats.Heap && !S->Searching)Immediate=false;
    if(Immediate)Delay=0;
    else if(S->Stats.Heap){xtime Due=S->Jobs[S->Heap[0]].Info.Next;
        if(Due>Now.Wall){uint64 Distance=(uint64)Due-(uint64)Now.Wall;if(Distance<Delay)Delay=Distance;}}
    xcronwake W={Delay>UINT64_MAX-Now.MonotonicMilliseconds?UINT64_MAX:Now.MonotonicMilliseconds+Delay,Delay,Immediate,S->Stats.Running!=0};
    *Wake=W;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerInspect(const xcronscheduler* S,xcronschedulerstats* Stats,xcronschedulerissue* Issue)
{
    if(!Stats || !xcronSchedulerArgs(S,Stats,sizeof(*Stats),Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    *Stats=S->Stats;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerDestroy(xcronscheduler* S,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerArgs(S,NULL,0,Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(S->Stats.Jobs || S->Stats.Running)return xcronSchedulerFail(XCRON_SCHEDULER_BUSY,NULL,Issue);
    S->Magic=0;xrtFree(S);xcronSchedulerOK(Issue);return true;
}
#endif
