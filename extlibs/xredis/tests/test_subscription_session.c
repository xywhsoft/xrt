#include "test.h"
#include "support/subscription_session_peer.h"
static void sessionBarrierWorker(xnetworker* W,ptr Data){(void)W;xrtAtomic32FetchAdd((xatomic32*)Data,1,XMEMORY_RELEASE);}
static void sessionBarrier(sessionpeer* P)
{xatomic32 N;xrtAtomic32Init(&N,0);for(unsigned I=0;I<2;++I)testRequire(xrtNetEnginePost(P->Engine,I,sessionBarrierWorker,&N),"barrier admission");redisPeerWait(&N,2);}
static xredissubscriptions* sessionStore(void)
{xredissubscriptions* S=NULL;testRequire(xredisSubscriptionsCreate(NULL,&S),"session desired set");return S;}
static uint64 sessionSeed(xredissubscriptions* S,xredispubsubcommand Op,const xbytesview* Names,size_t N)
{uint64 Revision=0;testRequire(xredisSubscriptionsApply(S,Op,Names,N,&Revision)==XREDIS_ADMISSION_ACCEPTED,"initial intent");return Revision;}
static xredissubscriptionsessioninfo sessionInfo(xredissubscriptionsession* S)
{xredissubscriptionsessioninfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriptionSessionInfo(S,&I),"session info");return I;}
static xredissubscriptionsessioncurrent sessionCurrent(xredissubscriptionsession* S,uint64 Generation,uint64 Revision)
{
    double End=xrtTimer()+10;
    for(;;){
#if defined(XREDIS_SESSION_WHITEBOX)
        xrtMutexLock(&S->Gate);testRequire(S->Closing || S->Info.State!=XREDIS_SESSION_ONLINE || S->Published!=NULL,"online state and current pointer publish atomically");xrtMutexUnlock(&S->Gate);
#endif
        xredissubscriptionsessioncurrent C={(xredisclient*)(uintptr_t)1,99,88};xredisadmission A=xredisSubscriptionSessionCurrent(S,&C);
        if(A==XREDIS_ADMISSION_ACCEPTED){if(C.Generation>=Generation && C.Revision>=Revision)return C;xredisClientRelease(C.Client);}
        else {if(A!=XREDIS_ADMISSION_OVERLOADED){xredissubscriptionsessioninfo I=sessionInfo(S);fprintf(stderr,"current gen=%llu rev=%llu admission=%d state=%d attempts=%llu failures=%llu last=%d applied=%llu desired=%llu\n",(unsigned long long)Generation,(unsigned long long)Revision,A,I.State,(unsigned long long)I.Attempts,(unsigned long long)I.Failures,I.LastFailure,(unsigned long long)I.AppliedRevision,(unsigned long long)I.DesiredRevision);}
        testRequire(A==XREDIS_ADMISSION_OVERLOADED && C.Client==(xredisclient*)(uintptr_t)1 && C.Generation==99 && C.Revision==88,"pending current preserves output");}
        testRequire(xrtTimer()<End,"online deadline");xrtThreadYield();}
}
static xredissubscriptionsession* sessionCreate(sessionpeer* P,sessionfactory* F,xredissubscriptions* Store,xredissubscriptionsessionconfig* K)
{xredissubscriptionsession* S=NULL;testRequire(xredisSubscriptionSessionCreate(P->Engine,0,Store,K,&S)==XREDIS_ADMISSION_ACCEPTED && S,"session constructor");(void)F;return S;}
static xfuture* sessionClose(xredissubscriptionsession* S,sessionfactory* F)
{
    xfuture* Closed=xredisSubscriptionSessionClosed(S);testRequire(Closed && xredisSubscriptionSessionClose(S) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"session physical close");
    xredissubscriptionsessioninfo I=sessionInfo(S);testRequire(I.State==XREDIS_SESSION_CLOSED && !I.NextAttempt && xrtAtomic32Load(&F->Drops,XMEMORY_ACQUIRE)==1,"closed context and state");
    xredissubscriptionsessioncurrent C={(xredisclient*)(uintptr_t)1,7,8};testRequire(xredisSubscriptionSessionCurrent(S,&C)==XREDIS_ADMISSION_CLOSED && C.Generation==7,"closed current");return Closed;
}
static void sessionAbortWorker(xnetworker* W,ptr Data)
{(void)W;sessionpeer* P=Data;uint32 N=xrtAtomic32Load(&P->Accepted,XMEMORY_ACQUIRE);testRequire(N!=0,"abort generation exists");xrtNetStreamAbort(P->Peers[(N-1)%16].Stream);}
static void sessionPublishWorker(xnetworker* W,ptr Data)
{
    (void)W;sessionpeer* P=Data;uint32 N=xrtAtomic32Load(&P->Accepted,XMEMORY_ACQUIRE);redispeer* C=P->Peers+(N-1)%16;
    static const char Tail[]="$7\r\nmessage\r\n$6\r\nstable\r\n$3\r\nv\0x\r\n";redisPeerSend(C,P->Resp2?"*3\r\n":">3\r\n",4);redisPeerSend(C,Tail,sizeof(Tail)-1);
}
static void sessionFlow(bool Resp2)
{
    sessionpeer P;sessionPeerStart(&P);P.Resp2=Resp2;sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);F.FailFirst=2;K.Replay.MaxBatchNames=1;
    xredissubscriptions* Store=sessionStore();xbytesview Channels[]={XRT_BYTES_LITERAL("stable"),XRT_BYTES_LITERAL("old\0x"),{NULL,0}},Pattern=XRT_BYTES_LITERAL("p*"),Shard[]={XRT_BYTES_LITERAL("{same}:one"),XRT_BYTES_LITERAL("{same}:two")};
    uint64 Revision=sessionSeed(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,Channels,3);Revision=sessionSeed(Store,XREDIS_PUBSUB_CMD_PSUBSCRIBE,&Pattern,1);Revision=sessionSeed(Store,XREDIS_PUBSUB_CMD_SSUBSCRIBE,Shard,2);
    xredissubscriptionsession* S=sessionCreate(&P,&F,Store,&K);testRequire(xredisSubscriptionSessionRetain(S),"metadata retain");xredisSubscriptionSessionRelease(S);
    xfuture* Closed=xredisSubscriptionSessionClosed(S);testRequire(xrtFutureCancel(Closed),"shared observer cooperative cancellation");xredissubscriptionsessioncurrent First=sessionCurrent(S,1,Revision);
    testRequire(!xrtFutureDone(Closed) && sessionInfo(S).Attempts==3 && sessionInfo(S).Failures==2,"retry accounting and observer independence");
    for(unsigned N=1;N<3;++N)testRequire(F.Times[N]-F.Times[N-1]>=.009,"bounded retry backoff");
    uint64 Saved=999;testRequire(xredisSubscriptionsApply(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,Channels,1,&Saved)==XREDIS_ADMISSION_OVERLOADED && Saved==999,"exclusive edit ownership");
    xfuture* Control=(xfuture*)(uintptr_t)1;testRequire(xredisSubscriberCommandAsync(First.Client,XREDIS_PUBSUB_CMD_PING,NULL,0,100,&Control)==XREDIS_ADMISSION_OVERLOADED && Control==(xfuture*)(uintptr_t)1,"persistent native control ownership");
    xbytesview New=XRT_BYTES_LITERAL("new\0x");testRequire(xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,Channels+1,1,&Revision)==XREDIS_ADMISSION_ACCEPTED && xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&New,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"live local edits");
    xredissubscriptionsessioncurrent Updated=sessionCurrent(S,1,Revision);testRequire(Updated.Client==First.Client && Updated.Generation==1 && !xrtAtomic32Load(&P.StableRemoves,XMEMORY_ACQUIRE) && xrtAtomic32Load(&P.Removes,XMEMORY_ACQUIRE)==1,"incremental diff preserves unchanged generation and keys");xredisClientRelease(Updated.Client);
    uint32 Controls=xrtAtomic32Load(&P.Controls,XMEMORY_ACQUIRE);testRequire(xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&New,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"idempotent live edit");sessionBarrier(&P);testRequire(xrtAtomic32Load(&P.Controls,XMEMORY_ACQUIRE)==Controls,"no-op sends nothing");
    testRequire(xrtNetEnginePost(P.Engine,1,sessionPublishWorker,&P),"publish owned stable message");xredisreply* Retained=NULL;double End=xrtTimer()+10;while(!xredisClientTakePush(First.Client,&Retained)){testRequire(xrtTimer()<End,"message deadline");xrtThreadYield();}
    testRequire(xrtNetEnginePost(P.Engine,1,sessionAbortWorker,&P),"actual peer EOF");xredissubscriptionsessioncurrent Again=sessionCurrent(S,2,Revision);testRequire(Again.Client!=First.Client && sessionInfo(S).Attempts==4,"automatic replacement generation");
    xredissubscriberinfo Native={0};Native.Size=sizeof(Native);testRequire(xredisSubscriberInfo(Again.Client,&Native) && Native.Entries==6 && Native.Subscriptions==4 && Native.ShardSubscriptions==2,"replacement restores exact desired set");xredisClientRelease(Again.Client);
    xfuture* Final=sessionClose(S,&F);testRequire(((const xredissubscriptionsessionclosed*)xrtFutureValue(Final))->Status==XREDIS_REQUEST_OK,"explicit close remains OK after recovered failures");uint64 V=0;testRequire(xredisSubscriptionsApply(Store,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,NULL,0,&V)==XREDIS_ADMISSION_ACCEPTED,"claim returned after close");xredisSubscriptionsRelease(Store);sessionPeerStop(&P);
    testRequire(xrtFutureDone(Closed) && sessionInfo(S).State==XREDIS_SESSION_CLOSED,"retained metadata after Engine");xredispubsubevent E;testRequire(xredisPubSubEventRead(Retained,&E)==XREDIS_PUBSUB_EVENT && E.Payload.Size==3 && !memcmp(E.Payload.Data,"v\0x",3),"message after all physical owners");
    xredisclientinfo I={0};I.Size=sizeof(I);testRequire(xredisClientInfo(First.Client,&I) && I.State==XREDIS_CLIENT_CLOSED,"retained retired native metadata");xredisClientRelease(First.Client);xredisReplyRelease(Retained);xrtFutureDestroy(Closed);xrtFutureDestroy(Final);xredisSubscriptionSessionRelease(S);
}
static void sessionHeldRevision(void)
{
    sessionpeer P;sessionPeerStart(&P);sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);K.Replay.MaxBatchNames=1;xredissubscriptions* Store=sessionStore();xbytesview Stable=XRT_BYTES_LITERAL("stable"),A=XRT_BYTES_LITERAL("a"),B=XRT_BYTES_LITERAL("b");uint64 Revision=sessionSeed(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Stable,1);xredissubscriptionsession* S=sessionCreate(&P,&F,Store,&K);xredissubscriptionsessioncurrent Initial=sessionCurrent(S,1,Revision);
    xrtAtomic32Store(&P.HoldControl,1,XMEMORY_RELEASE);uint32 Controls=xrtAtomic32Load(&P.Controls,XMEMORY_ACQUIRE);testRequire(xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&A,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"start held target");redisPeerWait(&P.Controls,Controls+1);
    testRequire(xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&B,1,&Revision)==XREDIS_ADMISSION_ACCEPTED && xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,&A,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"new intent while prior target in flight");
    xrtAtomic32Store(&P.HoldControl,0,XMEMORY_RELEASE);redisPeerReleaseBlocked(P.Peers);xredissubscriptionsessioncurrent Current=sessionCurrent(S,1,Revision);testRequire(Current.Client==Initial.Client && !xrtAtomic32Load(&P.StableRemoves,XMEMORY_ACQUIRE) && xrtAtomic32Load(&P.Removes,XMEMORY_ACQUIRE)==1,"finish captured target then reconcile latest without outage");xredisClientRelease(Current.Client);
    testRequire(xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,NULL,0,&Revision)==XREDIS_ADMISSION_ACCEPTED,"live family unsubscribe-all");Current=sessionCurrent(S,1,Revision);xredissubscriberinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriberInfo(Current.Client,&I) && !I.Entries && Current.Client==Initial.Client,"empty target stays on same transport");xredisClientRelease(Current.Client);xredisClientRelease(Initial.Client);xfuture* Closed=sessionClose(S,&F);xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);sessionPeerStop(&P);xrtFutureDestroy(Closed);
}
typedef struct sessionrace {xredissubscriptionsession* Session;xatomic32 Start,Done;unsigned Index;} sessionrace;
static int32 sessionWriter(ptr Data)
{
    sessionrace* R=Data;redisPeerWait(&R->Start,1);unsigned char Bytes[2]={'w',(unsigned char)R->Index};xbytesview Name={Bytes,2};
    for(unsigned N=0;N<64;++N){uint64 V;for(;;){xredisadmission A=xredisSubscriptionSessionApply(R->Session,(N&1u)?XREDIS_PUBSUB_CMD_UNSUBSCRIBE:XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&V);if(A==XREDIS_ADMISSION_ACCEPTED)break;testRequire(A==XREDIS_ADMISSION_OVERLOADED,"bounded concurrent constructor");xrtThreadYield();}}
    xrtAtomic32Store(&R->Done,1,XMEMORY_RELEASE);return 0;
}
static int32 sessionReader(ptr Data)
{
    sessionrace* R=Data;redisPeerWait(&R->Start,1);uint64 Revision=0;
    for(unsigned N=0;N<1024;++N){xredissubscriptionsessioninfo I=sessionInfo(R->Session);testRequire(I.DesiredRevision>=Revision,"desired revision monotonic across callers/worker");Revision=I.DesiredRevision;xredissubscriptionsessioncurrent C;xredisadmission A=xredisSubscriptionSessionCurrent(R->Session,&C);if(A==XREDIS_ADMISSION_ACCEPTED){testRequire(C.Generation==1,"live edits did not replace generation");xredisClientRelease(C.Client);}else testRequire(A==XREDIS_ADMISSION_OVERLOADED,"concurrent current admission");}
    xrtAtomic32Store(&R->Done,1,XMEMORY_RELEASE);return 0;
}
static void sessionConcurrent(void)
{
    sessionpeer P;sessionPeerStart(&P);sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);xredissubscriptions* Store=sessionStore();xbytesview Stable=XRT_BYTES_LITERAL("stable");uint64 Revision=sessionSeed(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Stable,1);xredissubscriptionsession* S=sessionCreate(&P,&F,Store,&K);xredissubscriptionsessioncurrent C=sessionCurrent(S,1,Revision);xredisClientRelease(C.Client);
    sessionrace R[6];xthread* T[6];for(unsigned N=0;N<6;++N){R[N].Session=S;R[N].Index=N;xrtAtomic32Init(&R[N].Start,0);xrtAtomic32Init(&R[N].Done,0);T[N]=xrtThreadCreate(N<2?sessionWriter:sessionReader,R+N,0);testRequire(T[N]!=NULL,"session concurrency thread");}for(unsigned N=0;N<6;++N)xrtAtomic32Store(&R[N].Start,1,XMEMORY_RELEASE);
    for(unsigned N=0;N<6;++N){testRequire(xrtThreadWaitFor(T[N],10000)==XWAIT_OK,"session concurrency join");xrtThreadDestroy(T[N]);}xredissubscriptionsinfo Info={0};Info.Size=sizeof(Info);testRequire(xredisSubscriptionsInfo(Store,&Info) && Info.Entries==1,"both writer final intents committed");C=sessionCurrent(S,1,Info.Revision);xredissubscriberinfo Native={0};Native.Size=sizeof(Native);testRequire(xredisSubscriberInfo(C.Client,&Native) && Native.Entries==1 && !xrtAtomic32Load(&P.StableRemoves,XMEMORY_ACQUIRE),"concurrent deltas settle exact set without unchanged loss");xredisClientRelease(C.Client);
    xfuture* Closed=sessionClose(S,&F);xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);sessionPeerStop(&P);xrtFutureDestroy(Closed);
}
static void sessionLargeDiff(void)
{
    sessionpeer P;sessionPeerStart(&P);P.KeyLimit=1102;sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);K.AttemptTimeout=20000;xredissubscriptions* Store=sessionStore();unsigned char Names[1100][16];xbytesview Views[1100];for(size_t N=0;N<1100;++N){int L=snprintf((char*)Names[N],16,"n%zu",N);testRequire(L>0 && L<16,"large fixture name");Views[N]=(xbytesview){Names[N],(size_t)L};}
    uint64 Revision=sessionSeed(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,Views,1024);Revision=sessionSeed(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,Views+1024,76);xredissubscriptionsession* S=sessionCreate(&P,&F,Store,&K);xredissubscriptionsessioncurrent C=sessionCurrent(S,1,Revision);xredisClientRelease(C.Client);
    xbytesview Extra=XRT_BYTES_LITERAL("extra");testRequire(xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Extra,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"diff across scan-turn boundary");C=sessionCurrent(S,1,Revision);xredissubscriberinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriberInfo(C.Client,&I) && I.Entries==1101 && !xrtAtomic32Load(&P.Removes,XMEMORY_ACQUIRE) && xrtAtomic32Load(&P.Adds,XMEMORY_ACQUIRE)==1101,"bounded multi-turn merge adds one key only");xredisClientRelease(C.Client);xfuture* Closed=sessionClose(S,&F);xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);sessionPeerStop(&P);xrtFutureDestroy(Closed);
}
static void sessionFailures(void)
{
    for(unsigned Mode=0;Mode<7;++Mode){sessionpeer P;sessionPeerStart(&P);sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);K.AttemptTimeout=60;K.MaxFailures=2;
        if(Mode==0)xrtAtomic32Store(&P.HoldHello,1,XMEMORY_RELEASE);
        if(Mode==1)xrtAtomic32Store(&P.BadControl,1,XMEMORY_RELEASE);
        if(Mode==2)F.Raw=true;
        if(Mode==3)P.Resp2=true;
        if(Mode==4)xrtAtomic32Store(&P.HoldControl,1,XMEMORY_RELEASE);
        if(Mode==5){F.FailFirst=100;K.MaxFailures=3;}
        if(Mode==6)F.CloseOnline=true;
        xredissubscriptions* Store=sessionStore();xbytesview Name=XRT_BYTES_LITERAL("stable");sessionSeed(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1);xredissubscriptionsession* S=sessionCreate(&P,&F,Store,&K);xfuture* Closed=xredisSubscriptionSessionClosed(S);
        xwaitresult Wait=xrtFutureWaitFor(Closed,10000);if(Wait!=XWAIT_OK){xredissubscriptionsessioninfo I=sessionInfo(S);fprintf(stderr,"mode=%u state=%d attempts=%llu failures=%llu generations=%llu last=%d\n",Mode,I.State,(unsigned long long)I.Attempts,(unsigned long long)I.Failures,(unsigned long long)I.Generations,I.LastFailure);}
        testRequire(Wait==XWAIT_OK && xrtFutureValue(Closed),"bounded failure/notify close");const xredissubscriptionsessionclosed* R=xrtFutureValue(Closed);
        xredisrequeststatus Expected=Mode==0 || Mode==4?XREDIS_REQUEST_TIMEOUT:Mode==1?XREDIS_REQUEST_SERVER_ERROR:Mode==6?XREDIS_REQUEST_OK:XREDIS_REQUEST_INITIALIZATION;
        testRequire(R->Status==Expected && R->Attempts==(Mode==0 || Mode==4?2u:Mode==5?3u:1u) && xrtAtomic32Load(&F.Drops,XMEMORY_ACQUIRE)==1,"typed terminal retry/fatal outcome");
        xredisSubscriptionsRelease(Store);xredisSubscriptionSessionRelease(S);sessionPeerStop(&P);testRequire(((const xredissubscriptionsessionclosed*)xrtFutureValue(Closed))->Status==Expected,"close result after Engine");xrtFutureDestroy(Closed);
    }
}
static void sessionPreclosedCandidate(void)
{
    sessionpeer P;sessionPeerStart(&P);sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);K.MaxFailures=2;
    xredisclientinitconfig Init;xredisClientInitConfigInit(&Init);testRequire(xredisClientConnectInit(P.Engine,&P.Address,1,NULL,&Init,&F.Preclosed),"managed early-close candidate");xfuture* Ready=xredisClientReady(F.Preclosed);testRequire(xrtFutureWaitFor(Ready,10000)==XWAIT_OK,"candidate readiness");xrtFutureDestroy(Ready);xfuture* NativeClosed=xredisClientClosed(F.Preclosed);testRequire(xredisClientClose(F.Preclosed) && xrtFutureWaitFor(NativeClosed,10000)==XWAIT_OK,"candidate retired before factory transfer");xrtFutureDestroy(NativeClosed);
    xredissubscriptions* Store=sessionStore();xbytesview Name=XRT_BYTES_LITERAL("stable");uint64 Revision=sessionSeed(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1);xredissubscriptionsession* S=sessionCreate(&P,&F,Store,&K);xredissubscriptionsessioncurrent C=sessionCurrent(S,1,Revision);testRequire(sessionInfo(S).Attempts==2 && sessionInfo(S).Failures==1 && sessionInfo(S).LastFailure==XREDIS_REQUEST_CLOSED,"already closed managed candidate retries its native transport outcome");xredisClientRelease(C.Client);xfuture* Closed=sessionClose(S,&F);testRequire(((const xredissubscriptionsessionclosed*)xrtFutureValue(Closed))->Status==XREDIS_REQUEST_OK,"recovered preclosed candidate does not poison explicit close");xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);sessionPeerStop(&P);xrtFutureDestroy(Closed);
}
static void sessionBounds(void)
{
    sessionpeer P;sessionPeerStart(&P);sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);xredissubscriptions* Store=sessionStore();
    for(unsigned Mode=0;Mode<14;++Mode){xredissubscriptionsessionconfig Bad=K;if(Mode==0)--Bad.Size;if(Mode==1)Bad.Connect=NULL;if(Mode==2)Bad.Protocol=XREDIS_PROTOCOL_UNKNOWN;if(Mode==3)Bad.AttemptTimeout=0;if(Mode==4)Bad.AttemptTimeout=60001;if(Mode==5)Bad.RetryMin=0;if(Mode==6)Bad.RetryMax=Bad.RetryMin-1;if(Mode==7)Bad.RetryMax=60001;if(Mode==8)--Bad.Replay.Size;if(Mode==9)Bad.Replay.MaxBatchNames=0;if(Mode==10)Bad.Replay.MaxBatchNames=1025;if(Mode==11)Bad.Replay.MaxBatchBytes=0;if(Mode==12)Bad.Replay.Registry.MaxSubscriptions=1;if(Mode==13)Bad.Replay.Registry.MaxNameBytes=SIZE_MAX;
        xredissubscriptionsession* Output=(xredissubscriptionsession*)(uintptr_t)1;testRequire(xredisSubscriptionSessionCreate(P.Engine,0,Store,&Bad,&Output)==XREDIS_ADMISSION_ERROR && Output==(xredissubscriptionsession*)(uintptr_t)1 && !xrtAtomic32Load(&F.Calls,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&F.Drops,XMEMORY_ACQUIRE),"invalid config preserves caller context/output");xrtClearError();}
    F.FailFirst=100;K.RetryMin=K.RetryMax=60000;xredissubscriptionsession* S=sessionCreate(&P,&F,Store,&K);double End=xrtTimer()+10;while(sessionInfo(S).State!=XREDIS_SESSION_BACKOFF){testRequire(xrtTimer()<End,"backoff state");xrtThreadYield();}sessionBarrier(&P);testRequire(sessionInfo(S).NextAttempt>xrtTimer()+50,"long retry timer armed");
    xredissubscriptionsession* Other=(xredissubscriptionsession*)(uintptr_t)1;testRequire(xredisSubscriptionSessionCreate(P.Engine,0,Store,&K,&Other)==XREDIS_ADMISSION_OVERLOADED && Other==(xredissubscriptionsession*)(uintptr_t)1,"one editor per desired store");xfuture* Closed=sessionClose(S,&F);testRequire(xrtAtomic32Load(&F.Calls,XMEMORY_ACQUIRE)==1,"close cancels timer without another attempt");xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);sessionPeerStop(&P);xrtFutureDestroy(Closed);
}
typedef struct sessionbudget {xallocationbudget* Budget;xallocationscope Scope;xatomic32 Done;bool Enter;} sessionbudget;
static void sessionBudgetWorker(xnetworker* W,ptr Data)
{(void)W;sessionbudget* B=Data;testRequire(B->Enter?xrtAllocationBudgetEnter(B->Budget,&B->Scope):xrtAllocationBudgetLeave(&B->Scope),"controller worker quota scope");xrtAtomic32Store(&B->Done,1,XMEMORY_RELEASE);}
static void sessionWorkerOOM(void)
{
    sessionpeer P;sessionPeerStart(&P);sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);xredissubscriptions* Store=sessionStore();xbytesview Name=XRT_BYTES_LITERAL("stable");uint64 Revision=sessionSeed(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1);xredissubscriptionsession* S=sessionCreate(&P,&F,Store,&K);xredissubscriptionsessioncurrent C=sessionCurrent(S,1,Revision);xredisClientRelease(C.Client);sessionBarrier(&P);
    sessionbudget B={0};B.Enter=true;xrtAtomic32Init(&B.Done,0);testRequire(xrtAllocationBudgetCreate(1,&B.Budget) && xrtNetEnginePost(P.Engine,0,sessionBudgetWorker,&B),"arm controller OOM");redisPeerWait(&B.Done,1);Name=(xbytesview)XRT_BYTES_LITERAL("new");testRequire(xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"local intent survives worker OOM");xfuture* Closed=xredisSubscriptionSessionClosed(S);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK && ((const xredissubscriptionsessionclosed*)xrtFutureValue(Closed))->Status==XREDIS_REQUEST_MEMORY,"controller OOM is fatal typed close");
    B.Enter=false;xrtAtomic32Store(&B.Done,0,XMEMORY_RELEASE);testRequire(xrtNetEnginePost(P.Engine,0,sessionBudgetWorker,&B),"leave worker budget");redisPeerWait(&B.Done,1);sessionBarrier(&P);xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B.Budget,&I) && !I.LiveBytes && !I.LiveCount && xrtAtomic32Load(&F.Calls,XMEMORY_ACQUIRE)==1,"OOM neither leaks nor retry spins");xrtAllocationBudgetFree(B.Budget);xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);sessionPeerStop(&P);xrtFutureDestroy(Closed);
}
#if defined(XREDIS_SESSION_WHITEBOX)
typedef struct sessioneditprobe {xredissubscriptionsession* Session;xatomic32 Done;xredisadmission Admission;uint64 Revision;} sessioneditprobe;
static int32 sessionEditProbe(ptr Data)
{sessioneditprobe* P=Data;xbytesview Name=XRT_BYTES_LITERAL("later");P->Admission=xredisSubscriptionSessionApply(P->Session,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&P->Revision);xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);return 0;}
static void sessionWhiteboxClose(void)
{
    sessionpeer P;sessionPeerStart(&P);sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);xredissubscriptions* Store=sessionStore();xredissubscriptionsession* Output=(xredissubscriptionsession*)(uintptr_t)1;
    testRequire(xredisSubscriptionSessionCreate(P.Engine,0,Store,&K,(xredissubscriptionsession**)&Store->Current)==XREDIS_ADMISSION_ERROR && Output==(xredissubscriptionsession*)(uintptr_t)1,"store/output disjoint");xrtClearError();
    xbytesview Stable=XRT_BYTES_LITERAL("stable");uint64 Revision=sessionSeed(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Stable,1);xredissubscriptionsession* S=sessionCreate(&P,&F,Store,&K);xredissubscriptionsessioncurrent C=sessionCurrent(S,1,Revision);xredisClientRelease(C.Client);sessionBarrier(&P);
    sessioneditprobe Q={0};Q.Session=S;xrtAtomic32Init(&Q.Done,0);xrtMutexLock(&Store->Gate);xthread* Thread=xrtThreadCreate(sessionEditProbe,&Q,0);testRequire(Thread!=NULL,"accepted edit thread");double End=xrtTimer()+10;
    for(;;){xrtMutexLock(&S->Gate);bool Entered=S->APIs==1;xrtMutexUnlock(&S->Gate);if(Entered)break;testRequire(xrtTimer()<End,"accepted edit entry");xrtThreadYield();}
    xfuture* Closed=xredisSubscriptionSessionClosed(S);testRequire(xredisSubscriptionSessionClose(S) && !xrtFutureDone(Closed),"close retains admitted constructor duty");xrtMutexUnlock(&Store->Gate);testRequire(xrtThreadWaitFor(Thread,10000)==XWAIT_OK,"edit completion");xrtThreadDestroy(Thread);
    testRequire(Q.Admission==XREDIS_ADMISSION_ACCEPTED && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"close waits accepted intent and candidate");xredissubscriptionsinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriptionsInfo(Store,&I) && I.Entries==2,"admitted edit not rolled back by close");xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);sessionPeerStop(&P);xrtFutureDestroy(Closed);
}
typedef struct sessiondeadlineprobe {xredissubscriptionsession* Session;xatomic32 Done;bool Correct;} sessiondeadlineprobe;
static void sessionDeadlineProbe(xnetworker* W,ptr Data)
{(void)W;sessiondeadlineprobe* P=Data;xredissubscriptionsession* S=P->Session;P->Correct=S->Replay && S->Replay->Entered==S->Entered && S->Replay->Deadline==S->Due && xrtTimer()-S->Entered>.04;xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);}
static void sessionLateHello(xnetworker* W,uint64 Id,xnetresult Result,ptr Data)
{(void)W;(void)Id;testRequire(Result==XNET_RESULT_OK,"late HELLO timer");sessionpeer* P=Data;static const char Hello[]="%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n";redisPeerSend(P->Peers,Hello,sizeof(Hello)-1);}
static void sessionWhiteboxDeadline(void)
{
    sessionpeer P;sessionPeerStart(&P);xrtAtomic32Store(&P.HoldHello,1,XMEMORY_RELEASE);xrtAtomic32Store(&P.HoldControl,1,XMEMORY_RELEASE);sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);K.AttemptTimeout=1000;K.MaxFailures=1;xredissubscriptions* Store=sessionStore();xbytesview Stable=XRT_BYTES_LITERAL("stable");sessionSeed(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Stable,1);xredissubscriptionsession* S=sessionCreate(&P,&F,Store,&K);redisPeerWait(&P.Accepted,1);redisPeerWait(&P.Peers[0].Commands,1);
    testRequire(xrtNetEngineAfter(P.Engine,1,60,sessionLateHello,&P)!=0,"delayed setup");redisPeerWait(&P.Controls,1);sessiondeadlineprobe Q={0};Q.Session=S;xrtAtomic32Init(&Q.Done,0);testRequire(xrtNetEnginePost(P.Engine,0,sessionDeadlineProbe,&Q),"exact deadline probe");redisPeerWait(&Q.Done,1);testRequire(Q.Correct,"setup delay counted in original replay deadline");xfuture* Closed=xredisSubscriptionSessionClosed(S);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK && ((const xredissubscriptionsessionclosed*)xrtFutureValue(Closed))->Status==XREDIS_REQUEST_TIMEOUT,"shared attempt deadline terminal");xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);sessionPeerStop(&P);xrtFutureDestroy(Closed);
}
#endif
static void sessionOOM(void)
{
    sessionpeer P;sessionPeerStart(&P);xredissubscriptions* Store=sessionStore();unsigned Accepted=0,Failed=0;
    for(size_t Limit=1;Limit<=4096;Limit+=47){sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"controller constructor budget");
        xredissubscriptionsession* S=(xredissubscriptionsession*)(uintptr_t)1;xredisadmission A=xredisSubscriptionSessionCreate(P.Engine,0,Store,&K,&S);testRequire(xrtAllocationBudgetLeave(&Scope),"constructor budget leave");
        if(A==XREDIS_ADMISSION_ACCEPTED){++Accepted;xfuture* FClosed=sessionClose(S,&F);xredisSubscriptionSessionRelease(S);xrtFutureDestroy(FClosed);sessionBarrier(&P);}
        else{++Failed;testRequire(A==XREDIS_ADMISSION_ERROR && S==(xredissubscriptionsession*)(uintptr_t)1 && !xrtAtomic32Load(&F.Drops,XMEMORY_ACQUIRE),"failed constructor success-only ownership");xrtClearError();}
        xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes,"constructor allocation duty returned after worker turns");xrtAllocationBudgetFree(B);
    }
    testRequire(Accepted && Failed,"OOM sweep reaches both sides");xredisSubscriptionsRelease(Store);sessionPeerStop(&P);
}
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
static void sessionTLS(void)
{
    sessionpeer P;sessionPeerStart(&P);redistlspeer TLS={0};xrtAtomic32Init(&TLS.Verified,0);redisTlsPeerCredentials(&TLS,P.Peers,false,false);P.TLS=&TLS.Options;xredisclienttlsconfig Secure;redisTlsPeerConfig(&TLS,&Secure);Secure.WriteBytes=3;
    sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);F.TLS=&Secure;xredissubscriptions* Store=sessionStore();xbytesview Name=XRT_BYTES_LITERAL("stable");uint64 Revision=sessionSeed(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1);
    xredissubscriptionsession* S=sessionCreate(&P,&F,Store,&K);xredissubscriptionsessioncurrent C=sessionCurrent(S,1,Revision);xredisClientRelease(C.Client);
    Name=(xbytesview)XRT_BYTES_LITERAL("next");testRequire(xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"TLS live diff");C=sessionCurrent(S,1,Revision);xredisClientRelease(C.Client);testRequire(xrtNetEnginePost(P.Engine,1,sessionAbortWorker,&P),"TLS peer disconnect");C=sessionCurrent(S,2,Revision);xredisClientRelease(C.Client);
    xfuture* Closed=sessionClose(S,&F);xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);sessionPeerStop(&P);redisTlsPeerCredentialsDrop(&TLS);xrtFutureDestroy(Closed);
}
#endif
int main(void)
{
    puts("[case] flow3");sessionFlow(false);puts("[case] flow2");sessionFlow(true);puts("[case] held target");sessionHeldRevision();puts("[case] concurrent");sessionConcurrent();puts("[case] large");sessionLargeDiff();puts("[case] failures");sessionFailures();sessionPreclosedCandidate();sessionBounds();sessionWorkerOOM();sessionOOM();
#if defined(XREDIS_SESSION_WHITEBOX)
    sessionWhiteboxClose();sessionWhiteboxDeadline();
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    sessionTLS();
#endif
    puts("[PASS] automatic subscription session: worker factories, shared attempt deadline, bounded backoff, live diffs, generations, native receive, fatal/retry/close/OOM and independent lifetimes");return 0;
}
