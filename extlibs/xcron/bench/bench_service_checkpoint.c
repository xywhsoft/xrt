#include <xcron/service_checkpoint.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool checkpointBenchClock(xcronclock* Out,ptr Context)
{
    (void)Context;
    *Out=(xcronclock){0,0};
    return true;
}

static xcronrunresult checkpointBenchRun(const xcronrun* Run,const xcronexecution* Token,ptr Context)
{
    (void)Run;(void)Token;(void)Context;
    return XCRON_RUN_FAILED;
}

static int compareCheckpointSamples(const void* A,const void* B)
{
    double X=*(const double*)A,Y=*(const double*)B;
    return (X>Y)-(X<Y);
}

int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):10000;
    bool Pause=Argc==3 && !strcmp(Argv[2],"pause");
    if((Argc>2 && !Pause) || !Count || Count>200000)return 2;
    double* Samples=malloc(Count*sizeof(*Samples));
    if(!Samples)return 3;
    xexecutor* Pool=xrtExecutorCreate(&(xexecutorconfig){1,2,0});
    const xcronserviceconfig C={
        .Scheduler={1,1,16,0,65536},.Executor=Pool,.Proc=checkpointBenchRun,.Clock=checkpointBenchClock,
        .CommandLimit=1,.CompletionLimit=1,.BatchLimit=1,.CommandWorkLimit=1,.ResampleMilliseconds=50,
        .RetryIntervalMilliseconds=1,.RetryAttempts=1,.BytesLimit=262144,.AdapterBytesLimit=65536
    };
    xcronjobconfig J={.Begin=1000,.End=3000,.RunningLimit=1};
    xcronservice* S=NULL;
    xcronreply Reply;
    xcronrequestid Q;
    if(!Pool || !xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&J.Zone,NULL) ||
       !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&J.Expression,NULL) ||
       !xcronServiceCreateCheckpointed(&C,&S,NULL) || !xcronServiceAddAsync(S,&J,&Q,NULL) ||
       xcronServiceCollect(S,Q,10000,&Reply,NULL)!=XWAIT_OK || Reply.Code || !Reply.Job || Reply.Error)return 4;
    xcronjobid Job=Reply.Job;
    xcronZoneRelease(J.Zone);
    const xcroncheckpointkey Key={1,1};
    xcroncheckpoint Record;
    double Begin=xrtTimer();
    for(size_t I=0;I<Count;++I){
        double Start=xrtTimer();
        if(Pause){
            bool Hold=(I&1)==0;
            if(!xcronServiceSetPausedAsync(S,Job,Hold,&Q,NULL) ||
               xcronServiceCollect(S,Q,10000,&Reply,NULL)!=XWAIT_OK || Reply.Code || Reply.Job!=Job ||
               Reply.Error || Reply.Kind!=(Hold?XCRON_REQUEST_PAUSE:XCRON_REQUEST_RESUME))return 5;
        }else if(!xcronServiceSaveCheckpointAsync(S,Job,Key,&Q,NULL) ||
                 xcronServiceCollectCheckpoint(S,Q,10000,&Reply,&Record,NULL)!=XWAIT_OK || Reply.Code ||
                 Reply.Job!=Job || Reply.Error || Record.Bytes[0]!='X')return 5;
        Samples[I]=(xrtTimer()-Start)*1000000;
    }
    double Elapsed=xrtTimer()-Begin;
    xcronretired Retired;
    bool Found;
    if(!xcronServiceClose(S,false,NULL) || xcronServiceWaitFor(S,10000,NULL)!=XWAIT_OK ||
       !xcronServiceReapStopped(S,&Retired,&Found,NULL) || !Found || Retired.Id!=Job ||
       !xcronServiceDestroy(S,NULL) || !xrtExecutorClose(Pool) || xrtExecutorWaitFor(Pool,10000)!=XWAIT_OK ||
       !xrtExecutorDestroy(Pool) || Elapsed<=0)return 6;
    qsort(Samples,Count,sizeof(*Samples),compareCheckpointSamples);
    const char* Metric=Pause?"pause":"capture";
    printf("%ss_per_sec: %.3f\n%s_p50_us: %.3f\n%s_p95_us: %.3f\n%s_p99_us: %.3f\n%s_p999_us: %.3f\n",
           Metric,Count/Elapsed,Metric,Samples[(Count+1)/2-1],Metric,Samples[(Count*95+99)/100-1],
           Metric,Samples[(Count*99+99)/100-1],Metric,Samples[(Count*999+999)/1000-1]);
    free(Samples);
    return 0;
}
