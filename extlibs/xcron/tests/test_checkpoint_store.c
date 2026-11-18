#include "test.h"
#include <xcron/checkpoint_store.h>
#include <xrt/crc32.h>
#include <xrt/memory.h>
#include <xrt/path.h>
#include <stdlib.h>
#include <string.h>

static const xcroncheckpointkey StoreKey={123,456};
static const char* StoreName="checkpoint.bin";

static void storeRecords(xcroncheckpoint* First,xcroncheckpoint* Second)
{
    const xcronschedulerconfig Limits={1,1,8,0,65536};
    xcronjobconfig C={.RunningLimit=1,.End=3000};
    xcronscheduler* S=NULL;xcronjobid Job;
    testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&C.Zone,NULL) &&
                xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&C.Expression,NULL) &&
                xcronSchedulerCreate(&Limits,&S,NULL) && xcronSchedulerAdd(S,&C,&Job,NULL) &&
                xcronSchedulerSaveCheckpoint(S,Job,StoreKey,First,NULL),"first durable record");
    xcronrun Run;xcronpoll Poll;xcronretired R;bool Found;
    testRequire(xcronSchedulerPoll(S,(xcronclock){0,0},&Run,1,&Poll,NULL) && Poll.Count==1 &&
                xcronSchedulerComplete(S,Run.Id,XCRON_RUN_SUCCEEDED,NULL) &&
                xcronSchedulerSaveCheckpoint(S,Job,StoreKey,Second,NULL) &&
                xcronSchedulerClose(S,NULL) && xcronSchedulerReap(S,&R,&Found,NULL) &&
                Found && xcronSchedulerDestroy(S,NULL),"second durable record");
    xcronZoneRelease(C.Zone);
}

static void storeDamage(xroot Root,cstr Path,uint64 Offset)
{
    const xfileoptions Opt={XFILE_READ|XFILE_WRITE,0600,XFILE_SHARE_ALL};
    xfile File=xrtRootFileOpen(Root,Path,&Opt);
    uint8 B;
    testRequire(File && xrtReadAtFull(File,Offset,&B,1,NULL),"read raw store byte");
    B^=0x80;
    testRequire(xrtWriteAtFull(File,Offset,&B,1,NULL) && xrtFlush(File) && xrtClose(File),"damage owned slot");
}

static void storeExpect(xcroncheckpointstore* S,uint64 Generation,uint32 Valid,const xcroncheckpoint* Expected)
{
    xcroncheckpoint R;
    memset(&R,0xa5,sizeof(R));
    xcroncheckpoint Original=R;
    xcroncheckpointstoreinfo Info;
    testRequire(xcronCheckpointStoreRead(S,&R,&Info)==(Generation?XCRON_CHECKPOINT_STORE_OK:XCRON_CHECKPOINT_STORE_EMPTY) &&
                Info.Generation==Generation && Info.ValidSlots==Valid &&
                !memcmp(&R,Expected?Expected:&Original,sizeof(R)),"recover exact checkpoint image");
}

