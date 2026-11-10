#define MQTT_EXAMPLE_SUBSCRIPTIONS
#define MQTT_EXAMPLE_SUBSCRIPTION_TOPICS 1024
#include "../examples/common/client_peer.h"
#include <xmqtt/reconnect_subscriptions.h>
#include <inttypes.h>
typedef struct intentBench {xmutex Gate;xcond Changed;uint64 Settled;} intentBench;
static xmqttclientstatus intentFactory(xnetengine* E,uint64 A,const xmqttclientconfig* K,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,ptr Data,xmqttclient** C)
{return xmqttClientConnectSession(E,Data,A,K,S,I,Q,C)?XMQTT_STATUS_OK:XMQTT_STATUS_INITIALIZATION;}
static void intentNotify(xmqttreconnect* R,ptr Data)
{
    xmqttsubscriptionsinfo I;intentBench* B=Data;mqttExampleCheck(xmqttReconnectSubscriptionsInfo(R,&I),"benchmark synchronization snapshot");
    if(I.Synchronized){mqttExampleCheck(I.Status==XMQTT_STATUS_OK,"benchmark filter grants");xrtMutexLock(&B->Gate);B->Settled=I.SettledRevision;xrtCondBroadcast(&B->Changed);xrtMutexUnlock(&B->Gate);}
}
static void intentWait(intentBench* B,uint64 Revision)
{
    xrtMutexLock(&B->Gate);while(B->Settled<Revision)mqttExampleCheck(xrtCondWaitFor(&B->Changed,&B->Gate,10000)==XWAIT_OK,"benchmark completion deadline");xrtMutexUnlock(&B->Gate);
}
static int intentCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return X<Y?-1:X>Y;}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):1000;
    unsigned Version=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):5;
    size_t Filters=Argc>3?(size_t)strtoull(Argv[3],NULL,10):1024,Batch=Argc>4?(size_t)strtoull(Argv[4],NULL,10):32;
    bool Instrument=Argc>5 && strtoul(Argv[5],NULL,10)!=0;
    bool BatchUpdates=Argc>6 && strtoul(Argv[6],NULL,10)!=0;
    if(!Count || Count>200000 || !Filters || Filters>4096 || !Batch || Batch>1024 || Batch>Filters || (BatchUpdates && Filters%Batch) || (Version!=4 && Version!=5) || Argc>7)return 1;
    mqttExamplePeer P;mqttExampleStart(&P,(xmqttversion)Version);xtaskpoolconfig PC={1,8,0};xtaskpool* Pool=xrtTaskPoolCreate(&PC);mqttExampleCheck(Pool!=NULL,"benchmark Pool");
    intentBench B={0};mqttExampleCheck(xrtMutexInit(&B.Gate) && xrtCondInit(&B.Changed),"benchmark completion condition");
    xmqttsubscription* Topics=calloc(Filters,sizeof(*Topics));char (*Names)[32]=calloc(Filters,sizeof(*Names));double* Latency=malloc(Count*sizeof(*Latency));mqttExampleCheck(Topics && Names && Latency,"benchmark caller buffers");
    for(size_t I=0;I<Filters;++I){int N=snprintf(Names[I],sizeof(*Names),"intent/%zu/+",I);mqttExampleCheck(N>0 && (size_t)N<sizeof(*Names),"benchmark filter");Topics[I]=(xmqttsubscription){{Names[I],(size_t)N},1};}
    xmqttreconnectconfig K;xmqttReconnectConfigInit(&K);K.Client.Version=(xmqttversion)Version;if(Filters>K.Client.Codec.MaxTopics)K.Client.Codec.MaxTopics=Filters;K.Factory=intentFactory;K.FactoryData=&P.Address;K.Notify=intentNotify;K.User=&B;
    xmqttsubscriptionsconfig S;xmqttSubscriptionsConfigInit(&S);S.MaxFilters=Filters;S.MaxBatch=Batch;S.MaxBytes=8u*1024u*1024u;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.ClientId=XRT_STR_LITERAL("intent-benchmark");Connect.Body.Connect.CleanStart=true;
    xmqttpacket Initial;xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Initial);Initial.Body.Topics=(xmqttsubscriptionlist){Topics,Filters};
    xmqttproperty Id={XMQTT_PROPERTY_SUBSCRIPTION_ID,XMQTT_PROPERTY_VARIABLE,{.Number=42}};if(Version==5)Initial.Properties=(xmqttpropertylist){&Id,1};
    double Start=xrtTimer();xmqttreconnect* R=NULL;mqttExampleCheck(xmqttReconnectCreateSubscribed(P.Engine,Pool,1,&K,&S,&Connect,&Initial,1,&R),"benchmark initial intents");intentWait(&B,1);double Cold=(xrtTimer()-Start)*1000;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    Start=xrtTimer();uint64 Sum=0;
    for(size_t N=0;N<Count;++N){size_t Width=BatchUpdates?Batch:1,Offset=(N*Width)%Filters;uint64 Revision=0;double At=xrtTimer();
        for(size_t I=0;I<Width;++I)Topics[Offset+I].Options=(uint8)(N%3);
        xmqttpacket Update=Initial;Update.Body.Topics=(xmqttsubscriptionlist){Topics+Offset,Width};
        mqttExampleCheck(xmqttReconnectSubscribeIntent(R,&Update,&Revision)==XMQTT_ADMISSION_ACCEPTED,"benchmark update admission");intentWait(&B,Revision);Latency[N]=(xrtTimer()-At)*1000000;Sum+=Revision;
    }
    double Seconds=xrtTimer()-Start;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    xmqttsubscriptionsinfo Info;mqttExampleCheck(xmqttReconnectSubscriptionsInfo(R,&Info) && Info.Synchronized && Info.Desired==Filters && Info.SettledRevision==Count+1 && Info.Requests==(Filters+Batch-1)/Batch+Count && Sum==(uint64)Count*(Count+3)/2,"benchmark exact protocol/accounting");
    mqttExampleCheck(xmqttReconnectClose(R) && xmqttReconnectWaitFor(R,10000)==XWAIT_OK && xmqttReconnectDestroy(R),"benchmark physical controller retirement");mqttExampleCheck(xrtTaskPoolClose(Pool) && xrtTaskPoolWaitFor(Pool,10000)==XWAIT_OK && xrtTaskPoolDestroy(Pool),"benchmark Pool retirement");mqttExampleStop(&P);xrtCondUnit(&B.Changed);xrtMutexUnit(&B.Gate);
    qsort(Latency,Count,sizeof(*Latency),intentCompare);
    printf("initial_restore_ms: %.3f\nupdates_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p99_us: %.3f\nlatency_max_us: %.3f\nregistry_bytes: %zu\nchecksum: %" PRIu64 "\n",Cold,(double)Count/Seconds,Latency[Count/2],Latency[(Count-1)*99/100],Latency[Count-1],Info.Bytes,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_update: %.6f\nheap_bytes_per_update: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    free(Topics);free(Names);free(Latency);return 0;
}
