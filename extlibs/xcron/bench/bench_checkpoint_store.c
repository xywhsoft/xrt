#include <xcron/checkpoint_store.h>
#include <xrt/memory.h>
#include <xrt/path.h>
#include <stdio.h>
#include <stdlib.h>

static int compareTimes(const void* A,const void* B)
{
    double X=*(const double*)A,Y=*(const double*)B;
    return (X>Y)-(X<Y);
}

int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):1000;
    if(Argc>2 || !Count || Count>100000)return 2;
    double* Samples=malloc(Count*sizeof(*Samples));
    if(!Samples)return 3;
    cstr Base=getenv("TMPDIR");
#if defined(_WIN32)
    if(!Base || !*Base)Base=getenv("TEMP");
#else
    if(!Base || !*Base)Base="/tmp";
#endif
    if(!Base || !*Base)Base=".";
    xroot Parent=xrtRootOpen(Base);
    char Fresh[96];
    snprintf(Fresh,sizeof(Fresh),"xcron-store-bench-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
    if(!Parent || !xrtRootDirCreate(Parent,Fresh,0700))return 3;
    str Path=xrtPathJoin(Base,Fresh);
    xroot Root=Path?xrtRootOpen(Path):NULL;
    xrtFree(Path);
    const xcroncheckpointkey Key={1,1};
    const xcronschedulerconfig Limits={1,1,8,0,65536};
    xcronjobconfig Config={.End=60000,.RunningLimit=1};
    xcronscheduler* S=NULL;xcronjobid Job;
    xcroncheckpointstore* Store=NULL;
    xcroncheckpoint Record;
    if(!Root || !xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Config.Zone,NULL) ||
       !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Config.Expression,NULL) ||
       !xcronSchedulerCreate(&Limits,&S,NULL) || !xcronSchedulerAdd(S,&Config,&Job,NULL) ||
       !xcronSchedulerSaveCheckpoint(S,Job,Key,&Record,NULL) ||
       xcronCheckpointStoreOpen(Root,"bench.xcs",Key,true,&Store)!=XCRON_CHECKPOINT_STORE_OK)return 3;

    double Begin=xrtTimer();
    for(size_t I=0;I<Count;++I){
        double Start=xrtTimer();
        if(xcronCheckpointStoreCommit(Store,I,&Record)!=XCRON_CHECKPOINT_STORE_OK)return 4;
        Samples[I]=(xrtTimer()-Start)*1000000;
    }
    double Elapsed=xrtTimer()-Begin;
    xcronretired Retired;bool Found;
    if(xcronCheckpointStoreClose(Store)!=XCRON_CHECKPOINT_STORE_OK || !xrtRootRemove(Root,"bench.xcs") ||
       !xrtRootClose(Root) || !xrtRootRemove(Parent,Fresh) || !xrtRootClose(Parent) ||
       !xcronSchedulerClose(S,NULL) || !xcronSchedulerReap(S,&Retired,&Found,NULL) ||
       !Found || !xcronSchedulerDestroy(S,NULL) || Elapsed<=0)return 5;
    xcronZoneRelease(Config.Zone);
    qsort(Samples,Count,sizeof(*Samples),compareTimes);
    printf("commits_per_sec: %.3f\ncommit_p50_us: %.3f\ncommit_p99_us: %.3f\ncommit_p999_us: %.3f\n",
           Count/Elapsed,Samples[(Count+1)/2-1],Samples[(Count*99+99)/100-1],Samples[(Count*999+999)/1000-1]);
    free(Samples);
    return 0;
}
