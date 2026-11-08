/* Closed-loop QoS1/2 sender or full duplex echo workload. The in-process
 * fixture, observer, CPU clock and native transport are included. No broker,
 * independent comparator, durability or production acceptance is implied. */
#include "../../../dev/bench/bench_common.h"
#include <xmqtt.h>
#include <xrt.h>
#include <math.h>

typedef struct qosPeerSlot {uint64 Sequence;uint16 RequestId;bool Request,Echo,Released;} qosPeerSlot;
typedef struct qosPeer {
    xnetengine* Engine;xnetlistener* Listener;xnetaddr Address;xmqttversion Version;unsigned Qos,Window;bool Echo;
    xatomicptr Stream;xatomic32 Closed,ListenerClosed; xatomic64 RequestDone[64],EchoDone[64];
    xatomic32 RequestIds[64];qosPeerSlot Slots[64];uint16 Requests[65536];
    unsigned char Input[65536];size_t Used;
} qosPeer;
typedef struct qosObserver {xfuture* Future;xmqttmessage* Message;uint64 Sequence,Start;bool Admitting;} qosObserver;
typedef struct qosPhase {uint64 Wall,Cpu,Checksum,Overloads;double First;size_t PrivateBefore,PrivateAfter;} qosPhase;
static void need(bool B,const char* M){if(!B){fprintf(stderr,"[FAIL] QoS workload: %s (%s)\n",M,xrtErrorMessage(xrtGetError()));exit(1);}}
static uint64 cpuNs(void){
#if defined(_WIN32) || defined(_WIN64)
    FILETIME Created,Exited,Kernel,User;need(GetProcessTimes(GetCurrentProcess(),&Created,&Exited,&Kernel,&User)!=0,"process CPU clock");
    return ((((uint64)Kernel.dwHighDateTime<<32)|Kernel.dwLowDateTime)+(((uint64)User.dwHighDateTime<<32)|User.dwLowDateTime))*100u;
#else
    struct timespec T;need(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&T)==0,"process CPU clock");return (uint64)T.tv_sec*1000000000u+(uint64)T.tv_nsec;
