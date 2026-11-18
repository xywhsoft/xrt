#include <xcron/checkpoint_writer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int groupCompareTimes(const void* A,const void* B)
{
    double X=*(const double*)A,Y=*(const double*)B;
    return (X>Y)-(X<Y);
}

int main(int Argc,char** Argv)
{
    size_t Commits=Argc>1?(size_t)strtoul(Argv[1],NULL,10):200;
    uint32 Count=Argc>2?(uint32)strtoul(Argv[2],NULL,10):64;
    bool Async=false;
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
    Async=Argc==4 && !strcmp(Argv[3],"async");
#endif
    if((Argc>3 && !Async) || !Commits || Commits>100000 || !Count || Count>1024)return 2;
    double* Samples=malloc(Commits*sizeof(*Samples));
    xcroncheckpoint* Records=malloc((size_t)Count*sizeof(*Records));
    if(!Samples || !Records)return 3;
    cstr Base=getenv("TMPDIR");
#if defined(_WIN32)
    if(!Base || !*Base)Base=getenv("TEMP");
#else
    if(!Base || !*Base)Base="/tmp";
#endif
    if(!Base || !*Base)Base=".";
    char Name[96],File[128];
    snprintf(Name,sizeof(Name),"xcron-group-bench-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
    snprintf(File,sizeof(File),"%s/bench.xcg",Name);
    xroot Parent=xrtRootOpen(Base);
    if(!Parent || !xrtRootDirCreate(Parent,Name,0700))return 3;
    xroot Root=xrtRootOpenIn(Parent,Name);
    const xcronschedulerconfig Limits={1,1,16,0,65536};
    xcronjobconfig Job={.Begin=1000,.End=60000,.RunningLimit=1};
    xcronscheduler* S=NULL;
    xcronjobid Id;
    xcroncheckpointgroup* Store=NULL;
    if(!Root || !xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Job.Zone,NULL) ||
       !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Job.Expression,NULL) ||
       !xcronSchedulerCreate(&Limits,&S,NULL) || !xcronSchedulerAdd(S,&Job,&Id,NULL) ||
       xcronCheckpointGroupOpen(Root,"bench.xcg",99,Count,true,&Store)!=XCRON_CHECKPOINT_STORE_OK)return 3;
    for(uint32 I=0;I<Count;++I)
        if(!xcronSchedulerSaveCheckpoint(S,Id,(xcroncheckpointkey){I+1,1},&Records[I],NULL))return 3;

#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
    xexecutor* Pool=NULL;
    xcroncheckpointwriter* Writer=NULL;
    if(Async){
        Pool=xrtExecutorCreate(&(xexecutorconfig){1,2,0});
        if(!Pool || !xcronCheckpointWriterCreate(Store,Pool,&Writer))return 3;
    }
#endif
    double Start=xrtTimer();
    for(size_t I=0;I<Commits;++I){
        double Begin=xrtTimer();
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
        if(Async){
            xcroncheckpointwriteticket Ticket;
            xcroncheckpointwriterreply Reply;
            if(!xcronCheckpointWriterSubmit(Writer,I,Records,Count,&Ticket) ||
               xcronCheckpointWriterCollect(Writer,Ticket,-1,&Reply)!=XWAIT_OK ||
               Reply.Status!=XCRON_CHECKPOINT_STORE_OK || Reply.Generation!=I+1 || Reply.Error)return 4;
        }else
#endif
        if(xcronCheckpointGroupCommit(Store,I,Records,Count)!=XCRON_CHECKPOINT_STORE_OK)return 4;
        Samples[I]=(xrtTimer()-Begin)*1000000;
    }
    double Elapsed=xrtTimer()-Start;
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
    if(Async && (xcronCheckpointWriterDestroy(Writer)!=XCRON_CHECKPOINT_STORE_OK ||
       !xrtExecutorClose(Pool) || xrtExecutorWait(Pool)!=XWAIT_OK || !xrtExecutorDestroy(Pool) ||
       xcronCheckpointGroupOpen(Root,"bench.xcg",99,Count,false,&Store)!=XCRON_CHECKPOINT_STORE_OK))return 5;
#endif
    xcroncheckpointgroupview View;
    xcronretired Retired;
    bool Found;
    if(xcronCheckpointGroupRead(Store,&View)!=XCRON_CHECKPOINT_STORE_OK || View.Info.Generation!=Commits ||
       View.Count!=Count || xcronCheckpointGroupClose(Store)!=XCRON_CHECKPOINT_STORE_OK ||
       !xrtRootClose(Root) || !xrtRootRemove(Parent,File) || !xrtRootRemove(Parent,Name) || !xrtRootClose(Parent) ||
       !xcronSchedulerClose(S,NULL) || !xcronSchedulerReap(S,&Retired,&Found,NULL) || !Found ||
       !xcronSchedulerDestroy(S,NULL) || Elapsed<=0)return 5;
    xcronZoneRelease(Job.Zone);
    qsort(Samples,Commits,sizeof(*Samples),groupCompareTimes);
    printf("groups_per_sec: %.3f\nrecords_per_sec: %.3f\ngroup_p50_us: %.3f\ngroup_p95_us: %.3f\ngroup_p99_us: %.3f\n",
           Commits/Elapsed,(double)Commits*Count/Elapsed,Samples[(Commits+1)/2-1],
           Samples[(Commits*95+99)/100-1],Samples[(Commits*99+99)/100-1]);
    free(Records);
    free(Samples);
    return 0;
}
