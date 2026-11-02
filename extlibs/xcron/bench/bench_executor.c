#include <xcron.h>
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/thread.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct execbench {xatomic32 Completed;} execbench;
static xcronrunresult execBenchProc(const xcronrun* R,const xcronexecution* T,ptr C)
{(void)R;(void)T;(void)C;return XCRON_RUN_SUCCEEDED;}
static void execBenchDirect(ptr Data)
{(void)Data;(void)execBenchProc(NULL,NULL,NULL);}
static void execBenchDrop(ptr Data,ptr Context)
{(void)Context;execbench* B=Data;xrtAtomic32FetchAdd(&B->Completed,1,XMEMORY_RELEASE);}
static int execBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    uint32 Rounds=Argc>1?(uint32)strtoul(Argv[1],NULL,10):1000,Threads=Argc>2?(uint32)strtoul(Argv[2],NULL,10):4;const uint32 Batch=64;
    if(!Rounds || Rounds>1000000 || !Threads || Threads>16)return 1;
    xexecutorconfig PC={Threads,Batch*2,0};xexecutor* Pool=xrtExecutorCreate(&PC);if(!Pool)return 2;
    xcronexecutor* Adapter=NULL;xcronexecutorconfig EC={Pool,execBenchProc,NULL,NULL,NULL,Batch,0,1024*1024};double* Samples=xrtMalloc((size_t)Rounds*sizeof(double));if(!Samples || !xcronExecutorCreate(&EC,&Adapter,NULL))return 3;
    execbench Direct={0};double DirectTime=0,AdapterTime=0,P99[2]={0};uint64 Sum=0;
    for(unsigned Mode=0;Mode<2;++Mode){double Start=xrtTimer();
        for(uint32 Round=0;Round<Rounds;++Round){double Begin=xrtTimer();
            if(!Mode){xrtAtomic32Store(&Direct.Completed,0,XMEMORY_RELAXED);for(uint32 J=0;J<Batch;++J)if(!xrtExecutorSubmit(Pool,execBenchDirect,&Direct,execBenchDrop,NULL))return 4;
                while(xrtAtomic32Load(&Direct.Completed,XMEMORY_ACQUIRE)!=Batch)xrtThreadYield();
                Sum+=Batch;
            }else{
                for(uint32 J=0;J<Batch;++J){xcronrun R={.Id=(uint64)Round*Batch+J+1,.Job=1};if(!xcronExecutorSubmit(Adapter,&R,NULL))return 5;}
                uint32 Count=0;while(Count<Batch){xcroncompletion C[64];size_t N;if(!xcronExecutorDrain(Adapter,C,Batch-Count,&N,NULL))return 6;for(size_t J=0;J<N;++J){if(C[J].Error || C[J].Result!=XCRON_RUN_SUCCEEDED)return 7;Sum+=C[J].Run.Id;}Count+=(uint32)N;if(!N)xrtThreadYield();}
            }
            Samples[Round]=(xrtTimer()-Begin)*1e6;
        }
        double Time=xrtTimer()-Start;qsort(Samples,Rounds,sizeof(*Samples),execBenchCompare);P99[Mode]=Samples[(Rounds-1)*99/100];if(Mode)AdapterTime=Time;else DirectTime=Time;
    }
    xcronexecutorstats Stats;do{if(!xcronExecutorInspect(Adapter,&Stats,NULL))return 8;if(Stats.Producers)xrtThreadYield();}while(Stats.Producers);
    if(!xcronExecutorDestroy(Adapter,NULL) || !xrtExecutorClose(Pool) || xrtExecutorWaitFor(Pool,10000)!=XWAIT_OK || !xrtExecutorDestroy(Pool))return 9;
    xrtFree(Samples);if(!(DirectTime>0 && AdapterTime>0))return 10;
    printf("executions_per_sec: %.3f\nbatch_p99_us: %.3f\nminimal_xrt_per_sec: %.3f\nminimal_xrt_batch_p99_us: %.3f\nbatch_size: %u\nchecksum: %" PRIu64 "\n",(double)Rounds*Batch/AdapterTime,P99[1],(double)Rounds*Batch/DirectTime,P99[0],Batch,Sum);return 0;
}
