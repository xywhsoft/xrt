#include "../../../dev/bench/bench_common.h"
#include <xmqtt/journal.h>
#include <xrt/memory.h>
#include <xrt/memory_stats.h>
#include <xrt/time.h>
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
    if(Argc!=5)return 2;
    size_t Bytes=(size_t)strtoull(Argv[2],NULL,10),Count=(size_t)strtoull(Argv[3],NULL,10);bool Instrument=atoi(Argv[4])!=0;
    if(!Count||Count>1000000||Bytes>1048576)return 2;
    unsigned char* Payload=calloc(Bytes?Bytes:1,1);double* Times=malloc(Count*sizeof(*Times));jbCheck(Payload&&Times);
    xfile F=xrtOpen(Argv[1],XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_TRUNCATE);jbCheck(F!=NULL);
    xmqttjournalconfig C;xmqttJournalConfigInit(&C);C.Binding=XRT_BYTES_LITERAL("bench/journal");C.Generation=1;C.MaxTransactionBytes=Bytes;
    xmqttjournal* J=NULL;jbCheck(xmqttJournalCreate(F,&C,&J)==XMQTT_JOURNAL_DONE);
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Cpu=jbCpu();double Start=xrtTimer();uint64 Seq=0;
    for(size_t I=0;I<Count;++I){double One=xrtTimer();jbCheck(xmqttJournalAppend(J,(xbytesview){Payload,Bytes},&Seq)==XMQTT_JOURNAL_DONE&&Seq==I+1);Times[I]=(xrtTimer()-One)*1e6;}
    double Elapsed=xrtTimer()-Start;Cpu=jbCpu()-Cpu;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    xmqttjournalinfo Info;jbCheck(xmqttJournalInfo(J,&Info)&&Info.Sequence==Count);jbCheck(xmqttJournalDestroy(J)&&xrtClose(F));
    qsort(Times,Count,sizeof(*Times),jbCompare);
    printf("operations_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p95_us: %.3f\nlatency_p99_us: %.3f\nlatency_p999_us: %.3f\nlatency_max_us: %.3f\ntransaction_bytes: %zu\ncommitted_bytes: %" PRIu64 "\n",Count/Elapsed,Times[Count/2],Times[(Count-1)*95/100],Times[(Count-1)*99/100],Times[(Count-1)*999/1000],Times[Count-1],Bytes,Info.CommittedEnd);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_op: %.6f\nheap_bytes_per_op: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    printf("process_cpu_ns_per_op: %.3f\n",(double)Cpu/Count);
    puts("scope: closed-loop single storage owner; each commit includes data sync and both-witness publication sync; no group commit, admission queue, MQTT/network/Inbox or namespace publication. Native XRT sync errors are fatal. Device cache guarantees and physical powerloss are unverified. XRT allocation counts exclude CRT/kernel costs. Shared-host diagnostics only; no production/performance acceptance.");free(Times);free(Payload);return 0;
}
