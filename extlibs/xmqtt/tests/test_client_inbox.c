#include "support/inbox_peer.h"
static xmqttpacket inboxConnectPacket(void)
{xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("inbox");return Q;}
static const void* inboxResult(xfuture* F)
{testRequire(F && xrtFutureWaitFor(F,5000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"Inbox Future");return xrtFutureValue(F);}
static xmqttclientinbox* inboxCreate(xmqttclientconfig* K)
{
    xmqttclientinboxconfig B;xmqttClientInboxConfigInit(&B);B.Version=K->Version;B.MaxObjects=K->MaxObjects;B.MaxObjectBytes=K->MaxObjectBytes;B.MaxMessages=K->MaxMessages;B.MaxMessageBytes=K->MaxMessageBytes;
    xmqttclientinbox* I=NULL;testRequire(xmqttClientInboxCreate(&B,&I),"Inbox create");return I;
}
static xmqttclient* inboxConnect(inboxPeer* P,xmqttclientconfig* K,xmqttclientinbox* I,xfuture** Ready)
{
    xmqttpacket Q=inboxConnectPacket();xmqttclient* C=NULL;testRequire(xmqttClientConnectInbox(P->Engine,&P->Address,0,K,I,&Q,&C),"Inbox factory");
    *Ready=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)inboxResult(*Ready))->Status==XMQTT_STATUS_OK,"Inbox readiness");return C;
}
static void inboxClose(xmqttclient* C)
{testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"Inbox Client retirement");}
static void inboxPublish(inboxPeer* P,unsigned Qos,unsigned Value)
{
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(uint8)(Qos<<1);Q.Identifier=Qos?7:0;Q.Body.Publish.Topic=XRT_STR_LITERAL("inbox/topic");unsigned char V=(unsigned char)Value;Q.Body.Publish.Payload=(xbytesview){&V,1};
    unsigned char Wire[128];size_t N=0;testRequire(P->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),&Q,XMQTT_SERVER_TO_CLIENT,NULL,&N):xmqttPacketWrite311(Wire,sizeof(Wire),&Q,XMQTT_SERVER_TO_CLIENT,NULL,&N),"Inbox message encode");inboxSend(P,Wire,N);
}
static xmqttclientinboxinfo inboxInfo(xmqttclientinbox* I)
{xmqttclientinboxinfo B;testRequire(xmqttClientInboxInfo(I,&B),"Inbox snapshot");return B;}
static void inboxMessages(xmqttclientinbox* I,size_t N)
{double End=xrtTimer()+5;while(inboxInfo(I).Messages!=N){testRequire(xrtTimer()<End,"Inbox message progress");xrtSleep(1);}}
static void inboxCheckMessage(xmqttmessage* M,unsigned Value)
{const xmqttpacket* Q=xmqttMessagePacket(M);testRequire(Q && Q->Body.Publish.Topic.Size==11 && !memcmp(Q->Body.Publish.Topic.Data,"inbox/topic",11) && Q->Body.Publish.Payload.Size==1 && Q->Body.Publish.Payload.Data[0]==Value,"Inbox immutable message");}
static void inboxLifecycle(xmqttversion V)
{
    xmqttclientconfig K={0};xmqttClientConfigInit(&K);K.Version=V;xmqttclientinbox* I=inboxCreate(&K);xfuture *Ready[3],*Write[3];xmqttmessage* Taken=NULL;
    for(unsigned G=0;G<3;++G){inboxPeer P;inboxStart(&P,V);xmqttclient* C=inboxConnect(&P,&K,I,Ready+G);
        testRequire(inboxInfo(I).Attached && !xmqttClientInboxDestroy(I),"attached Inbox cannot retire");xrtClearError();
        xmqttclient* Other=(xmqttclient*)(uintptr_t)1;xmqttpacket Q=inboxConnectPacket();testRequire(!xmqttClientConnectInbox(P.Engine,&P.Address,1,&K,I,&Q,&Other) && Other==(xmqttclient*)(uintptr_t)1,"exclusive attachment");xrtClearError();
        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("out");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("held");testRequire(xmqttClientPublishAsync(C,&Q,5000,Write+G)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)inboxResult(Write[G]))->Status==XMQTT_STATUS_OK,"retained ACK/result");
        inboxPublish(&P,G,10+G);inboxMessages(I,G+1);if(G==1)inboxWait(&P.Acks,1);if(G==2)inboxWait(&P.Comps,1);
        xmqttclientinfo CI;xmqttClientInfo(C,&CI);testRequire(CI.Messages==G+1 && CI.Received==1 && CI.Objects==inboxInfo(I).Objects,"shared gauges/per-connection counters");
        testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK,"closed Client wait");testRequire(!xmqttClientInboxDestroy(I),"attachment survives logical/physical wait until Destroy");xrtClearError();
        testRequire(xmqttClientDestroy(C),"destroy old Client");inboxStop(&P);testRequire(!inboxInfo(I).Attached && inboxInfo(I).Messages==G+1,"messages survive old Engine");
    }
    testRequire(xmqttClientInboxTake(I,&Taken),"offline take");inboxCheckMessage(Taken,10);
    xmqttmessage* M;testRequire(xmqttClientInboxTake(I,&M),"FIFO second take");inboxCheckMessage(M,11);xmqttMessageRelease(M);
    testRequire(xmqttClientInboxDestroy(I),"offline destroy releases untaken third message");inboxCheckMessage(Taken,10);xmqttMessageRelease(Taken);
    for(unsigned G=0;G<3;++G){const xmqttwriteresult* W=inboxResult(Write[G]);testRequire(W->Puback && xmqttMessagePacket(W->Puback)->Type==XMQTT_PACKET_PUBACK,"result survives Inbox/all Engines");xrtFutureDestroy(Write[G]);xrtFutureDestroy(Ready[G]);}
}
static void inboxQueueLimit(xmqttversion V,bool ByteLimit)
{
    xmqttclientconfig K={0};xmqttClientConfigInit(&K);K.Version=V;K.MaxMessages=1;xmqttclientinbox* I=inboxCreate(&K);inboxPeer P;inboxStart(&P,V);xfuture* Ready;xmqttclient* C=inboxConnect(&P,&K,I,&Ready);
    inboxPublish(&P,1,31);inboxMessages(I,1);inboxWait(&P.Acks,1);size_t Bytes=inboxInfo(I).MessageBytes;inboxClose(C);inboxStop(&P);xrtFutureDestroy(Ready);
    if(ByteLimit){/* Recreate an equally sized queue with a one-message byte cap. */
        xmqttmessage* M;testRequire(xmqttClientInboxTake(I,&M),"limit calibration take");xmqttMessageRelease(M);testRequire(xmqttClientInboxDestroy(I),"limit calibration retire");K.MaxMessages=8;K.MaxMessageBytes=Bytes;I=inboxCreate(&K);
        inboxStart(&P,V);C=inboxConnect(&P,&K,I,&Ready);inboxPublish(&P,1,31);inboxMessages(I,1);inboxWait(&P.Acks,1);inboxClose(C);inboxStop(&P);xrtFutureDestroy(Ready);
    }
    inboxStart(&P,V);C=inboxConnect(&P,&K,I,&Ready);inboxPublish(&P,1,32);testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"shared queue closes overfull new Client");xmqttclientinfo CI;xmqttClientInfo(C,&CI);
    testRequire(CI.CloseStatus==XMQTT_STATUS_LIMIT && CI.Received==0 && inboxInfo(I).Messages==1 && !xrtAtomic32Load(&P.Acks,XMEMORY_ACQUIRE),"no ACK or drop of accepted old message on overflow");
    xmqttmessage* M=NULL;testRequire(xmqttClientTakeMessage(C,&M),"Client Take consumes earlier queue");inboxCheckMessage(M,31);xmqttMessageRelease(M);inboxClose(C);inboxStop(&P);xrtFutureDestroy(Ready);testRequire(xmqttClientInboxDestroy(I),"queue limit cleanup");
}
static void inboxObjectLimit(xmqttversion V)
{
    xmqttclientconfig K={0};xmqttClientConfigInit(&K);K.Version=V;K.MaxObjects=4;xmqttclientinbox* I=inboxCreate(&K);inboxPeer P;inboxStart(&P,V);xfuture* Ready;xmqttclient* C=inboxConnect(&P,&K,I,&Ready);
    inboxPublish(&P,0,55);inboxMessages(I,1);xmqttmessage* M;testRequire(xmqttClientTakeMessage(C,&M),"object retained take");inboxClose(C);inboxStop(&P);
    size_t Old=inboxInfo(I).Objects;testRequire(Old==3,"Ready, CONNACK and held message charged");inboxStart(&P,V);xmqttpacket Q=inboxConnectPacket();C=NULL;testRequire(xmqttClientConnectInbox(P.Engine,&P.Address,0,&K,I,&Q,&C),"new ready box fits last object slot");xfuture* Next=xmqttClientReady(C);
    testRequire(((const xmqttreadyresult*)inboxResult(Next))->Status==XMQTT_STATUS_LIMIT,"old results prevent new CONNACK object overcommit");inboxClose(C);inboxStop(&P);xrtFutureDestroy(Next);
    testRequire(!inboxInfo(I).Attached && inboxInfo(I).Objects==Old,"failed connection releases only its own objects");xmqttMessageRelease(M);xrtFutureDestroy(Ready);testRequire(inboxInfo(I).Objects==0,"quota refunds physical old owners");testRequire(xmqttClientInboxDestroy(I),"object cap cleanup");
}
static void inboxObjectByteLimit(xmqttversion V)
{
    xmqttclientconfig K={0};xmqttClientConfigInit(&K);K.Version=V;size_t Exact=0;
    for(unsigned Pass=0;Pass<2;++Pass){if(Pass)K.MaxObjectBytes=Exact;xmqttclientinbox* I=inboxCreate(&K);inboxPeer P;inboxStart(&P,V);xfuture* Ready;xmqttclient* C=inboxConnect(&P,&K,I,&Ready);
        inboxPublish(&P,0,77);inboxMessages(I,1);xmqttmessage* M;testRequire(xmqttClientInboxTake(I,&M),"byte cap retained take");inboxClose(C);inboxStop(&P);size_t Used=inboxInfo(I).ObjectBytes;
        if(!Pass)Exact=Used;else {testRequire(Used==Exact,"exact physical object-byte cap");inboxStart(&P,V);xmqttpacket Q=inboxConnectPacket();C=(xmqttclient*)(uintptr_t)1;
            testRequire(!xmqttClientConnectInbox(P.Engine,&P.Address,0,&K,I,&Q,&C) && C==(xmqttclient*)(uintptr_t)1 && !inboxInfo(I).Attached && inboxInfo(I).ObjectBytes==Exact,"retained byte cap rejects factory transactionally");xrtClearError();inboxStop(&P);
            xrtFutureDestroy(Ready);Ready=NULL;inboxStart(&P,V);C=inboxConnect(&P,&K,I,&Ready);testRequire(inboxInfo(I).ObjectBytes==Exact,"old message and new readiness reuse refunded bytes exactly");inboxClose(C);inboxStop(&P);
        }
        inboxCheckMessage(M,77);xmqttMessageRelease(M);xrtFutureDestroy(Ready);testRequire(!inboxInfo(I).Objects && xmqttClientInboxDestroy(I),"physical byte cap cleanup");
    }
}
static void inboxArgumentsAndOom(void)
{
    xmqttclientinboxconfig B;testRequire(xmqttClientInboxConfigInit(&B),"Inbox defaults");xmqttclientinbox* I=(xmqttclientinbox*)(uintptr_t)1;B.MaxObjects=1;testRequire(!xmqttClientInboxCreate(&B,&I) && I==(xmqttclientinbox*)(uintptr_t)1,"invalid config transactional");xrtClearError();
    testRequire(xmqttClientInboxDestroy(NULL),"NULL destroy");bool Failed=false,Passed=false;
    for(size_t Limit=1;Limit<=4096;Limit+=61){xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"Inbox OOM scope");I=(xmqttclientinbox*)(uintptr_t)1;
        if(xmqttClientInboxCreate(NULL,&I)){Passed=true;testRequire(xmqttClientInboxDestroy(I),"OOM successful control");}else {Failed=true;testRequire(I==(xmqttclientinbox*)(uintptr_t)1,"OOM preserves output");}
        xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"Inbox leave scope");xallocationbudgetinfo A={0};A.Size=sizeof(A);testRequire(xrtAllocationBudgetInfo(Budget,&A) && !A.LiveBytes && !A.LiveCount,"Inbox factory OOM no leak");xrtAllocationBudgetFree(Budget);
    }testRequire(Failed && Passed,"Inbox factory failure/success coverage");
}
static void inboxFactoryFailures(void)
{
    const size_t Limits[]={1,64,256,1024,4096,16384,65536,262144};bool Failed=false,Passed=false;
    for(size_t L=0;L<sizeof(Limits)/sizeof(*Limits);++L){inboxPeer P;inboxStart(&P,XMQTT_VERSION_5);xmqttclientconfig K={0};xmqttClientConfigInit(&K);xmqttclientinbox* I=inboxCreate(&K);xmqttpacket Q=inboxConnectPacket();
        xmqttclient* C=(xmqttclient*)(uintptr_t)1;xmqttclientconfig Bad=K;++Bad.MaxObjects;
        testRequire(!xmqttClientConnectInbox(P.Engine,&P.Address,0,&Bad,I,&Q,&C) && C==(xmqttclient*)(uintptr_t)1 && !inboxInfo(I).Attached,"mismatched caps preserve owner/output");xrtClearError();
        Bad=K;Bad.Version=XMQTT_VERSION_311;testRequire(!xmqttClientConnectInbox(P.Engine,&P.Address,0,&Bad,I,&Q,&C) && !inboxInfo(I).Attached,"mismatched version");xrtClearError();
        xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[L],&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"Client Inbox failure scope");
        bool OK=xmqttClientConnectInbox(P.Engine,&P.Address,0,&K,I,&Q,&C);testRequire(xrtAllocationBudgetLeave(&Scope),"Client Inbox scope leave");
        if(OK){Passed=true;inboxClose(C);}else {Failed=true;testRequire(C==(xmqttclient*)(uintptr_t)1 && !inboxInfo(I).Attached && !inboxInfo(I).Objects,"failed factory refunds attachment/objects");
            /* The same Inbox must be immediately reusable; no failed lease. */
            xfuture* Ready;C=inboxConnect(&P,&K,I,&Ready);inboxClose(C);xrtFutureDestroy(Ready);
        }
        inboxStop(&P);testRequire(xmqttClientInboxDestroy(I),"failed factory Inbox cleanup");xrtClearError();xallocationbudgetinfo A={0};A.Size=sizeof(A);testRequire(xrtAllocationBudgetInfo(Budget,&A) && !A.LiveBytes && !A.LiveCount,"failed Client factory no physical leaks");xrtAllocationBudgetFree(Budget);
    }testRequire(Failed && Passed,"Client constructor failure/success controls");
}
typedef struct inboxConsumer {xmqttclientinbox* Inbox;xatomic32 Stop,Count,Sum,Callbacks;} inboxConsumer;
static int32 inboxConsume(ptr Data)
{
    inboxConsumer* R=Data;
    while(!xrtAtomic32Load(&R->Stop,XMEMORY_ACQUIRE)){xmqttclientinboxinfo I;testRequire(xmqttClientInboxInfo(R->Inbox,&I),"concurrent Inbox info");xmqttmessage* M=NULL;
        if(xmqttClientInboxTake(R->Inbox,&M)){const xmqttpacket* Q=xmqttMessagePacket(M);testRequire(Q->Body.Publish.Payload.Size==1,"consumer payload");unsigned Value=Q->Body.Publish.Payload.Data[0];
            testRequire(xmqttMessageRetain(M),"retained concurrent message");xmqttMessageRelease(M);xmqttMessageRelease(M);xrtAtomic32FetchAdd(&R->Sum,Value,XMEMORY_RELAXED);xrtAtomic32FetchAdd(&R->Count,1,XMEMORY_RELEASE);
        }else xrtThreadYield();
    }return 0;
}
static void inboxNotify(xmqttclient* C,ptr Data)
{
    inboxConsumer* R=Data;xmqttclientinfo CI;xmqttclientinboxinfo I;testRequire(xmqttClientInfo(C,&CI) && xmqttClientInboxInfo(R->Inbox,&I),"Notify reentry without held Inbox/Client gate");
    xrtAtomic32FetchAdd(&R->Callbacks,1,XMEMORY_RELAXED);
}
static void inboxConcurrent(xmqttversion V)
{
    xmqttclientconfig K={0};xmqttClientConfigInit(&K);K.Version=V;xmqttclientinbox* I=inboxCreate(&K);inboxConsumer R={0};R.Inbox=I;xrtAtomic32Init(&R.Stop,0);xrtAtomic32Init(&R.Count,0);xrtAtomic32Init(&R.Sum,0);xrtAtomic32Init(&R.Callbacks,0);
    K.Notify=inboxNotify;K.User=&R;xthread* T[4];for(unsigned N=0;N<4;++N){T[N]=xrtThreadCreate(inboxConsume,&R,0);testRequire(T[N]!=NULL,"Inbox consumer thread");}
    for(unsigned G=0;G<2;++G){inboxPeer P;inboxStart(&P,V);xfuture* Ready;xmqttclient* C=inboxConnect(&P,&K,I,&Ready);
        for(unsigned N=0;N<64;++N){inboxPublish(&P,0,1+G*64+N);/* Keep the network workload bounded even under a stalled scheduler. */
            inboxWait(&R.Count,G*64+N+1);}
        inboxClose(C);inboxStop(&P);xrtFutureDestroy(Ready);
    }
    xrtAtomic32Store(&R.Stop,1,XMEMORY_RELEASE);for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(T[N],5000)==XWAIT_OK && xrtThreadExitCode(T[N])==0,"Inbox consumer join");xrtThreadDestroy(T[N]);}
    testRequire(xrtAtomic32Load(&R.Count,XMEMORY_ACQUIRE)==128 && xrtAtomic32Load(&R.Sum,XMEMORY_RELAXED)==128*129/2 && xrtAtomic32Load(&R.Callbacks,XMEMORY_RELAXED)>128,"concurrent exactly one queue transfer and callback progress");
    testRequire(!inboxInfo(I).Objects && !inboxInfo(I).Messages && xmqttClientInboxDestroy(I),"concurrent owners fully refunded");
}
int main(void)
{
    inboxArgumentsAndOom();inboxFactoryFailures();
    for(unsigned V=0;V<2;++V){xmqttversion Version=V?XMQTT_VERSION_311:XMQTT_VERSION_5;inboxLifecycle(Version);inboxQueueLimit(Version,false);inboxQueueLimit(Version,true);inboxObjectLimit(Version);inboxObjectByteLimit(Version);inboxConcurrent(Version);}
    puts("xmqtt Client Inbox tests passed");return 0;
}
