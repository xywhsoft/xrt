#include "../../../dev/bench/bench_common.h"
#include "../tests/support/ws_resume_peer.h"
#include <inttypes.h>
static uint64 mrBenchCpu(void)
{
#if defined(_WIN32)||defined(_WIN64)
    FILETIME Created,Exited,Kernel,User;testRequire(GetProcessTimes(GetCurrentProcess(),&Created,&Exited,&Kernel,&User)!=0,"process CPU clock");return ((((uint64)Kernel.dwHighDateTime<<32)|Kernel.dwLowDateTime)+(((uint64)User.dwHighDateTime<<32)|User.dwLowDateTime))*100u;
#else
    struct timespec T;testRequire(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&T)==0,"process CPU clock");return (uint64)T.tv_sec*1000000000u+(uint64)T.tv_nsec;
#endif
}
typedef struct mrBenchSlot {xfuture* Future;double Start;} mrBenchSlot;
static int mrBenchCompare(const void* Left,const void* Right)
{double A=*(const double*)Left,B=*(const double*)Right;return A<B?-1:A>B?1:0;}
int main(int Argc,char** Argv)
{
    uint64 Count=Argc>1?strtoull(Argv[1],NULL,10):20000;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):0;bool Instrument=Argc>3&&strtoul(Argv[3],NULL,10)!=0;
    if(!Count||Count>1000000||Mode>3||Argc>4)return 1;
    mrState State={0};mrPeer P;xtlsresume* Credential=NULL;xmqttclientconfig K;xmqttClientConfigInit(&K);char Authority[128];
    if(Mode==3){mrInit(&P,&State,XMQTT_VERSION_5,1,XNET_PORT_SELECT);xmqttclient* Warm=mrOpen(&P,1,NULL,false,4,&K,Authority,sizeof(Authority));xfuture* Ready=xmqttClientReady(Warm);testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready))->Status==XMQTT_STATUS_OK,"full handshake for resumed benchmark credential");mrIssue(&P,1);(void)mrCaptured(Warm,1);testRequire(xmqttClientTlsTakeResume(Warm,&Credential),"take real warm credential");mrClose(Warm);testRequire(xmqttClientDestroy(Warm),"warm Client join");mqttWsConnectStop(&P.Peer);xrtFutureDestroy(Ready);}
    mrInit(&P,&State,XMQTT_VERSION_5,1,XNET_PORT_SELECT);
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After,ColdBefore,ColdAfter;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&ColdBefore);
#else
    if(Instrument)return 2;
#endif
    double Setup=xrtTimer();xmqttclient* C=mrOpen(&P,1,Credential,Mode==3,Mode?4:0,&K,Authority,sizeof(Authority));xrtTlsResumeRelease(Credential);xfuture* Ready=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready))->Status==XMQTT_STATUS_OK,"benchmark fresh MQTT CONNACK");Setup=(xrtTimer()-Setup)*1000000.0;testRequire(mrInfo(C).Resumed==(Mode==3),"benchmark actual PSK state");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&ColdAfter);
#endif
    size_t Cached=0;if(Mode>=2){mrIssue(&P,1);Cached=mrCaptured(C,1).CachedBytes;testRequire(Cached!=0,"real cached credential outside steady timed window");}
    double* Latencies=malloc((size_t)Count*sizeof(*Latencies));testRequire(Latencies!=NULL,"latency storage");mrBenchSlot Slots[32]={{0}};xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("bench/qos1");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("0123456789abcdef0123456789abcdef");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&Before);
#endif
    uint64 Submitted=0,Completed=0,Sum=0,Cpu=mrBenchCpu();double Start=xrtTimer();
    while(Completed<Count){bool Progress=false;for(unsigned I=0;I<32;++I){mrBenchSlot* S=Slots+I;if(S->Future&&xrtFutureState(S->Future)!=XFUTURE_PENDING){const xmqttwriteresult* R=xrtFutureValue(S->Future);testRequire(R&&R->Status==XMQTT_STATUS_OK&&R->Delivery==XMQTT_PEER_ACCEPTED&&R->Identifier&&R->Puback,"measured actual PUBACK");Latencies[Completed++]=(xrtTimer()-S->Start)*1000000.0;Sum+=R->Identifier;xrtFutureDestroy(S->Future);S->Future=NULL;Progress=true;}if(!S->Future&&Submitted<Count){S->Start=xrtTimer();testRequire(xmqttClientPublishAsync(C,&Q,5000,&S->Future)==XMQTT_ADMISSION_ACCEPTED,"measured QoS1 admission");++Submitted;Progress=true;}}testRequire(xrtTimer()-Start<110,"benchmark deadline");if(!Progress)xrtSleep(0);}
    double Elapsed=xrtTimer()-Start;Cpu=mrBenchCpu()-Cpu;uint64 Cycles=Count/65535u,Tail=Count%65535u;testRequire(Sum==Cycles*65535u*65536u/2u+Tail*(Tail+1u)/2u,"actual identifier wrap and checksum");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    mrClose(C);testRequire(xmqttClientDestroy(C),"benchmark actual Client retirement");mqttWsConnectStop(&P.Peer);xrtFutureDestroy(Ready);xrtTlsResumeRelease(State.ServerTicket);qsort(Latencies,(size_t)Count,sizeof(*Latencies),mrBenchCompare);
    printf("publishes_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p95_us: %.3f\nlatency_p99_us: %.3f\nlatency_p999_us: %.3f\nlatency_max_us: %.3f\nprocess_cpu_ns_per_publish: %.3f\nsetup_us: %.3f\ncached_bytes: %zu\nchecksum: %" PRIu64 "\n",(double)Count/Elapsed,Latencies[Count/2],Latencies[(Count-1)*95/100],Latencies[(Count-1)*99/100],Latencies[(Count-1)*999/1000],Latencies[Count-1],(double)Cpu/(double)Count,Setup,Cached,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_publish: %.6f\nheap_bytes_per_publish: %.6f\nsetup_heap_blocks: %" PRIu64 "\nsetup_heap_bytes: %" PRIu64 "\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/(double)Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/(double)Count,(uint64)(ColdAfter.BlockAllocCalls-ColdBefore.BlockAllocCalls),(uint64)(ColdAfter.BlockAllocBytes-ColdBefore.BlockAllocBytes));
#endif
    puts("scope: same selected code; native TLS/HTTP/WSS QoS1 window32. Cold setup excludes fixture creation; warm credential/cache preparation and final native joins outside steady window. Closed-loop unpinned process CPU/XRT heap include both peers; CRT/kernel costs incomplete.");free(Latencies);return 0;
}
