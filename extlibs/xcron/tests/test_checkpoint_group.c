#include "test.h"
#include <xcron/checkpoint_writer.h>
#include <xrt/sync.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { GROUP_CAPACITY=32,GROUP_SLOT=8192,GROUP_HEADER=40 };
static const uint64 GroupId=940;
static const char GroupPath[]="group.xcg";

static void groupRecords(uint32 Count,xcroncheckpoint* First,xcroncheckpoint* Second)
{
    xcronjobconfig Config={.Begin=1000,.End=60000,.RunningLimit=1};
    const xcronschedulerconfig Limits={GROUP_CAPACITY,GROUP_CAPACITY,256,0,262144};
    xcronscheduler* S=NULL;
    xcronjobid Jobs[GROUP_CAPACITY];
    testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Config.Zone,NULL) &&
                xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Config.Expression,NULL) &&
                xcronSchedulerCreate(&Limits,&S,NULL),"create group checkpoint sources");
    for(uint32 I=0;I<Count;++I)
        testRequire(xcronSchedulerAdd(S,&Config,&Jobs[I],NULL) &&
                    xcronSchedulerSaveCheckpoint(S,Jobs[I],(xcroncheckpointkey){I+1,1},&First[I],NULL),
                    "capture first group");
    xcronrun Runs[GROUP_CAPACITY];
    xcronpoll Poll;
    testRequire(xcronSchedulerPoll(S,(xcronclock){1000,1},Runs,GROUP_CAPACITY,&Poll,NULL) && Poll.Count==Count,
                "take one occurrence per job");
    for(size_t I=0;I<Poll.Count;++I)
        testRequire(Runs[I].ScheduledAt==1000 && xcronSchedulerComplete(S,Runs[I].Id,XCRON_RUN_SUCCEEDED,NULL),
                    "finish physical lease");
    for(uint32 I=0;I<Count;++I)
        testRequire(xcronSchedulerSaveCheckpoint(S,Jobs[I],(xcroncheckpointkey){I+1,1},&Second[I],NULL),"capture advanced group");
    testRequire(xcronSchedulerClose(S,NULL),"close checkpoint sources");
    for(uint32 I=0;I<Count;++I){
        xcronretired Retired;
        bool Found=false;
        testRequire(xcronSchedulerReap(S,&Retired,&Found,NULL) && Found,"physical retirement");
    }
    testRequire(xcronSchedulerDestroy(S,NULL),"destroy drained scheduler");
    xcronZoneRelease(Config.Zone);
}

static xcroncheckpointgroupview groupExpect(xcroncheckpointgroup* S,uint64 Generation,uint32 Slots,
    const xcroncheckpoint* Records,uint32 Count)
{
    xcroncheckpointgroupview V;
    testRequire(xcronCheckpointGroupRead(S,&V)==(Generation?XCRON_CHECKPOINT_STORE_OK:XCRON_CHECKPOINT_STORE_EMPTY) &&
                V.Info.Generation==Generation && V.Info.ValidSlots==Slots && V.Count==Count &&
                (Count?V.Records && !memcmp(V.Records,Records,(size_t)Count*sizeof(*Records)):V.Records==NULL),
                "checkpoint group exactly matches one committed generation");
    return V;
}

static void groupReplaceRecord(xroot Root,uint64 Offset,const xcroncheckpoint* Record)
{
    const xfileoptions Options={XFILE_READ|XFILE_WRITE,0600,XFILE_SHARE_ALL};
    xfile F=xrtRootFileOpen(Root,GroupPath,&Options);
    testRequire(F && xrtWriteAtFull(F,Offset,Record,sizeof(*Record),NULL) && xrtFlush(F) && xrtClose(F),
                "persist foreign valid record to simulate mixed sectors");
}

