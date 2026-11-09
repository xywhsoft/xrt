#include "test.h"
#include "../examples/common/auth_peer.h"
static const void* authResult(xfuture* F)
{testRequire(xrtFutureWaitFor(F,5000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed authentication result");return xrtFutureValue(F);}
static xmqttclient* authConnect(mqttAuthPeer* P,xmqttclientconfig* K,xmqttauthconfig* A,bool Empty)
{
    xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_CONNECT,0,Props,Empty);xmqttclient* C=NULL;
    bool OK=xmqttClientConnectAuth(P->Engine,&P->Address,1,K,A,&Q,&C);
    if(!OK)fprintf(stderr,"auth factory failure: %s; private=%zu objects=%zu\n",xrtErrorMessage(xrtGetError()),K?K->MaxPrivateBytes:0,K?K->MaxObjects:0);
    testRequire(OK,"enhanced authentication factory");return C;
}
static void authClose(xmqttclient* C,mqttAuthPeer* P)
{testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"authentication physical retirement");mqttAuthStop(P);}
static xfuture* authPublish(xmqttclient* C)
{
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Body.Publish.Topic=XRT_STR_LITERAL("other");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("traffic");xfuture* F=NULL;
    testRequire(xmqttClientPublishAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"ordinary traffic admission");return F;
}
static void testAuthRoundtrip(void)
{
    for(unsigned Tiny=0;Tiny<2;++Tiny)for(unsigned Empty=0;Empty<2;++Empty){
        mqttAuthPeer P;mqttAuthStart(&P,MQTT_AUTH_EARLY);xmqttclientconfig K;xmqttClientConfigInit(&K);K.MaxRequests=1;
        if(Tiny){K.Stream.WriteLimit=K.Stream.WriteHighWater=K.WriteBytes=K.WriteChunkBytes=1;K.Stream.WriteLowWater=0;K.ReadBytes=1;K.WorkPackets=1;}
        xmqttclient* C=authConnect(&P,&K,NULL,Empty!=0);xfuture* Ready=xmqttClientReady(C);xfuture* Queued=authPublish(C);
        for(unsigned Round=0;Round<2;++Round){xmqttauthticket* T=mqttAuthTicket(C);testRequire(xmqttAuthPhase(T)==XMQTT_AUTH_CHALLENGE,"multiple initial challenges");
            testRequire(xrtFutureState(Ready)==XFUTURE_PENDING && !xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE),"no ordinary output before authentication");
            xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_AUTH,0x18,Props,Empty!=0);
            testRequire(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ERROR,"challenge is not final proof");xrtClearError();
            testRequire(xmqttAuthRespond(T,&Q)==XMQTT_ADMISSION_ACCEPTED && xmqttAuthRespond(T,&Q)==XMQTT_ADMISSION_ERROR,"single-use response, independent saturated request quota");xrtClearError();xmqttAuthRelease(T);
        }
        xmqttauthticket* Final=mqttAuthTicket(C);testRequire(xmqttAuthPhase(Final)==XMQTT_AUTH_VERIFY_CONNECT,"CONNACK verification phase");
        const xmqttmessage* Proof=xmqttAuthMessage(Final);testRequire(xmqttMessageRetain(Proof),"retain final proof");
        mqttAuthWait(&P.Acks,1);xmqttmessage* M=(xmqttmessage*)(uintptr_t)1;
        testRequire(!xmqttClientTakeMessage(C,&M) && M==(xmqttmessage*)(uintptr_t)1 && xrtFutureState(Ready)==XFUTURE_PENDING,"coalesced publication quarantined until local verification");
        mqttAuthAnswer(Final,Empty!=0);testRequire(((const xmqttreadyresult*)authResult(Ready))->Status==XMQTT_STATUS_OK,"initial local proof accepted");
        testRequire(((const xmqttwriteresult*)authResult(Queued))->Status==XMQTT_STATUS_OK,"queued publication released");
        testRequire(xmqttClientTakeMessage(C,&M),"quarantined publication released");xmqttMessageRelease(M);
        xmqttproperty Props[2];xmqttpacket Start=mqttAuthPacket(XMQTT_PACKET_AUTH,0x19,Props,Empty!=0);xfuture* Reauth=NULL;
        testRequire(xmqttClientReauthenticateAsync(C,&Start,5000,&Reauth)==XMQTT_ADMISSION_ACCEPTED,"reauthentication admission");
        xmqttauthticket* T=mqttAuthTicket(C);xfuture* Other=(xfuture*)(uintptr_t)1;
        testRequire(xmqttClientReauthenticateAsync(C,&Start,5000,&Other)==XMQTT_ADMISSION_OVERLOADED && Other==(xfuture*)(uintptr_t)1,"one reauthentication exchange");
        testRequire(xmqttClientDisconnectAsync(C,NULL,5000,&Other)==XMQTT_ADMISSION_OVERLOADED,"graceful close cannot interrupt authentication");
        xfuture* Traffic=authPublish(C);testRequire(((const xmqttwriteresult*)authResult(Traffic))->Status==XMQTT_STATUS_OK,"ordinary output progresses during reauthentication");
        mqttAuthAnswer(T,Empty!=0);mqttAuthAnswer(mqttAuthTicket(C),Empty!=0);
        Final=mqttAuthTicket(C);testRequire(xmqttAuthPhase(Final)==XMQTT_AUTH_VERIFY_REAUTH && xrtFutureState(Reauth)==XFUTURE_PENDING,"reauth final proof gate");
        testRequire(xmqttAuthComplete(Final,true)==XMQTT_ADMISSION_ACCEPTED,"reauth local final proof accepted");
        const xmqttauthresult* R=authResult(Reauth);testRequire(R->Status==XMQTT_STATUS_OK && R->AcceptedBytes && R->Final,"typed reauthentication success");
        testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,0)==XWAIT_TIMEOUT,"held ticket pins physical client");xmqttAuthRelease(Final);
        authClose(C,&P);testRequire(xmqttMessagePacket(Proof)->Type==XMQTT_PACKET_CONNACK && xmqttMessagePacket(R->Final)->Reason==0,"retained proof/result after Client/Engine destruction");
        const xmqttreadyresult* ReadyValue=authResult(Ready);testRequire(ReadyValue->Negotiated.AuthMethod.Size==(Empty?0:7),"negotiated method survives startup/Client erasure");
        if(!Empty)testRequire(!memcmp(ReadyValue->Negotiated.AuthMethod.Data,"fixture",7),"owned negotiated method");
        xmqttMessageRelease(Proof);xrtFutureDestroy(Traffic);xrtFutureDestroy(Reauth);xrtFutureDestroy(Queued);xrtFutureDestroy(Ready);
    }
}
static void testAuthTerminal(void)
{
    const unsigned Modes[]={MQTT_AUTH_BAD_METHOD,MQTT_AUTH_MISSING_METHOD|MQTT_AUTH_DIRECT,MQTT_AUTH_REFUSE,MQTT_AUTH_HOLD};
    const xmqttclientstatus Status[]={XMQTT_STATUS_PROTOCOL,XMQTT_STATUS_PROTOCOL,XMQTT_STATUS_REJECTED,XMQTT_STATUS_TIMEOUT};
    for(unsigned I=0;I<4;++I){mqttAuthPeer P;mqttAuthStart(&P,Modes[I]);xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=100;
        xmqttclient* C=authConnect(&P,&K,NULL,false);xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)authResult(F))->Status==Status[I],"initial authentication terminal status");authClose(C,&P);xrtFutureDestroy(F);}
    for(unsigned Kind=0;Kind<3;++Kind){mqttAuthPeer P;mqttAuthStart(&P,MQTT_AUTH_DIRECT);xmqttauthconfig A;xmqttAuthConfigInit(&A);A.ResponseTimeout=50;
        xmqttclient* C=authConnect(&P,NULL,&A,false);xfuture* F=xmqttClientReady(C);xmqttauthticket* T=mqttAuthTicket(C);
        if(Kind==0)testRequire(xmqttAuthComplete(T,false)==XMQTT_ADMISSION_ACCEPTED,"reject server final proof");
        if(Kind==1)testRequire(xrtFutureCancel(F),"cancel pending final proof");
        const xmqttreadyresult* R=authResult(F);testRequire(R->Status==(Kind==0?XMQTT_STATUS_AUTHENTICATION:Kind==1?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_TIMEOUT),"local proof reject/cancel/timeout");
        xmqttAuthRelease(T);authClose(C,&P);xrtFutureDestroy(F);}
}
static void authVerifyInitial(xmqttclient* C,xfuture* F,bool Direct)
{
    for(unsigned I=0;I<(Direct?1u:3u);++I)mqttAuthAnswer(mqttAuthTicket(C),false);
    testRequire(((const xmqttreadyresult*)authResult(F))->Status==XMQTT_STATUS_OK,"initial authentication control");
}
static void testAuthReauthTerminal(void)
{
    for(unsigned Kind=0;Kind<5;++Kind){mqttAuthPeer P;mqttAuthStart(&P,Kind==4?0:MQTT_AUTH_DIRECT);xmqttclient* C=authConnect(&P,NULL,NULL,false);xfuture* Ready=xmqttClientReady(C);authVerifyInitial(C,Ready,Kind!=4);
        xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_AUTH,0x19,Props,false);xfuture* F=NULL;
        testRequire(xmqttClientReauthenticateAsync(C,&Q,Kind==2?80:5000,&F)==XMQTT_ADMISSION_ACCEPTED,"terminal reauthentication admission");xmqttauthticket* T=mqttAuthTicket(C);
        if(Kind==0)testRequire(xmqttAuthComplete(T,false)==XMQTT_ADMISSION_ACCEPTED,"reauth proof rejection");
        if(Kind==1)testRequire(xrtFutureCancel(F),"reauth future cancellation");
        if(Kind==3){const unsigned char Wire[]={0xf0,0};mqttAuthSend(&P,Wire,sizeof(Wire));}
        if(Kind==4){Q.Reason=0;testRequire(xmqttAuthRespond(T,&Q)==XMQTT_ADMISSION_ERROR,"client cannot send AUTH success");xrtClearError();testRequire(xmqttClientClose(C),"explicit Close during challenge");}
        const xmqttauthresult* R=authResult(F);xmqttclientstatus Want=Kind==0?XMQTT_STATUS_AUTHENTICATION:Kind==1?XMQTT_STATUS_CANCELLED:Kind==2?XMQTT_STATUS_TIMEOUT:Kind==3?XMQTT_STATUS_PROTOCOL:XMQTT_STATUS_CLOSED;
        testRequire(R->Status==Want && R->AcceptedBytes,"reauth terminal outcome retains accepted prefix");xmqttAuthRelease(T);authClose(C,&P);xrtFutureDestroy(F);xrtFutureDestroy(Ready);
    }
    /* A server may not initiate reauthentication on an idle connection. */
    mqttAuthPeer P;mqttAuthStart(&P,MQTT_AUTH_DIRECT);xmqttclient* C=authConnect(&P,NULL,NULL,false);xfuture* Ready=xmqttClientReady(C);authVerifyInitial(C,Ready,true);
    unsigned char Wire[128];size_t N=mqttAuthEncode(&P,XMQTT_PACKET_AUTH,0x18,Wire,sizeof(Wire));Wire[2]=0x19;mqttAuthSend(&P,Wire,N);
    testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"unsolicited server reauth closes");xmqttclientinfo I;xmqttClientInfo(C,&I);testRequire(I.CloseStatus==XMQTT_STATUS_PROTOCOL,"server reauth direction");authClose(C,&P);xrtFutureDestroy(Ready);
}
static void testAuthLimits(void)
{
    for(unsigned Kind=0;Kind<3;++Kind){mqttAuthPeer P;mqttAuthStart(&P,0);xmqttclientconfig K;xmqttClientConfigInit(&K);xmqttauthconfig A;xmqttAuthConfigInit(&A);
        if(Kind==0)A.MaxRounds=1;
        if(Kind==1)A.MaxExchangeBytes=1;
        if(Kind==2)K.MaxObjects=2;
        xmqttclient* C=authConnect(&P,&K,&A,false);xfuture* Ready=xmqttClientReady(C);if(Kind==0)mqttAuthAnswer(mqttAuthTicket(C),false);
        testRequire(((const xmqttreadyresult*)authResult(Ready))->Status==XMQTT_STATUS_LIMIT,"auth round/aggregate/object quota closes");authClose(C,&P);xrtFutureDestroy(Ready);
    }
    mqttAuthPeer P;mqttAuthStart(&P,0);xmqttclientconfig K;xmqttClientConfigInit(&K);K.MaxPrivateBytes=8192;K.Codec.MaxProperties=8;
    xmqttclient* C=authConnect(&P,&K,NULL,false);xfuture* Ready=xmqttClientReady(C);xmqttauthticket* T=mqttAuthTicket(C);
    unsigned char Large[8192]={0};xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_AUTH,0x18,Props,false);Props[1].Value.Binary=(xbytesview){Large,sizeof(Large)};
    xrtSetErrorInfo(XERR_STATE,"auth.test",7,"ambient");const xerror* Before=xrtErrorRef(xrtGetError());
    testRequire(xmqttAuthRespond(T,&Q)==XMQTT_ADMISSION_OVERLOADED && xrtGetError()==Before,"private quota overload preserves caller error and ticket");xrtClearError();xrtErrorFree((xerror*)Before);
    mqttAuthAnswer(T,false);mqttAuthAnswer(mqttAuthTicket(C),false);mqttAuthAnswer(mqttAuthTicket(C),false);testRequire(((const xmqttreadyresult*)authResult(Ready))->Status==XMQTT_STATUS_OK,"smaller response retries after quota refusal");authClose(C,&P);xrtFutureDestroy(Ready);
}
static void testAuthAllocation(void)
{
    const size_t Limits[]={1,32,64,128,256,512,1024,4096,8192,16384,32768,65536};unsigned FailedFactory=0,OKFactory=0,FailedReauth=0,OKReauth=0;
    for(unsigned Pass=0;Pass<2;++Pass)for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){mqttAuthPeer P;mqttAuthStart(&P,MQTT_AUTH_DIRECT);xmqttclient* C=NULL;xfuture *Ready=NULL,*F=NULL;
        if(Pass){C=authConnect(&P,NULL,NULL,false);Ready=xmqttClientReady(C);authVerifyInitial(C,Ready,true);}
        xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&B) && xrtAllocationBudgetEnter(B,&Scope),"auth allocation prefix scope");
        xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(Pass?XMQTT_PACKET_AUTH:XMQTT_PACKET_CONNECT,Pass?0x19:0,Props,false);
        xmqttclientadmission S=XMQTT_ADMISSION_ERROR;bool OK;
        if(Pass){F=(xfuture*)(uintptr_t)1;S=xmqttClientReauthenticateAsync(C,&Q,5000,&F);OK=S==XMQTT_ADMISSION_ACCEPTED;}
        else {C=(xmqttclient*)(uintptr_t)1;OK=xmqttClientConnectAuth(P.Engine,&P.Address,1,NULL,NULL,&Q,&C);}
        testRequire(xrtAllocationBudgetLeave(&Scope),"auth scope leave");
        if(OK){if(Pass){++OKReauth;mqttAuthAnswer(mqttAuthTicket(C),false);testRequire(((const xmqttauthresult*)authResult(F))->Status==XMQTT_STATUS_OK,"reauth allocation success");}
            else {++OKFactory;Ready=xmqttClientReady(C);authVerifyInitial(C,Ready,true);}authClose(C,&P);}
        else {if(Pass){++FailedReauth;testRequire(S==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"reauth OOM output unchanged");F=NULL;authClose(C,&P);}
            else {++FailedFactory;testRequire(C==(xmqttclient*)(uintptr_t)1,"auth factory OOM output unchanged");mqttAuthStop(&P);}}
        xrtFutureDestroy(F);xrtFutureDestroy(Ready);xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);
        testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes,"authentication OOM/producer/native allocations physically retired");xrtAllocationBudgetFree(B);
    }
    testRequire(FailedFactory && OKFactory && FailedReauth && OKReauth,"factory and reauth failure/success prefixes covered");
}
static void testAuthTrafficBoundaries(void)
{
    mqttAuthPeer P;mqttAuthStart(&P,0);xmqttclientconfig K;xmqttClientConfigInit(&K);K.WriteBytes=K.WriteChunkBytes=1;K.Stream.WriteLimit=K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;
    xmqttclient* C=authConnect(&P,&K,NULL,false);xfuture* Ready=xmqttClientReady(C);authVerifyInitial(C,Ready,false);
    for(unsigned Loop=0;Loop<16;++Loop){xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_AUTH,0x19,Props,false);xfuture* F=NULL;
        testRequire(xmqttClientReauthenticateAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"short-write reauth");xfuture* Traffic=authPublish(C);
        for(unsigned J=0;J<3;++J)mqttAuthAnswer(mqttAuthTicket(C),false);
        testRequire(((const xmqttauthresult*)authResult(F))->Status==XMQTT_STATUS_OK && ((const xmqttwriteresult*)authResult(Traffic))->Status==XMQTT_STATUS_OK,"AUTH and application partial frames remain whole");
        xmqttmessage* M=NULL;double End=xrtTimer()+5;while(!xmqttClientTakeMessage(C,&M)){testRequire(xrtTimer()<End,"concurrent reauth traffic receive");xrtSleep(1);}xmqttMessageRelease(M);xrtFutureDestroy(Traffic);xrtFutureDestroy(F);
    }
    authClose(C,&P);xrtFutureDestroy(Ready);
}
typedef struct authWorkerBudget {xallocationbudget* Budget;xallocationscope Scope;bool Entered,Active;} authWorkerBudget;
static void authWorkerNotify(xmqttclient* C,ptr User)
{
    authWorkerBudget* B=User;xmqttclientinfo I;xmqttClientInfo(C,&I);
    if(!B->Entered && I.State==XMQTT_CLIENT_CONNECTING){testRequire(xrtAllocationBudgetEnter(B->Budget,&B->Scope),"auth worker quota enter");B->Entered=B->Active=true;}
    else if(B->Active){testRequire(xrtAllocationBudgetLeave(&B->Scope),"auth worker quota leave");B->Active=false;}
}
static void testAuthWorkerAllocation(void)
{
    const size_t Limits[]={1,64,128,256,384,512,768,1024,2048,4096,16384};unsigned Failed=0,Passed=0;
    for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){mqttAuthPeer P;mqttAuthStart(&P,0);authWorkerBudget B={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&B.Budget),"auth worker allocation budget");
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=authWorkerNotify;K.User=&B;xmqttclient* C=authConnect(&P,&K,NULL,false);xfuture* Ready=xmqttClientReady(C);
        mqttAuthAnswer(mqttAuthTicket(C),false);double End=xrtTimer()+5;
        while(xrtFutureState(Ready)==XFUTURE_PENDING){xmqttauthticket* T=NULL;if(xmqttClientTakeAuth(C,&T))mqttAuthAnswer(T,false);else xrtSleep(1);testRequire(xrtTimer()<End,"worker OOM authentication progress");}
        const xmqttreadyresult* R=authResult(Ready);
        if(R->Status==XMQTT_STATUS_OK)++Passed;else {++Failed;testRequire(R->Status==XMQTT_STATUS_MEMORY || R->Status==XMQTT_STATUS_TRANSPORT,"worker OOM typed failure");}
        authClose(C,&P);xrtFutureDestroy(Ready);xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);
        testRequire(B.Entered && !B.Active && xrtAllocationBudgetInfo(B.Budget,&I) && !I.LiveBytes && !I.LiveCount,"worker auth ticket/native/message/OOM owners retired");xrtAllocationBudgetFree(B.Budget);
    }
    testRequire(Failed && Passed,"auth worker failure and success controls");
}
typedef struct authRace {
    xmqttclient* Client;xmqttauthticket* Ticket;xatomic32 *Entered,*Go;
    unsigned Action;xmqttclientadmission Admission;xfuture* Future;
} authRace;
static int32 authRaceProducer(ptr User)
{
    authRace* R=User;xrtAtomic32FetchAdd(R->Entered,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(R->Go,XMEMORY_ACQUIRE))xrtThreadYield();
    xmqttproperty Props[2];xmqttpacket P=mqttAuthPacket(XMQTT_PACKET_AUTH,R->Action==2?0x19:0x18,Props,false);
    R->Admission=R->Action==0?xmqttAuthRespond(R->Ticket,&P):R->Action==1?xmqttAuthComplete(R->Ticket,true):xmqttClientReauthenticateAsync(R->Client,&P,5000,&R->Future);return 0;
}
static unsigned authRaceRun(xmqttclient* C,xmqttauthticket* T,unsigned Action,xfuture** Winner)
{
    xatomic32 Entered,Go;xrtAtomic32Init(&Entered,0);xrtAtomic32Init(&Go,0);authRace R[2]={{0}};xthread* Threads[2];
    for(unsigned I=0;I<2;++I){R[I].Client=C;R[I].Ticket=T;R[I].Entered=&Entered;R[I].Go=&Go;R[I].Action=Action;R[I].Future=(xfuture*)(uintptr_t)1;Threads[I]=xrtThreadCreate(authRaceProducer,R+I,0);testRequire(Threads[I]!=NULL,"auth producer thread");}
    mqttAuthWait(&Entered,2);xrtAtomic32Store(&Go,1,XMEMORY_RELEASE);unsigned Wins=0;
    for(unsigned I=0;I<2;++I){testRequire(xrtThreadWaitFor(Threads[I],5000)==XWAIT_OK,"auth producer join");xrtThreadDestroy(Threads[I]);
        if(R[I].Admission==XMQTT_ADMISSION_ACCEPTED){++Wins;if(Winner)*Winner=R[I].Future;}
        else {testRequire(R[I].Admission==(Action==2?XMQTT_ADMISSION_OVERLOADED:XMQTT_ADMISSION_ERROR),"losing auth producer has distinct busy/stale result");testRequire(R[I].Future==(xfuture*)(uintptr_t)1,"loser output unchanged");}}
    return Wins;
}
static void testAuthProducerRaces(void)
{
    mqttAuthPeer P;mqttAuthStart(&P,0);xmqttclient* C=authConnect(&P,NULL,NULL,false);xfuture* Ready=xmqttClientReady(C);xmqttauthticket* T=mqttAuthTicket(C);
    testRequire(authRaceRun(C,T,0,NULL)==1,"exactly one response commits");xmqttAuthRelease(T);mqttAuthAnswer(mqttAuthTicket(C),false);T=mqttAuthTicket(C);
    testRequire(authRaceRun(C,T,1,NULL)==1,"exactly one final proof decision commits");xmqttAuthRelease(T);testRequire(((const xmqttreadyresult*)authResult(Ready))->Status==XMQTT_STATUS_OK,"racing initial authentication succeeds");
    for(unsigned I=0;I<8;++I){xfuture* F=NULL;testRequire(authRaceRun(C,NULL,2,&F)==1,"exactly one reauthentication producer commits");for(unsigned J=0;J<3;++J)mqttAuthAnswer(mqttAuthTicket(C),false);testRequire(((const xmqttauthresult*)authResult(F))->Status==XMQTT_STATUS_OK,"racing reauth result");xrtFutureDestroy(F);}
    authClose(C,&P);xrtFutureDestroy(Ready);
}
typedef struct authDecisionGate {xatomic32 Entered,Release;} authDecisionGate;
static void authDecisionNotify(xmqttclient* C,ptr User)
{
    (void)C;authDecisionGate* G=User;if(xrtAtomic32Load(&G->Entered,XMEMORY_ACQUIRE))return;
    xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);double End=xrtTimer()+5;
    /* Test-only barrier: force the timer owner to lag behind an expired app
     * decision. Production Notify must never wait like this. */
    while(!xrtAtomic32Load(&G->Release,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"late auth decision barrier");xrtThreadYield();}
}
static void testAuthLateDecision(void)
{
    for(unsigned Final=0;Final<2;++Final){mqttAuthPeer P;mqttAuthStart(&P,Final?MQTT_AUTH_DIRECT:0);authDecisionGate G={0};xrtAtomic32Init(&G.Entered,0);xrtAtomic32Init(&G.Release,0);
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=authDecisionNotify;K.User=&G;xmqttauthconfig A;xmqttAuthConfigInit(&A);A.ResponseTimeout=20;
        xmqttclient* C=authConnect(&P,&K,&A,false);xfuture* Ready=xmqttClientReady(C);mqttAuthWait(&G.Entered,1);xmqttauthticket* T=mqttAuthTicket(C);xrtSleep(80);
        xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_AUTH,0x18,Props,false);
        testRequire((Final?xmqttAuthComplete(T,true):xmqttAuthRespond(T,&Q))==XMQTT_ADMISSION_ERROR,"expired decision cannot reset local deadline before owner timer runs");
        xrtAtomic32Store(&G.Release,1,XMEMORY_RELEASE);xmqttAuthRelease(T);const xmqttreadyresult* R=authResult(Ready);
        testRequire(R->Status==XMQTT_STATUS_TIMEOUT && R->Cause && xrtErrorKind(R->Cause)==XERR_TIMEOUT,"expired decision preserves typed timeout/cause");authClose(C,&P);xrtFutureDestroy(Ready);xrtClearError();
    }
}
int main(int Argc,char** Argv)
{
    static const struct {cstr Name;void (*Run)(void);} Cases[]={
        {"roundtrip",testAuthRoundtrip},{"terminal",testAuthTerminal},{"reauth-terminal",testAuthReauthTerminal},
        {"limits",testAuthLimits},{"allocation",testAuthAllocation},{"traffic",testAuthTrafficBoundaries},{"worker-allocation",testAuthWorkerAllocation},{"producer-races",testAuthProducerRaces},{"late-decision",testAuthLateDecision}};
    unsigned Ran=0;if(Argc>2)return 2;
    for(size_t I=0;I<sizeof(Cases)/sizeof(*Cases);++I)if(Argc<2 || !strcmp(Argv[1],Cases[I].Name)){fprintf(stderr,"[case] auth %s\n",Cases[I].Name);Cases[I].Run();++Ran;}
    if(!Ran)return 2;
    puts("[PASS] MQTT5 enhanced authentication: initial/reauth rounds, final proof, traffic gating, short writes, tickets, quotas, OOM and terminal retirement");return 0;
}
