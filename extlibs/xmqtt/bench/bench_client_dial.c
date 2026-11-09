#include "../examples/common/dial_peer.h"
static int mqttDialCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return X<Y?-1:X>Y?1:0;}
int main(int Argc,char** Argv)
{
    unsigned Count=Argc>1?(unsigned)strtoul(Argv[1],NULL,10):200,Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):2;bool Instrument=Argc>3 && strtoul(Argv[3],NULL,10)!=0;
    if(!Count || Count>100000 || Mode>2 || Argc>4)return 2;
    mqttDialPeer P;mqttDialStart(&P,XMQTT_VERSION_5,0);xnetaddrlist* Addresses=xrtNetAddrListCreate(&P.Peer.Address,1);mqttExampleCheck(Addresses!=NULL,"benchmark address list");
    double *Times=malloc(Count*sizeof(*Times)),*Setup=malloc(Count*sizeof(*Setup));mqttExampleCheck(Times && Setup,"benchmark latency arrays");
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("dial-bench");
    xmqttpacket Publish;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Publish);Publish.Flags=2;Publish.Body.Publish.Topic=XRT_STR_LITERAL("dial/bench");Publish.Body.Publish.Payload=XRT_BYTES_LITERAL("payload");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    double Begin=xrtTimer();
    for(unsigned I=0;I<Count;++I){double At=xrtTimer();xmqttclient* C=NULL;
        bool OK=Mode==0?xmqttClientConnect(P.Peer.Engine,&P.Peer.Address,1,NULL,&Connect,&C):Mode==1?xmqttClientDialAddresses(P.Peer.Engine,Addresses,1,NULL,NULL,&Connect,&C):
            xmqttClientDial(P.Peer.Engine,P.Resolver,XRT_STR_LITERAL("mqtt.fixture"),P.Peer.Address.Port,1,NULL,NULL,&Connect,&C);
        mqttExampleCheck(OK,"benchmark factory");xfuture* R=xmqttClientReady(C);mqttExampleCheck(xrtFutureWaitFor(R,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(R))->Status==XMQTT_STATUS_OK,"benchmark readiness");Setup[I]=(xrtTimer()-At)*1000000;
        xfuture* F=NULL;mqttExampleCheck(xmqttClientPublish(C,&Publish,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)xrtFutureValue(F))->Status==XMQTT_STATUS_OK,"benchmark QoS1");
        mqttExampleCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"benchmark Client retirement");
        xnetstream* S=xrtAtomicPtrLoad(&P.Peer.Stream,XMEMORY_ACQUIRE);mqttExampleCheck(S && xrtNetStreamWaitRetired(S,5000)==XWAIT_OK,"benchmark peer retirement");xrtNetStreamDestroy(S);
        xrtAtomicPtrStore(&P.Peer.Stream,NULL,XMEMORY_RELEASE);xrtAtomic32Store(&P.Peer.Closed,0,XMEMORY_RELEASE);P.Peer.Used=0;xrtFutureDestroy(R);xrtFutureDestroy(F);Times[I]=(xrtTimer()-At)*1000000;
    }
    double Elapsed=xrtTimer()-Begin;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    xrtNetAddrListDestroy(Addresses);mqttDialStop(&P);qsort(Times,Count,sizeof(*Times),mqttDialCompare);qsort(Setup,Count,sizeof(*Setup),mqttDialCompare);
    printf("connections_per_sec: %.3f\nsetup_p99_us: %.3f\ncycle_p99_us: %.3f\nmode: %u\nmeasured_ops: %u\n",Count/Elapsed,Setup[(Count-1)*99/100],Times[(Count-1)*99/100],Mode,Count);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("process_heap_blocks_per_cycle: %.6f\nprocess_heap_bytes_per_cycle: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    free(Times);free(Setup);return 0;
}