static void testCheckpointGroups(xroot Root,const xcroncheckpoint* First,const xcroncheckpoint* Second)
{
    xallocationbudget* Budget=NULL;
    xallocationscope Scope={0};
    xcroncheckpointgroup *S=NULL,*Other=NULL;
    xcroncheckpointkey Key={100,200};
    testRequire(xcronCheckpointGetKey(First,&Key) && Key.Job==1 && Key.Revision==1,"inspect valid checkpoint identity");
    xcroncheckpoint Bad=First[0];
    Bad.Bytes[40]^=1;
    testRequire(!xcronCheckpointGetKey(&Bad,&Key) && Key.Job==1 && Key.Revision==1,"invalid key read preserves output");
    xrtClearError();
    testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"enter group allocation refusal");
    testRequire(xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,true,&S)==XCRON_CHECKPOINT_STORE_MEMORY && !S,
                "allocation refusal precedes file creation and output publication");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave group allocation refusal");
    xrtAllocationBudgetFree(Budget);
    xrtClearError();

    testRequire(xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,true,&S)==XCRON_CHECKPOINT_STORE_OK,"create group");
    groupExpect(S,0,1,NULL,0);
    testRequire(xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,false,&Other)==XCRON_CHECKPOINT_STORE_LOCK &&
                !Other && xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,true,&Other)==XCRON_CHECKPOINT_STORE_IO &&
                !Other,"independent contenders cannot reopen active store");
    const xcroncheckpoint Duplicate[]={First[0],First[0]},Unordered[]={First[1],First[0]};
    testRequire(xcronCheckpointGroupCommit(S,0,Duplicate,2)==XCRON_CHECKPOINT_STORE_CORRUPT &&
                xcronCheckpointGroupCommit(S,0,Unordered,2)==XCRON_CHECKPOINT_STORE_CORRUPT &&
                xcronCheckpointGroupCommit(S,0,&Bad,1)==XCRON_CHECKPOINT_STORE_CORRUPT &&
                xcronCheckpointGroupCommit(S,0,NULL,1)==XCRON_CHECKPOINT_STORE_ARGUMENT &&
                xcronCheckpointGroupCommit(S,0,First,GROUP_CAPACITY+1)==XCRON_CHECKPOINT_STORE_LIMIT,
                "invalid records and counts leave generation untouched");
    groupExpect(S,0,1,NULL,0);
    testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"guard warmed commits");
    testRequire(xcronCheckpointGroupCommit(S,0,First,GROUP_CAPACITY)==XCRON_CHECKPOINT_STORE_OK,"commit full multi-page group without allocation");
    xcroncheckpointgroupview V=groupExpect(S,1,2,First,GROUP_CAPACITY);
    testRequire(xcronCheckpointGroupCommit(S,1,V.Records,V.Count)==XCRON_CHECKPOINT_STORE_OK,"current borrowed view can be recommitted");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave warmed commit budget");
    xrtAllocationBudgetFree(Budget);xrtClearError();
    testRequire(xcronCheckpointGroupCommit(S,1,Second,GROUP_CAPACITY)==XCRON_CHECKPOINT_STORE_STALE,"stale commit is refused");
    groupExpect(S,2,2,First,GROUP_CAPACITY);
    testRequire(xcronCheckpointGroupCommit(S,2,First,GROUP_CAPACITY)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointGroupCommit(S,3,Second,GROUP_CAPACITY)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointGroupClose(S)==XCRON_CHECKPOINT_STORE_OK,"publish adjacent full images");
    testRequire(xcronCheckpointGroupOpen(Root,GroupPath,GroupId+1,GROUP_CAPACITY,false,&Other)==XCRON_CHECKPOINT_STORE_MISMATCH &&
                !Other && xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY-1,false,&Other)==XCRON_CHECKPOINT_STORE_MISMATCH &&
                !Other,"reject wrong persistent group identity and capacity");
    testRequire(xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,false,&S)==XCRON_CHECKPOINT_STORE_OK,"recover newest full image");
    groupExpect(S,4,2,Second,GROUP_CAPACITY);
    testRequire(xcronCheckpointGroupClose(S)==XCRON_CHECKPOINT_STORE_OK,"close before simulated damage");
    groupReplaceRecord(Root,GROUP_HEADER+23*sizeof(*First),&First[23]);
    testRequire(xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,false,&S)==XCRON_CHECKPOINT_STORE_OK,
                "recover after a complete old record replaces a new record in newer slot");
    groupExpect(S,3,1,First,GROUP_CAPACITY);
    testRequire(xcronCheckpointGroupClose(S)==XCRON_CHECKPOINT_STORE_OK,"close fallback view");
    groupReplaceRecord(Root,GROUP_SLOT+GROUP_HEADER+19*sizeof(*Second),&Second[19]);
    testRequire(xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,false,&Other)==XCRON_CHECKPOINT_STORE_CORRUPT &&
                !Other,"two mixed generations remain corrupt");
    testRequire(xrtRootRemove(Root,GroupPath),"remove corrupt owned file");

    testRequire(xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,true,&S)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointGroupCommit(S,0,NULL,0)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointGroupClose(S)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,false,&S)==XCRON_CHECKPOINT_STORE_OK,
                "durably commit empty replacement group");
    groupExpect(S,1,2,NULL,0);
    testRequire(xcronCheckpointGroupClose(S)==XCRON_CHECKPOINT_STORE_OK && xrtRootRemove(Root,GroupPath),"remove empty owned file");
}

