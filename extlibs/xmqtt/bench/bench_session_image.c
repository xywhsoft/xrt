#include "../../../dev/bench/bench_common.h"
#include <xmqtt.h>
#include <xmqtt/session_image.h>
#include <xrt/time.h>
#include <xrt/memory_stats.h>
#include <inttypes.h>
#include <stdlib.h>
static void imageBenchCheck(bool OK,cstr Text)
{if(!OK){fprintf(stderr,"%s\n",Text);exit(1);}}
static int imageBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return X<Y?-1:X>Y?1:0;}
static uint64 imageBenchCpu(void)
{
#if defined(_WIN32)||defined(_WIN64)
    FILETIME Created,Exited,Kernel,User;imageBenchCheck(GetProcessTimes(GetCurrentProcess(),&Created,&Exited,&Kernel,&User)!=0,"process CPU clock");return ((((uint64)Kernel.dwHighDateTime<<32)|Kernel.dwLowDateTime)+(((uint64)User.dwHighDateTime<<32)|User.dwLowDateTime))*100u;
#else
    struct timespec T;imageBenchCheck(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&T)==0,"process CPU clock");return (uint64)T.tv_sec*1000000000u+(uint64)T.tv_nsec;
#endif
}
int main(int Argc,char** Argv)
{
    uint64 Count=Argc>1?strtoull(Argv[1],NULL,10):2000;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):1;bool Instrument=Argc>3&&strtoul(Argv[3],NULL,10)!=0;if(!Count||Count>1000000||Mode>2||Argc>4)return 2;
    xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.ClientId=XRT_STR_LITERAL("bench-image");K.MaxIncoming=0;xmqttsession* S=NULL;imageBenchCheck(xmqttSessionCreate(&K,&S),"benchmark Session create");xmqttpacket C,A;xmqttPacketInit(XMQTT_PACKET_CONNECT,&C);xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);C.Body.Connect.ClientId=K.ClientId;xmqttproperty E={0};E.Id=XMQTT_PROPERTY_SESSION_EXPIRY;E.Type=XMQTT_PROPERTY_UINT32;E.Value.Number=UINT32_MAX;C.Properties=(xmqttpropertylist){&E,1};xmqttnegotiated Negotiated;imageBenchCheck(xmqttSessionOpen(S,&C,&A,1,&Negotiated)==XMQTT_SESSION_DONE,"benchmark source open");
    unsigned char Payload[256]={0};xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("bench/image");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
    for(unsigned I=0;I<64;++I){const xmqttsessionpacket* P=NULL;imageBenchCheck(xmqttSessionPublish(S,&Q,&P)==XMQTT_SESSION_DONE,"64 canonical source packets");xmqttSessionPacketRelease(P);}
    imageBenchCheck(xmqttSessionEnd(S,NULL,2)==XMQTT_SESSION_DONE,"benchmark source offline");xmqttsessionimageconfig Config;xmqttSessionImageConfigInit(&Config);Config.Binding=XRT_BYTES_LITERAL("bench/broker/slot");Config.MonotonicTime=2;Config.WallMilliseconds=1000;size_t Bytes=0;imageBenchCheck(xmqttSessionImageSize(S,&Config,&Bytes)==XMQTT_SESSION_DONE,"benchmark exact image Size");unsigned char* Wire=xrtMalloc(Bytes);double* Latency=malloc((size_t)Count*sizeof(*Latency));imageBenchCheck(Wire&&Latency,"benchmark caller storage");size_t Written=0;imageBenchCheck(xmqttSessionImageWrite(S,&Config,Wire,Bytes,&Written)==XMQTT_SESSION_DONE&&Written==Bytes,"benchmark initial image");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Checksum=0,Cpu=imageBenchCpu();double Start=xrtTimer();
    for(uint64 I=0;I<Count;++I){double One=xrtTimer();size_t N=0;if(Mode==0)imageBenchCheck(xmqttSessionImageSize(S,&Config,&N)==XMQTT_SESSION_DONE,"measured Size");else if(Mode==1)imageBenchCheck(xmqttSessionImageWrite(S,&Config,Wire,Bytes,&N)==XMQTT_SESSION_DONE,"measured Write");else{xmqttsession* R=NULL;xmqttsessioninfo Info;imageBenchCheck(xmqttSessionImageRestore(&K,&Config,(xbytesview){Wire,Bytes},&R)==XMQTT_SESSION_DONE&&xmqttSessionInfo(R,&Info)&&Info.ActiveOutgoing==64&&xmqttSessionDestroy(R),"measured validated Restore/Info/Destroy");N=Bytes;}Latency[I]=(xrtTimer()-One)*1000000.0;Checksum+=N;}
    double Elapsed=xrtTimer()-Start;Cpu=imageBenchCpu()-Cpu;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    imageBenchCheck(Checksum==Count*(uint64)Bytes,"benchmark exact byte checksum");imageBenchCheck(xmqttSessionDestroy(S),"benchmark source destruction");xrtFree(Wire);qsort(Latency,(size_t)Count,sizeof(*Latency),imageBenchCompare);
    printf("operations_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p95_us: %.3f\nlatency_p99_us: %.3f\nlatency_p999_us: %.3f\nlatency_max_us: %.3f\nimage_bytes: %zu\nchecksum: %" PRIu64 "\n",(double)Count/Elapsed,Latency[Count/2],Latency[(Count-1)*95/100],Latency[(Count-1)*99/100],Latency[(Count-1)*999/1000],Latency[Count-1],Bytes,Checksum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_op: %.6f\nheap_bytes_per_op: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/(double)Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/(double)Count);
#endif
    printf("process_cpu_ns_per_op: %.3f\n",(double)Cpu/(double)Count);puts("scope:64 QoS1 packets,256-byte payload, existing passive Session gate. Caller-observed API timing includes per-op clock/validation; Restore includes Info/Destroy. CRC, copy, allocation and cleanup remain real; XRT stats exclude caller latency storage and incomplete CRT/kernel costs. Windows CPU resolution may quantize short runs. No durable I/O or network/performance acceptance.");free(Latency);return 0;
}
