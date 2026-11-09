#include "test.h"
#include "support/tls_resume_peer.h"
static const void* resumeFuture(xfuture* F)
{testRequire(F && xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed resume outcome");return xrtFutureValue(F);}
static xmqttclienttlsresumeinfo resumeInfo(xmqttclient* C)
{xmqttclienttlsresumeinfo I;testRequire(xmqttClientTlsResumeInfo(C,&I) && I.Size==sizeof(I),"resume info");return I;}
static xmqttclienttlsresumeinfo resumeCaptured(xmqttclient* C,uint64 Count)
{double End=xrtTimer()+10;xmqttclienttlsresumeinfo I;do{I=resumeInfo(C);testRequire(xrtTimer()<End,"bounded credential publication");if(I.Captured<Count)xrtSleep(1);}while(I.Captured<Count);return I;}
static xmqttpacket resumeConnect(void)
{xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("resume");return Q;}
static void resumeClose(xmqttclient* C)
{
    xfuture* F=NULL;testRequire(xmqttClientDisconnectAsync(C,NULL,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)resumeFuture(F))->Status==XMQTT_STATUS_OK,"resumed MQTT DISCONNECT plus authenticated close");
    testRequire(xmqttClientWaitFor(C,15000)==XWAIT_OK,"real resumed client/native retirement");xrtFutureDestroy(F);
}
static void resumePublish(xmqttclient* C,unsigned Qos)
{
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(uint8)(Qos<<1);Q.Body.Publish.Topic=XRT_STR_LITERAL("p");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* F=NULL;
    testRequire(xmqttClientPublishAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)resumeFuture(F))->Status==XMQTT_STATUS_OK,"MQTT QoS1 on full/resumed channel");xrtFutureDestroy(F);
    xmqttclientinfo I;double End=xrtTimer()+10;do{testRequire(xmqttClientInfo(C,&I) && xrtTimer()<End,"request physical quota retirement");if(I.Requests)xrtSleep(1);}while(I.Requests);
}
static xmqttclient* resumeOpen(mqttTlsResumePeer* R,xtlsresume* Ticket,unsigned Factory,size_t Private,size_t Limit,xmqttclientnotify Notify,ptr User)
{
    xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&R->Peer,&T,Name,&Protocol,Alpn);T.Session.Resume=Ticket;T.Session.ResumeOnly=Ticket!=NULL;T.Session.ResumeLimit=Limit;
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=R->Peer.Version;K.Notify=Notify;K.User=User;if(Private)K.MaxPrivateBytes=Private;
    xmqttpacket Q=resumeConnect();xmqttclient* C=NULL;
    if(Factory==0)testRequire(xmqttClientTlsConnect(R->Peer.Engine,&R->Peer.Address,1,&K,&T,&Q,&C),"numeric full/resumed factory");
    else if(Factory==1)testRequire(xmqttClientTlsDial(R->Peer.Engine,R->Peer.Resolver,XRT_STR_LITERAL("example.com"),R->Peer.Address.Port,1,&K,&T,&Q,&C),"hostname full/resumed factory");
    else {xnetaddrlist* L=xrtNetAddrListCreate(&R->Peer.Address,1);testRequire(L && xmqttClientTlsDialAddresses(R->Peer.Engine,L,1,&K,&T,&Q,&C),"prepared full/resumed factory");xrtNetAddrListDestroy(L);}
    xrtTlsVerifierRelease(V);memset(Name,'x',sizeof(Name));memset(Alpn,'x',sizeof(Alpn));xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)resumeFuture(F))->Status==XMQTT_STATUS_OK,"fresh MQTT readiness even when TLS resumed");xrtFutureDestroy(F);
    xmqttclientinfo I;double End=xrtTimer()+10;do{testRequire(xmqttClientInfo(C,&I) && xrtTimer()<End,"startup physical quota retirement");if(I.StartupBytes)xrtSleep(1);}while(I.StartupBytes);return C;
}
static void resumeRounds(void)
{
    for(unsigned Version=0;Version<2;++Version){mqttTlsResumePeer R;mqttTlsResumeStart(&R,Version?XMQTT_VERSION_311:XMQTT_VERSION_5);xtlsresume* Ticket=NULL;
        for(unsigned Round=0;Round<4;++Round){xmqttclient* C=resumeOpen(&R,Ticket,Round%3,0,4,NULL,NULL);xrtTlsResumeRelease(Ticket);Ticket=NULL;
            xmqttclienttlsresumeinfo I=resumeInfo(C);testRequire(I.Enabled && I.Established && I.Resumed==(Round!=0) && !I.CachedBytes,"actual TLS full/resumed state before new credential");resumePublish(C,1);
            xmqttclientinfo Before,After;testRequire(xmqttClientInfo(C,&Before),"before credential bytes");mqttTlsResumeIssue(&R,1);I=resumeCaptured(C,1);
            testRequire(I.CachedBytes && !I.Rejected && xrtAtomic32Load(&R.ServerResumed,XMEMORY_ACQUIRE)==(Round!=0),"both peers confirm real PSK resumption");
            testRequire(xmqttClientInfo(C,&After) && After.PrivateBytes==Before.PrivateBytes+I.CachedBytes,"credential data charged to local private cap");
            resumeClose(C);testRequire(resumeInfo(C).Resumed==(Round!=0),"handshake observation survives close");
            xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"allocation-free handoff scope");
            testRequire(xmqttClientTlsTakeResume(C,&Ticket),"take credential after physical Client wait");I=resumeInfo(C);testRequire(!I.CachedBytes,"handoff refunds local byte charge");
            xtlsresume* Empty=(xtlsresume*)(uintptr_t)1;testRequire(!xmqttClientTlsTakeResume(C,&Empty) && Empty==(xtlsresume*)(uintptr_t)1,"empty handoff preserves output");
            testRequire(xrtAllocationBudgetLeave(&Scope),"leave handoff scope");xallocationbudgetinfo Info={.Size=sizeof(Info)};testRequire(xrtAllocationBudgetInfo(Budget,&Info) && !Info.PeakBytes && !Info.Rejections,"info/take allocate nothing");xrtAllocationBudgetFree(Budget);
            testRequire(xmqttClientDestroy(C),"destroy prior MQTT owner");mqttTlsResumeReset(&R);
        }
        mqttTlsResumeStop(&R);xtlsresumeinfo I;testRequire(xrtTlsResumeInfo(Ticket,&I) && I.Ticket.Size && I.Secret.Size && xrtTlsResumePeerCertificateCountV1(Ticket)==1,"credential survives Client and actual Engine join");xrtTlsResumeRelease(Ticket);
    }
}
static void resumeReplaceAndQuota(void)
{
    mqttTlsResumePeer R;mqttTlsResumeStart(&R,XMQTT_VERSION_5);xmqttclient* C=resumeOpen(&R,NULL,0,0,4,NULL,NULL);xmqttclientinfo Base;testRequire(xmqttClientInfo(C,&Base),"resume baseline private bytes");
    for(unsigned I=1;I<=6;++I){mqttTlsResumeIssue(&R,1);xmqttclienttlsresumeinfo Info=resumeCaptured(C,I);testRequire(Info.Evicted==I-1 && !Info.Rejected && Info.CachedBytes,"one latest bounded cache");}
    xtlsresume* T=NULL;testRequire(xmqttClientTlsTakeResume(C,&T),"take latest replaced credential");xtlsresumeinfo A,B;testRequire(xrtTlsResumeInfo(T,&A) && xrtTlsResumeInfo(R.Ticket,&B) && A.Ticket.Size==B.Ticket.Size && !memcmp(A.Ticket.Data,B.Ticket.Data,A.Ticket.Size),"latest credential matches native server cache");
    xrtTlsResumeRelease(T);xmqttclientinfo Now;testRequire(xmqttClientInfo(C,&Now) && Now.PrivateBytes==Base.PrivateBytes,"take returns exact cache data charge");resumeClose(C);testRequire(xmqttClientDestroy(C),"replace owner destroy");mqttTlsResumeReset(&R);
    C=resumeOpen(&R,NULL,0,Base.PrivateBytes+256,4,NULL,NULL);mqttTlsResumeIssue(&R,1);xmqttclienttlsresumeinfo I=resumeCaptured(C,1);testRequire(I.Rejected==1 && !I.CachedBytes,"optional credential retention obeys local private cap");resumePublish(C,0);
    T=(xtlsresume*)(uintptr_t)1;xrtSetErrorInfo(XERR_STATE,"mqtt.resume.sentinel",1,"preserve empty error");const xerror* Error=xrtGetError();testRequire(!xmqttClientTlsTakeResume(C,&T) && T==(xtlsresume*)(uintptr_t)1 && xrtGetError()==Error,"empty/refused cache preserves ambient error");xrtClearError();
    resumeClose(C);testRequire(xmqttClientDestroy(C),"quota owner destroy");mqttTlsResumeReset(&R);
    C=resumeOpen(&R,NULL,0,0,0,NULL,NULL);mqttTlsResumeIssue(&R,2);resumePublish(C,1);testRequire(!resumeInfo(C).Enabled && !resumeInfo(C).CachedBytes,"explicit native ResumeLimit0 disables retention");resumeClose(C);testRequire(xmqttClientDestroy(C),"disabled owner destroy");mqttTlsResumeReset(&R);mqttTlsResumeStop(&R);
}
typedef struct resumeConsumer {xmqttclient* Client;xtlsresume* Value;} resumeConsumer;
static int32 resumeTakeThread(ptr Data)
{resumeConsumer* T=Data;(void)xmqttClientTlsTakeResume(T->Client,&T->Value);return 0;}
static void resumeConcurrency(void)
{
    mqttTlsResumePeer R;mqttTlsResumeStart(&R,XMQTT_VERSION_5);xmqttclient* C=resumeOpen(&R,NULL,0,0,4,NULL,NULL);mqttTlsResumeIssue(&R,8);resumePublish(C,1);(void)resumeCaptured(C,1);
    resumeConsumer Consumers[8]={{0}};xthread* Threads[8]={0};for(size_t I=0;I<8;++I){Consumers[I].Client=C;Threads[I]=xrtThreadCreate(resumeTakeThread,Consumers+I,0);testRequire(Threads[I]!=NULL,"concurrent resume takers");}
    unsigned Won=0;for(size_t I=0;I<8;++I){testRequire(xrtThreadWaitFor(Threads[I],15000)==XWAIT_OK,"join actual resume consumer");xrtThreadDestroy(Threads[I]);if(Consumers[I].Value){++Won;xrtTlsResumeRelease(Consumers[I].Value);}}
    testRequire(Won==1 && !resumeInfo(C).CachedBytes,"one real cached reference has one winner");resumeClose(C);testRequire(xmqttClientDestroy(C),"concurrent owner destroy");mqttTlsResumeReset(&R);mqttTlsResumeStop(&R);
}
typedef struct resumeNotification {xnetengine* Engine;xtlsresume* Value;xatomic32 Taken;} resumeNotification;
static void resumeNotify(xmqttclient* C,ptr Data)
{
    resumeNotification* N=Data;testRequire(xrtNetWorkerIsCurrent(xrtNetEngineWorker(N->Engine,1)),"ticket notification on the actual Client Worker");
    if(!xrtAtomic32Load(&N->Taken,XMEMORY_ACQUIRE) && xmqttClientTlsTakeResume(C,&N->Value)){
        testRequire(xmqttClientClose(C),"reentrant take/Close from ticket notification");xrtAtomic32Store(&N->Taken,1,XMEMORY_RELEASE);
    }
}
static void resumeNotificationAndValidation(void)
{
    mqttTlsResumePeer R;mqttTlsResumeStart(&R,XMQTT_VERSION_5);resumeNotification N={0};N.Engine=R.Peer.Engine;xmqttclient* C=resumeOpen(&R,NULL,0,0,4,resumeNotify,&N);
    xmqttclienttlsresumeinfo I;memset(&I,0xa5,sizeof(I));unsigned char Saved[sizeof(I)];memcpy(Saved,&I,sizeof(I));
    testRequire(!xmqttClientTlsResumeInfo(NULL,&I) && !memcmp(Saved,&I,sizeof(I)),"invalid info preserves all output bytes");xrtClearError();
    testRequire(!xmqttClientTlsResumeInfo(C,(xmqttclienttlsresumeinfo*)C),"info refuses owner overlap");xrtClearError();
    testRequire(!xmqttClientTlsTakeResume(C,(xtlsresume**)C),"take refuses owner overlap");xrtClearError();
    mqttTlsResumeIssue(&R,1);mqttTlsWait(&N.Taken,1,"deferred credential notification");testRequire(N.Value && xmqttClientWaitFor(C,15000)==XWAIT_OK && xmqttClientDestroy(C),"notification-triggered physical close");
    mqttTlsResumeStop(&R);xtlsresumeinfo Info;testRequire(xrtTlsResumeInfo(N.Value,&Info) && Info.Secret.Size,"notification transfers actual credential ownership");xrtTlsResumeRelease(N.Value);
    mqttTlsResumeStart(&R,XMQTT_VERSION_5);xmqttpacket Q=resumeConnect();C=NULL;
    testRequire(xmqttClientConnect(R.Peer.Engine,&R.Peer.Address,1,NULL,&Q,&C),"plain client handle");xtlsresume* Empty=(xtlsresume*)(uintptr_t)1;
    testRequire(!xmqttClientTlsTakeResume(C,&Empty) && Empty==(xtlsresume*)(uintptr_t)1,"non-TLS handle refuses resume API");xrtClearError();
    xfuture* Ready=xmqttClientReady(C);(void)resumeFuture(Ready);xrtFutureDestroy(Ready);
    testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,15000)==XWAIT_OK && xmqttClientDestroy(C),"plain owner shutdown");mqttTlsResumeStop(&R);
}
int main(void)
{resumeRounds();resumeReplaceAndQuota();resumeConcurrency();resumeNotificationAndValidation();puts("[PASS] MQTT TLS1.3 credential handoff: true resumption, factories, quota, replacement, cross-thread ownership and physical retirement");return 0;}