#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
typedef struct groupwritergate {
    xmutex Mutex;
    xcond Changed;
    unsigned Step;
    xcroncheckpointwriter* Writer;
    xcroncheckpointwriteticket Ticket;
} groupwritergate;

static void groupWriterGateAdvance(groupwritergate* G,unsigned Step)
{
    testRequire(xrtMutexLock(&G->Mutex),"lock writer test gate");
    G->Step=Step;
    testRequire(xrtCondBroadcast(&G->Changed) && xrtMutexUnlock(&G->Mutex),"advance writer test gate");
}

static void groupWriterGateWait(groupwritergate* G,unsigned Step)
{
    testRequire(xrtMutexLock(&G->Mutex),"lock writer test gate");
    while(G->Step<Step)
        testRequire(xrtCondWaitFor(&G->Changed,&G->Mutex,10000)==XWAIT_OK,"wait for writer test gate");
    testRequire(xrtMutexUnlock(&G->Mutex),"unlock writer test gate");
}

static void groupWriterGate(ptr Context)
{
    groupwritergate* G=Context;
    groupWriterGateAdvance(G,1);
    groupWriterGateWait(G,2);
    xcroncheckpointwriterreply R={.Generation=UINT64_MAX};
    testRequire(xcronCheckpointWriterCollect(G->Writer,G->Ticket,-1,&R)==XWAIT_ERROR &&
                xrtErrorKind(xrtGetError())==XERR_STATE && R.Generation==UINT64_MAX,"refuse to wait from the same executor");
    groupWriterGateAdvance(G,3);
    groupWriterGateWait(G,4);
}