#endif
}
static uint64 number(const char* S){char* End=NULL;errno=0;unsigned long long N=strtoull(S,&End,10);need(*S && *S!='-' && End!=S && !*End && !errno,"numeric argument");return (uint64)N;}
static void sendAck(xnetstream* S,uint8 Type,uint16 Id){const unsigned char W[]={Type,2,(unsigned char)(Id>>8),(unsigned char)Id};need(xrtNetStreamSend(S,W,sizeof(W))==XNET_RESULT_OK,"fixture ACK output");}
static void peerRead(xnetstream* S,xnetbuf* Buffer,ptr Data){qosPeer* P=Data;size_t N=xrtNetBufSize(Buffer);need(N<=sizeof(P->Input)-P->Used,"fixture input bound");P->Used+=xrtNetBufRead(Buffer,P->Input+P->Used,N);
    while(P->Used){xmqttwireconfig K;xmqttWireConfigInit(&K);K.Version=P->Version;xmqttframe F;xmqttwirestatus Status=xmqttFrameRead((xbytesview){P->Input,P->Used},&K,&F);need(Status!=XMQTT_WIRE_ERROR,"fixture frame");if(Status==XMQTT_WIRE_MORE)break;
        if(F.Header.Type==XMQTT_PACKET_CONNECT){const unsigned char V5[]={0x20,3,0,0,0},V4[]={0x20,2,0,0};need(xrtNetStreamSend(S,P->Version==XMQTT_VERSION_5?V5:V4,P->Version==XMQTT_VERSION_5?sizeof(V5):sizeof(V4))==XNET_RESULT_OK,"CONNACK");}
        else if(F.Header.Type==XMQTT_PACKET_PUBLISH){xmqttpacket Pub;xmqttproperty Props[8];xmqttcodecbuffers B={Props,8,NULL,0};need((P->Version==XMQTT_VERSION_5?xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Pub,NULL):xmqttPacketRead311(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Pub,NULL))==XMQTT_CODEC_DONE && ((Pub.Flags>>1)&3u)==P->Qos && Pub.Body.Publish.Payload.Size==32 && Pub.Properties.Count==0,"fixture publication");
            uint32 I;uint64 Seq;memcpy(&I,Pub.Body.Publish.Payload.Data,sizeof(I));memcpy(&Seq,Pub.Body.Publish.Payload.Data+8,sizeof(Seq));need(I<P->Window && !P->Requests[Pub.Identifier],"fixture request id/slot");qosPeerSlot* T=P->Slots+I;need(!T->Request && !T->Echo,"fixture slot remains live");T->Sequence=Seq;T->RequestId=Pub.Identifier;T->Request=P->Qos==2;T->Echo=P->Echo;T->Released=false;
            xrtAtomic32Store(P->RequestIds+I,Pub.Identifier,XMEMORY_RELEASE);
            if(P->Qos==2)P->Requests[Pub.Identifier]=(uint16)(I+1);else xrtAtomic64Store(P->RequestDone+I,Seq,XMEMORY_RELEASE);
            sendAck(S,P->Qos==2?0x50:0x40,Pub.Identifier);
            if(P->Echo){/* Separate incoming/outgoing id namespaces: echo id is slot+1. */
                xmqttpacket Echo=Pub;Echo.Identifier=(uint16)(I+1);unsigned char Wire[128];size_t Bytes=0;
                need(P->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),&Echo,XMQTT_SERVER_TO_CLIENT,NULL,&Bytes):xmqttPacketWrite311(Wire,sizeof(Wire),&Echo,XMQTT_SERVER_TO_CLIENT,NULL,&Bytes),"fixture echo encode");need(xrtNetStreamSend(S,Wire,Bytes)==XNET_RESULT_OK,"fixture echo output");}
        }else if(F.Header.Type==XMQTT_PACKET_PUBREL){need(P->Qos==2 && F.Wire.Size==4,"request PUBREL");uint16 Id=(uint16)(((uint16)F.Wire.Data[2]<<8)|F.Wire.Data[3]);unsigned I=P->Requests[Id];need(I && I<=P->Window,"live request PUBREL");qosPeerSlot* T=P->Slots+(I-1);need(T->Request,"request phase");T->Request=false;P->Requests[Id]=0;xrtAtomic64Store(P->RequestDone+(I-1),T->Sequence,XMEMORY_RELEASE);sendAck(S,0x70,Id);
        }else if(F.Header.Type==XMQTT_PACKET_PUBREC || F.Header.Type==XMQTT_PACKET_PUBCOMP || F.Header.Type==XMQTT_PACKET_PUBACK){need(P->Echo && F.Wire.Size==4,"echo ACK");uint16 Id=(uint16)(((uint16)F.Wire.Data[2]<<8)|F.Wire.Data[3]);need(Id && Id<=P->Window,"echo id");qosPeerSlot* T=P->Slots+(Id-1);need(T->Echo,"live echo phase");
            if(F.Header.Type==XMQTT_PACKET_PUBREC){need(P->Qos==2 && !T->Released,"echo PUBREC");T->Released=true;sendAck(S,0x62,Id);}
            else {need((P->Qos==1 && F.Header.Type==XMQTT_PACKET_PUBACK) || (P->Qos==2 && T->Released && F.Header.Type==XMQTT_PACKET_PUBCOMP),"echo completion phase");T->Echo=false;xrtAtomic64Store(P->EchoDone+(Id-1),T->Sequence,XMEMORY_RELEASE);}
        }else need(false,"unexpected fixture packet");
        P->Used-=F.Wire.Size;if(P->Used)memmove(P->Input,P->Input+F.Wire.Size,P->Used);
    }
}
static void peerEnd(xnetstream* S,ptr Data){(void)Data;need(xrtNetStreamClose(S),"peer EOF");}
static void peerClosed(xnetstream* S,xnetresult R,const xerror* E,ptr Data){(void)S;(void)R;(void)E;xrtAtomic32Store(&((qosPeer*)Data)->Closed,1,XMEMORY_RELEASE);}
static bool peerAccept(xnetlistener* L,xnetstream* S,ptr Data){(void)L;qosPeer* P=Data;static const xnetstreamevents Events={NULL,peerRead,peerEnd,NULL,NULL,NULL,peerClosed};need(!xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE) && xrtNetStreamSetEvents(S,&Events,P),"one fixture stream");xrtAtomicPtrStore(&P->Stream,S,XMEMORY_RELEASE);return true;}
static void listenerClosed(xnetlistener* L,ptr Data){(void)L;xrtAtomic32Store(&((qosPeer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static void waitFlag(xatomic32* A){double End=xrtTimer()+5;while(!xrtAtomic32Load(A,XMEMORY_ACQUIRE)){need(xrtTimer()<End,"fixture physical progress");xrtSleep(1);}}
static void peerStart(qosPeer* P,xmqttversion V,unsigned Qos,unsigned Window,bool Echo){memset(P,0,sizeof(*P));P->Version=V;P->Qos=Qos;P->Window=Window;P->Echo=Echo;xrtAtomicPtrInit(&P->Stream,NULL);xrtAtomic32Init(&P->Closed,0);xrtAtomic32Init(&P->ListenerClosed,0);
    for(unsigned I=0;I<64;++I){xrtAtomic64Init(P->RequestDone+I,UINT64_MAX);xrtAtomic64Init(P->EchoDone+I,UINT64_MAX);xrtAtomic32Init(P->RequestIds+I,0);}
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;P->Engine=xrtNetEngineCreate(&E);need(P->Engine && xrtNetEngineStart(P->Engine),"fixture Engine");xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);L.Distribution=XNET_ACCEPT_LOCAL;static const xnetlistenerevents Events={peerAccept,NULL,listenerClosed};P->Listener=xrtNetListen(P->Engine,&L,&Events,NULL,P);need(P->Listener && xrtNetListenerLocal(P->Listener,&P->Address),"fixture listener");
}
static void peerStop(qosPeer* P){need(xrtNetListenerClose(P->Listener),"listener close");waitFlag(&P->ListenerClosed);waitFlag(&P->Closed);xnetstream* S=xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE);need(xrtNetStreamWaitRetired(S,5000)==XWAIT_OK,"peer native retirement");xrtNetStreamDestroy(S);xrtNetListenerDestroy(P->Listener);double End=xrtTimer()+5;while(!xrtNetEngineStop(P->Engine)){need(xrtTimer()<End,"fixture Engine retirement");xrtClearError();xrtSleep(1);}need(xrtNetEngineDestroy(P->Engine),"Engine destruction");}
static qosPhase phase(qosPeer* P,xmqttclient* C,uint64 Base,uint64 Count,double* Latencies){qosObserver Slots[64]={{0}};qosPhase R={0};xmqttclientinfo Info;xmqttClientInfo(C,&Info);R.PrivateBefore=Info.PrivateBytes;uint64 Received=Info.Received,Accepted=Info.Accepted,Assigned=0,Completed=0;uint64 Cpu=cpuNs(),Start=xbenchNowNs();
    while(Completed<Count){bool Progress=false;
        for(unsigned I=0;I<P->Window;++I){qosObserver* S=Slots+I;
            if(S->Future && xrtFutureState(S->Future)!=XFUTURE_PENDING && (!P->Echo || (S->Message && xrtAtomic64Load(P->EchoDone+I,XMEMORY_ACQUIRE)==S->Sequence))){const xmqttwriteresult* W=xrtFutureValue(S->Future);
                need(W && W->Status==XMQTT_STATUS_OK && W->Delivery==XMQTT_PEER_ACCEPTED && xrtAtomic64Load(P->RequestDone+I,XMEMORY_ACQUIRE)==S->Sequence && W->Identifier==xrtAtomic32Load(P->RequestIds+I,XMEMORY_ACQUIRE),"full measured outcome");need(P->Qos==2?(W->PubrecReceived && W->PubcompReceived && W->Pubrec && W->Pubcomp && !W->Puback):W->Puback!=NULL,"ACK metadata");
                double Us=(double)(xbenchNowNs()-S->Start)/1000.0;if(Latencies)Latencies[Completed]=Us;if(S->Sequence==Base)R.First=Us;R.Checksum+=S->Sequence;++Completed;xrtFutureDestroy(S->Future);xmqttMessageRelease(S->Message);S->Future=NULL;S->Message=NULL;Progress=true;
            }
            if(!S->Future && !S->Admitting && Assigned<Count){S->Sequence=Base+Assigned++;S->Start=xbenchNowNs();S->Admitting=true;}
            if(S->Admitting){unsigned char Payload[32]={0};uint32 Index=I;memcpy(Payload,&Index,sizeof(Index));memcpy(Payload+8,&S->Sequence,sizeof(S->Sequence));xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Flags=(uint8)(P->Qos<<1);Pub.Body.Publish.Topic=XRT_STR_LITERAL("bench/qos");Pub.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
                uint64 Elapsed=xbenchNowNs()-S->Start;need(Elapsed<5000000000u,"admission operation deadline");int64 Remaining=(int64)((5000000000u-Elapsed+999999u)/1000000u);xmqttclientadmission A=xmqttClientPublishAsync(C,&Pub,Remaining,&S->Future);need(A==XMQTT_ADMISSION_ACCEPTED || A==XMQTT_ADMISSION_OVERLOADED,"measured admission");if(A==XMQTT_ADMISSION_ACCEPTED){S->Admitting=false;Progress=true;}else ++R.Overloads;
            }
        }
        if(P->Echo){xmqttmessage* M=NULL;while(xmqttClientTakeMessage(C,&M)){const xmqttpacket* Packet=xmqttMessagePacket(M);uint32 I;uint64 Seq;need(Packet->Body.Publish.Payload.Size==32,"echo payload size");memcpy(&I,Packet->Body.Publish.Payload.Data,sizeof(I));memcpy(&Seq,Packet->Body.Publish.Payload.Data+8,sizeof(Seq));need(I<P->Window && Slots[I].Future && !Slots[I].Message && Slots[I].Sequence==Seq && Packet->Identifier==I+1 && ((Packet->Flags>>1)&3u)==P->Qos,"unique echo correlation");Slots[I].Message=M;Progress=true;}}
        need(xbenchNowNs()-Start<110000000000u,"phase deadline");if(!Progress)xrtSleep(0);
    }
    do{xmqttClientInfo(C,&Info);need(xbenchNowNs()-Start<110000000000u,"physical request tail deadline");if(Info.Requests)xrtSleep(0);}while(Info.Requests);
    R.Wall=xbenchNowNs()-Start;R.Cpu=cpuNs()-Cpu;R.PrivateAfter=Info.PrivateBytes;need(!Info.OutgoingBytes && !Info.Messages && Info.Received-Received==(P->Echo?Count:0) && Info.Accepted-Accepted==Count && Info.Completed==Info.Accepted && R.Checksum==Count*(2*Base+Count-1)/2,"measured checksum/accounting");return R;
}
static int compare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return X<Y?-1:X>Y?1:0;}
#if defined(XRT_FEATURE_MEMORY_STATS)
static void statsPrint(const char* Name,const xmemstats* A,const xmemstats* B){printf("%s_xrt_blocks: %" PRIu64 "\n%s_xrt_block_bytes: %" PRIu64 "\n%s_xrt_frees: %" PRIu64 "\n%s_xrt_free_bytes: %" PRIu64 "\n",Name,B->BlockAllocCalls-A->BlockAllocCalls,Name,B->BlockAllocBytes-A->BlockAllocBytes,Name,B->BlockFreeCalls-A->BlockFreeCalls,Name,B->BlockFreeBytes-A->BlockFreeBytes);printf("%s_malloc_calls: %" PRIu64 "\n%s_malloc_bytes: %" PRIu64 "\n%s_calloc_calls: %" PRIu64 "\n%s_calloc_bytes: %" PRIu64 "\n%s_backing_alloc_calls: %" PRIu64 "\n%s_backing_alloc_bytes: %" PRIu64 "\n",Name,B->MallocCalls-A->MallocCalls,Name,B->MallocBytes-A->MallocBytes,Name,B->CallocCalls-A->CallocCalls,Name,B->CallocBytes-A->CallocBytes,Name,B->BackingAllocCalls-A->BackingAllocCalls,Name,B->BackingAllocBytes-A->BackingAllocBytes);}
#endif
int main(int Argc,char** Argv){uint64 Values[]={200000,5,2,32,1,0,4096};need(Argc<=8,"COUNT VERSION QOS WINDOW ECHO INSTRUMENT WARMUP");for(int I=1;I<Argc;++I)Values[I-1]=number(Argv[I]);uint64 Count=Values[0],Warm=Values[6];unsigned Version=(unsigned)Values[1],Qos=(unsigned)Values[2],Window=(unsigned)Values[3];bool Echo=Values[4]!=0,Instrument=Values[5]!=0;
    need(Count && Count<=1000000 && Warm<=1000000 && (Values[1]==4 || Values[1]==5) && (Values[2]==1 || Values[2]==2) && Window && Values[3]<=64 && Values[4]<=1 && Values[5]<=1,"argument ranges");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats LifeBefore,MeasureBefore,MeasureAfter,LifeAfter;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&LifeBefore);
#else
    need(!Instrument,"memory-stats module required");
#endif
    uint64 LifeCpu=cpuNs(),LifeStart=xbenchNowNs();qosPeer P;peerStart(&P,(xmqttversion)Version,Qos,Window,Echo);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)Version;xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("bench-qos2");xmqttclient* C=NULL;need(xmqttClientConnect(P.Engine,&P.Address,1,&K,&Connect,&C),"benchmark factory");xfuture* Ready=xmqttClientReady(C);need(Ready && xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"readiness");xmqttclientinfo Info;double End=xrtTimer()+5;do{xmqttClientInfo(C,&Info);need(xrtTimer()<End,"startup native references");if(Info.StartupBytes)xrtSleep(0);}while(Info.StartupBytes);
    double* Latencies=malloc((size_t)Count*sizeof(*Latencies));need(Latencies!=NULL,"observer latency storage");uint64 SetupWall=xbenchNowNs()-LifeStart,SetupCpu=cpuNs()-LifeCpu;qosPhase Warmed={0};if(Warm)Warmed=phase(&P,C,0,Warm,NULL);
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&MeasureBefore);
#endif
    qosPhase R=phase(&P,C,Warm,Count,Latencies);
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&MeasureAfter);
#endif
    uint64 StopCpu=cpuNs(),StopStart=xbenchNowNs();need(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"benchmark client physical close");peerStop(&P);xrtFutureDestroy(Ready);uint64 StopWall=xbenchNowNs()-StopStart,StopCpuUsed=cpuNs()-StopCpu,LifeWall=xbenchNowNs()-LifeStart,LifeCpuUsed=cpuNs()-LifeCpu;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&LifeAfter);xrtMemStatsEnable(false);
