#include "../tests/test.h"
#include "../tests/support/tls_peer.h"
#define mqttExampleCheck testRequire
#include <inttypes.h>
typedef struct mqttBenchSlot {xfuture* Future;double Start;} mqttBenchSlot;
static int mqttBenchCompare(const void* Left,const void* Right)
{double A=*(const double*)Left,B=*(const double*)Right;return A<B?-1:A>B?1:0;}
int main(int Argc,char** Argv)
{
    uint64 Count=Argc>1?strtoull(Argv[1],NULL,10):20000;
    unsigned Version=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):5,Window=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):32;
    bool Instrument=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    if(!Count || Count>1000000 || (Version!=4 && Version!=5) || !Window || Window>64 || Argc>5)return 1;
    mqttTlsPeer P;mqttTlsStart(&P,(xmqttversion)Version,0);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)Version;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("bench-qos1");xmqttclient* C=NULL;
    xmqttclienttlsconfig TLS;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* Verifier=mqttTlsConfigure(&P,&TLS,Name,&Protocol,Alpn);
#if defined(XMQTT_BENCH_SESSION_TLS)
    xmqttsessionconfig SK;xmqttSessionConfigInit(&SK);SK.Version=K.Version;SK.ClientId=Connect.Body.Connect.ClientId;xmqttsession* Session=NULL;xmqttclientinbox* Inbox=NULL;xmqttclientinboxconfig IK;xmqttClientInboxConfigInit(&IK);IK.Version=K.Version;
    mqttExampleCheck(xmqttSessionCreate(&SK,&Session) && xmqttClientInboxCreate(&IK,&Inbox),"benchmark passive TLS Session/Inbox");Connect.Body.Connect.CleanStart=false;
    mqttExampleCheck(xmqttClientTlsConnectSession(P.Engine,&P.Address,1,&K,&TLS,Session,Inbox,&Connect,&C),"benchmark TLS Session factory");
#else
    mqttExampleCheck(xmqttClientTlsConnect(P.Engine,&P.Address,1,&K,&TLS,&Connect,&C),"benchmark TLS factory");
#endif
    xrtTlsVerifierRelease(Verifier);xfuture* Ready=xmqttClientReady(C);
    mqttExampleCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"benchmark CONNACK");
    double* Latencies=malloc((size_t)Count*sizeof(*Latencies));mqttExampleCheck(Latencies!=NULL,"latency storage");mqttBenchSlot Slots[64]={{0}};
    xmqttpacket Publish;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Publish);Publish.Flags=2;Publish.Body.Publish.Topic=XRT_STR_LITERAL("bench/qos1");Publish.Body.Publish.Payload=XRT_BYTES_LITERAL("0123456789abcdef0123456789abcdef");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Submitted=0,Completed=0,Sum=0;double Start=xrtTimer();
    while(Completed<Count){bool Progress=false;
        for(unsigned I=0;I<Window;++I){mqttBenchSlot* S=Slots+I;
            if(S->Future && xrtFutureState(S->Future)!=XFUTURE_PENDING){double End=xrtTimer();const xmqttwriteresult* R=xrtFutureValue(S->Future);
                mqttExampleCheck(R && R->Status==XMQTT_STATUS_OK && R->Delivery==XMQTT_PEER_ACCEPTED && R->Identifier && R->Puback,"measured PUBACK result");
                Latencies[Completed++]=(End-S->Start)*1000000.0;Sum+=R->Identifier;xrtFutureDestroy(S->Future);S->Future=NULL;Progress=true;}
            if(!S->Future && Submitted<Count){S->Start=xrtTimer();mqttExampleCheck(xmqttClientPublishAsync(C,&Publish,5000,&S->Future)==XMQTT_ADMISSION_ACCEPTED,"measured admission");++Submitted;Progress=true;}
        }
        mqttExampleCheck(xrtTimer()-Start<110,"benchmark deadline");if(!Progress)xrtSleep(0);
    }
    double Elapsed=xrtTimer()-Start;
    uint64 Cycles=Count/65535u,Tail=Count%65535u;
    mqttExampleCheck(Sum==Cycles*65535u*65536u/2u+Tail*(Tail+1u)/2u,"automatic identifier wrap/checksum");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    mqttExampleCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"benchmark physical Client close");mqttTlsStop(&P);xrtFutureDestroy(Ready);
#if defined(XMQTT_BENCH_SESSION_TLS)
    mqttExampleCheck(xmqttSessionDestroy(Session) && xmqttClientInboxDestroy(Inbox),"benchmark passive owner retirement");
#endif
    qsort(Latencies,(size_t)Count,sizeof(*Latencies),mqttBenchCompare);
    printf("publishes_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p99_us: %.3f\nlatency_max_us: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Elapsed,Latencies[(size_t)(Count/2)],Latencies[(size_t)((Count-1)*99/100)],Latencies[(size_t)(Count-1)],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_publish: %.6f\nheap_bytes_per_publish: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/(double)Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/(double)Count);
#endif
    free(Latencies);return 0;
}