static void groupWriterTest(xroot Root,const xcroncheckpoint* Records,bool Cancel)
{
    xcroncheckpointgroup* S=NULL;
    xcroncheckpointwriter* W=NULL;
    xexecutor* Pool=xrtExecutorCreate(&(xexecutorconfig){1,1,0});
    testRequire(Pool && xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,true,&S)==XCRON_CHECKPOINT_STORE_OK,
                "open fixture for async checkpoint writer");
    xallocationbudget* Budget=NULL;
    xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"deny writer allocation");
    testRequire(!xcronCheckpointWriterCreate(S,Pool,&W) && !W && xrtErrorKind(xrtGetError())==XERR_MEMORY,
                "failed writer creation transfers no store");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave denied writer allocation");
    groupExpect(S,0,1,NULL,0);
    testRequire(xcronCheckpointWriterCreate(S,Pool,&W),"writer owns the file lock");
    groupwritergate G={.Writer=W};
    testRequire(xrtMutexInit(&G.Mutex) && xrtCondInit(&G.Changed) &&
                xrtExecutorSubmit(Pool,groupWriterGate,&G,NULL,NULL),"occupy storage worker");
    groupWriterGateWait(&G,1);

    union {xcroncheckpoint Record;xcroncheckpointwriterreply Reply;} Borrowed={.Record=Records[0]};
    testRequire(xrtAllocationBudgetEnter(Budget,&Scope),"deny admission allocation");
    testRequire(xcronCheckpointWriterSubmit(W,0,&Borrowed.Record,1,&G.Ticket),"queue one borrowed record without allocation");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave denied admission allocation");
    xrtAllocationBudgetFree(Budget);
    groupWriterGateAdvance(&G,2);
    groupWriterGateWait(&G,3);
    xcroncheckpointwriteticket Extra=999;
    xcroncheckpointwriterreply Reply={.Generation=UINT64_MAX};
    testRequire(xcronCheckpointWriterCollect(W,G.Ticket,0,&Borrowed.Reply)==XWAIT_ERROR &&
                xrtErrorKind(xrtGetError())==XERR_ARGUMENT &&
                xcronCheckpointWriterCollect(W,G.Ticket,25,&Reply)==XWAIT_TIMEOUT && Reply.Generation==UINT64_MAX,
                "pending result preserves caller buffers");
    testRequire(!xcronCheckpointWriterSubmit(W,0,NULL,0,&Extra) && Extra==999 && xrtErrorKind(xrtGetError())==XERR_AGAIN &&
                xcronCheckpointWriterDestroy(W)==XCRON_CHECKPOINT_STORE_BUSY,
                "outstanding checkpoint keeps one ticket and the store");
    xcroncheckpointgroup* Other=NULL;
    testRequire(xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,false,&Other)==XCRON_CHECKPOINT_STORE_LOCK && !Other,
                "external opener cannot take the writer's store");
    xcroncheckpointwriter* Second=NULL;
    testRequire(xcronCheckpointGroupOpen(Root,"other.xcg",GroupId+1,1,true,&Other)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointWriterCreate(Other,Pool,&Second) &&
                !xcronCheckpointWriterSubmit(Second,0,Records,1,&Extra) && Extra==999 && xrtErrorKind(xrtGetError())==XERR_AGAIN &&
                xcronCheckpointWriterDestroy(Second)==XCRON_CHECKPOINT_STORE_OK && xrtRootRemove(Root,"other.xcg"),
                "native queue saturation leaves the other writer idle");

    if(Cancel){
        testRequire(xrtExecutorCancel(Pool),"cancel the queued checkpoint before it reaches the disk");
        testRequire(xcronCheckpointWriterDestroy(W)==XCRON_CHECKPOINT_STORE_BUSY,"discarded queue entry must be collected");
        testRequire(xcronCheckpointWriterCollect(W,G.Ticket,0,&Reply)==XWAIT_OK &&
                    Reply.Status==XCRON_CHECKPOINT_STORE_CANCELLED && !Reply.Generation && !Reply.Error,
                    "queued cancellation releases the borrow once");
        testRequire(!xcronCheckpointWriterSubmit(W,0,NULL,0,&Extra) && Extra==999 && xrtErrorKind(xrtGetError())==XERR_CLOSED,
                    "closed storage executor does not consume the next ticket");
    }
    groupWriterGateAdvance(&G,4);
    if(!Cancel){
        testRequire(xcronCheckpointWriterCollect(W,G.Ticket,10000,&Reply)==XWAIT_OK &&
                    Reply.Status==XCRON_CHECKPOINT_STORE_OK && Reply.Generation==1 && !Reply.Error,"collect native commit result");
        testRequire(xcronCheckpointWriterSubmit(W,0,Records,1,&Extra) && Extra!=G.Ticket &&
                    xcronCheckpointWriterCollect(W,G.Ticket,0,&Reply)==XWAIT_ERROR && Reply.Generation==1,
                    "stale ticket cannot consume a later result");
        testRequire(xcronCheckpointWriterCollect(W,Extra,10000,&Reply)==XWAIT_OK &&
                    Reply.Status==XCRON_CHECKPOINT_STORE_STALE && !Reply.Generation && Reply.Error &&
                    xrtErrorCode(Reply.Error)==XCRON_CHECKPOINT_STORE_STALE,"transfer the worker's typed store error");
        xrtErrorFree(Reply.Error);
        testRequire(xcronCheckpointWriterSubmit(W,1,Records,GROUP_CAPACITY,&Extra) &&
                    xcronCheckpointWriterCollect(W,Extra,10000,&Reply)==XWAIT_OK &&
                    Reply.Status==XCRON_CHECKPOINT_STORE_OK && Reply.Generation==2 && !Reply.Error,"retry with current generation");
    }
    testRequire(xcronCheckpointWriterDestroy(W)==XCRON_CHECKPOINT_STORE_OK && xrtExecutorClose(Pool) &&
                xrtExecutorWaitFor(Pool,10000)==XWAIT_OK && xrtExecutorDestroy(Pool),"retire writer and caller-owned executor");
    testRequire(xrtCondUnit(&G.Changed) && xrtMutexUnit(&G.Mutex),"retire writer test gate");
    testRequire(xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,false,&S)==XCRON_CHECKPOINT_STORE_OK,
                "reopen file after destroying writer");
    groupExpect(S,Cancel?0:2,Cancel?1:2,Records,Cancel?0:GROUP_CAPACITY);
    testRequire(xcronCheckpointGroupClose(S)==XCRON_CHECKPOINT_STORE_OK && xrtRootRemove(Root,GroupPath),"remove async store fixture");
}
#endif

