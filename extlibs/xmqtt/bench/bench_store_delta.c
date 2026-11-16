#include "../../../dev/bench/bench_common.h"
#include <xmqtt/store_delta.h>
#include <xrt/time.h>
#include <xrt/memory_stats.h>
static void dbCheck(bool OK){if(!OK){fputs("store delta benchmark failed\n",stderr);exit(1);}}
static int dbCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
static uint64 dbCpu(void)
{
#if defined(_WIN32)||defined(_WIN64)
    FILETIME A,B,K,U;dbCheck(GetProcessTimes(GetCurrentProcess(),&A,&B,&K,&U)!=0);return ((((uint64)K.dwHighDateTime<<32)|K.dwLowDateTime)+(((uint64)U.dwHighDateTime<<32)|U.dwLowDateTime))*100u;
#else
    struct timespec T;dbCheck(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&T)==0);return (uint64)T.tv_sec*1000000000u+(uint64)T.tv_nsec;
#endif
}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):5000;unsigned Mode=Argc>2?(unsigned)atoi(Argv[2]):1;bool Instrument=Argc>3&&atoi(Argv[3]);if(Argc>4||!Count||Count>1000000||Mode>2)return 2;
    xmqttstoredeltaconfig C;xmqttStoreDeltaConfigInit(&C);xmqttstoredelta Ops[64]={{0}},Read[64];unsigned char Wire[64][1024],Payload[256]={0},Buffer[65536];xmqttproperty Scratch[4],P={0};P.Id=XMQTT_PROPERTY_USER;P.Type=XMQTT_PROPERTY_STRING_PAIR;P.Value.Pair.Name=XRT_STR_LITERAL("bench");P.Value.Pair.Value=XRT_STR_LITERAL("delta");
    for(unsigned I=0;I<64;++I){xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=(uint16)(I+1);Q.Body.Publish.Topic=XRT_STR_LITERAL("bench/delta");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};Q.Properties=(xmqttpropertylist){&P,1};size_t N=0;dbCheck(xmqttPacketWrite5(Wire[I],sizeof(Wire[I]),&Q,XMQTT_SERVER_TO_CLIENT,&C.Codec,&N));Ops[I]=(xmqttstoredelta){.Type=XMQTT_STORE_IN_ACCEPT,.Identifier=Q.Identifier,.Epoch=1,.Key=I+1,.Data={Wire[I],N}};}
    size_t Bytes=0;dbCheck(xmqttStoreDeltaWrite(&C,Ops,64,Scratch,4,Buffer,sizeof(Buffer),&Bytes)==XMQTT_STORE_DELTA_DONE);double* Times=malloc(Count*sizeof(*Times));dbCheck(Times!=NULL);uint64 Checksum=0;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Cpu=dbCpu();double Start=xrtTimer();
    for(size_t I=0;I<Count;++I){double One=xrtTimer();size_t N=0;if(Mode==0)dbCheck(xmqttStoreDeltaSize(&C,Ops,64,Scratch,4,&N)==XMQTT_STORE_DELTA_DONE);else if(Mode==1)dbCheck(xmqttStoreDeltaWrite(&C,Ops,64,Scratch,4,Buffer,sizeof(Buffer),&N)==XMQTT_STORE_DELTA_DONE);else {xmqttstoredeltadocument Doc;dbCheck(xmqttStoreDeltaRead(&C,(xbytesview){Buffer,Bytes},Read,64,Scratch,4,&Doc)==XMQTT_STORE_DELTA_DONE&&Doc.Count==64);N=Doc.Raw.Size;}Times[I]=(xrtTimer()-One)*1e6;Checksum+=N;}
    double Elapsed=xrtTimer()-Start;Cpu=dbCpu()-Cpu;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    dbCheck(Checksum==(uint64)Count*Bytes);qsort(Times,Count,sizeof(*Times),dbCompare);
    printf("operations_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p95_us: %.3f\nlatency_p99_us: %.3f\nlatency_p999_us: %.3f\nlatency_max_us: %.3f\nprocess_cpu_ns_per_op: %.3f\nbatch_bytes: %zu\nrecords_per_batch: 64\nchecksum: %" PRIu64 "\n",Count/Elapsed,Times[Count/2],Times[(Count-1)*95/100],Times[(Count-1)*99/100],Times[(Count-1)*999/1000],Times[Count-1],(double)Cpu/Count,Bytes,Checksum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_op: %.6f\nheap_bytes_per_op: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    puts("scope: one operation is one64-record incoming-QoS1 batch with distinct ids/keys and256-byte payload each. Includes64 native packet validations, string/property scans and error preservation; Write copies each wire once; Read borrows wire. No journal sync, queue/admission delay, active state/leases, MQTT network, broker/comparator, powerloss or production acceptance. Caller arrays/latency storage and CRT/kernel costs are outside XRT allocation counts. Shared-host diagnostics only.");free(Times);return 0;
}
