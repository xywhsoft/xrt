/* Combined QoS1 publish/echo workload, including both directions' PUBACKs.
 * The local fixture and observer are included; this is not a receiver-only
 * microbenchmark, independent broker comparison or production acceptance. */
#include "../examples/common/client_peer.h"
#include <inttypes.h>
typedef struct mqttEchoSlot {xfuture* Future;xmqttmessage* Message;uint64 Sequence;double Start;} mqttEchoSlot;
static int mqttEchoCompare(const void* Left,const void* Right)
{double A=*(const double*)Left,B=*(const double*)Right;return A<B?-1:A>B?1:0;}
int main(int Argc,char** Argv)
{
    uint64 Count=Argc>1?strtoull(Argv[1],NULL,10):20000;unsigned Version=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):5,Window=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):32;
    bool Instrument=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    if(!Count || Count>1000000 || (Version!=4 && Version!=5) || !Window || Window>64 || Argc>5)return 1;
    mqttExamplePeer P;mqttExampleStart(&P,(xmqttversion)Version);P.EchoQosOne=true;
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)Version;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("bench-receive");xmqttclient* C=NULL;
    mqttExampleCheck(xmqttClientConnect(P.Engine,&P.Address,1,&K,&Connect,&C),"echo factory");xfuture* Ready=xmqttClientReady(C);
    mqttExampleCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"echo CONNACK");
    double* Latencies=malloc((size_t)Count*sizeof(*Latencies));mqttExampleCheck(Latencies!=NULL,"echo latency storage");mqttEchoSlot Slots[64]={{0}};
    xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Flags=2;Pub.Body.Publish.Topic=XRT_STR_LITERAL("bench/receive");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Submitted=0,Completed=0,Sum=0;double Start=xrtTimer();
    while(Completed<Count){bool Progress=false;
        for(unsigned I=0;I<Window;++I){mqttEchoSlot* S=Slots+I;
            if(S->Future && S->Message && xrtFutureState(S->Future)!=XFUTURE_PENDING){const xmqttwriteresult* R=xrtFutureValue(S->Future);
                mqttExampleCheck(R && R->Status==XMQTT_STATUS_OK && R->Delivery==XMQTT_PEER_ACCEPTED && R->Identifier==xmqttMessagePacket(S->Message)->Identifier,"paired PUBACK/echo");
                Latencies[Completed++]=(xrtTimer()-S->Start)*1000000.0;Sum+=S->Sequence;xrtFutureDestroy(S->Future);xmqttMessageRelease(S->Message);S->Future=NULL;S->Message=NULL;Progress=true;}
            if(!S->Future && Submitted<Count){unsigned char Payload[32]={0};uint32 Index=I;memcpy(Payload,&Index,sizeof(Index));memcpy(Payload+8,&Submitted,sizeof(Submitted));Pub.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
                S->Sequence=Submitted;S->Start=xrtTimer();mqttExampleCheck(xmqttClientPublishAsync(C,&Pub,5000,&S->Future)==XMQTT_ADMISSION_ACCEPTED,"echo admission");++Submitted;Progress=true;}
        }
        xmqttmessage* M=NULL;while(xmqttClientTakeMessage(C,&M)){const xmqttpacket* Packet=xmqttMessagePacket(M);uint32 Index;uint64 Sequence;
            mqttExampleCheck(Packet->Body.Publish.Payload.Size==32,"echo size");memcpy(&Index,Packet->Body.Publish.Payload.Data,sizeof(Index));memcpy(&Sequence,(const unsigned char*)Packet->Body.Publish.Payload.Data+8,sizeof(Sequence));
            mqttExampleCheck(Index<Window && Slots[Index].Future && !Slots[Index].Message && Slots[Index].Sequence==Sequence,"echo correlation");Slots[Index].Message=M;Progress=true;}
        mqttExampleCheck(xrtTimer()-Start<110,"echo deadline");if(!Progress)xrtSleep(0);
    }
    while(xrtAtomic32Load(&P.Acks,XMEMORY_ACQUIRE)!=Count){mqttExampleCheck(xrtTimer()-Start<110,"final receiver PUBACKs");xrtSleep(0);}
    double Elapsed=xrtTimer()-Start;mqttExampleCheck(Sum==Count*(Count-1)/2,"echo checksum");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    mqttExampleCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"echo physical close");mqttExampleStop(&P);xrtFutureDestroy(Ready);
    qsort(Latencies,(size_t)Count,sizeof(*Latencies),mqttEchoCompare);
    printf("exchanges_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p99_us: %.3f\nlatency_max_us: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Elapsed,Latencies[(size_t)(Count/2)],Latencies[(size_t)((Count-1)*99/100)],Latencies[(size_t)(Count-1)],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_exchange: %.6f\nheap_bytes_per_exchange: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/(double)Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/(double)Count);
#endif
    free(Latencies);return 0;
}