#if !defined(XCRON_SINGLE_HEADER)
static unsigned GroupFault;
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
static groupwritergate* GroupFlushGate;
#endif

static bool groupTestWrite(xfile File,uint64 Offset,const void* Data,size_t Bytes,size_t* Written)
{
    if(GroupFault==1 || GroupFault==3){
        const size_t Prefix=GROUP_HEADER+XCRON_CHECKPOINT_BYTES;
        testRequire(Bytes>Prefix && xrtWriteAtFull(File,Offset,Data,Prefix,NULL) && xrtFlush(File),
                    "persist new header and entire first record only");
        if(GroupFault==3){puts("GROUP TORN");fflush(stdout);(void)getchar();}
        xrtSetErrorInfo(XERR_IO,"test.cron.group",1,"injected write failure");
        return false;
    }
    return xrtWriteAtFull(File,Offset,Data,Bytes,Written);
}

static bool groupTestFlush(xfile File)
{
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
    if(GroupFault==5 || GroupFault==6){
        groupWriterGateAdvance(GroupFlushGate,1);
        groupWriterGateWait(GroupFlushGate,2);
        if(GroupFault==6){
            xrtSetErrorInfo(XERR_IO,"test.cron.group",6,"injected async flush failure");
            return false;
        }
    }
#endif
    if(GroupFault==2 || GroupFault==4){
        if(GroupFault==4){puts("GROUP BEFORE FLUSH");fflush(stdout);(void)getchar();}
        xrtSetErrorInfo(XERR_IO,"test.cron.group",2,"injected flush failure");
        return false;
    }
    return xrtFlush(File);
}

#define xrtWriteAtFull groupTestWrite
#define xrtFlush groupTestFlush
#define xcronCheckpointGroupOpen xcronCheckpointGroupOpenFault
#define xcronCheckpointGroupRead xcronCheckpointGroupReadFault
#define xcronCheckpointGroupCommit xcronCheckpointGroupCommitFault
#define xcronCheckpointGroupClose xcronCheckpointGroupCloseFault
#include "../src/checkpoint_group/checkpoint_group.c"
#undef xcronCheckpointGroupClose
#undef xcronCheckpointGroupCommit
#undef xcronCheckpointGroupRead
#undef xcronCheckpointGroupOpen
#undef xrtFlush
#undef xrtWriteAtFull

