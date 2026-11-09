#include "../examples/common/auth_peer.h"
#include <inttypes.h>
static void mqttAuthBenchNotify(xmqttclient* C,ptr User)
{
    (void)User;xmqttauthticket* T=NULL;
    while(xmqttClientTakeAuth(C,&T))mqttAuthAnswer(T,false);
}
static int mqttAuthCompare(const void* Left,const void* Right)
{double A=*(const double*)Left,B=*(const double*)Right;return A<B?-1:A>B?1:0;}
int main(int Argc,char** Argv)
{
    uint64 Count=Argc>1?strtoull(Argv[1],NULL,10):2000;bool Instrument=Argc>2 && strtoul(Argv[2],NULL,10)!=0;
    if(!Count || Count>100000 || Argc>3)return 2;
    mqttAuthPeer P;mqttAuthStart(&P,0);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=mqttAuthBenchNotify;
    xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_CONNECT,0,Props,false);xmqttclient* C=NULL;
    mqttAuthCheck(xmqttClientConnectAuth(P.Engine,&P.Address,1,&K,NULL,&Q,&C),"benchmark connection");xfuture* Ready=xmqttClientReady(C);
    mqttAuthCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"benchmark verified readiness");
    double* Times=malloc((size_t)Count*sizeof(*Times));mqttAuthCheck(Times!=NULL,"latency storage");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    Q=mqttAuthPacket(XMQTT_PACKET_AUTH,0x19,Props,false);double Begin=xrtTimer();uint64 Accepted=0;
    for(uint64 I=0;I<Count;++I){double Start=xrtTimer();xfuture* F=NULL;
        mqttAuthCheck(xmqttClientReauthenticateAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"measured reauth admission");
        mqttAuthCheck(xrtFutureWaitFor(F,5000)==XWAIT_OK,"measured reauth completion");const xmqttauthresult* R=xrtFutureValue(F);
        mqttAuthCheck(R && R->Status==XMQTT_STATUS_OK && R->Final && xmqttMessagePacket(R->Final)->Reason==0,"measured verified auth outcome");
        Accepted+=R->AcceptedBytes;Times[I]=(xrtTimer()-Start)*1000000.0;xrtFutureDestroy(F);
    }
    double Elapsed=xrtTimer()-Begin;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    mqttAuthCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"benchmark physical Client retirement");mqttAuthStop(&P);xrtFutureDestroy(Ready);
    qsort(Times,(size_t)Count,sizeof(*Times),mqttAuthCompare);
    printf("reauths_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p99_us: %.3f\nlatency_max_us: %.3f\naccepted_auth_bytes: %" PRIu64 "\n",(double)Count/Elapsed,Times[Count/2],Times[(Count-1)*99/100],Times[Count-1],Accepted);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("process_heap_blocks_per_reauth: %.6f\nprocess_heap_bytes_per_reauth: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/(double)Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/(double)Count);
#endif
    free(Times);return 0;
}