static void checkpointStore(xroot Root,const xcroncheckpoint* First,const xcroncheckpoint* Second)
{
    xcroncheckpointstore *S=NULL,*Other=NULL;
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"enter store allocation refusal");
    testRequire(xcronCheckpointStoreOpen(Root,StoreName,StoreKey,true,&S)==XCRON_CHECKPOINT_STORE_MEMORY && !S,
                "store allocation refusal publishes no handle");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave store allocation refusal");
    xrtAllocationBudgetFree(Budget);xrtClearError();

    testRequire(xcronCheckpointStoreOpen(Root,StoreName,StoreKey,true,&S)==XCRON_CHECKPOINT_STORE_OK,"create empty store");
    storeExpect(S,0,1,NULL);
    testRequire(xcronCheckpointStoreOpen(Root,StoreName,StoreKey,false,&Other)==XCRON_CHECKPOINT_STORE_LOCK && !Other,
                "independent open respects the held native lock");
    testRequire(xcronCheckpointStoreOpen(Root,StoreName,StoreKey,true,&Other)==XCRON_CHECKPOINT_STORE_IO && !Other,
                "exclusive create respects existing store");
    testRequire(xcronCheckpointStoreCommit(S,0,First)==XCRON_CHECKPOINT_STORE_OK,"commit generation one");
    xcroncheckpoint Damaged=*Second;Damaged.Bytes[40]^=1;
    testRequire(xcronCheckpointStoreCommit(S,1,&Damaged)==XCRON_CHECKPOINT_STORE_CORRUPT &&
                xcronCheckpointStoreCommit(S,0,Second)==XCRON_CHECKPOINT_STORE_STALE,"reject corrupt and stale preflight");
    storeExpect(S,1,2,First);
    testRequire(xcronCheckpointStoreCommit(S,1,Second)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointStoreClose(S)==XCRON_CHECKPOINT_STORE_OK,"commit generation two");
    testRequire(xcronCheckpointStoreOpen(Root,StoreName,StoreKey,false,&S)==XCRON_CHECKPOINT_STORE_OK,"reopen committed store");
    storeExpect(S,2,2,Second);
    testRequire(xcronCheckpointStoreClose(S)==XCRON_CHECKPOINT_STORE_OK,"close before offline damage");

    testRequire(xcronCheckpointStoreOpen(Root,StoreName,(xcroncheckpointkey){123,457},false,&Other)==XCRON_CHECKPOINT_STORE_MISMATCH &&
                !Other,"wrong store identity publishes nothing");
    storeDamage(Root,StoreName,212);
    testRequire(xcronCheckpointStoreOpen(Root,StoreName,StoreKey,false,&S)==XCRON_CHECKPOINT_STORE_OK,"recover the previous slot after tear");
    storeExpect(S,1,1,First);
    testRequire(xcronCheckpointStoreCommit(S,1,Second)==XCRON_CHECKPOINT_STORE_OK,"replace a damaged slot");
    storeExpect(S,2,2,Second);
    testRequire(xcronCheckpointStoreClose(S)==XCRON_CHECKPOINT_STORE_OK,"close repaired store");
    storeDamage(Root,StoreName,212);
    storeDamage(Root,StoreName,4096+212);
    testRequire(xcronCheckpointStoreOpen(Root,StoreName,StoreKey,false,&Other)==XCRON_CHECKPOINT_STORE_CORRUPT && !Other,
                "two damaged slots fail without reinitializing");
    testRequire(xrtRootRemove(Root,StoreName),"remove owned store");
}

#if !defined(XCRON_SINGLE_HEADER)
static unsigned xcronStoreFailure;

static bool storeFaultWrite(xfile File,uint64 Offset,const void* Data,size_t Count,size_t* Written)
{
    if(xcronStoreFailure==1 || xcronStoreFailure==3){
        testRequire(Count>64 && xrtWriteAtFull(File,Offset,Data,64,NULL) && xrtFlush(File),"persist torn prefix");
        if(xcronStoreFailure==3){puts("TORN");fflush(stdout);(void)getchar();}
        xrtSetErrorInfo(XERR_STATE,"test.cron.store",7,"injected short write");
        return false;
    }
    return xrtWriteAtFull(File,Offset,Data,Count,Written);
}
static bool storeFaultFlush(xfile File)
{
    if(xcronStoreFailure==2 || xcronStoreFailure==4){
        if(xcronStoreFailure==4){puts("BEFORE FLUSH");fflush(stdout);(void)getchar();}
        xrtSetErrorInfo(XERR_STATE,"test.cron.store",8,"injected flush failure");
        return false;
    }
    return xrtFlush(File);
}

#define xrtWriteAtFull storeFaultWrite
#define xrtFlush storeFaultFlush
#define xcronCheckpointStoreOpen xcronCheckpointStoreOpenFault
#define xcronCheckpointStoreRead xcronCheckpointStoreReadFault
#define xcronCheckpointStoreCommit xcronCheckpointStoreCommitFault
#define xcronCheckpointStoreClose xcronCheckpointStoreCloseFault
#include "../src/checkpoint_store/checkpoint_store.c"
#undef xcronCheckpointStoreClose
#undef xcronCheckpointStoreCommit
#undef xcronCheckpointStoreRead
#undef xcronCheckpointStoreOpen
#undef xrtFlush
#undef xrtWriteAtFull