static void testGroupFaults(xroot Root,const xcroncheckpoint* First,const xcroncheckpoint* Second)
{
    for(unsigned Pass=1;Pass<=2;++Pass){
        xcroncheckpointgroup* S=NULL;
        testRequire(xcronCheckpointGroupOpenFault(Root,GroupPath,GroupId,GROUP_CAPACITY,true,&S)==XCRON_CHECKPOINT_STORE_OK &&
                    xcronCheckpointGroupCommitFault(S,0,First,GROUP_CAPACITY)==XCRON_CHECKPOINT_STORE_OK,"prime fault store");
        GroupFault=Pass;
        testRequire(xcronCheckpointGroupCommitFault(S,1,Second,GROUP_CAPACITY)==XCRON_CHECKPOINT_STORE_UNCERTAIN,
                    "failed write or flush returns uncertain");
        xcroncheckpointgroupview V={.Count=UINT32_MAX,.Info={UINT64_MAX,UINT32_MAX}};
        testRequire(xcronCheckpointGroupReadFault(S,&V)==XCRON_CHECKPOINT_STORE_UNCERTAIN &&
                    V.Count==UINT32_MAX && V.Info.Generation==UINT64_MAX &&
                    xcronCheckpointGroupCommitFault(S,1,Second,GROUP_CAPACITY)==XCRON_CHECKPOINT_STORE_UNCERTAIN,
                    "uncertain group preserves output and refuses retry");
        GroupFault=0;
        testRequire(xcronCheckpointGroupCloseFault(S)==XCRON_CHECKPOINT_STORE_OK &&
                    xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,false,&S)==XCRON_CHECKPOINT_STORE_OK,
                    "reopen after uncertain write");
        groupExpect(S,Pass,Pass==1?1:2,Pass==1?First:Second,GROUP_CAPACITY);
        testRequire(xcronCheckpointGroupClose(S)==XCRON_CHECKPOINT_STORE_OK && xrtRootRemove(Root,GroupPath),"remove fault fixture");
    }
}
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
#define xcronCheckpointGroupCommit xcronCheckpointGroupCommitFault
#define xcronCheckpointWriterCreate xcronCheckpointWriterCreateFault
#define xcronCheckpointWriterSubmit xcronCheckpointWriterSubmitFault
#define xcronCheckpointWriterCollect xcronCheckpointWriterCollectFault
#define xcronCheckpointWriterDestroy xcronCheckpointWriterDestroyFault
#define xcronCheckpointWriterSubmitSignal xcronCheckpointWriterSubmitSignalFault
#define xcronCheckpointWriterOutside xcronCheckpointWriterOutsideFault
#define xcronCheckpointWriterCurrent xcronCheckpointWriterCurrentFault
#include "../src/checkpoint_group/checkpoint_writer.c"
#undef xcronCheckpointWriterCurrent
#undef xcronCheckpointWriterOutside
#undef xcronCheckpointWriterSubmitSignal
#undef xcronCheckpointWriterDestroy
#undef xcronCheckpointWriterCollect
#undef xcronCheckpointWriterSubmit
#undef xcronCheckpointWriterCreate
#undef xcronCheckpointGroupCommit

