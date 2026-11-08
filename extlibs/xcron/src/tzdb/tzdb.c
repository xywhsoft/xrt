#include "tzdb_internal.h"
#include "../timezone/timezone_internal.h"
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_TZDB)
static xcrontzdbissue xcronTzdbWork(xcrontzdberror Code,size_t Index)
{return (xcrontzdbissue){.Code=Code,.Index=Index};}
static bool xcronTzdbReport(xcrontzdbissue Work,xcrontzdbissue* Issue)
{
    if(Issue)*Issue=Work;
    xerrkind Kind=Work.Code==XCRON_TZDB_ARGUMENT?XERR_ARGUMENT:Work.Code==XCRON_TZDB_MEMORY?XERR_MEMORY:
        Work.Code==XCRON_TZDB_LIMIT?XERR_RANGE:Work.Code==XCRON_TZDB_IO?XERR_IO:
        (Work.Code==XCRON_TZDB_NAME || Work.Code==XCRON_TZDB_VERSION || Work.Code==XCRON_TZDB_DUPLICATE || Work.Code==XCRON_TZDB_ZONE)?XERR_VALUE:XERR_STATE;
    xrtSetErrorInfo(Kind,"xrt.cron.tzdb",Work.Code,"cron named timezone database operation failed");return false;
}
static bool xcronTzdbFail(xcrontzdberror Code,xcrontzdbissue* Issue)
{return xcronTzdbReport(xcronTzdbWork(Code,XCRON_TZDB_NO_INDEX),Issue);}
static void xcronTzdbOK(xcrontzdbissue* Issue)
{if(Issue)*Issue=xcronTzdbWork(XCRON_TZDB_OK,XCRON_TZDB_NO_INDEX);}
static bool xcronTzdbOutputs(const void* Owner,size_t Bytes,const void* Input,size_t InputBytes,void* const* O,const size_t* N,size_t Count)
{
    if(!xrtMemRangeValid(Owner,Bytes) || !xrtMemRangeValid(Input,InputBytes))return false;
    for(size_t I=0;I<Count;++I){
        if(!xrtMemRangeValid(O[I],N[I]) || xrtMemRangesOverlap(Owner,Bytes,O[I],N[I]) || xrtMemRangesOverlap(Input,InputBytes,O[I],N[I]))return false;
        for(size_t J=0;J<I;++J)if(xrtMemRangesOverlap(O[I],N[I],O[J],N[J]))return false;
    }
    return true;
}
static bool xcronTzdbHandle(const xcrontzdb* D)
{return D && xrtMemRangeValid(D,sizeof(*D)) && D->Magic==XCRON_TZDB_MAGIC;}
static bool xcronTzdbSnapshotHandle(const xcrontzdbsnapshot* S)
{return S && xrtMemRangeValid(S,sizeof(*S)) && S->Magic==XCRON_TZDB_SNAPSHOT_MAGIC && xrtMemRangeValid(S,S->ArenaBytes);}
static bool xcronTzdbArgs(const xcrontzdb* D,void* Output,size_t Bytes,xcrontzdbissue* Issue)
{void* O[]={Output,Issue};size_t N[]={Bytes,Issue?sizeof(*Issue):0};return xcronTzdbHandle(D) && xcronTzdbOutputs(D,sizeof(*D),NULL,0,O,N,2);}
static bool xcronTzdbName(xstrview Name)
{
    if(!Name.Size || Name.Size>XCRON_TZDB_NAME_MAX)return false;
    size_t Start=0;
    for(size_t I=0;I<=Name.Size;++I){
        unsigned C=I==Name.Size?'/':(unsigned char)Name.Data[I];
        if(C=='/'){
            if(I==Start || Name.Data[Start]=='-' || Name.Data[I-1]=='.' ||
                (I-Start==1 && Name.Data[Start]=='.') || (I-Start==2 && Name.Data[Start]=='.' && Name.Data[Start+1]=='.'))return false;
            Start=I+1;
        }
        else if(!((C>='A' && C<='Z') || (C>='a' && C<='z') || (C>='0' && C<='9') || C=='_' || C=='.' || C=='-' || C=='+'))return false;
    }
    return true;
}
static bool xcronTzdbVersion(xstrview V)
{if(!V.Size || V.Size>XCRON_TZDB_VERSION_MAX)return false;for(size_t I=0;I<V.Size;++I)if((unsigned char)V.Data[I]<0x21 || (unsigned char)V.Data[I]>0x7e)return false;return true;}
static int xcronTzdbCompare(xstrview A,xstrview B)
{size_t N=A.Size<B.Size?A.Size:B.Size;int C=memcmp(A.Data,B.Data,N);return C?C:(A.Size>B.Size)-(A.Size<B.Size);}
static void xcronTzdbSwap(xcrontzdbrow* A,xcrontzdbrow* B)
{xcrontzdbrow T=*A;*A=*B;*B=T;}
static void xcronTzdbDown(xcrontzdbrow* Rows,size_t Count,size_t At)
{
    while(At<Count/2){size_t Child=At*2+1;
        if(Child+1<Count && xcronTzdbCompare(Rows[Child].Name,Rows[Child+1].Name)<0)++Child;
        if(xcronTzdbCompare(Rows[At].Name,Rows[Child].Name)>=0)break;
        xcronTzdbSwap(&Rows[At],&Rows[Child]);At=Child;
    }
}
static void xcronTzdbSort(xcrontzdbsnapshot* S)
{for(size_t I=S->Count/2;I;--I)xcronTzdbDown(S->Rows,S->Count,I-1);for(size_t I=S->Count;I>1;--I){xcronTzdbSwap(&S->Rows[0],&S->Rows[I-1]);xcronTzdbDown(S->Rows,I-1,0);}}
static bool xcronTzdbRetainValue(xcrontzdbsnapshot* S)
{uint32 N=xrtAtomic32Load(&S->Refs,XMEMORY_RELAXED);while(N && N<UINT32_MAX/2u)if(xrtAtomic32CompareExchange(&S->Refs,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;return false;}
XCRON_API bool xcronTzdbSnapshotRetain(const xcrontzdbsnapshot* S)
{if(!xcronTzdbSnapshotHandle(S))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);return xcronTzdbRetainValue((xcrontzdbsnapshot*)S) || xcronTzdbFail(XCRON_TZDB_LIMIT,NULL);}
XCRON_API void xcronTzdbSnapshotRelease(const xcrontzdbsnapshot* Raw)
{
    xcrontzdbsnapshot* S=(xcrontzdbsnapshot*)Raw;
    if(S && xrtAtomic32FetchSub(&S->Refs,1,XMEMORY_ACQ_REL)==1){S->Magic=0;for(size_t I=0;I<S->Count;++I)xcronZoneRelease(S->Rows[I].Zone);xrtFree(S);}
}
XCRON_API bool xcronTzdbCreate(const xcrontzdbconfig* C,xcrontzdb** Out,xcrontzdbissue* Issue)
{
    void* O[]={Out,Issue};size_t N[]={sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!C || !Out || !xcronTzdbOutputs(NULL,0,C,sizeof(*C),O,N,2) || !C->NamesLimit || C->NamesLimit>XCRON_TZDB_NAMES_MAX || !C->LoadLimit || C->LoadLimit>XCRON_TZDB_LOADS_MAX || !C->BytesLimit)return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    if(C->BytesLimit<sizeof(xcrontzdb))return xcronTzdbFail(XCRON_TZDB_LIMIT,Issue);
    xallocationbudget* Budget=NULL;if(!xrtAllocationBudgetCreate(C->BytesLimit,&Budget))return xcronTzdbFail(XCRON_TZDB_MEMORY,Issue);
    xallocationscope Scope={0};if(!xrtAllocationBudgetEnter(Budget,&Scope)){xrtAllocationBudgetFree(Budget);return xcronTzdbFail(XCRON_TZDB_MEMORY,Issue);}
    xcrontzdb* D=xrtMalloc(sizeof(*D));if(D){memset(D,0,sizeof(*D));D->Config=*C;D->Budget=Budget;xrtAtomic32Init(&D->Closed,0);}
    xcrontzdberror Code=D?XCRON_TZDB_OK:Scope.Rejections?XCRON_TZDB_LIMIT:XCRON_TZDB_MEMORY;
    if(D && !xrtMutexInit(&D->Mutex)){xrtFree(D);D=NULL;Code=XCRON_TZDB_SYSTEM;}
    (void)xrtAllocationBudgetLeave(&Scope);
    if(!D){xrtAllocationBudgetFree(Budget);return xcronTzdbFail(Code,Issue);}
    D->Magic=XCRON_TZDB_MAGIC;*Out=D;xcronTzdbOK(Issue);return true;
}
static xstrview xcronTzdbSourceName(const xcrontzdbsource* S,size_t I)
{return S->Entries?S->Entries[I].Name:S->Names[I];}
bool xcronTzdbLoadSource(xcrontzdb* D,xstrview Version,const xcrontzdbsource* Source,uint64 Expected,uint64* Revision,xcrontzdbissue* Issue)
{
    void* O[]={Revision,Issue};size_t N[]={sizeof(*Revision),Issue?sizeof(*Issue):0};size_t Count=Source->Count;
    size_t Stride=Source->Entries?sizeof(*Source->Entries):sizeof(*Source->Names);const void* Items=Source->Entries?(const void*)Source->Entries:(const void*)Source->Names;
    if(!xcronTzdbHandle(D) || !Revision || Count>XCRON_TZDB_NAMES_MAX || !xcronTzdbOutputs(D,sizeof(*D),Items,Count*Stride,O,N,2) || !xcronTzdbOutputs(NULL,0,Version.Data,Version.Size,O,N,2))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    for(size_t I=0;I<Count;++I){xstrview Name=xcronTzdbSourceName(Source,I);
        if(!xcronTzdbOutputs(NULL,0,Name.Data,Name.Size,O,N,2) || (Source->Entries && !xcronTzdbOutputs(NULL,0,Source->Entries[I].Tzif.Data,Source->Entries[I].Tzif.Size,O,N,2)))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);}
    if(Count>D->Config.NamesLimit)return xcronTzdbFail(XCRON_TZDB_LIMIT,Issue);
    if(!xcronTzdbVersion(Version))return xcronTzdbFail(XCRON_TZDB_VERSION,Issue);
    size_t Bytes=sizeof(xcrontzdbsnapshot)+Count*sizeof(xcrontzdbrow)+Version.Size+1;
    for(size_t I=0;I<Count;++I){xstrview Name=xcronTzdbSourceName(Source,I);if(!xcronTzdbName(Name))return xcronTzdbReport(xcronTzdbWork(XCRON_TZDB_NAME,I),Issue);Bytes+=Name.Size+1;}
    if(!xrtMutexLock(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    xcrontzdberror Code=xrtAtomic32Load(&D->Closed,XMEMORY_RELAXED)?XCRON_TZDB_CLOSED:
        (Expected!=XCRON_TZDB_ANY_REVISION && Expected!=D->Revision)?XCRON_TZDB_STALE:
        (D->Loading>=D->Config.LoadLimit || D->Revision==UINT64_MAX-1)?XCRON_TZDB_LIMIT:XCRON_TZDB_OK;
    if(Code==XCRON_TZDB_OK)++D->Loading;
    (void)xrtMutexUnlock(&D->Mutex);if(Code!=XCRON_TZDB_OK)return xcronTzdbFail(Code,Issue);
    xerror* Ambient=xrtTakeError();
    xcrontzdbissue Work=xcronTzdbWork(XCRON_TZDB_OK,XCRON_TZDB_NO_INDEX);xallocationscope Scope={0};xcrontzdbsnapshot *S=NULL,*Old=NULL;uint64 Published=0;
    bool Entered=xrtAllocationBudgetEnter(D->Budget,&Scope);
    if(!Entered)Work.Code=XCRON_TZDB_MEMORY;
    else {
        S=xrtMalloc(Bytes);
        if(!S)Work.Code=XCRON_TZDB_MEMORY;
        else {memset(S,0,Bytes);S->Magic=XCRON_TZDB_SNAPSHOT_MAGIC;xrtAtomic32Init(&S->Refs,1);S->ArenaBytes=S->Bytes=Bytes;S->Count=Count;
            char* Text=(char*)(S->Rows+Count);memcpy(Text,Version.Data,Version.Size);S->Version=(xstrview){Text,Version.Size};Text+=Version.Size+1;
            for(size_t I=0;I<Count;++I){xstrview Name=xcronTzdbSourceName(Source,I);memcpy(Text,Name.Data,Name.Size);S->Rows[I].Name=(xstrview){Text,Name.Size};S->Rows[I].Index=(uint32)I;Text+=Name.Size+1;}
            xcronTzdbSort(S);
            for(size_t I=1;I<Count;++I)if(!xcronTzdbCompare(S->Rows[I-1].Name,S->Rows[I].Name)){Work=xcronTzdbWork(XCRON_TZDB_DUPLICATE,S->Rows[I].Index);break;}
            for(size_t I=0;I<Count && Work.Code==XCRON_TZDB_OK;++I){
                size_t Index=S->Rows[I].Index;xbytesview Data={0};ptr Owned=NULL;Work.Zone=(xcronzoneissue){0};Work.Cause=XERR_NONE;Work.SystemCode=0;
                if(xrtAtomic32Load(&D->Closed,XMEMORY_ACQUIRE)){Work=xcronTzdbWork(XCRON_TZDB_CLOSED,Index);break;}
                bool Read=true;if(Source->Entries)Data=Source->Entries[Index].Tzif;else Read=Source->Read(Source->Context,S->Rows[I].Name,&Data,&Owned,&Work);
                if(Read){Work.Zone=(xcronzoneissue){0};if(!xcronZoneCreateTzif(Data,&S->Rows[I].Zone,&Work.Zone))Work.Code=XCRON_TZDB_ZONE;
                    else S->Bytes+=xcronZoneOwnedBytes(S->Rows[I].Zone);}
                xrtFree(Owned);if(Work.Code!=XCRON_TZDB_OK)Work.Index=Index;
            }
        }
    }
    if(Entered && Scope.Rejections && (Work.Code==XCRON_TZDB_MEMORY || (Work.Code==XCRON_TZDB_ZONE && Work.Zone.Code==XCRON_ZONE_MEMORY))){Work.Code=XCRON_TZDB_LIMIT;Work.Cause=XERR_MEMORY;}
    if(Work.Code==XCRON_TZDB_OK){
        if(!xrtMutexLock(&D->Mutex))Work.Code=XCRON_TZDB_SYSTEM;
        else {
            Work.Code=xrtAtomic32Load(&D->Closed,XMEMORY_RELAXED)?XCRON_TZDB_CLOSED:
                (Expected!=XCRON_TZDB_ANY_REVISION && Expected!=D->Revision)?XCRON_TZDB_STALE:D->Revision==UINT64_MAX-1?XCRON_TZDB_LIMIT:XCRON_TZDB_OK;
            if(Work.Code==XCRON_TZDB_OK){Published=++D->Revision;S->Revision=Published;Old=D->Current;D->Current=S;S=NULL;}
            (void)xrtMutexUnlock(&D->Mutex);
        }
    }
    xcronTzdbSnapshotRelease(S);xcronTzdbSnapshotRelease(Old);
    if(Entered)(void)xrtAllocationBudgetLeave(&Scope);
    (void)xrtMutexLock(&D->Mutex);--D->Loading;(void)xrtMutexUnlock(&D->Mutex);
    if(Work.Code!=XCRON_TZDB_OK){xrtErrorFree(Ambient);return xcronTzdbReport(Work,Issue);}
    xrtSetErrorTake(Ambient);*Revision=Published;xcronTzdbOK(Issue);return true;
}
XCRON_API bool xcronTzdbLoad(xcrontzdb* D,xstrview Version,const xcrontzdbentry* Entries,size_t Count,uint64 Expected,uint64* Revision,xcrontzdbissue* Issue)
{xcrontzdbsource Source={.Entries=Entries,.Count=Count};return xcronTzdbLoadSource(D,Version,&Source,Expected,Revision,Issue);}
XCRON_API bool xcronTzdbAcquire(xcrontzdb* D,xcrontzdbsnapshot** Out,xcrontzdbissue* Issue)
{
    if(!Out || !xcronTzdbArgs(D,Out,sizeof(*Out),Issue))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    if(!xrtMutexLock(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    xcrontzdberror Code=xrtAtomic32Load(&D->Closed,XMEMORY_RELAXED)?XCRON_TZDB_CLOSED:!D->Current?XCRON_TZDB_EMPTY:!xcronTzdbRetainValue(D->Current)?XCRON_TZDB_LIMIT:XCRON_TZDB_OK;
    xcrontzdbsnapshot* S=D->Current;(void)xrtMutexUnlock(&D->Mutex);
    if(Code!=XCRON_TZDB_OK)return xcronTzdbFail(Code,Issue);
    *Out=S;xcronTzdbOK(Issue);return true;
}
XCRON_API bool xcronTzdbSnapshotInfo(const xcrontzdbsnapshot* S,xcrontzdbsnapshotinfo* Info,xcrontzdbissue* Issue)
{
    void* O[]={Info,Issue};size_t N[]={sizeof(*Info),Issue?sizeof(*Issue):0};
    if(!Info || !xcronTzdbSnapshotHandle(S) || !xcronTzdbOutputs(S,S->ArenaBytes,NULL,0,O,N,2))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    *Info=(xcrontzdbsnapshotinfo){S->Revision,S->Count,S->Bytes,S->Version};xcronTzdbOK(Issue);return true;
}
XCRON_API xcrontzdbstatus xcronTzdbFind(const xcrontzdbsnapshot* S,xstrview Name,xcronzone** Zone,xcrontzdbissue* Issue)
{
    void* O[]={Zone,Issue};size_t N[]={sizeof(*Zone),Issue?sizeof(*Issue):0};
    if(!Zone || !xcronTzdbSnapshotHandle(S) || !xcronTzdbOutputs(S,S->ArenaBytes,Name.Data,Name.Size,O,N,2)){xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);return XCRON_TZDB_ERROR;}
    if(!xcronTzdbName(Name)){xcronTzdbFail(XCRON_TZDB_NAME,Issue);return XCRON_TZDB_ERROR;}
    size_t Low=0,High=S->Count;
    while(Low<High){size_t Mid=Low+(High-Low)/2;int C=xcronTzdbCompare(Name,S->Rows[Mid].Name);
        if(C<0)High=Mid;else if(C>0)Low=Mid+1;else {
            if(!xcronZoneOutputs(S->Rows[Mid].Zone,Name.Data,Name.Size,O,N,2)){xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);return XCRON_TZDB_ERROR;}
            if(!xcronZoneRetain(S->Rows[Mid].Zone)){xcronTzdbFail(XCRON_TZDB_LIMIT,Issue);return XCRON_TZDB_ERROR;}
            *Zone=S->Rows[Mid].Zone;xcronTzdbOK(Issue);return XCRON_TZDB_FOUND;
        }
    }
    xcronTzdbOK(Issue);return XCRON_TZDB_NONE;
}
XCRON_API bool xcronTzdbInspect(xcrontzdb* D,xcrontzdbstats* Stats,xcrontzdbissue* Issue)
{
    if(!Stats || !xcronTzdbArgs(D,Stats,sizeof(*Stats),Issue))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    if(!xrtMutexLock(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    xcrontzdbstats Value={.Revision=D->Revision,.Loading=D->Loading,.Names=D->Current?(uint32)D->Current->Count:0,.BytesLimit=D->Config.BytesLimit,.Closed=xrtAtomic32Load(&D->Closed,XMEMORY_RELAXED)!=0};
    xallocationbudgetinfo Q={.Size=sizeof(Q)};bool OK=xrtAllocationBudgetInfo(D->Budget,&Q);(void)xrtMutexUnlock(&D->Mutex);
    if(!OK)return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    Value.LiveBytes=Q.LiveBytes;Value.PeakBytes=Q.PeakBytes;Value.LiveAllocations=Q.LiveCount;Value.Rejections=Q.Rejections;
    *Stats=Value;xcronTzdbOK(Issue);return true;
}
XCRON_API bool xcronTzdbClose(xcrontzdb* D,xcrontzdbissue* Issue)
{
    if(!xcronTzdbArgs(D,NULL,0,Issue))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    if(!xrtMutexLock(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    xrtAtomic32Store(&D->Closed,1,XMEMORY_RELEASE);xcrontzdbsnapshot* Old=D->Current;D->Current=NULL;(void)xrtMutexUnlock(&D->Mutex);
    xcronTzdbSnapshotRelease(Old);xcronTzdbOK(Issue);return true;
}
XCRON_API bool xcronTzdbDestroy(xcrontzdb* D,xcrontzdbissue* Issue)
{
    if(!xcronTzdbArgs(D,NULL,0,Issue))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    if(!xrtMutexLock(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    bool Busy=!xrtAtomic32Load(&D->Closed,XMEMORY_RELAXED) || D->Loading;(void)xrtMutexUnlock(&D->Mutex);
    if(Busy)return xcronTzdbFail(XCRON_TZDB_BUSY,Issue);
    if(!xrtMutexUnit(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    xallocationbudget* Budget=D->Budget;D->Magic=0;xrtFree(D);xrtAllocationBudgetFree(Budget);xcronTzdbOK(Issue);return true;
}
#endif