static void checkpointStoreFaults(xroot Root,const xcroncheckpoint* First,const xcroncheckpoint* Second)
{
    for(unsigned Pass=1;Pass<=2;++Pass){
        xcroncheckpointstore* S=NULL;
        testRequire(xcronCheckpointStoreOpenFault(Root,StoreName,StoreKey,true,&S)==XCRON_CHECKPOINT_STORE_OK &&
                    xcronCheckpointStoreCommitFault(S,0,First)==XCRON_CHECKPOINT_STORE_OK,"prime fault probe");
        xcronStoreFailure=Pass;
        testRequire(xcronCheckpointStoreCommitFault(S,1,Second)==XCRON_CHECKPOINT_STORE_UNCERTAIN,"native failure makes outcome uncertain");
        xcroncheckpointstoreinfo Info={UINT64_MAX,UINT32_MAX};
        testRequire(xcronCheckpointStoreReadFault(S,NULL,&Info)==XCRON_CHECKPOINT_STORE_UNCERTAIN &&
                    Info.Generation==UINT64_MAX && Info.ValidSlots==UINT32_MAX &&
                    xcronCheckpointStoreCommitFault(S,1,Second)==XCRON_CHECKPOINT_STORE_UNCERTAIN,"poisoned store cannot publish or retry");
        xcronStoreFailure=0;
        testRequire(xcronCheckpointStoreCloseFault(S)==XCRON_CHECKPOINT_STORE_OK &&
                    xcronCheckpointStoreOpen(Root,StoreName,StoreKey,false,&S)==XCRON_CHECKPOINT_STORE_OK,"reopen after native failure");
        storeExpect(S,Pass,Pass==1?1:2,Pass==1?First:Second);
        testRequire(xcronCheckpointStoreClose(S)==XCRON_CHECKPOINT_STORE_OK && xrtRootRemove(Root,StoreName),"retire owned fault image");
    }
}
static int checkpointStoreInterrupted(cstr Directory,unsigned Mode)
{
    xroot Root=xrtRootOpen(Directory);
    const xcroncheckpointkey Key={1,1};
    const xcronschedulerconfig Limits={1,1,8,0,65536};
    xcroncheckpointstore* Store=NULL;
    xcroncheckpoint Record;xcroncheckpointstoreinfo Info;
    testRequire(Root && xcronCheckpointStoreOpenFault(Root,"job.xcs",Key,false,&Store)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointStoreReadFault(Store,&Record,&Info)==XCRON_CHECKPOINT_STORE_OK &&
                Info.Generation==1,"open initialized crash fixture");
    xcronjobconfig Config={.End=60000,.RunningLimit=1};
    xcronscheduler* S=NULL;xcronjobid Job;
    xcronrun Run;xcronpoll Poll;
    testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Config.Zone,NULL) &&
                xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Config.Expression,NULL) &&
                xcronSchedulerCreate(&Limits,&S,NULL) &&
                xcronSchedulerRestoreCheckpoint(S,&Config,Key,(xbytesview){Record.Bytes,sizeof(Record.Bytes)},&Job,NULL) &&
                xcronSchedulerPoll(S,(xcronclock){1000,1},&Run,1,&Poll,NULL) && Poll.Count==1 && Run.ScheduledAt==1000 &&
                xcronSchedulerComplete(S,Run.Id,XCRON_RUN_SUCCEEDED,NULL) &&
                xcronSchedulerSaveCheckpoint(S,Job,Key,&Record,NULL),"prepare next valid generation");
    xcronStoreFailure=Mode;
    (void)xcronCheckpointStoreCommitFault(Store,Info.Generation,&Record);
    return 3; /* The test driver terminates the paused process. */
}
#endif

int main(int Argc,char** Argv)
{
#if !defined(XCRON_SINGLE_HEADER)
    if(Argc==3 && !strcmp(Argv[1],"interrupt-torn"))return checkpointStoreInterrupted(Argv[2],3);
    if(Argc==3 && !strcmp(Argv[1],"interrupt-flush"))return checkpointStoreInterrupted(Argv[2],4);
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
    xroot Parent=xrtRootOpen(Base);
    char Name[96];
    snprintf(Name,sizeof(Name),"xcron-cp-store-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
    testRequire(Parent && xrtRootDirCreate(Parent,Name,0700),"create owned checkpoint store test directory");
    str Path=xrtPathJoin(Base,Name);
    testRequire(Path!=NULL,"test directory path");
    xroot Root=xrtRootOpen(Path);
    xrtFree(Path);
    testRequire(Root!=NULL,"test directory root");
    xcroncheckpoint First,Second;
    storeRecords(&First,&Second);
    checkpointStore(Root,&First,&Second);
#if !defined(XCRON_SINGLE_HEADER)
    checkpointStoreFaults(Root,&First,&Second);
#endif
    testRequire(xrtRootClose(Root) && xrtRootRemove(Parent,Name) && xrtRootClose(Parent),"cleanup checkpoint fixtures");
    puts("cron checkpoint store tests passed");
    return 0;
}
