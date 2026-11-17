#include "../../../dev/bench/bench_common.h"
#include <xmqtt/store_write.h>
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
/* Matched semantic-owner component diagnostics,not a live MQTT workload. */
int main(int Argc,char** Argv)
{
    if(Argc>4)return 2;
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):1000;unsigned Mode=Argc>2?(unsigned)atoi(Argv[2]):0;bool Instrument=Argc>3&&atoi(Argv[3]);if(!Count||Count>1000000||Mode>1)return 2;
    unsigned char Payload[256]={0},Wire[512],Batch[1024];double* Times=malloc(Count*sizeof(*Times));jbCheck(Times!=NULL);size_t WireBytes=0;
    xmqttpacket Packet;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet);Packet.Body.Publish.Topic=XRT_STR_LITERAL("bench/typed");Packet.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};jbCheck(xmqttPacketWrite5(Wire,sizeof(Wire),&Packet,XMQTT_SERVER_TO_CLIENT,NULL,&WireBytes));
    str Path=NULL;xfile F=xrtFileTemp(NULL,"xmqtt-store-write-",NULL,&Path);jbCheck(F!=NULL);xmqttjournalconfig JC;xmqttJournalConfigInit(&JC);JC.Binding=XRT_BYTES_LITERAL("bench/typed");JC.Generation=1;JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    xmqttstorestateconfig SC;xmqttStoreStateConfigInit(&SC);SC.Binding=JC.Binding;SC.Generation=1;SC.ClientId=XRT_STR_LITERAL("bench-write");SC.WallMilliseconds=1001;SC.MaxOutgoing=SC.MaxIncoming=0;SC.MaxInbox=1;SC.MaxTransactions=UINT64_MAX;SC.Delta.MaxOperations=2;SC.Delta.Codec.MaxProperties=0;
    xmqttjournal* J=NULL;jbCheck(xmqttJournalCreate(F,&JC,&J)==XMQTT_JOURNAL_DONE);xmqttstoredelta Seed[2]={{.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=1000,.Data=XRT_BYTES_LITERAL("bench-write")},{.Type=XMQTT_STORE_OPEN,.Epoch=1,.Expiry=UINT32_MAX,.WallMilliseconds=1001}};size_t N=0;uint64 Sequence=0;
    jbCheck(xmqttStoreDeltaWrite(&SC.Delta,Seed,2,NULL,0,Batch,sizeof(Batch),&N)==XMQTT_STORE_DELTA_DONE&&xmqttJournalAppend(J,(xbytesview){Batch,N},&Sequence)==XMQTT_JOURNAL_DONE&&xmqttJournalDestroy(J));J=NULL;jbCheck(xmqttJournalOpen(F,&JC,&J)==XMQTT_JOURNAL_DONE);xmqttstorestate* State=NULL;jbCheck(xmqttStoreStateRecover(J,&SC,&State)==XMQTT_STORE_DONE);
    xtaskpoolconfig PC={.Threads=2,.QueueLimit=8};xtaskpool* Pool=xrtTaskPoolCreate(&PC);jbCheck(Pool!=NULL);xmqttjournalworker* Raw=NULL;xmqttstorewrite* Writer=NULL;
    if(Mode){xmqttstorewriteconfig C;xmqttStoreWriteConfigInit(&C);jbCheck(xmqttStoreWriteCreate(Pool,J,State,&C,&Writer));}else{xmqttStoreStateDestroy(State);xmqttjournalworkerconfig C;xmqttJournalWorkerConfigInit(&C);jbCheck(xmqttJournalWorkerCreate(Pool,J,&C,&Raw));}
    /* A newly recovered owner requires an actual new OPEN before any flow.
     * Apply the same resumptive typed record to BOTH modes outside timing. */
    xmqttstoredelta Resume={.Type=XMQTT_STORE_OPEN,.Flags=2,.Epoch=1,.Expiry=UINT32_MAX,.WallMilliseconds=1002};
    jbCheck(xmqttStoreDeltaWrite(&SC.Delta,&Resume,1,NULL,0,Batch,sizeof(Batch),&N)==XMQTT_STORE_DELTA_DONE);xfuture* Ready=NULL;
    jbCheck((Mode?xmqttStoreWriteAppendAsync(Writer,(xbytesview){Batch,N},10000,&Ready):xmqttJournalWorkerAppendAsync(Raw,(xbytesview){Batch,N},10000,&Ready))==XMQTT_JWORK_ACCEPTED&&xrtFutureWaitFor(Ready,10000)==XWAIT_OK&&xrtFutureState(Ready)==XFUTURE_RESOLVED);
    const xmqttcommitresult* Open=xrtFutureValue(Ready);jbCheck(Open->Status==XMQTT_COMMIT_OK&&Open->Sequence==2);xrtFutureDestroy(Ready);double InitEnd=xrtTimer()+10;
    for(;;){xmqttjournalworkerinfo I;if(Mode){xmqttstorewriteinfo M;jbCheck(xmqttStoreWriteInfo(Writer,&M));I=M.Worker;}else jbCheck(xmqttJournalWorkerInfo(Raw,&I));if(!I.Tasks)break;jbCheck(xrtTimer()<InitEnd);xrtSleep(1);}
    xtaskpoolstats InitialStats;jbCheck(xrtTaskPoolGet(Pool,&InitialStats));
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    double Start=xrtTimer();uint64 Cpu=jbCpu(),Overloads=0;
    for(size_t I=0;I<Count;++I){double One=xrtTimer(),Deadline=One+10;xmqttstoredelta Ops[2]={{.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=I+1,.Data={Wire,WireBytes}},{.Type=XMQTT_STORE_INBOX_ACK,.Epoch=1,.Key=I+1}};
        jbCheck(xmqttStoreDeltaWrite(&SC.Delta,Ops,2,NULL,0,Batch,sizeof(Batch),&N)==XMQTT_STORE_DELTA_DONE);xfuture* Future=NULL;xmqttjournaladmission A;int64 Left;
        do{Left=(int64)((Deadline-xrtTimer())*1000);jbCheck(Left>0);A=Mode?xmqttStoreWriteAppendAsync(Writer,(xbytesview){Batch,N},Left,&Future):xmqttJournalWorkerAppendAsync(Raw,(xbytesview){Batch,N},Left,&Future);if(A==XMQTT_JWORK_OVERLOADED){++Overloads;xrtThreadYield();}}while(A==XMQTT_JWORK_OVERLOADED);
        Left=(int64)((Deadline-xrtTimer())*1000);jbCheck(A==XMQTT_JWORK_ACCEPTED&&Left>0&&xrtFutureWaitFor(Future,Left)==XWAIT_OK&&xrtFutureState(Future)==XFUTURE_RESOLVED);const xmqttcommitresult* R=xrtFutureValue(Future);jbCheck(R->Status==XMQTT_COMMIT_OK&&R->Effect==XMQTT_JOURNAL_COMMITTED&&R->Sequence==I+3);xrtFutureDestroy(Future);Times[I]=(xrtTimer()-One)*1e6;
    }
    jbCheck(Mode?(xmqttStoreWriteClose(Writer)&&xmqttStoreWriteWaitFor(Writer,10000)==XWAIT_OK):(xmqttJournalWorkerClose(Raw)&&xmqttJournalWorkerWaitFor(Raw,10000)==XWAIT_OK));double Elapsed=xrtTimer()-Start;Cpu=jbCpu()-Cpu;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    xmqttjournalworkerinfo Info;size_t ModelBytes=0,ModelPeak=0;
    if(Mode){xmqttstorewriteinfo I;jbCheck(xmqttStoreWriteInfo(Writer,&I)&&I.State.Transactions==Count+2&&I.State.LastKey==Count&&!I.State.Inbox);Info=I.Worker;ModelBytes=I.State.MemoryBytes;ModelPeak=I.State.PeakMemoryBytes;jbCheck(xmqttStoreWriteDestroy(Writer));}else jbCheck(xmqttJournalWorkerInfo(Raw,&Info)&&xmqttJournalWorkerDestroy(Raw));
    xtaskpoolstats PS;jbCheck(xrtTaskPoolGet(Pool,&PS)&&!Info.Tasks&&!Info.Requests&&!Info.Objects&&Info.Journal.Sequence==Count+2&&xrtTaskPoolClose(Pool)&&xrtTaskPoolWaitFor(Pool,10000)==XWAIT_OK&&xrtTaskPoolDestroy(Pool)&&xrtClose(F)&&xrtFileDelete(Path));xrtFree(Path);qsort(Times,Count,sizeof(*Times),jbCompare);
    printf("operations_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p95_us: %.3f\nlatency_p99_us: %.3f\nlatency_p999_us: %.3f\nlatency_max_us: %.3f\nprocess_cpu_ns_per_op: %.3f\ntransaction_bytes: %zu\nmodel_bytes: %zu\nmodel_peak_bytes: %zu\nnative_tasks_per_op: %.6f\nadmission_overloads_per_op: %.6f\n",Count/Elapsed,Times[Count/2],Times[(Count-1)*95/100],Times[(Count-1)*99/100],Times[(Count-1)*999/1000],Times[Count-1],(double)Cpu/Count,N,ModelBytes,ModelPeak,(double)(PS.Submitted-InitialStats.Submitted)/Count,(double)Overloads/Count);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_op: %.6f\nheap_bytes_per_op: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    puts("scope: same native two-sync typed acceptance+Inbox-ACK transactions,256-byte payload,host2-thread pool and codec encoding. Seed/file/recovery/constructor/resumed-OPEN/native task initialization outside timing. Mode0 opaque worker; mode1 additionally native decode,legal transition validation,touched metadata plan,new-wire copy and atomic model installation. Both include whole job calloc zeroing,input copy,secure wipe,Core allocations/tasks/retries,Future/release and extension physical tail; remaining executor bookkeeping belongs to host join. Closed-loop1 outstanding,no fixed-arrival load,network Client/delivery lease/business effect/powerloss/comparator or production performance acceptance.");free(Times);return 0;
}
