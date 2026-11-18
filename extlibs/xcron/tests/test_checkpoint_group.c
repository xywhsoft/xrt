#include "test.h"
#include <xcron/checkpoint_group.h>
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

#if !defined(XCRON_SINGLE_HEADER)
static unsigned GroupFault;

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
#if !defined(XCRON_SINGLE_HEADER)
    testGroupFaults(Root,First,Second);
#endif
    testRequire(xrtRootClose(Root) && xrtRootRemove(Parent,Name) && xrtRootClose(Parent),"remove owned checkpoint group test directory");
    puts("cron checkpoint group tests passed");
    return 0;
}
