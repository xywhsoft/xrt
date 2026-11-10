#include "test.h"
#include <xmqtt/reconnect.h>
#include <xrt/thread.h>
#include <xrt/atomic.h>
#include <xrt/time.h>
enum {RC_FAIL=1,RC_PERSIST=2,RC_CUT_PUBLISH=4,RC_ASSIGNED=8,RC_WRONG_FACTORY=16,RC_WORKER_BUDGET=32,RC_ALWAYS_CLEAN=64};
typedef struct rcPeer rcPeer;
typedef struct rcConnection {rcPeer* Peer;xnetstream* Stream;xatomic32 Closed;size_t Used;unsigned Index;unsigned char Input[4096];} rcConnection;
struct rcPeer {
    xnetengine* Engine;xtaskpool* Pool;xnetlistener* Listener;xnetaddr Address;xmqttversion Version;unsigned Mode;
    xatomic32 Accepted,Connects,Publishes,Replays,FactoryCalls,ListenerClosed,Notifications;
    rcConnection Connections[16];double FactoryAt[32];bool Clean[32];size_t IdBytes[32];uint16 ReplayId;
    xallocationbudget* Budget;
};
static void rcWait(xatomic32* Value,uint32 Count)
{double End=xrtTimer()+5;while(xrtAtomic32Load(Value,XMEMORY_ACQUIRE)<Count){testRequire(xrtTimer()<End,"reconnect fixture deadline");xrtSleep(1);}}
static void rcSend(rcConnection* C,const xmqttpacket* P)
{
    unsigned char Wire[512];size_t N=0;bool OK=C->Peer->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),P,XMQTT_SERVER_TO_CLIENT,NULL,&N):xmqttPacketWrite311(Wire,sizeof(Wire),P,XMQTT_SERVER_TO_CLIENT,NULL,&N);
    testRequire(OK && xrtNetStreamSend(C->Stream,Wire,N)==XNET_RESULT_OK,"reconnect fixture output");
}
static void rcRead(xnetstream* Stream,xnetbuf* Buffer,ptr Data)
{
    (void)Stream;rcConnection* C=Data;rcPeer* P=C->Peer;size_t N=xrtNetBufSize(Buffer);testRequire(N<=sizeof(C->Input)-C->Used,"reconnect fixture capacity");C->Used+=xrtNetBufRead(Buffer,C->Input+C->Used,N);
    while(C->Used){xmqttwireconfig W;xmqttWireConfigInit(&W);W.Version=P->Version;xmqttframe F;xmqttwirestatus S=xmqttFrameRead((xbytesview){C->Input,C->Used},&W,&F);
        testRequire(S!=XMQTT_WIRE_ERROR,"reconnect framing");if(S==XMQTT_WIRE_MORE)break;
        xmqttproperty Props[16];xmqttcodecbuffers B={Props,16,NULL,0};xmqttpacket Q;testRequire((P->Version==XMQTT_VERSION_5?xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Q,NULL):xmqttPacketRead311(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Q,NULL))==XMQTT_CODEC_DONE,"reconnect fixture packet");
        if(Q.Type==XMQTT_PACKET_CONNECT){
            if(P->Mode&RC_ASSIGNED)testRequire(C->Index?Q.Body.Connect.ClientId.Size==10 && !memcmp(Q.Body.Connect.ClientId.Data,"assigned-r",10):!Q.Body.Connect.ClientId.Size,"assigned id reused by automatic retry");
            xmqttpacket A;xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);A.SessionPresent=C->Index && (P->Mode&RC_PERSIST);
            xmqttproperty Id={XMQTT_PROPERTY_ASSIGNED_CLIENT_ID,XMQTT_PROPERTY_STRING,{0}};Id.Value.Text=XRT_STR_LITERAL("assigned-r");if((P->Mode&RC_ASSIGNED) && !C->Index)A.Properties=(xmqttpropertylist){&Id,1};
            rcSend(C,&A);xrtAtomic32FetchAdd(&P->Connects,1,XMEMORY_RELEASE);
            if(!C->Index){xmqttPacketInit(XMQTT_PACKET_PUBLISH,&A);A.Body.Publish.Topic=XRT_STR_LITERAL("queued");A.Body.Publish.Payload=XRT_BYTES_LITERAL("before-reconnect");rcSend(C,&A);}
        }else if(Q.Type==XMQTT_PACKET_PUBLISH){
            if((Q.Flags>>1)&3u){
                if((P->Mode&RC_CUT_PUBLISH) && !C->Index){P->ReplayId=Q.Identifier;testRequire(xrtNetStreamAbort(C->Stream),"cut original QoS1 before ACK");}
                else {if(P->Mode&RC_CUT_PUBLISH){testRequire((Q.Flags&8u) && Q.Identifier==P->ReplayId,"automatic Session replay uses original id and DUP");xrtAtomic32FetchAdd(&P->Replays,1,XMEMORY_RELEASE);}
                    xmqttpacket A;xmqttPacketInit(XMQTT_PACKET_PUBACK,&A);A.Identifier=Q.Identifier;rcSend(C,&A);}
            }
            xrtAtomic32FetchAdd(&P->Publishes,1,XMEMORY_RELEASE);
        }else testRequire(false,"unexpected reconnect fixture request");
        C->Used-=F.Wire.Size;if(C->Used)memmove(C->Input,C->Input+F.Wire.Size,C->Used);
    }
}
static void rcClosed(xnetstream* Stream,xnetresult Result,const xerror* Error,ptr Data)
{(void)Stream;(void)Result;(void)Error;xrtAtomic32Store(&((rcConnection*)Data)->Closed,1,XMEMORY_RELEASE);}
static void rcEnd(xnetstream* Stream,ptr Data)
{(void)Data;(void)xrtNetStreamAbort(Stream);}
static bool rcAccept(xnetlistener* Listener,xnetstream* Stream,ptr Data)
{
    (void)Listener;rcPeer* P=Data;unsigned N=xrtAtomic32Load(&P->Accepted,XMEMORY_RELAXED);testRequire(N<16,"bounded reconnect fixture connections");rcConnection* C=P->Connections+N;C->Peer=P;C->Index=N;C->Stream=Stream;
    static const xnetstreamevents E={NULL,rcRead,rcEnd,NULL,NULL,NULL,rcClosed};testRequire(xrtNetStreamSetEvents(Stream,&E,C),"reconnect fixture events");xrtAtomic32Store(&P->Accepted,N+1,XMEMORY_RELEASE);return true;
}
static void rcListenerClosed(xnetlistener* Listener,ptr Data)
{(void)Listener;xrtAtomic32Store(&((rcPeer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static void rcStart(rcPeer* P,xmqttversion Version,unsigned Mode)
{
    memset(P,0,sizeof(*P));P->Version=Version;P->Mode=Mode;
    xrtAtomic32Init(&P->Accepted,0);xrtAtomic32Init(&P->Connects,0);xrtAtomic32Init(&P->Publishes,0);xrtAtomic32Init(&P->Replays,0);xrtAtomic32Init(&P->FactoryCalls,0);xrtAtomic32Init(&P->ListenerClosed,0);xrtAtomic32Init(&P->Notifications,0);
    for(unsigned I=0;I<16;++I)xrtAtomic32Init(&P->Connections[I].Closed,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;P->Engine=xrtNetEngineCreate(&E);testRequire(P->Engine && xrtNetEngineStart(P->Engine),"reconnect Engine");
    xtaskpoolconfig T={1,1,0};P->Pool=xrtTaskPoolCreate(&T);testRequire(P->Pool!=NULL,"borrowed bounded cleanup Pool");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);L.Distribution=XNET_ACCEPT_LOCAL;static const xnetlistenerevents V={rcAccept,NULL,rcListenerClosed};
    P->Listener=xrtNetListen(P->Engine,&L,&V,NULL,P);testRequire(P->Listener && xrtNetListenerLocal(P->Listener,&P->Address),"reconnect listener");
}
static void rcStop(rcPeer* P)
{
    testRequire(xrtNetListenerClose(P->Listener),"reconnect listener close");rcWait(&P->ListenerClosed,1);
    unsigned N=xrtAtomic32Load(&P->Accepted,XMEMORY_ACQUIRE);
    for(unsigned I=0;I<N;++I){rcConnection* C=P->Connections+I;rcWait(&C->Closed,1);testRequire(xrtNetStreamWaitRetired(C->Stream,5000)==XWAIT_OK,"fixture native retirement");xrtNetStreamDestroy(C->Stream);}
    xrtNetListenerDestroy(P->Listener);testRequire(xrtTaskPoolClose(P->Pool) && xrtTaskPoolWaitFor(P->Pool,5000)==XWAIT_OK && xrtTaskPoolDestroy(P->Pool),"cleanup Pool join");
    testRequire(xrtNetEngineStop(P->Engine) && xrtNetEngineDestroy(P->Engine),"reconnect Engine physical join");
}
static xmqttclientstatus rcFactory(xnetengine* E,uint64 A,const xmqttclientconfig* K,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,ptr Data,xmqttclient** C)
{
    rcPeer* P=Data;unsigned N=xrtAtomic32Load(&P->FactoryCalls,XMEMORY_RELAXED);testRequire(N<32,"bounded fixture attempts");P->FactoryAt[N]=xrtTimer();P->Clean[N]=Q->Body.Connect.CleanStart;P->IdBytes[N]=Q->Body.Connect.ClientId.Size;bool OK;
    if(P->Mode&RC_FAIL){xrtSetErrorKind(XERR_STATE);OK=false;}
    else {xallocationscope Scope={0};if(P->Mode&RC_WORKER_BUDGET)testRequire(xrtAllocationBudgetEnter(P->Budget,&Scope),"factory allocation scope");
        OK=P->Mode&RC_WRONG_FACTORY?xmqttClientConnect(E,&P->Address,A,K,Q,C):xmqttClientConnectSession(E,&P->Address,A,K,S,I,Q,C);
        if(P->Mode&RC_WORKER_BUDGET)testRequire(xrtAllocationBudgetLeave(&Scope),"factory scope leave");}
    xrtAtomic32Store(&P->FactoryCalls,N+1,XMEMORY_RELEASE);return OK?XMQTT_STATUS_OK:(P->Mode&RC_FAIL)?XMQTT_STATUS_INITIALIZATION:xrtErrorKind(xrtGetError())==XERR_MEMORY?XMQTT_STATUS_MEMORY:XMQTT_STATUS_INITIALIZATION;
}
static void rcNotify(xmqttreconnect* R,ptr Data)
{
    rcPeer* P=Data;testRequire(xmqttReconnectWaitFor(R,0)==XWAIT_ERROR && !xmqttReconnectDestroy(R),"controller worker cannot wait/destroy");xrtClearError();xrtAtomic32FetchAdd(&P->Notifications,1,XMEMORY_RELEASE);
}
static void rcConfig(rcPeer* P,xmqttreconnectconfig* K,xmqttpacket* Q,xmqttproperty* Expiry)
{
    testRequire(xmqttReconnectConfigInit(K),"reconnect defaults");K->Factory=rcFactory;K->FactoryData=P;K->Notify=rcNotify;K->User=P;K->Client.Version=P->Version;K->InitialDelay=5;K->MaxDelay=20;K->JitterPermille=0;
    if(P->Mode&RC_ALWAYS_CLEAN)K->CleanStartFirstOnly=false;
    xmqttPacketInit(XMQTT_PACKET_CONNECT,Q);Q->Body.Connect.ClientId=P->Mode&RC_ASSIGNED?(xstrview){NULL,0}:XRT_STR_LITERAL("managed");Q->Body.Connect.CleanStart=!(P->Mode&RC_PERSIST);
    *Expiry=(xmqttproperty){XMQTT_PROPERTY_SESSION_EXPIRY,XMQTT_PROPERTY_UINT32,{0}};Expiry->Value.Number=600;if(P->Version==XMQTT_VERSION_5)Q->Properties=(xmqttpropertylist){Expiry,1};
}
static xmqttreconnectinfo rcOnline(xmqttreconnect* R,uint64 Connections)
{
    double End=xrtTimer()+5;xmqttreconnectinfo I;
    do{testRequire(xmqttReconnectInfo(R,&I) && I.State<XMQTT_RECONNECT_STOPPED && xrtTimer()<End,"automatic online deadline");if(I.State==XMQTT_RECONNECT_ONLINE && I.Connections>=Connections)return I;xrtSleep(1);}while(true);
}
static void rcClose(xmqttreconnect* R)
{testRequire(xmqttReconnectClose(R) && xmqttReconnectWaitFor(R,5000)==XWAIT_OK && xmqttReconnectWaitFor(R,0)==XWAIT_OK && xmqttReconnectDestroy(R),"controller and all old Clients physically retired");}
static void rcHappy(xmqttversion Version,unsigned Mode)
{
    rcPeer P;rcStart(&P,Version,Mode);xmqttreconnectconfig K;xmqttpacket Q;xmqttproperty Expiry;rcConfig(&P,&K,&Q,&Expiry);K.MaxLeases=1;xmqttreconnect* R=NULL;
    testRequire(xmqttReconnectCreate(P.Engine,P.Pool,1,&K,&Q,&R),"automatic connection factory");(void)rcOnline(R,1);xmqttclientlease L={0},Other={0};testRequire(xmqttReconnectAcquire(R,&L),"online lease");
    xrtSetErrorKind(XERR_STATE);const xerror* Ambient=xrtGetError();testRequire(!xmqttReconnectAcquire(R,&Other) && !Other.Owner && xrtGetError()==Ambient,"lease-cap output/error atomicity");xrtClearError();
    xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Body.Publish.Topic=XRT_STR_LITERAL("work");Pub.Body.Publish.Payload=XRT_BYTES_LITERAL("one");if(Mode&RC_CUT_PUBLISH)Pub.Flags=2;
    xfuture* F=NULL;testRequire(xmqttClientPublishAsync(L.Client,&Pub,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"leased ordinary admission");rcWait(&P.Publishes,1);
    if(!(Mode&RC_CUT_PUBLISH))testRequire(xrtFutureWaitFor(F,5000)==XWAIT_OK && ((const xmqttwriteresult*)xrtFutureValue(F))->Status==XMQTT_STATUS_OK && xrtNetStreamAbort(P.Connections[0].Stream),"first Client work and transport interruption");
    testRequire(xmqttClientWaitFor(L.Client,5000)==XWAIT_OK && xmqttReconnectWaitFor(R,0)==XWAIT_TIMEOUT,"lease prevents controller retirement");
    xmqttreconnectinfo I;testRequire(xmqttReconnectInfo(R,&I) && I.State==XMQTT_RECONNECT_RETIRING,"logical Client closed under lease");xrtSleep(25);testRequire(xrtAtomic32Load(&P.FactoryCalls,XMEMORY_ACQUIRE)==1,"no replacement before lease and physical owner retire");
    xmqttmessage* M=NULL;double End=xrtTimer()+5;while(!xmqttReconnectTakeMessage(R,&M)){testRequire(xrtTimer()<End,"cross-generation Inbox");xrtSleep(1);}testRequire(xmqttMessagePacket(M)->Body.Publish.Payload.Size==16,"old Inbox message preserved offline");
    testRequire(xmqttReconnectRelease(&L) && !L.Owner,"lease release wakes retirement");I=rcOnline(R,2);testRequire(I.Generation==2 && I.Attempts==2,"automatic new generation");
    if(Mode&RC_CUT_PUBLISH){rcWait(&P.Replays,1);testRequire(xrtFutureWaitFor(F,0)==XWAIT_OK && ((const xmqttwriteresult*)xrtFutureValue(F))->Status!=XMQTT_STATUS_OK,"old Future not rewritten by recovered ACK");}
    testRequire(P.Clean[1]==((Mode&RC_ALWAYS_CLEAN)!=0) && P.Clean[0]==!(Mode&RC_PERSIST),"explicit first-only versus always-clean retry policy");
    if(Mode&RC_ASSIGNED)testRequire(!P.IdBytes[0] && P.IdBytes[1]==10,"bound assigned id reused");
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"lease allocation budget");
    for(unsigned J=0;J<10000;++J)testRequire(xmqttReconnectAcquire(R,&L) && xmqttReconnectRelease(&L),"allocation-free lease cycle");
    testRequire(xrtAllocationBudgetLeave(&Scope),"lease scope leave");xallocationbudgetinfo BI={0};BI.Size=sizeof(BI);testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.LiveBytes && !BI.LiveCount && !BI.Rejections,"no per-lease heap");xrtAllocationBudgetFree(B);
    rcClose(R);rcStop(&P);testRequire(xmqttMessagePacket(M)->Body.Publish.Payload.Size==16 && xrtFutureState(F)==XFUTURE_RESOLVED,"messages/Futures outlive controller and Engines");xmqttMessageRelease(M);xrtFutureDestroy(F);
}
static void rcPolicy(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){rcPeer P;rcStart(&P,XMQTT_VERSION_5,RC_FAIL);xmqttreconnectconfig K;xmqttpacket Q;xmqttproperty Expiry;rcConfig(&P,&K,&Q,&Expiry);K.RetryStatuses=UINT32_C(1)<<XMQTT_STATUS_INITIALIZATION;K.InitialDelay=10;K.MaxDelay=25;K.MaxAttempts=4;
        if(Mode==1){K.MaxAttempts=0;K.RecoveryTimeout=25;K.InitialDelay=K.MaxDelay=15;}if(Mode==2){K.InitialDelay=K.MaxDelay=10000;}
        xmqttreconnect* R=NULL;testRequire(xmqttReconnectCreate(P.Engine,P.Pool,1,&K,&Q,&R),"policy controller");
        if(Mode==2){rcWait(&P.FactoryCalls,1);double End=xrtTimer()+5;xmqttreconnectinfo I;do{testRequire(xmqttReconnectInfo(R,&I) && xrtTimer()<End,"backoff arm");if(I.State!=XMQTT_RECONNECT_WAITING)xrtSleep(1);}while(I.State!=XMQTT_RECONNECT_WAITING);testRequire(xmqttReconnectClose(R),"stop long backoff");}
        testRequire(xmqttReconnectWaitFor(R,5000)==XWAIT_OK,"bounded policy terminates");xmqttreconnectinfo I;xmqttReconnectInfo(R,&I);
        if(!Mode){testRequire(I.State==XMQTT_RECONNECT_FAILED && I.Attempts==4,"exact maximum consecutive attempts");testRequire(P.FactoryAt[1]-P.FactoryAt[0]>=0.009 && P.FactoryAt[2]-P.FactoryAt[1]>=0.019 && P.FactoryAt[3]-P.FactoryAt[2]>=0.024,"saturating exponential intervals");}
        else if(Mode==1)testRequire(I.State==XMQTT_RECONNECT_FAILED && I.LastStatus==XMQTT_STATUS_TIMEOUT && I.Attempts<=2,"one original outage deadline");
        else testRequire(I.State==XMQTT_RECONNECT_STOPPED && I.Attempts==1,"cancelled timer joins without waiting ten seconds");
        testRequire(xmqttReconnectDestroy(R),"policy physical owner");rcStop(&P);
    }
}
static void rcCleanupFailure(void)
{
    for(unsigned Wrong=0;Wrong<2;++Wrong){rcPeer P;rcStart(&P,XMQTT_VERSION_5,Wrong?RC_WRONG_FACTORY:0);xmqttreconnectconfig K;xmqttpacket Q;xmqttproperty E;rcConfig(&P,&K,&Q,&E);xmqttreconnect* R=NULL;
        testRequire(xmqttReconnectCreate(P.Engine,P.Pool,1,&K,&Q,&R),"cleanup fallback controller");
        if(!Wrong){(void)rcOnline(R,1);testRequire(xrtTaskPoolClose(P.Pool) && xmqttReconnectClose(R),"closed borrowed Pool");}
        testRequire(xmqttReconnectWaitFor(R,5000)==XWAIT_OK,"external wait assists retained physical cleanup");xmqttreconnectinfo I;xmqttReconnectInfo(R,&I);testRequire(I.State==XMQTT_RECONNECT_FAILED,"invalid factory/cleanup admission has a typed terminal failure");testRequire(xmqttReconnectDestroy(R),"fallback owner consumed once");rcStop(&P);
    }
}
static void rcAllocation(void)
{
    const size_t Limits[]={1,512,2048,8192,65536,262144,1048576};
    for(unsigned Worker=0;Worker<2;++Worker)for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){rcPeer P;rcStart(&P,XMQTT_VERSION_5,Worker?RC_WORKER_BUDGET:0);xmqttreconnectconfig K;xmqttpacket Q;xmqttproperty E;rcConfig(&P,&K,&Q,&E);testRequire(xrtAllocationBudgetCreate(Limits[J],&P.Budget),"controller allocation quota");xallocationscope Scope={0};
        if(!Worker)testRequire(xrtAllocationBudgetEnter(P.Budget,&Scope),"constructor allocation scope");
        xmqttreconnect* R=(xmqttreconnect*)(uintptr_t)1;bool OK=xmqttReconnectCreate(P.Engine,P.Pool,1,&K,&Q,&R);
        if(!Worker)testRequire(xrtAllocationBudgetLeave(&Scope),"constructor scope leave");
        if(OK){if(Worker){rcWait(&P.FactoryCalls,1);xmqttreconnectinfo I;xmqttReconnectInfo(R,&I);testRequire(I.Attempts==1,"worker quota attempt");}rcClose(R);}else testRequire(R==(xmqttreconnect*)(uintptr_t)1,"constructor output atomicity");
        rcStop(&P);xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(P.Budget,&I) && !I.LiveBytes && !I.LiveCount,"controller/Session/Client OOM lifetime leak");xrtAllocationBudgetFree(P.Budget);
    }
}
typedef struct rcLeaseRace {xmqttreconnect* Controller;xatomic32 Done;} rcLeaseRace;
static int32 rcLeaseThread(ptr Data)
{
    rcLeaseRace* T=Data;
    for(unsigned I=0;I<5000;++I){xmqttclientlease L={0};if(xmqttReconnectAcquire(T->Controller,&L)){xmqttclientinfo C;testRequire(xmqttClientInfo(L.Client,&C),"leased Client stays live across transport closure");testRequire(xmqttReconnectRelease(&L),"concurrent lease release");}}
    xrtAtomic32FetchAdd(&T->Done,1,XMEMORY_RELEASE);return 0;
}
static void rcConcurrentLeases(void)
{
    rcPeer P;rcStart(&P,XMQTT_VERSION_5,0);xmqttreconnectconfig K;xmqttpacket Q;xmqttproperty E;rcConfig(&P,&K,&Q,&E);xmqttreconnect* R=NULL;testRequire(xmqttReconnectCreate(P.Engine,P.Pool,1,&K,&Q,&R),"concurrent controller");(void)rcOnline(R,1);
    rcWait(&P.Accepted,1);rcLeaseRace T={R,{0}};xrtAtomic32Init(&T.Done,0);xthread* Threads[4];for(unsigned I=0;I<4;++I){Threads[I]=xrtThreadCreate(rcLeaseThread,&T,0);testRequire(Threads[I]!=NULL,"lease producer");}
    testRequire(xrtNetStreamAbort(P.Connections[0].Stream),"transport closes during concurrent lease entries");rcWait(&T.Done,4);
    for(unsigned I=0;I<4;++I){testRequire(xrtThreadWaitFor(Threads[I],5000)==XWAIT_OK,"lease producer join");xrtThreadDestroy(Threads[I]);}
    (void)rcOnline(R,2);rcClose(R);rcStop(&P);
}
typedef struct rcBlocker {xatomic32 Entered,Release;} rcBlocker;
static xtaskoutcome rcBlockTask(xcancel* Cancel,ptr Data,xtaskvalue* Value)
{
    (void)Cancel;(void)Value;rcBlocker* B=Data;xrtAtomic32Store(&B->Entered,1,XMEMORY_RELEASE);double End=xrtTimer()+5;
    while(!xrtAtomic32Load(&B->Release,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"bounded blocked cleanup Pool");xrtSleep(1);}return XTASK_SUCCESS;
}
static void rcCancelledCleanup(void)
{
    rcPeer P;rcStart(&P,XMQTT_VERSION_5,0);xmqttreconnectconfig K;xmqttpacket Q;xmqttproperty E;rcConfig(&P,&K,&Q,&E);xmqttreconnect* R=NULL;testRequire(xmqttReconnectCreate(P.Engine,P.Pool,1,&K,&Q,&R),"cancelled cleanup controller");(void)rcOnline(R,1);
    rcBlocker B={0};xrtAtomic32Init(&B.Entered,0);xrtAtomic32Init(&B.Release,0);xfuture* F=xrtTaskSubmit(P.Pool,rcBlockTask,&B,NULL);testRequire(F!=NULL,"occupy borrowed Pool worker");rcWait(&B.Entered,1);testRequire(xmqttReconnectClose(R),"close with queued cleanup");
    xtaskpoolstats Stats;double End=xrtTimer()+5;do{testRequire(xrtTaskPoolGet(P.Pool,&Stats) && xrtTimer()<End,"cleanup accepted behind blocked worker");if(!Stats.Queued)xrtSleep(1);}while(!Stats.Queued);
    testRequire(xrtTaskPoolCancel(P.Pool),"cancel accepted cleanup before its Proc");xrtAtomic32Store(&B.Release,1,XMEMORY_RELEASE);
    testRequire(xmqttReconnectWaitFor(R,5000)==XWAIT_OK && xmqttReconnectDestroy(R),"Data finalizer retires Client even when Proc is skipped");rcStop(&P);xrtFutureDestroy(F);
}
static void rcInvalid(void)
{
    rcPeer P;rcStart(&P,XMQTT_VERSION_5,0);xmqttreconnectconfig K;xmqttpacket Q;xmqttproperty E;rcConfig(&P,&K,&Q,&E);
    for(unsigned I=0;I<4;++I){xmqttreconnectconfig Bad=K;if(!I)Bad.Client.Version=(xmqttversion)99;else if(I==1)Bad.JitterPermille=1001;else if(I==2)Bad.Factory=NULL;else Bad.RecoveryTimeout=-2;
        xmqttreconnect* R=(xmqttreconnect*)(uintptr_t)1;xrtSetErrorKind(XERR_MEMORY);
        testRequire(!xmqttReconnectCreate(P.Engine,P.Pool,1,&Bad,&Q,&R) && R==(xmqttreconnect*)(uintptr_t)1 && xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"invalid policy has new diagnostic and unchanged output");xrtClearError();}
    testRequire(!xrtAtomic32Load(&P.FactoryCalls,XMEMORY_ACQUIRE),"invalid policy never invokes factory");rcStop(&P);
}
int main(void)
{
    testRequire(xmqttReconnectDestroy(NULL) && xmqttReconnectWaitFor(NULL,0)==XWAIT_ERROR,"null controller lifecycle");xrtClearError();
    rcInvalid();
    rcHappy(XMQTT_VERSION_311,0);rcHappy(XMQTT_VERSION_5,0);rcHappy(XMQTT_VERSION_311,RC_ALWAYS_CLEAN);rcHappy(XMQTT_VERSION_5,RC_ALWAYS_CLEAN);rcHappy(XMQTT_VERSION_5,RC_ASSIGNED);rcHappy(XMQTT_VERSION_311,RC_PERSIST|RC_CUT_PUBLISH);rcHappy(XMQTT_VERSION_5,RC_PERSIST|RC_CUT_PUBLISH);rcPolicy();rcCleanupFailure();rcConcurrentLeases();rcCancelledCleanup();rcAllocation();
    puts("[PASS] automatic MQTT reconnect: generation leases, Inbox/assigned-id/QoS recovery, backoff/deadline/stop, cleanup fallback and OOM physical joins");return 0;
}
