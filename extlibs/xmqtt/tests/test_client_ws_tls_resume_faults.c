#include "support/ws_resume_peer.h"
static xmqttclient* mfOpen(mrPeer* P,mrState* S,unsigned Route,size_t Private,size_t Limit,xmqttclientnotify Notify,ptr User)
{mrInit(P,S,XMQTT_VERSION_5,Route,XNET_PORT_SELECT);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=Notify;K.User=User;if(Private)K.MaxPrivateBytes=Private;static char Authority[128];xmqttclient* C=mrOpen(P,Route,NULL,false,Limit,&K,Authority,sizeof(Authority));xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"healthy WSS fault fixture");xrtFutureDestroy(F);return C;}
static void mfStop(mrPeer* P,xmqttclient* C)
{testRequire(xmqttClientClose(C)&&xmqttClientWaitFor(C,15000)==XWAIT_OK&&xmqttClientDestroy(C),"fault client physical join");mqttWsConnectStop(&P->Peer);testRequire(xrtAtomic32Load(&P->Peer.Base.Owners,XMEMORY_ACQUIRE)==0,"all original native TLS owners joined");if(P->Peer.Base.Budget){xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(P->Peer.Base.Budget,&I)&&!I.LiveCount&&!I.LiveBytes,"actual constructor OOM resources returned after joins");xrtAllocationBudgetFree(P->Peer.Base.Budget);}}
static void mfQuota(void)
{
    mrState S={0};mrPeer P;xmqttclient* C=mfOpen(&P,&S,1,0,4,NULL,NULL);mrPublish(C,1);xmqttclientinfo Base;double End=xrtTimer()+10;do{testRequire(xmqttClientInfo(C,&Base)&&xrtTimer()<End,"WSS quota physical requests");if(Base.Requests)xrtSleep(1);}while(Base.Requests);
    for(unsigned I=1;I<=6;++I){mrIssue(&P,1);xmqttclienttlsresumeinfo Info=mrCaptured(C,I);testRequire(Info.Evicted==I-1&&!Info.Rejected&&Info.CachedBytes,"bounded latest WSS credential slot");}
    xtlsresume* R=NULL;testRequire(xmqttClientTlsTakeResume(C,&R),"take latest WSS credential");xtlsresumeinfo A,B;testRequire(xrtTlsResumeInfo(R,&A)&&xrtTlsResumeInfo(S.ServerTicket,&B)&&A.Ticket.Size==B.Ticket.Size&&!memcmp(A.Ticket.Data,B.Ticket.Data,A.Ticket.Size),"latest immutable client credential matches real issuer");size_t DataBytes=mrInfo(C).CachedBytes;(void)DataBytes;xrtTlsResumeRelease(R);mfStop(&P,C);
    C=mfOpen(&P,&S,1,Base.PrivateBytes+256,4,NULL,NULL);mrPublish(C,1);mrIssue(&P,1);xmqttclienttlsresumeinfo I=mrCaptured(C,1);testRequire(I.Rejected==1&&!I.CachedBytes,"optional cache refusal obeys real local private cap");mrPublish(C,0);mfStop(&P,C);
    for(unsigned Direct=0;Direct<2;++Direct){C=mfOpen(&P,&S,Direct?0:1,0,0,NULL,NULL);testRequire(!mrInfo(C).Enabled&&mrInfo(C).Established,"configured native zero capacity reflected by direct and HTTP WSS");mrIssue(&P,2);mrPublish(C,1);I=mrInfo(C);testRequire(!I.Captured&&!I.CachedBytes,"disabled native capture, ordered PUBACK proves peer records processed");mfStop(&P,C);}
    xrtTlsResumeRelease(S.ServerTicket);
}
typedef struct mfTaker {xmqttclient* Client;xtlsresume* Value;} mfTaker;
static int32 mfTake(ptr Data){mfTaker* T=Data;(void)xmqttClientTlsTakeResume(T->Client,&T->Value);return 0;}
static void mfThreads(void)
{
    mrState S={0};mrPeer P;xmqttclient* C=mfOpen(&P,&S,1,0,4,NULL,NULL);mrIssue(&P,1);(void)mrCaptured(C,1);mrClose(C);mfTaker T[8]={{0}};xthread* Threads[8];for(unsigned I=0;I<8;++I){T[I].Client=C;Threads[I]=xrtThreadCreate(mfTake,T+I,0);testRequire(Threads[I]!=NULL,"concurrent immutable credential consumers");}
    unsigned Won=0;for(unsigned I=0;I<8;++I){testRequire(xrtThreadWaitFor(Threads[I],5000)==XWAIT_OK,"real credential taker join");xrtThreadDestroy(Threads[I]);if(T[I].Value){++Won;xrtTlsResumeRelease(T[I].Value);}}testRequire(Won==1&&!mrInfo(C).CachedBytes,"one WSS cached reference has one winner");mfStop(&P,C);xrtTlsResumeRelease(S.ServerTicket);
}
typedef struct mfNotification {xnetengine* Engine;xtlsresume* Value;xatomic32 Taken;} mfNotification;
static void mfNotify(xmqttclient* C,ptr Data)
{mfNotification* N=Data;if(!xrtAtomic32Load(&N->Taken,XMEMORY_ACQUIRE)&&xmqttClientTlsTakeResume(C,&N->Value)){testRequire(xrtNetWorkerIsCurrent(xrtNetEngineWorker(N->Engine,1))&&xmqttClientClose(C),"ordinary deferred Client Notify can take/Close reentrantly");xrtAtomic32Store(&N->Taken,1,XMEMORY_RELEASE);}}
static void mfNotifyCase(void)
{
    mrState S={0};mrPeer P;mrInit(&P,&S,XMQTT_VERSION_5,1,XNET_PORT_SELECT);mfNotification N={.Engine=P.Peer.Base.Engine};xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=mfNotify;K.User=&N;char Authority[128];xmqttclient* C=mrOpen(&P,1,NULL,false,4,&K,Authority,sizeof(Authority));xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"Notify fixture ready");xrtFutureDestroy(F);mrIssue(&P,1);mqttWsWait(&N.Taken,1,"deferred captured-credential callback");mfStop(&P,C);xtlsresumeinfo I;testRequire(N.Value&&xrtTlsResumeInfo(N.Value,&I)&&I.Secret.Size,"reentrant callback independently owns credential after native joins");xrtTlsResumeRelease(N.Value);xrtTlsResumeRelease(S.ServerTicket);
}
static void mfOom(void)
{
    const size_t Limits[]={1,4096,8192,16384,32768,65536};for(unsigned I=0;I<sizeof(Limits)/sizeof(*Limits);++I){mrState S={0};mrPeer P;mrInit(&P,&S,XMQTT_VERSION_5,0,XNET_PORT_SELECT);P.Peer.Base.UseBudget=true;P.Peer.Base.AllocationLimit=Limits[I];xmqttclientconfig K;xmqttClientConfigInit(&K);char Authority[128];xmqttclient* C=mrOpen(&P,0,NULL,false,4,&K,Authority,sizeof(Authority));xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"OOM leaves native owner usable for the fixture's explicit second admission");xrtFutureDestroy(F);(void)mrCaptured(C,1);mfStop(&P,C);xrtTlsResumeRelease(S.ServerTicket);}
}
static void mfIdentityAndFallback(void)
{
    mrState S={0};mrPeer P;xmqttclient* C=mfOpen(&P,&S,1,0,4,NULL,NULL);mrIssue(&P,1);(void)mrCaptured(C,1);xtlsresume* Credential=NULL;testRequire(xmqttClientTlsTakeResume(C,&Credential),"credential for actual fallback/identity policy");mfStop(&P,C);
    mrInit(&P,&S,XMQTT_VERSION_5,4,XNET_PORT_SELECT);xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Context=P.Peer.Base.Context;T.Session.Verifier=P.Peer.Base.Verifier;T.Session.ServerName=XRT_STR_LITERAL("example.com");T.Session.Resume=Credential;xmqttclient* Sentinel=(xmqttclient*)(uintptr_t)1;char URI[128];int N=snprintf(URI,sizeof(URI),"wss://127.0.0.1:%u/mqtt?x=%%00",(unsigned)P.Peer.Base.Address.Port);
    testRequire(N>0&&!xmqttClientWssConnectUri(P.Peer.Base.Engine,NULL,(xstrview){URI,(size_t)N},1,NULL,NULL,&T,&P.Peer.Base.Connect,&Sentinel)&&Sentinel==(xmqttclient*)(uintptr_t)1,"URI verification-name inference cannot reuse another principal's credential");xrtClearError();mqttWsConnectStop(&P.Peer);
    S.Refuse=true;mrInit(&P,&S,XMQTT_VERSION_5,1,XNET_PORT_SELECT);xmqttclientconfig K;xmqttClientConfigInit(&K);char Authority[128];C=mrOpen(&P,1,Credential,false,4,&K,Authority,sizeof(Authority));xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK&&mrInfo(C).Established&&!mrInfo(C).Resumed,"native unknown-ticket policy performs actual full-handshake fallback");mrPublish(C,1);mfStop(&P,C);xrtFutureDestroy(F);
    mrInit(&P,&S,XMQTT_VERSION_5,1,XNET_PORT_SELECT);C=mrOpen(&P,1,Credential,true,4,&K,Authority,sizeof(Authority));F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status!=XMQTT_STATUS_OK&&!mrInfo(C).Established&&!mrInfo(C).Resumed&&xrtAtomic32Load(&P.Peer.Header,XMEMORY_ACQUIRE)==0,"ResumeOnly really rejects fallback before HTTP/MQTT application bytes");mfStop(&P,C);xrtFutureDestroy(F);xrtTlsResumeRelease(Credential);xrtTlsResumeRelease(S.ServerTicket);
}
int main(void){setbuf(stdout,NULL);mfQuota();mfIdentityAndFallback();mfThreads();mfNotifyCase();mfOom();puts("[PASS] WSS credential quota/replacement/disabled/concurrency/reentrant Notify/real OOM and joins");return 0;}
