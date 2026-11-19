#include "../../../dev/bench/bench_common.h"
#include "../tests/support/ws_reconnect_peer.h"
#include <inttypes.h>
#define mqttExampleCheck testRequire
static uint64 wrBenchCpu(void)
{
#if defined(_WIN32)||defined(_WIN64)
    FILETIME Created,Exited,Kernel,User;testRequire(GetProcessTimes(GetCurrentProcess(),&Created,&Exited,&Kernel,&User)!=0,"process CPU clock");return ((((uint64)Kernel.dwHighDateTime<<32)|Kernel.dwLowDateTime)+(((uint64)User.dwHighDateTime<<32)|User.dwLowDateTime))*100u;
#else
    struct timespec T;testRequire(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&T)==0,"process CPU clock");return (uint64)T.tv_sec*1000000000u+(uint64)T.tv_nsec;
#endif
}
typedef struct mqttBenchSlot {xfuture* Future;double Start;} mqttBenchSlot;
static int mqttBenchCompare(const void* Left,const void* Right)
{double A=*(const double*)Left,B=*(const double*)Right;return A<B?-1:A>B?1:0;}
int main(int Argc,char** Argv)
{
    uint64 Count=Argc>1?strtoull(Argv[1],NULL,10):20000;
    unsigned Version=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):5,Window=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):32;
    bool Instrument=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    unsigned Managed=Argc>5?(unsigned)strtoul(Argv[5],NULL,10):1;
    bool Secure=Argc>6&&strtoul(Argv[6],NULL,10)!=0;
    if(Managed>2 || !Count || Count>1000000 || (Version!=4 && Version!=5) || !Window || Window>64 || Argc>7)return 1;
    wrService P;wrStart(&P,Secure,(xmqttversion)Version,3,XNET_PORT_SELECT);xrtAtomic32Store(&P.NoNotify,1,XMEMORY_RELEASE);xrtAtomic32Store(&P.NoIncoming,1,XMEMORY_RELEASE);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)Version;K.SetupTimeout=10000;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=Version==5;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("ws-managed");xmqttproperty Expiry={0};Expiry.Id=XMQTT_PROPERTY_SESSION_EXPIRY;Expiry.Type=XMQTT_PROPERTY_UINT32;Expiry.Value.Number=60;if(Version==5)Connect.Properties=(xmqttpropertylist){&Expiry,1};xmqttclient* C=NULL;
    xmqttsession* Session=NULL;xmqttreconnect* Controller=NULL;xmqttclientlease Lease={0};
    if(Managed){Controller=wrCreate(&P,60000,false);wrOnline(Controller,1,&Lease);C=Lease.Client;}
    else{xmqttsessionconfig SC;xmqttSessionConfigInit(&SC);SC.Version=K.Version;SC.ClientId=Connect.Body.Connect.ClientId;SC.MaxOutgoing=128;SC.MaxIncoming=64;SC.MaxBytes=16u*1024u*1024u;
        mqttExampleCheck(xmqttSessionCreate(&SC,&Session),"manual passive Session");xmqttclientwsconnectconfig W;xmqttClientWsConnectConfigInit(&W);W.WebSocket.Session=Session;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
        if(Secure){xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.ResumeLimit=0;T.Session.Context=P.Context;T.Session.Verifier=P.Verifier;mqttExampleCheck(xmqttClientWssConnectUri(P.Engine,NULL,(xstrview){P.URI,strlen(P.URI)},1,&K,&W,&T,&Connect,&C),"manual URI/TLS baseline");}else
#endif
        mqttExampleCheck(xmqttClientWsConnectUri(P.Engine,NULL,(xstrview){P.URI,strlen(P.URI)},1,&K,&W,&Connect,&C),"manual URI baseline");}
    xfuture* Ready=xmqttClientReady(C);mqttExampleCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"benchmark CONNACK");
    if(Managed==2){mqttExampleCheck(xmqttReconnectRelease(&Lease),"initial lease release");C=NULL;}
    double* Latencies=malloc((size_t)Count*sizeof(*Latencies));mqttExampleCheck(Latencies!=NULL,"latency storage");mqttBenchSlot Slots[64]={{0}};
    xmqttpacket Publish;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Publish);Publish.Flags=2;Publish.Body.Publish.Topic=XRT_STR_LITERAL("bench/qos1");Publish.Body.Publish.Payload=XRT_BYTES_LITERAL("0123456789abcdef0123456789abcdef");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Submitted=0,Completed=0,Sum=0;uint64 CpuStart=wrBenchCpu();double Start=xrtTimer();
    while(Completed<Count){bool Progress=false;
        for(unsigned I=0;I<Window;++I){mqttBenchSlot* S=Slots+I;
            if(S->Future && xrtFutureState(S->Future)!=XFUTURE_PENDING){double End=xrtTimer();const xmqttwriteresult* R=xrtFutureValue(S->Future);
                mqttExampleCheck(R && R->Status==XMQTT_STATUS_OK && R->Delivery==XMQTT_PEER_ACCEPTED && R->Identifier && R->Puback,"measured PUBACK result");
                Latencies[Completed++]=(End-S->Start)*1000000.0;Sum+=R->Identifier;xrtFutureDestroy(S->Future);S->Future=NULL;Progress=true;}
            if(!S->Future && Submitted<Count){S->Start=xrtTimer();if(Managed==2)mqttExampleCheck(xmqttReconnectAcquire(Controller,&Lease),"per-operation lease");
                mqttExampleCheck(xmqttClientPublishAsync(Managed==2?Lease.Client:C,&Publish,5000,&S->Future)==XMQTT_ADMISSION_ACCEPTED,"measured admission");
                if(Managed==2)mqttExampleCheck(xmqttReconnectRelease(&Lease),"per-operation release");
                ++Submitted;Progress=true;}
        }
        mqttExampleCheck(xrtTimer()-Start<110,"benchmark deadline");if(!Progress)xrtSleep(0);
    }
    double Elapsed=xrtTimer()-Start;uint64 Cpu=wrBenchCpu()-CpuStart;
    uint64 Cycles=Count/65535u,Tail=Count%65535u;
    mqttExampleCheck(Sum==Cycles*65535u*65536u/2u+Tail*(Tail+1u)/2u,"automatic identifier wrap/checksum");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    if(Managed){if(Managed==1)mqttExampleCheck(xmqttReconnectRelease(&Lease),"batch lease release");mqttExampleCheck(xmqttReconnectClose(Controller) && xmqttReconnectWaitFor(Controller,5000)==XWAIT_OK && xmqttReconnectDestroy(Controller),"benchmark controller retirement");}
    else mqttExampleCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C) && xmqttSessionDestroy(Session),"benchmark manual retirement");
    wrStopService(&P);xrtFutureDestroy(Ready);
    qsort(Latencies,(size_t)Count,sizeof(*Latencies),mqttBenchCompare);
    printf("publishes_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p99_us: %.3f\nlatency_max_us: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Elapsed,Latencies[(size_t)(Count/2)],Latencies[(size_t)((Count-1)*99/100)],Latencies[(size_t)(Count-1)],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_publish: %.6f\nheap_bytes_per_publish: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/(double)Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/(double)Count);
#endif
    printf("latency_p95_us: %.3f\nlatency_p999_us: %.3f\nprocess_cpu_ns_per_publish: %.3f\n",Latencies[(size_t)((Count-1)*95/100)],Latencies[(size_t)((Count-1)*999/1000)],(double)Cpu/(double)Count);
    puts("scope: closed-loop client PUBACK completion; real final retirement outside timed window; process CPU/XRT stats include both native peers; CRT/native costs not fully instrumented");
    free(Latencies);return 0;
}
