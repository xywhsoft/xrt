#include "../../../dev/bench/bench_common.h"
#include <xmqtt/store_state.h>
#include <xrt/time.h>
#include <xrt/memory.h>
#include <xrt/memory_stats.h>
static void sbCheck(bool OK){if(!OK){fputs("store recovery benchmark failed\n",stderr);exit(1);}}
static int sbCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
static uint64 sbCpu(void)
{
#if defined(_WIN32)||defined(_WIN64)
    FILETIME A,B,K,U;sbCheck(GetProcessTimes(GetCurrentProcess(),&A,&B,&K,&U)!=0);return ((((uint64)K.dwHighDateTime<<32)|K.dwLowDateTime)+(((uint64)U.dwHighDateTime<<32)|U.dwLowDateTime))*100u;
#else
    struct timespec T;sbCheck(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&T)==0);return (uint64)T.tv_sec*1000000000u+(uint64)T.tv_nsec;
#endif
}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):1000;unsigned Mode=Argc>2?(unsigned)atoi(Argv[2]):0;bool Instrument=Argc>3&&atoi(Argv[3]);if(Argc>4||!Count||Count>1000000||Mode>1)return 2;
    xmqttstorestateconfig C;xmqttStoreStateConfigInit(&C);C.Binding=XRT_BYTES_LITERAL("bench/broker/principal/slot");C.Generation=1;C.ClientId=XRT_STR_LITERAL("bench-state");C.WallMilliseconds=1001;C.MaxOutgoing=C.MaxIncoming=0;C.MaxInbox=64;
    xmqttstoredelta Ops[66]={{.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=1000,.Data=XRT_BYTES_LITERAL("bench-state")},{.Type=XMQTT_STORE_OPEN,.Epoch=1,.Expiry=3600,.WallMilliseconds=1001}};
    unsigned char Wire[64][512],Payload[256]={0},Batch[32768];xmqttproperty Scratch[4];
    for(unsigned I=0;I<64;++I){xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=(uint16)(I+1);Q.Body.Publish.Topic=XRT_STR_LITERAL("bench/state");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};size_t N=0;sbCheck(xmqttPacketWrite5(Wire[I],sizeof(Wire[I]),&Q,XMQTT_SERVER_TO_CLIENT,&C.Delta.Codec,&N));Ops[I+2]=(xmqttstoredelta){.Type=XMQTT_STORE_IN_ACCEPT,.Identifier=Q.Identifier,.Epoch=1,.Key=I+1,.Data={Wire[I],N}};}
    size_t Bytes=0;sbCheck(xmqttStoreDeltaWrite(&C.Delta,Ops,66,Scratch,4,Batch,sizeof(Batch),&Bytes)==XMQTT_STORE_DELTA_DONE);
    str Path=NULL;xfile F=xrtFileTemp(NULL,"xmqtt-state-bench-",NULL,&Path);sbCheck(F!=NULL);xmqttjournalconfig JC;xmqttJournalConfigInit(&JC);JC.Binding=C.Binding;JC.Generation=C.Generation;JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;JC.MaxTransactionBytes=sizeof(Batch);JC.MaxMemoryBytes=65536;
    xmqttjournal* J=NULL;uint64 Sequence=0;sbCheck(xmqttJournalCreate(F,&JC,&J)==XMQTT_JOURNAL_DONE&&xmqttJournalAppend(J,(xbytesview){Batch,Bytes},&Sequence)==XMQTT_JOURNAL_DONE&&xmqttJournalDestroy(J));
    xmqttstorestate* Held=NULL;if(Mode){sbCheck(xmqttJournalOpen(F,&JC,&J)==XMQTT_JOURNAL_DONE&&xmqttStoreStateRecover(J,&C,&Held)==XMQTT_STORE_DONE&&xmqttJournalDestroy(J));}
    double* Times=malloc(Count*sizeof(*Times));sbCheck(Times!=NULL);uint64 Checksum=0;size_t Memory=0,Peak=0;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Cpu=sbCpu();double Start=xrtTimer();
    for(size_t I=0;I<Count;++I){double One=xrtTimer();xmqttstorestate* S=Held;if(!Mode)sbCheck(xmqttJournalOpen(F,&JC,&J)==XMQTT_JOURNAL_DONE&&xmqttStoreStateRecover(J,&C,&S)==XMQTT_STORE_DONE);
        xmqttstorestateinfo Info;sbCheck(xmqttStoreStateInfo(S,&Info)&&Info.Inbox==64);Memory=Info.MemoryBytes;Peak=Info.PeakMemoryBytes;
        for(size_t K=0;K<Info.Inbox;++K){xmqttstoreview V;sbCheck(xmqttStoreStateView(S,XMQTT_STORE_INBOX,K,&V)==XMQTT_STORE_DONE&&V.Key==K+1);Checksum+=V.Key+V.Wire.Size;}
        if(!Mode){xmqttStoreStateDestroy(S);sbCheck(xmqttJournalDestroy(J));}Times[I]=(xrtTimer()-One)*1e6;}
    double Elapsed=xrtTimer()-Start;Cpu=sbCpu()-Cpu;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    qsort(Times,Count,sizeof(*Times),sbCompare);printf("operations_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p95_us: %.3f\nlatency_p99_us: %.3f\nlatency_p999_us: %.3f\nlatency_max_us: %.3f\nprocess_cpu_ns_per_op: %.3f\nretained_bytes: %zu\nrecovery_peak_bytes: %zu\nchecksum: %" PRIu64 "\n",Count/Elapsed,Times[Count/2],Times[(Count-1)*95/100],Times[(Count-1)*99/100],Times[(Count-1)*999/1000],Times[Count-1],(double)Cpu/Count,Memory,Peak,Checksum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_op: %.6f\nheap_bytes_per_op: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    puts("scope: mode0 opens/verifies/replays one native cached-file transaction,66 records/64 QoS1 deliveries with256-byte payload,reads all views and destroys state/journal; mode1 reads Info+64 indexed views of held recovery. Initial file creation/two syncs outside timing; no live commit queue, network, broker, comparator, powerloss or production qualification. Includes validation/copies/allocation/reset/indexing/native reads in mode0; caller latency array and CRT/kernel costs not fully instrumented.");
    xmqttStoreStateDestroy(Held);sbCheck(xrtClose(F)&&xrtFileDelete(Path));xrtFree(Path);free(Times);return Checksum?0:1;
}
