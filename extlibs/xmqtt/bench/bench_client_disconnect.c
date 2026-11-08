/* Whole connection/error/retirement cycles on one persistent local Engine.
 * Includes construction, CONNECT/CONNACK, final diagnostic (5) or abort (311),
 * observer waits and destruction. Not isolated send speed or broker parity. */
#include "../examples/common/client_peer.h"
static int mqttCloseCompare(const void* Left,const void* Right)
{double A=*(const double*)Left,B=*(const double*)Right;return A<B?-1:A>B?1:0;}
int main(int Argc,char** Argv)
{
    unsigned Count=Argc>1?(unsigned)strtoul(Argv[1],NULL,10):100,Version=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):5;bool Instrument=Argc>3 && strtoul(Argv[3],NULL,10)!=0;
    if(!Count || Count>10000 || (Version!=4 && Version!=5) || Argc>4)return 1;
    mqttExamplePeer P;mqttExampleStartMode(&P,(xmqttversion)Version,false,true);double* Latencies=malloc(Count*sizeof(*Latencies));mqttExampleCheck(Latencies!=NULL,"close latency array");
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)Version;K.CloseTimeout=1000;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("bench-disconnect");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    unsigned Sum=0;double Start=xrtTimer();
    for(unsigned I=0;I<Count;++I){double At=xrtTimer();xmqttclient* C=NULL;mqttExampleCheck(xmqttClientConnect(P.Engine,&P.Address,1,&K,&Connect,&C),"cycle factory");xfuture* Ready=xmqttClientReady(C);
        mqttExampleCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"cycle CONNACK");
        mqttExampleCheck(xmqttClientWaitFor(C,5000)==XWAIT_OK,"cycle physical retirement");Latencies[I]=(xrtTimer()-At)*1000000.0;xmqttclientinfo Info;mqttExampleCheck(xmqttClientInfo(C,&Info) && Info.CloseStatus==XMQTT_STATUS_PROTOCOL && Info.LocalReason==(Version==5?0x81:0) && Info.DisconnectBytes==(Version==5?3u:0u) && !Info.DrainTimedOut,"cycle original result");Sum+=Info.LocalReason;
        mqttExampleCheck(xmqttClientDestroy(C),"cycle Client destruction");xrtFutureDestroy(Ready);mqttExampleWait(&P.Closed);
        mqttExampleCheck(xrtAtomic32Load(&P.Disconnects,XMEMORY_ACQUIRE)==(Version==5?I+1:0),"cycle peer-observed final packets");
        if(I+1<Count){xrtNetStreamDestroy(xrtAtomicPtrLoad(&P.Stream,XMEMORY_ACQUIRE));xrtAtomicPtrStore(&P.Stream,NULL,XMEMORY_RELEASE);P.Used=0;xrtAtomic32Store(&P.Closed,0,XMEMORY_RELEASE);}
    }
    double Elapsed=xrtTimer()-Start;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    mqttExampleStop(&P);qsort(Latencies,Count,sizeof(*Latencies),mqttCloseCompare);
    printf("cycles_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p99_us: %.3f\nlatency_max_us: %.3f\nchecksum: %u\n",Count/Elapsed,Latencies[Count/2],Latencies[(Count-1)*99/100],Latencies[Count-1],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_cycle: %.6f\nheap_bytes_per_cycle: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    free(Latencies);return 0;
}