#endif
    qsort(Latencies,(size_t)Count,sizeof(*Latencies),compare);printf("exchanges_per_sec: %.6f\nlatency_p50_us: %.6f\nlatency_p99_us: %.6f\nlatency_max_us: %.6f\nfirst_exchange_us: %.6f\nprocess_cpu_ns_per_exchange: %.6f\nphase_wall_ns: %" PRIu64 "\nphase_cpu_ns: %" PRIu64 "\nchecksum: %" PRIu64 "\nadmission_overloads: %" PRIu64 "\nprivate_before: %zu\nprivate_after: %zu\n",(double)Count*1000000000.0/(double)R.Wall,Latencies[(size_t)(Count/2)],Latencies[(size_t)((Count-1)*99/100)],Latencies[(size_t)(Count-1)],R.First,(double)R.Cpu/(double)Count,R.Wall,R.Cpu,R.Checksum,R.Overloads,R.PrivateBefore,R.PrivateAfter);
    printf("setup_wall_ns: %" PRIu64 "\nsetup_cpu_ns: %" PRIu64 "\nwarmup_wall_ns: %" PRIu64 "\nwarmup_cpu_ns: %" PRIu64 "\nteardown_wall_ns: %" PRIu64 "\nteardown_cpu_ns: %" PRIu64 "\nlifecycle_wall_ns: %" PRIu64 "\nlifecycle_cpu_ns: %" PRIu64 "\nobserver_latency_bytes: %zu\nfixture_bytes: %zu\n",SetupWall,SetupCpu,Warmed.Wall,Warmed.Cpu,StopWall,StopCpuUsed,LifeWall,LifeCpuUsed,(size_t)Count*sizeof(*Latencies),sizeof(P));
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument){statsPrint("phase",&MeasureBefore,&MeasureAfter);statsPrint("lifecycle",&LifeBefore,&LifeAfter);printf("heap_blocks_per_exchange: %.6f\nheap_bytes_per_exchange: %.6f\n",(double)(MeasureAfter.BlockAllocCalls-MeasureBefore.BlockAllocCalls)/(double)Count,(double)(MeasureAfter.BlockAllocBytes-MeasureBefore.BlockAllocBytes)/(double)Count);}
#endif
    printf("mqtt_version: %u\nqos: %u\nwindow: %u\necho: %u\ninstrumented: %u\nmeasured_ops: %" PRIu64 "\nwarmup_ops: %" PRIu64 "\npayload_bytes: 32\nphase_completion: %s\n",Version,Qos,Window,Echo?1u:0u,Instrument?1u:0u,Count,Warm,Echo?"client-final-ack+echo-message+peer-final-ack+request-retirement":"client-final-ack+request-retirement");
#if defined(_WIN32) || defined(_WIN64)
    puts("process_cpu_clock: GetProcessTimes (100ns units; OS resolution applies)");
#else
    puts("process_cpu_clock: CLOCK_PROCESS_CPUTIME_ID");
#endif
    printf("latency_completion: %s\n",Echo?"client-final-ack+echo-message+peer-final-ack":"client-final-ack");
    printf("wire_frames_per_exchange: %u\nwire_bytes_per_exchange: %u\naffinity_policy: inherited; benchmark does not change affinity\n",(Echo?2u:1u)*(Qos==2?4u:2u),(Echo?2u:1u)*(47u+(Version==5?1u:0u)+(Qos==2?12u:4u)));
#if defined(XRT_FEATURE_MEMORY_STATS)
    puts("memory_stats_compiled: 1");
#else
    puts("memory_stats_compiled: 0");
#endif
    free(Latencies);return 0;
}
