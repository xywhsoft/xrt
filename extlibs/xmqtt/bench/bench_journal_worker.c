#include "../../../dev/bench/bench_common.h"
#include <xmqtt/journal_worker.h>
#include <xrt/memory.h>
#include <xrt/memory_stats.h>
#include <xrt/time.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
static int jbCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
static void jbCheck(bool OK){if(!OK){fputs("journal benchmark failed\n",stderr);exit(1);}}
static void jbCpuCheck(bool OK,cstr Text){(void)Text;jbCheck(OK);}
static uint64 jbCpu(void)
{
#if defined(_WIN32)||defined(_WIN64)
    FILETIME Created,Exited,Kernel,User;jbCpuCheck(GetProcessTimes(GetCurrentProcess(),&Created,&Exited,&Kernel,&User)!=0,"process CPU clock");return ((((uint64)Kernel.dwHighDateTime<<32)|Kernel.dwLowDateTime)+(((uint64)User.dwHighDateTime<<32)|User.dwLowDateTime))*100u;
#else
    struct timespec T;jbCpuCheck(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&T)==0,"process CPU clock");return (uint64)T.tv_sec*1000000000u+(uint64)T.tv_nsec;
#endif
}
/* Native sync diagnostics. Caller owns/publishes path; creation and namespace
 * publication are outside timed operations and outside durability claims. */
int main(int Argc,char** Argv)
{
    if(Argc>4)return 2;
    size_t Bytes=256,Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):1000;unsigned Mode=Argc>2?(unsigned)atoi(Argv[2]):0;bool Instrument=Argc>3&&atoi(Argv[3]);
    if(!Count||Count>1000000||Mode>1)return 2;
    unsigned char* Payload=calloc(Bytes?Bytes:1,1);double* Times=malloc(Count*sizeof(*Times));jbCheck(Payload&&Times);
    str Path=NULL;xfile F=xrtFileTemp(NULL,"xmqtt-worker-bench-",NULL,&Path);jbCheck(F!=NULL);
    xmqttjournalconfig C;xmqttJournalConfigInit(&C);C.Binding=XRT_BYTES_LITERAL("bench/journal");C.Generation=1;C.MaxTransactionBytes=Bytes;
    xmqttjournal* J=NULL;jbCheck(xmqttJournalCreate(F,&C,&J)==XMQTT_JOURNAL_DONE);
    xtaskpool* Pool=NULL;xmqttjournalworker* W=NULL;
    if(Mode){xtaskpoolconfig PC={.Threads=2,.QueueLimit=8};Pool=xrtTaskPoolCreate(&PC);jbCheck(Pool!=NULL);xmqttjournalworkerconfig WC;xmqttJournalWorkerConfigInit(&WC);jbCheck(xmqttJournalWorkerCreate(Pool,J,&WC,&W));}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Cpu=jbCpu();double Start=xrtTimer();uint64 Seq=0,Refused=0;
    for(size_t I=0;I<Count;++I){double One=xrtTimer();
        if(!Mode)jbCheck(xmqttJournalAppend(J,(xbytesview){Payload,Bytes},&Seq)==XMQTT_JOURNAL_DONE&&Seq==I+1);
        else {xfuture* Future=NULL;xmqttjournaladmission A;double Deadline=One+10;int64 Left;
            do{Left=(int64)((Deadline-xrtTimer())*1000.0);jbCheck(Left>0);A=xmqttJournalWorkerAppendAsync(W,(xbytesview){Payload,Bytes},Left,&Future);if(A==XMQTT_JWORK_OVERLOADED){++Refused;xrtThreadYield();}}while(A==XMQTT_JWORK_OVERLOADED);
            Left=(int64)((Deadline-xrtTimer())*1000.0);jbCheck(A==XMQTT_JWORK_ACCEPTED&&Left>0&&xrtFutureWaitFor(Future,Left)==XWAIT_OK&&xrtFutureState(Future)==XFUTURE_RESOLVED);const xmqttcommitresult* R=xrtFutureValue(Future);jbCheck(R->Status==XMQTT_COMMIT_OK&&R->Effect==XMQTT_JOURNAL_COMMITTED&&R->Sequence==I+1);xrtFutureDestroy(Future);}
        Times[I]=(xrtTimer()-One)*1e6;}
    if(Mode)jbCheck(xmqttJournalWorkerClose(W)&&xmqttJournalWorkerWaitFor(W,10000)==XWAIT_OK);
    double Elapsed=xrtTimer()-Start;Cpu=jbCpu()-Cpu;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    xmqttjournalinfo Info;uint64 Tasks=0;
    if(Mode){xmqttjournalworkerinfo WI;jbCheck(xmqttJournalWorkerInfo(W,&WI)&&WI.Journal.Sequence==Count&&!WI.Objects&&!WI.Tasks&&WI.Bytes==WI.BaseBytes);Info=WI.Journal;xtaskpoolstats PS;jbCheck(xrtTaskPoolGet(Pool,&PS));Tasks=PS.Submitted;jbCheck(xmqttJournalWorkerDestroy(W)&&xrtTaskPoolClose(Pool)&&xrtTaskPoolWaitFor(Pool,10000)==XWAIT_OK&&xrtTaskPoolDestroy(Pool));}
    else jbCheck(xmqttJournalInfo(J,&Info)&&Info.Sequence==Count&&xmqttJournalDestroy(J));
    jbCheck(xrtClose(F)&&xrtFileDelete(Path));xrtFree(Path);
    qsort(Times,Count,sizeof(*Times),jbCompare);
    printf("operations_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p95_us: %.3f\nlatency_p99_us: %.3f\nlatency_p999_us: %.3f\nlatency_max_us: %.3f\ntransaction_bytes: %zu\ncommitted_bytes: %" PRIu64 "\n",Count/Elapsed,Times[Count/2],Times[(Count-1)*95/100],Times[(Count-1)*99/100],Times[(Count-1)*999/1000],Times[Count-1],Bytes,Info.CommittedEnd);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_op: %.6f\nheap_bytes_per_op: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    printf("process_cpu_ns_per_op: %.3f\n",(double)Cpu/Count);
    printf("native_tasks_per_op: %.6f\nadmission_overloads_per_op: %.6f\n",(double)Tasks/Count,(double)Refused/Count);
    puts("scope: matched256-byte opaque transactions and native two-sync Journal; mode0 direct single-owner append,mode1 borrowed2-thread pool,copy/admission/Promise/cancel/result/wait/Release plus final physical native-tail drain. Initial file initialization outside timing; caller mallocs,Journal,Pool creation outside. Each commit does two CRC scans,three frame writes,two4096-byte witnesses,two native flushes; no group commit. Mode1 adds one retained input copy and extension+Core allocation/task/synchronization bookkeeping,all real costs. Closed-loop1 outstanding operation; unaccepted overload retries yield under one original10s deadline and are timed/counted; no fixed-arrival queue-inclusive workload,MQTT ACK/Inbox driver,comparator product or production qualification. Stats include XRT allocations across threads,exclude CRT/kernel. CPU and throughput cover final task drain; per-operation latency ends at physical append result,not last task destruction. Device cache/powerloss unverified.");free(Times);free(Payload);return 0;
}
