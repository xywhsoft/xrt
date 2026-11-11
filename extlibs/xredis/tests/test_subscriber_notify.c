#include <xredis/subscriber_notify.h>
#define XREDIS_SUBSCRIBER_TEST_MAIN subscriberNotifyBaseCoverageReference
#include "test_subscriber.c"
#undef XREDIS_SUBSCRIBER_TEST_MAIN

typedef struct notifyburst {redispeer* Peer;uint64 First;size_t Count;bool Resp3;xatomic32 Done;} notifyburst;
static void notifySend(xnetworker* Worker,ptr Data)
{
    (void)Worker;notifyburst* B=Data;
    for(size_t N=0;N<B->Count;++N){uint64 Seq=B->First+N;const char* H=B->Resp3?">3\r\n+message\r\n$1\r\nn\r\n":"*3\r\n+message\r\n$1\r\nn\r\n";
        redisPeerSend(B->Peer,H,strlen(H));subBulk(B->Peer,(xbytesview){(const unsigned char*)&Seq,sizeof(Seq)});}
    xrtAtomic32Store(&B->Done,1,XMEMORY_RELEASE);
}
static void notifyEmit(redispeer* P,bool Resp3,uint64 First,size_t Count)
{
    notifyburst B={P,First,Count,Resp3,{0}};xrtAtomic32Init(&B.Done,0);
    testRequire(xrtNetEnginePost(P->Engine,1,notifySend,&B),"message-only producer post");redisPeerWait(&B.Done,1);
}
static void notifyReady(xfuture* F)
{
    testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK,"readability wake");xfutureresult R;
    testRequire(xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && !R.Value && !R.Error,"shared hint resolves with no payload");
}
static void notifyCount(xredisclient* C,size_t Count)
{
    double End=xrtTimer()+10;for(;;){xredisclientinfo I={0};I.Size=sizeof(I);testRequire(xredisClientInfo(C,&I),"queue snapshot");if(I.Pushes==Count)return;testRequire(xrtTimer()<End,"bounded producer count");xrtThreadYield();}
}
static void notifyValue(xredisreply* R,uint64 Seq)
{
    xredispubsubevent E;uint64 Got=UINT64_MAX;
    testRequire(xredisPubSubEventRead(R,&E)==XREDIS_PUBSUB_EVENT && E.Kind==XREDIS_PUBSUB_MESSAGE && E.Channel.Size==1 && *(const char*)E.Channel.Data=='n' && E.Payload.Size==sizeof(Got),"immutable binary message view");
    memcpy(&Got,E.Payload.Data,sizeof(Got));testRequire(Got==Seq,"shared FIFO sequence");
}
static void notifySubscribe(xredisclient* C)
{
    xbytesview N=XRT_BYTES_LITERAL("n");xfuture* F=NULL;
    testRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&N,1,5000,&F)==XREDIS_ADMISSION_ACCEPTED && subResult(F)->Request.Status==XREDIS_REQUEST_OK,"confirm message channel");xrtFutureDestroy(F);subIdle(C);
}
static void notifyFIFO(bool Resp3)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};S.Resp3=Resp3;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclientconfig K;xredisClientConfigInit(&K);K.MaxPushes=4;K.ReadBytes=3;K.ReadFrames=1;xredisclient* C=subConnect(&P,&S,&K,NULL);notifySubscribe(C);
    xfuture *F=NULL,*Shared=NULL;testRequire(xredisSubscriberReadable(C,&F)==XREDIS_ADMISSION_ACCEPTED && xredisSubscriberReadable(C,&Shared)==XREDIS_ADMISSION_ACCEPTED && F==Shared,"pending callers share one epoch");
    testRequire(xrtFutureCancel(Shared) && !xrtFutureDone(F),"shared cooperative token does not cancel message stream");
    xcancel* Cancel=xrtCancelCreate();testRequire(Cancel && xrtCancelRequest(Cancel) && xrtFutureWaitForCancel(F,1000,Cancel)==XWAIT_CANCELLED && !xrtFutureDone(F),"private wait cancellation changes no other observer");xrtCancelDestroy(Cancel);xrtClearError();
    xallocationbudget* SharedBudget=NULL;xallocationscope SharedScope={0};testRequire(xrtAllocationBudgetCreate(1,&SharedBudget) && xrtAllocationBudgetEnter(SharedBudget,&SharedScope),"shared observer allocation trap");
    xfuture* Again=NULL;testRequire(xredisSubscriberReadable(C,&Again)==XREDIS_ADMISSION_ACCEPTED && Again==F && xrtAllocationBudgetLeave(&SharedScope),"pending observation does not allocate");
    xallocationbudgetinfo SharedInfo={0};SharedInfo.Size=sizeof(SharedInfo);testRequire(xrtAllocationBudgetInfo(SharedBudget,&SharedInfo) && !SharedInfo.LiveCount && !SharedInfo.PeakBytes && !SharedInfo.Rejections,"no observer allocation attempts");xrtAllocationBudgetFree(SharedBudget);xrtFutureDestroy(Again);
    xredisreply* V[5];for(size_t N=0;N<5;++N)V[N]=(xredisreply*)(uintptr_t)1;size_t Taken=99;
    testRequire(!xredisSubscriberTakeBatch(C,V,5,&Taken) && Taken==99 && V[0]==(xredisreply*)(uintptr_t)1,"empty batch preserves outputs");
    testRequire(!xredisSubscriberTakeBatch(C,V,0,&Taken) && Taken==99,"zero capacity rejected without transfer");xrtClearError();
    testRequire(!xredisSubscriberTakeBatch(C,V,SIZE_MAX,&Taken),"batch size overflow");xrtClearError();
    testRequire(!xredisSubscriberTakeBatch(C,V,2,(size_t*)V),"overlapping output slots");const xerror* Ambient=xrtGetError();
    testRequire(!xredisSubscriberTakeBatch(C,V,5,&Taken) && xrtGetError()==Ambient && Taken==99,"empty preserves existing ambient diagnostic");xrtClearError();
    notifyEmit(&P,Resp3,0,3);notifyReady(F);notifyReady(Shared);notifyCount(C,3);
    testRequire(xredisSubscriberTakeBatch(C,V,2,&Taken) && Taken==2 && V[2]==(xredisreply*)(uintptr_t)1,"capacity prefix only");notifyValue(V[0],0);notifyValue(V[1],1);xredisReplyRelease(V[0]);xredisReplyRelease(V[1]);
    testRequire(xredisClientTakePush(C,&V[0]),"generic and batch consumers share FIFO");notifyValue(V[0],2);xredisReplyRelease(V[0]);
    xfuture* Next=NULL;testRequire(xredisSubscriberReadable(C,&Next)==XREDIS_ADMISSION_ACCEPTED && Next!=F && !xrtFutureDone(Next),"drained epoch rearms");xrtFutureDestroy(F);xrtFutureDestroy(Shared);
    notifyEmit(&P,Resp3,3,3);notifyReady(Next);notifyCount(C,3);V[3]=(xredisreply*)(uintptr_t)1;
    testRequire(xredisSubscriberTakeBatch(C,V,4,&Taken) && Taken==3 && V[3]==(xredisreply*)(uintptr_t)1,"ring wrap and untouched tail");for(size_t N=0;N<Taken;++N)notifyValue(V[N],3+N);
    xfuture* CloseHint=NULL;testRequire(xredisSubscriberReadable(C,&CloseHint)==XREDIS_ADMISSION_ACCEPTED && !xrtFutureDone(CloseHint),"arm physical close hint");
    subClose(&P,C);notifyReady(CloseHint);notifyReady(Next);for(size_t N=0;N<3;++N)xredisReplyRelease(V[N]);xrtFutureDestroy(CloseHint);xrtFutureDestroy(Next);
}
typedef struct notifywatch {
    xredisclient* Client;xnetengine* Engine;xfuture *F,*Next,*Closed;xfuturewatch First,Second;xatomic32 Released;
} notifywatch;
static void notifyWatchRelease(ptr Data)
{notifywatch* W=Data;xrtAtomic32FetchAdd(&W->Released,1,XMEMORY_RELEASE);}
static void notifyWatchSecond(ptr Data)
{
    notifywatch* W=Data;testRequire(xrtNetEngineCurrent(W->Engine)!=NULL && xrtNetWorkerIndex(xrtNetEngineCurrent(W->Engine))==0,"second callback on owner worker");
    xredisreply* R=NULL;size_t N=0;testRequire(xredisSubscriberTakeBatch(W->Client,&R,1,&N) && N==1,"callback takes second message without Gate held");notifyValue(R,1);xredisReplyRelease(R);
    testRequire(xredisSubscriberReadable(W->Client,&W->Closed)==XREDIS_ADMISSION_ACCEPTED && xredisClientClose(W->Client),"callback can rearm then close");
}
static void notifyWatchFirst(ptr Data)
{
    notifywatch* W=Data;testRequire(xrtNetEngineCurrent(W->Engine)!=NULL && xrtNetWorkerIndex(xrtNetEngineCurrent(W->Engine))==0,"callback on owner worker");
    xredissubscriberinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriberInfo(W->Client,&I),"callback snapshot outside Gate");
    xredisreply* R=NULL;size_t N=0;testRequire(xredisSubscriberTakeBatch(W->Client,&R,1,&N) && N==1,"callback transfers message");notifyValue(R,0);xredisReplyRelease(R);
    testRequire(xredisSubscriberReadable(W->Client,&W->Next)==XREDIS_ADMISSION_ACCEPTED && xrtFutureWatchAdd(W->Next,&W->Second)==XFUTURE_WATCH_PENDING,"callback rearms separate watch before next message");
}
static void notifyCallbacks(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subConnect(&P,&S,NULL,NULL);notifySubscribe(C);
    notifywatch W={0};W.Client=C;W.Engine=P.Engine;xrtAtomic32Init(&W.Released,0);
    testRequire(xrtFutureWatchInit(&W.First,notifyWatchFirst,notifyWatchRelease,&W) && xrtFutureWatchInit(&W.Second,notifyWatchSecond,notifyWatchRelease,&W),"caller-owned callback slots");
    testRequire(xredisSubscriberReadable(C,&W.F)==XREDIS_ADMISSION_ACCEPTED && xrtFutureWatchAdd(W.F,&W.First)==XFUTURE_WATCH_PENDING,"arm callback");
    notifyEmit(&P,false,0,1);redisPeerWait(&W.Released,1);notifyEmit(&P,false,1,1);redisPeerWait(&W.Released,2);
    xfuture* Closed=xredisClientClosed(C);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"reentrant physical close");notifyReady(W.F);notifyReady(W.Next);notifyReady(W.Closed);
    xrtFutureWatchRemove(W.F,&W.First);xrtFutureWatchRemove(W.Next,&W.Second);xredisClientRelease(C);redisPeerStop(&P);
    notifyReady(W.Closed);xrtFutureDestroy(W.F);xrtFutureDestroy(W.Next);xrtFutureDestroy(W.Closed);xrtFutureDestroy(Closed);
}
static void notifyReadyWatch(ptr Data)
{xrtAtomic32FetchAdd(Data,1,XMEMORY_RELEASE);}
static void notifyClosedQueue(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};S.Resp3=true;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subConnect(&P,&S,NULL,NULL);notifySubscribe(C);
    notifyEmit(&P,true,77,1);notifyCount(C,1);xfuture* Closed=xredisClientClosed(C);testRequire(xredisClientClose(C) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"close with queued message");redisPeerStop(&P);
    xfuture* F=NULL;testRequire(xredisSubscriberReadable(C,&F)==XREDIS_ADMISSION_ACCEPTED,"queued readiness after Engine destruction");notifyReady(F);
    xfuturewatch W;xatomic32 Calls;xrtAtomic32Init(&Calls,0);testRequire(xrtFutureWatchInit(&W,notifyReadyWatch,notifyReadyWatch,&Calls) && xrtFutureWatchAdd(F,&W)==XFUTURE_WATCH_READY && !xrtAtomic32Load(&Calls,XMEMORY_ACQUIRE),"already-ready epoch publishes before any user callback");
    xredisreply* R=NULL;size_t N=0;testRequire(xredisSubscriberTakeBatch(C,&R,1,&N) && N==1,"queued transfer after all physical owners");
    xfuture* Unchanged=(xfuture*)(uintptr_t)1;testRequire(xredisSubscriberReadable(C,&Unchanged)==XREDIS_ADMISSION_CLOSED && Unchanged==(xfuture*)(uintptr_t)1,"physical closed empty rejects without Engine or output access");
    N=91;testRequire(!xredisSubscriberTakeBatch(C,&R,1,&N) && N==91,"closed empty preserves batch");xredisClientRelease(C);notifyValue(R,77);xredisReplyRelease(R);notifyReady(F);xrtFutureDestroy(F);xrtFutureDestroy(Closed);(void)Calls;
}
static void notifyOwnerPassed(xnetworker* W,ptr Data)
{(void)W;xrtAtomic32Store(Data,1,XMEMORY_RELEASE);}
static void notifyOOM(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subConnect(&P,&S,NULL,NULL);notifySubscribe(C);
    unsigned Failures=0,Successes=0,Tails=0;uint64 Seq=0;
    for(size_t Limit=1;Limit<=2048;++Limit){
        xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"readability construction quota");
        xfuture* F=(xfuture*)(uintptr_t)1;xredisadmission A=xredisSubscriberReadable(C,&F);testRequire(xrtAllocationBudgetLeave(&Scope),"quota leave");
        if(A==XREDIS_ADMISSION_ERROR){++Failures;testRequire(F==(xfuture*)(uintptr_t)1,"failed epoch preserves output");xrtClearError();}
        else {++Successes;testRequire(A==XREDIS_ADMISSION_ACCEPTED,"serialized epoch admission");notifyEmit(&P,false,Seq++,1);notifyReady(F);xredisreply* R=NULL;size_t N=0;testRequire(xredisSubscriberTakeBatch(C,&R,1,&N),"drain budgeted epoch");xredisReplyRelease(R);xrtFutureDestroy(F);}
        /* Replace the stored completed epoch outside the budget before checking
         * live duties. Also proves a failed rearm did not strand its successor. */
        xfuture* Next=NULL;testRequire(xredisSubscriberReadable(C,&Next)==XREDIS_ADMISSION_ACCEPTED,"recovery epoch");
        xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I),"observe post-signal resources");if(I.LiveCount)++Tails;
        /* Future terminal state can precede its resolver's mechanical Promise
         * release. A post on the same IO worker observes that physical tail. */
        xatomic32 Passed;xrtAtomic32Init(&Passed,0);testRequire(xrtNetEnginePost(P.Engine,0,notifyOwnerPassed,&Passed),"observe resolver retirement");redisPeerWait(&Passed,1);
        testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes,"all epoch quota duties released after owner turn");xrtAllocationBudgetFree(B);
        notifyEmit(&P,false,Seq++,1);notifyReady(Next);xredisreply* R=NULL;size_t N=0;testRequire(xredisSubscriberTakeBatch(C,&R,1,&N),"drain recovery epoch");xredisReplyRelease(R);xrtFutureDestroy(Next);
    }
    testRequire(Failures && Successes,"both failing and fully constructed quotas exercised");printf("epoch_post_signal_tails_observed: %u\n",Tails);subClose(&P,C);
}
static void notifyStreaming(bool Resp3)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};S.Resp3=Resp3;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subConnect(&P,&S,NULL,NULL);notifySubscribe(C);
    for(uint64 Seq=0;Seq<256;++Seq){xfuture* F=NULL;testRequire(xredisSubscriberReadable(C,&F)==XREDIS_ADMISSION_ACCEPTED,"drain-before-arm epoch");notifyEmit(&P,Resp3,Seq,1);notifyReady(F);xrtFutureDestroy(F);
        xredisreply* R=NULL;size_t N=0;testRequire(xredisSubscriberTakeBatch(C,&R,1,&N) && N==1,"each arrived message remains transferable");notifyValue(R,Seq);xredisReplyRelease(R);}
    subClose(&P,C);
}

