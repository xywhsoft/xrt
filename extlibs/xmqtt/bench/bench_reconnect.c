#include "../examples/common/client_peer.h"
#include <inttypes.h>
#include <xmqtt/reconnect.h>
static xmqttclientstatus benchManagedFactory(xnetengine* E,uint64 A,const xmqttclientconfig* K,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,ptr Data,xmqttclient** C)
{return xmqttClientConnectSession(E,Data,A,K,S,I,Q,C)?XMQTT_STATUS_OK:XMQTT_STATUS_INITIALIZATION;}
typedef struct mqttBenchSlot {xfuture* Future;double Start;} mqttBenchSlot;
static int mqttBenchCompare(const void* Left,const void* Right)
{double A=*(const double*)Left,B=*(const double*)Right;return A<B?-1:A>B?1:0;}
int main(int Argc,char** Argv)
{
    uint64 Count=Argc>1?strtoull(Argv[1],NULL,10):20000;
    unsigned Version=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):5,Window=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):32;
    bool Instrument=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    unsigned Managed=Argc>5?(unsigned)strtoul(Argv[5],NULL,10):1;
    if(Managed>2 || !Count || Count>1000000 || (Version!=4 && Version!=5) || !Window || Window>64 || Argc>6)return 1;
    mqttExamplePeer P;mqttExampleStart(&P,(xmqttversion)Version);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)Version;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("bench-qos1");xmqttclient* C=NULL;
    xmqttsession* Session=NULL;xmqttclientinbox* Inbox=NULL;xmqttreconnect* Controller=NULL;xmqttclientlease Lease={0};
    xtaskpoolconfig PC={1,8,0};xtaskpool* Pool=xrtTaskPoolCreate(&PC);mqttExampleCheck(Pool!=NULL,"benchmark borrowed Pool");
    if(Managed){xmqttreconnectconfig RC;xmqttReconnectConfigInit(&RC);RC.Client=K;RC.Factory=benchManagedFactory;RC.FactoryData=&P.Address;
        mqttExampleCheck(xmqttReconnectCreate(P.Engine,Pool,1,&RC,&Connect,&Controller),"benchmark controller");double End=xrtTimer()+5;
        while(!xmqttReconnectAcquire(Controller,&Lease)){mqttExampleCheck(xrtTimer()<End,"benchmark online lease");xrtSleep(1);}C=Lease.Client;
    }else{xmqttsessionconfig SC;xmqttSessionConfigInit(&SC);SC.Version=K.Version;SC.ClientId=Connect.Body.Connect.ClientId;xmqttclientinboxconfig IC;xmqttClientInboxConfigInit(&IC);IC.Version=K.Version;
        mqttExampleCheck(xmqttSessionCreate(&SC,&Session) && xmqttClientInboxCreate(&IC,&Inbox) && xmqttClientConnectSession(P.Engine,&P.Address,1,&K,Session,Inbox,&Connect,&C),"manual Session baseline");}
    xfuture* Ready=xmqttClientReady(C);mqttExampleCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"benchmark CONNACK");
    if(Managed==2){mqttExampleCheck(xmqttReconnectRelease(&Lease),"initial lease release");C=NULL;}
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
            if(!S->Future && Submitted<Count){S->Start=xrtTimer();if(Managed==2)mqttExampleCheck(xmqttReconnectAcquire(Controller,&Lease),"per-operation lease");
                mqttExampleCheck(xmqttClientPublishAsync(Managed==2?Lease.Client:C,&Publish,5000,&S->Future)==XMQTT_ADMISSION_ACCEPTED,"measured admission");
                if(Managed==2)mqttExampleCheck(xmqttReconnectRelease(&Lease),"per-operation release");
                ++Submitted;Progress=true;}
        }
        mqttExampleCheck(xrtTimer()-Start<110,"benchmark deadline");if(!Progress)xrtSleep(0);
    }
    double Elapsed=xrtTimer()-Start;
    uint64 Cycles=Count/65535u,Tail=Count%65535u;
    mqttExampleCheck(Sum==Cycles*65535u*65536u/2u+Tail*(Tail+1u)/2u,"automatic identifier wrap/checksum");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    if(Managed){if(Managed==1)mqttExampleCheck(xmqttReconnectRelease(&Lease),"batch lease release");mqttExampleCheck(xmqttReconnectClose(Controller) && xmqttReconnectWaitFor(Controller,5000)==XWAIT_OK && xmqttReconnectDestroy(Controller),"benchmark controller retirement");}
    else mqttExampleCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C) && xmqttSessionDestroy(Session) && xmqttClientInboxDestroy(Inbox),"benchmark manual retirement");
    mqttExampleCheck(xrtTaskPoolClose(Pool) && xrtTaskPoolWaitFor(Pool,5000)==XWAIT_OK && xrtTaskPoolDestroy(Pool),"benchmark Pool join");mqttExampleStop(&P);xrtFutureDestroy(Ready);
    qsort(Latencies,(size_t)Count,sizeof(*Latencies),mqttBenchCompare);
    printf("publishes_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p99_us: %.3f\nlatency_max_us: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Elapsed,Latencies[(size_t)(Count/2)],Latencies[(size_t)((Count-1)*99/100)],Latencies[(size_t)(Count-1)],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_publish: %.6f\nheap_bytes_per_publish: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/(double)Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/(double)Count);
#endif
    free(Latencies);return 0;
}