static void groupWriterFlush(const xroot Root,const xcroncheckpoint* Records,bool Fail)
{
    xcroncheckpointgroup* S=NULL;
    xcroncheckpointwriter* W=NULL;
    xexecutor* Pool=xrtExecutorCreate(&(xexecutorconfig){1,2,0});
    groupwritergate G={0};
    testRequire(Pool && xrtMutexInit(&G.Mutex) && xrtCondInit(&G.Changed) &&
                xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,true,&S)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointWriterCreateFault(S,Pool,&W),"start deterministic blocked-flush fixture");
    GroupFault=Fail?6:5;
    GroupFlushGate=&G;
    xcroncheckpointwriteticket Ticket=0;
    testRequire(xcronCheckpointWriterSubmitFault(W,0,Records,GROUP_CAPACITY,&Ticket),"submit blocked-flush commit");
    groupWriterGateWait(&G,1);
    xcroncheckpointwriterreply Reply={.Generation=UINT64_MAX};
    testRequire(xrtExecutorCancel(Pool) && xcronCheckpointWriterCollectFault(W,Ticket,0,&Reply)==XWAIT_TIMEOUT &&
                Reply.Generation==UINT64_MAX && xcronCheckpointWriterDestroyFault(W)==XCRON_CHECKPOINT_STORE_BUSY,
                "running commit and borrow survive executor cancellation until physical flush completes");
    groupWriterGateAdvance(&G,2);
    testRequire(xcronCheckpointWriterCollectFault(W,Ticket,10000,&Reply)==XWAIT_OK &&
                (Fail?(Reply.Status==XCRON_CHECKPOINT_STORE_UNCERTAIN && !Reply.Generation &&
                       xrtErrorKind(Reply.Error)==XERR_IO && xrtErrorCode(Reply.Error)==6):
                      (Reply.Status==XCRON_CHECKPOINT_STORE_OK && Reply.Generation==1 && !Reply.Error)),
                "report the running commit's actual flush outcome");
    xrtErrorFree(Reply.Error);
    GroupFlushGate=NULL;
    GroupFault=0;
    testRequire(xcronCheckpointWriterDestroyFault(W)==XCRON_CHECKPOINT_STORE_OK &&
                xrtExecutorWaitFor(Pool,10000)==XWAIT_OK && xrtExecutorDestroy(Pool),"retire blocked-flush fixture");
    testRequire(xrtCondUnit(&G.Changed) && xrtMutexUnit(&G.Mutex),"retire flush barrier");
    testRequire(xcronCheckpointGroupOpen(Root,GroupPath,GroupId,GROUP_CAPACITY,false,&S)==XCRON_CHECKPOINT_STORE_OK,
                "reopen to resolve the generation");
    groupExpect(S,1,2,Records,GROUP_CAPACITY);
    testRequire(xcronCheckpointGroupClose(S)==XCRON_CHECKPOINT_STORE_OK && xrtRootRemove(Root,GroupPath),"remove flush fixture");
}
#endif

static int groupInterrupt(cstr Directory,unsigned Mode)
{
    xroot Root=xrtRootOpen(Directory);
    xcroncheckpointgroup* S=NULL;
    xcroncheckpoint First[2],Second[2];
    groupRecords(2,First,Second);
    testRequire(Root && xcronCheckpointGroupOpenFault(Root,GroupPath,GroupId,GROUP_CAPACITY,false,&S)==XCRON_CHECKPOINT_STORE_OK,
                "open initialized process fixture");
    groupExpect(S,1,2,First,2);
    GroupFault=Mode;
    (void)xcronCheckpointGroupCommitFault(S,1,Second,2);
    return 3;
}
#endif

int main(int Argc,char** Argv)
{
#if !defined(XCRON_SINGLE_HEADER)
    if(Argc==3 && !strcmp(Argv[1],"interrupt-torn"))return groupInterrupt(Argv[2],3);
    if(Argc==3 && !strcmp(Argv[1],"interrupt-flush"))return groupInterrupt(Argv[2],4);
#else
    (void)Argv;
#endif
    if(Argc!=1)return 2;
    cstr Base=getenv("TMPDIR");
#if defined(_WIN32)
    if(!Base || !*Base)Base=getenv("TEMP");
#else
    if(!Base || !*Base)Base="/tmp";
#endif
    if(!Base || !*Base)Base=".";
    char Name[96];
    snprintf(Name,sizeof(Name),"xcron-group-test-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
    xroot Parent=xrtRootOpen(Base);
    testRequire(Parent && xrtRootDirCreate(Parent,Name,0700),"create owned group test directory");
    xroot Root=xrtRootOpenIn(Parent,Name);
    testRequire(Root!=NULL,"open group test root");
    xcroncheckpoint First[GROUP_CAPACITY],Second[GROUP_CAPACITY];
    groupRecords(GROUP_CAPACITY,First,Second);
    testCheckpointGroups(Root,First,Second);
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
    groupWriterTest(Root,First,false);
    groupWriterTest(Root,Second,true);
#endif
#if !defined(XCRON_SINGLE_HEADER)
    testGroupFaults(Root,First,Second);
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
    groupWriterFlush(Root,First,false);
    groupWriterFlush(Root,Second,true);
#endif
#endif
    testRequire(xrtRootClose(Root) && xrtRootRemove(Parent,Name) && xrtRootClose(Parent),"remove owned checkpoint group test directory");
    puts("cron checkpoint group tests passed");
    return 0;
}