static void notifyOverflow(bool Bytes)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclientconfig K;xredisClientConfigInit(&K);K.MaxPushes=1;if(Bytes)K.MaxPushBytes=1;xredisclient* C=subConnect(&P,&S,&K,NULL);notifySubscribe(C);
    xfuture *F=NULL,*Closed=xredisClientClosed(C);testRequire(xredisSubscriberReadable(C,&F)==XREDIS_ADMISSION_ACCEPTED,"overflow observer pending");notifyEmit(&P,false,0,Bytes?1:2);
    testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"overflow physically retires stream");notifyReady(F);xredisclientinfo I={0};I.Size=sizeof(I);
    testRequire(xredisClientInfo(C,&I) && I.CloseStatus==XREDIS_REQUEST_LIMIT && I.Pushes==(Bytes?0u:1u),"overflow count/byte state explicit");
    xredisreply* R=NULL;size_t N=0;if(!Bytes){testRequire(xredisSubscriberTakeBatch(C,&R,1,&N) && N==1,"accepted prefix survives overflow");notifyValue(R,0);xredisReplyRelease(R);}
    subClose(&P,C);notifyReady(F);xrtFutureDestroy(F);xrtFutureDestroy(Closed);
}
typedef struct notifyrace {xredisclient* C;xatomic32 Go,Accepted;} notifyrace;
static int32 notifyRaceWorker(ptr Data)
{
    notifyrace* R=Data;redisPeerWait(&R->Go,1);
    for(unsigned N=0;N<256;++N){xfuture* F=(xfuture*)(uintptr_t)1;xredisadmission A=xredisSubscriberReadable(R->C,&F);
        testRequire(A==XREDIS_ADMISSION_ACCEPTED || A==XREDIS_ADMISSION_CLOSED || A==XREDIS_ADMISSION_OVERLOADED,"concurrent cold/rearm/close admission");
        if(A==XREDIS_ADMISSION_ACCEPTED){xrtAtomic32FetchAdd(&R->Accepted,1,XMEMORY_RELEASE);notifyReady(F);xrtFutureDestroy(F);}
        else testRequire(F==(xfuture*)(uintptr_t)1,"racing refusal preserves output");
    }
    return 0;
}
typedef struct notifyconsumers {xredisclient* C;xatomic32 Seen[1024],Received,Closing;} notifyconsumers;
static int32 notifyConsumer(ptr Data)
{
    notifyconsumers* R=Data;uint64 Previous=0;bool HavePrevious=false;
    for(;;){xredisreply* Values[7];size_t Taken=0;
        if(xredisSubscriberTakeBatch(R->C,Values,7,&Taken)){
            for(size_t N=0;N<Taken;++N){xredispubsubevent E;uint64 Seq=0;testRequire(xredisPubSubEventRead(Values[N],&E)==XREDIS_PUBSUB_EVENT && E.Payload.Size==8,"parallel binary view");memcpy(&Seq,E.Payload.Data,8);
                testRequire(Seq<1024 && (!HavePrevious || Seq>Previous) && xrtAtomic32FetchAdd(&R->Seen[Seq],1,XMEMORY_ACQ_REL)==0,"parallel FIFO with no duplicate ownership");Previous=Seq;HavePrevious=true;notifyValue(Values[N],Seq);xredisReplyRelease(Values[N]);xrtAtomic32FetchAdd(&R->Received,1,XMEMORY_RELEASE);}
        }else{xfuture* F=NULL;xredisadmission A=xredisSubscriberReadable(R->C,&F);
            if(A==XREDIS_ADMISSION_ACCEPTED){notifyReady(F);xrtFutureDestroy(F);}
            else if(A==XREDIS_ADMISSION_CLOSED){testRequire(xrtAtomic32Load(&R->Closing,XMEMORY_ACQUIRE),"only physical close ends consumers");break;}
            else {testRequire(A==XREDIS_ADMISSION_OVERLOADED,"only competing constructor can defer consumer");xrtThreadYield();}
        }
    }
    return 0;
}
static void notifyCompetingConsumers(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};S.Resp3=true;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subConnect(&P,&S,NULL,NULL);notifySubscribe(C);
    notifyconsumers R={0};R.C=C;xrtAtomic32Init(&R.Received,0);xrtAtomic32Init(&R.Closing,0);for(size_t N=0;N<1024;++N)xrtAtomic32Init(&R.Seen[N],0);xthread* Threads[4];
    for(size_t N=0;N<4;++N){Threads[N]=xrtThreadCreate(notifyConsumer,&R,0);testRequire(Threads[N]!=NULL,"parallel batch consumer");}
    for(uint64 Seq=0;Seq<1024;Seq+=32){notifyEmit(&P,true,Seq,32);redisPeerWait(&R.Received,(uint32)Seq+32);}
    xrtAtomic32Store(&R.Closing,1,XMEMORY_RELEASE);testRequire(xredisClientClose(C),"wake empty competing consumers at close");
    for(size_t N=0;N<4;++N){testRequire(xrtThreadWaitFor(Threads[N],10000)==XWAIT_OK,"all competing waiters terminate");xrtThreadDestroy(Threads[N]);}
    for(size_t N=0;N<1024;++N){testRequire(xrtAtomic32Load(&R.Seen[N],XMEMORY_ACQUIRE)==1,"every produced message transferred once");}
    subClose(&P,C);
}
static void notifyConcurrentClose(void)
{
    for(unsigned Round=0;Round<16;++Round){redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subConnect(&P,&S,NULL,NULL);
        notifyrace R;R.C=C;xrtAtomic32Init(&R.Go,0);xrtAtomic32Init(&R.Accepted,0);xthread* T[8];
        for(size_t N=0;N<8;++N){T[N]=xrtThreadCreate(notifyRaceWorker,&R,0);testRequire(T[N]!=NULL,"concurrent readiness thread");}
        xrtAtomic32Store(&R.Go,1,XMEMORY_RELEASE);if(!(Round&1u))redisPeerWait(&R.Accepted,1);testRequire(xredisClientClose(C),"close concurrent with epoch construction");
        for(size_t N=0;N<8;++N){testRequire(xrtThreadWaitFor(T[N],10000)==XWAIT_OK,"all observer threads wake at close");xrtThreadDestroy(T[N]);}subClose(&P,C);
    }
}
#if defined(XREDIS_SUBSCRIBER_NOTIFY_WHITEBOX)
typedef struct notifystopped {xredisclient* C;xatomic32 Result;} notifystopped;
static void notifyTransportStopped(xnetworker* Worker,ptr Data)
{(void)Worker;notifystopped* T=Data;xrtAtomic32Store(&T->Result,T->C->TransportClosed?1:2,XMEMORY_RELEASE);}
static void notifyConstructorClose(void)
{
    for(unsigned Success=0;Success<2;++Success){redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subConnect(&P,&S,NULL,NULL);
        xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);xrtMutexLock(&C->Gate);M->ReadablePreparing=true;xrtMutexUnlock(&C->Gate);
        xfuture* Closed=xredisClientClosed(C);testRequire(xredisClientClose(C),"physical close during reserved constructor");notifystopped T;T.C=C;xrtAtomic32Init(&T.Result,0);double End=xrtTimer()+10;
        for(;;){xrtAtomic32Store(&T.Result,0,XMEMORY_RELAXED);testRequire(xrtNetEnginePost(P.Engine,0,notifyTransportStopped,&T),"probe owning transport close");redisPeerWait(&T.Result,1);
            if(xrtAtomic32Load(&T.Result,XMEMORY_ACQUIRE)==1)break;
            testRequire(xrtTimer()<End,"physical transport stopped");}
        testRequire(!xrtFutureDone(Closed),"constructor reservation prevents premature Engine unpin");xfuture* Unchanged=(xfuture*)(uintptr_t)1;
        testRequire(xredisSubscriberReadable(C,&Unchanged)==XREDIS_ADMISSION_OVERLOADED && Unchanged==(xfuture*)(uintptr_t)1,"closing constructor is still bounded");
        xfuture* Returned=NULL;
        if(Success){xfuture* F=NULL;xpromise* Promise=xrtPromiseCreate(&F,NULL);testRequire(Promise && (Returned=xrtFutureRef(F))!=NULL,"unexposed constructor state");
            xrtMutexLock(&C->Gate);M->ReadableFuture=F;M->ReadablePromise=Promise;xrtMutexUnlock(&C->Gate);}
        /* Simulate publication or rollback at the paused construction boundary.
         * The real public constructors are exercised separately under races/OOM. */
        xredisSubscriberPrepared(C,M);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"published/rolled-back constructor wakes final close turn");
        subClose(&P,C);if(Returned){notifyReady(Returned);xrtFutureDestroy(Returned);}xrtFutureDestroy(Closed);
    }
}
typedef struct notifylate {xredisclient* C;xredisreply* Reply;subhold Hold;} notifylate;
static void notifyLateFrame(xnetworker* Worker,ptr Data)
{
    (void)Worker;notifylate* L=Data;xrtAtomic32Store(&L->Hold.Entered,1,XMEMORY_RELEASE);redisPeerWait(&L->Hold.Released,1);
    xredisreplyinfo I={0};I.Size=sizeof(I);testRequire(xredisReplyInfo(L->Reply,&I),"late frame storage");
    xredissubscriber* M=xrtAtomicPtrLoad(&L->C->Subscriber,XMEMORY_ACQUIRE);
    /* Isolate delivery of an already-owned parsed frame from parser allocation.
     * Resolving the pending epoch and transferring the queue need no allocation. */
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"producer notification allocation trap");
    testRequire(xredisSubscriberQueue(L->C,M,L->Reply,I.ReservedBytes,true),"already-parsed frame after logical close");
    testRequire(xrtAllocationBudgetLeave(&Scope),"producer budget leave");xallocationbudgetinfo BI={0};BI.Size=sizeof(BI);testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.LiveCount && !BI.PeakBytes && !BI.Rejections,"producer made no allocation attempt");xrtAllocationBudgetFree(B);xrtAtomic32Store(&L->Hold.Exited,1,XMEMORY_RELEASE);
}
static void notifyLateClose(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subConnect(&P,&S,NULL,NULL);notifySubscribe(C);
    notifyEmit(&P,false,91,1);notifyCount(C,1);notifylate L={0};L.C=C;subHoldInit(&L.Hold);testRequire(xredisClientTakePush(C,&L.Reply),"save pre-parsed owned frame");
    xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);xfuture* Unchanged=(xfuture*)(uintptr_t)1;
    testRequire(xredisSubscriberReadable(C,(xfuture**)&C->StorageBytes)==XREDIS_ADMISSION_ERROR && xredisSubscriberReadable(C,&M->ReadableFuture)==XREDIS_ADMISSION_ERROR,"client/registry alias outputs refused");xrtClearError();
    xrtMutexLock(&C->Gate);M->ReadablePreparing=true;xrtMutexUnlock(&C->Gate);
    testRequire(xredisSubscriberReadable(C,&Unchanged)==XREDIS_ADMISSION_OVERLOADED && Unchanged==(xfuture*)(uintptr_t)1,"one in-progress constructor bounds allocation");xredisSubscriberPrepared(C,M);
    testRequire(xrtNetEnginePost(P.Engine,0,notifyLateFrame,&L),"hold owning worker before late frame");redisPeerWait(&L.Hold.Entered,1);
    testRequire(xredisClientClose(C),"logical close while parsed frame is held");xfuture* F=NULL;
    testRequire(xredisSubscriberReadable(C,&F)==XREDIS_ADMISSION_ACCEPTED && !xrtFutureDone(F),"logical closing empty is not physical CLOSED");
    xrtAtomic32Store(&L.Hold.Released,1,XMEMORY_RELEASE);redisPeerWait(&L.Hold.Exited,1);notifyReady(F);
    xredisreply* R=NULL;size_t N=0;xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"batch allocation trap");
    testRequire(xredisSubscriberTakeBatch(C,&R,1,&N) && N==1 && xrtAllocationBudgetLeave(&Scope),"batch transfer does not allocate");xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.Rejections && !I.PeakBytes && !I.LiveCount,"batch made no allocation attempt");xrtAllocationBudgetFree(B);subClose(&P,C);notifyValue(R,91);xredisReplyRelease(R);notifyReady(F);xrtFutureDestroy(F);
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
static void notifyTLS(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};S.Resp3=true;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    redistlspeer TLS={0};xrtAtomic32Init(&TLS.Verified,0);redisTlsPeerCredentials(&TLS,&P,false,false);xredisclienttlsconfig Secure;redisTlsPeerConfig(&TLS,&Secure);Secure.WriteBytes=3;
    xredisclientconfig K;xredisClientConfigInit(&K);K.ReadBytes=3;K.ReadFrames=1;xredisclient* C=NULL;testRequire(xredisClientTlsConnect(P.Engine,&P.Address,0,&K,NULL,&Secure,&C),"TLS notification connect");P.Client=C;
    xfuture* Ready=xredisClientReady(C);testRequire(xrtFutureWaitFor(Ready,10000)==XWAIT_OK,"TLS ready");xrtFutureDestroy(Ready);testRequire(xredisSubscriberStart(C,NULL)==XREDIS_ADMISSION_ACCEPTED,"TLS adoption");notifySubscribe(C);
    xfuture* F=NULL;testRequire(xredisSubscriberReadable(C,&F)==XREDIS_ADMISSION_ACCEPTED,"TLS notification arm");notifyEmit(&P,true,42,1);notifyReady(F);xredisreply* R=NULL;size_t N=0;
    testRequire(xredisSubscriberTakeBatch(C,&R,1,&N) && N==1,"TLS owned batch");subClose(&P,C);redisTlsPeerCredentialsDrop(&TLS);notifyValue(R,42);xredisReplyRelease(R);notifyReady(F);xrtFutureDestroy(F);
}
#endif
int main(void)
{
    notifyFIFO(false);notifyFIFO(true);notifyOverflow(false);notifyOverflow(true);notifyCompetingConsumers();notifyConcurrentClose();notifyCallbacks();notifyClosedQueue();notifyOOM();notifyStreaming(false);notifyStreaming(true);
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    notifyTLS();
#endif
#if defined(XREDIS_SUBSCRIBER_NOTIFY_WHITEBOX)
    notifyConstructorClose();notifyLateClose();
#endif
    puts("[PASS] subscriber notifications: shared epochs, batch FIFO, cancelable private waits, owner callbacks/rearm, close/retained queues, allocation-prefix rollback and streaming");return 0;
}
