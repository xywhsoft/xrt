#ifdef NET_TCP_RETIRE_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

/* Local lifecycle only. A deliberately held owned Drop proves that CLOSED is
 * not physical retirement. No malformed traffic or external service is used. */
typedef struct retireContext {
    xmutex Lock;xcond Ready;xatomic32 Refs;
    bool Entered,Release,Returned;unsigned Started,Finished;
    xatomic32 WorkerRejected,OtherRejected;
    xnetpost OtherPost,ReadyPost[3];unsigned PollPosted;xnetstream* Stream;xatomic32 ReadyChecked;
    bool PollExpected;
} retireContext;
typedef struct retireWaiter {retireContext* Context;xwaitresult Result;} retireWaiter;
static bool retireCount(const void* Data,size_t* Count)
{*Count=xrtAtomic32Load(&((const retireContext*)Data)->Refs,XMEMORY_ACQUIRE);return true;}
static bool retireTrace(const void* Data,xrtownershipvisitor Visit,ptr User)
{(void)Data;(void)Visit;(void)User;return false;}
static void retireClosed(xnetstream* Stream,xnetresult Result,const xerror* Error,ptr Data)
{
    (void)Result;(void)Error;retireContext* C=Data;
    testRequire(xrtNetStreamPollRetired(Stream)==XWAIT_TIMEOUT,"own close callback is not physically retired");
    testRequire(xrtNetStreamWaitRetired(Stream,0)==XWAIT_ERROR,"own Worker retirement wait rejected");xrtClearError();xrtAtomic32Store(&C->WorkerRejected,1,XMEMORY_RELEASE);
}
static void retireDrop(const void* Data)
{
    retireContext* C=(retireContext*)Data;testRequire(xrtMutexLock(&C->Lock),"owned Drop lock");C->Entered=true;xrtCondBroadcast(&C->Ready);
    while(!C->Release)testRequire(xrtCondWaitFor(&C->Ready,&C->Lock,5000)==XWAIT_OK,"held actual callback tail");
    C->Returned=true;xrtAtomic32FetchSub(&C->Refs,1,XMEMORY_RELEASE);xrtCondBroadcast(&C->Ready);testRequire(xrtMutexUnlock(&C->Lock),"owned Drop unlock");
}
static void retireOtherWorker(xnetworker* Worker,ptr Data)
{
    (void)Worker;retireContext* C=Data;
    testRequire(xrtNetStreamPollRetired(C->Stream)==XWAIT_TIMEOUT,"same-Engine Worker observes real callback/alias tail");
    testRequire(xrtNetStreamWaitRetired(C->Stream,0)==XWAIT_ERROR,"other Worker of same Engine rejected");xrtClearError();
    xrtAtomic32Store(&C->OtherRejected,1,XMEMORY_RELEASE);
}
static void retireReadyWorker(xnetworker* Worker,ptr Data)
{
    (void)Worker;retireContext* C=Data;
    testRequire(xrtNetStreamPollRetired(C->Stream)==(C->PollExpected?XWAIT_OK:XWAIT_TIMEOUT),"worker immediate predicate/try-lock query");
    testRequire(xrtNetStreamWaitRetired(C->Stream,0)==XWAIT_ERROR,"legacy worker wait rejection remains");xrtClearError();
    xrtAtomic32Store(&C->ReadyChecked,1,XMEMORY_RELEASE);
}
static void retirePollWorker(retireContext* C,xnetengine* E,unsigned Affinity,bool Ready)
{
    C->PollExpected=Ready;xrtAtomic32Store(&C->ReadyChecked,0,XMEMORY_RELEASE);
    xnetpost* Post=C->ReadyPost+C->PollPosted++;testRequire(C->PollPosted<=3 && xrtNetPostInit(Post) && xrtNetPost(xrtNetEngineWorker(E,Affinity),Post,retireReadyWorker,C),"retired Worker query post");
    double End=xrtTimer()+5;while(!xrtAtomic32Load(&C->ReadyChecked,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"query must not wait for busy predicate owner");xrtThreadYield();}
}
typedef struct retireAmbient {const xerror* Error;xatomic32 Calls;} retireAmbient;
static void retireErrorHandler(const xerror* Error,ptr Data)
{retireAmbient* A=Data;if(Error==A->Error)xrtAtomic32FetchAdd(&A->Calls,1,XMEMORY_RELAXED);}
static void retirePollAmbient(xnetstream* S)
{
    xrtSetErrorInfo(XERR_STATE,"poll.ambient",19,"preserve without redelivery");retireAmbient A={0};A.Error=xrtGetError();xrtAtomic32Init(&A.Calls,0);xrtSetErrorHandler(retireErrorHandler,&A);
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"poll allocation scope");
    for(unsigned I=0;I<10000;++I)testRequire(xrtNetStreamPollRetired(S)==XWAIT_OK && xrtGetError()==A.Error,"ready poll keeps exact ambient error");
    xnetstream* Alias=xrtNetStreamRef(S);testRequire(Alias && xrtNetStreamPollRetired(S)==XWAIT_TIMEOUT && xrtGetError()==A.Error,"alias poll keeps exact ambient error");xrtNetStreamDestroy(Alias);
    testRequire(xrtAllocationBudgetLeave(&Scope),"poll allocation leave");xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveBytes && !Info.LiveCount && !Info.Rejections,"poll requires no requested allocation");xrtAllocationBudgetFree(B);
    testRequire(!xrtAtomic32Load(&A.Calls,XMEMORY_ACQUIRE),"poll does not redeliver old diagnostic");xrtSetErrorHandler(NULL,NULL);xrtClearError();
}
static int32 retireWaitThread(ptr Data)
{
    retireWaiter* W=Data;retireContext* C=W->Context;xrtMutexLock(&C->Lock);++C->Started;xrtCondBroadcast(&C->Ready);xrtMutexUnlock(&C->Lock);
    W->Result=xrtNetStreamWaitRetired(C->Stream,5000);
    xrtMutexLock(&C->Lock);testRequire(C->Returned,"no early success during owned Drop");++C->Finished;xrtCondBroadcast(&C->Ready);xrtMutexUnlock(&C->Lock);return 0;
}
#ifdef NET_TCP_RETIRE_SINGLE
static void retirePollSyncFailure(xnetstream* S)
{
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"poll diagnostic-failure scope");
    xrtSetErrorKind(XERR_STATE);const xerror* Old=xrtGetError();
    xrt_mutex_impl* M=__xrtMutexImpl(&S->RetirementLock);uint32 Magic=M->Magic;M->Magic=0;xwaitresult Result=xrtNetStreamPollRetired(S);M->Magic=Magic;
    testRequire(Result==XWAIT_ERROR && xrtErrorKind(xrtGetError())==XERR_STATE && xrtGetError()==Old,"new failure with identical static error identity is ERROR");
    xrtSetErrorInfo(XERR_STATE,"poll.old",31,"must fall back under one-byte budget");testRequire(xrtErrorKind(xrtGetError())==XERR_MEMORY,"old OOM diagnostic");
    M->Magic=0;Result=xrtNetStreamPollRetired(S);M->Magic=Magic;
    testRequire(Result==XWAIT_ERROR && xrtErrorKind(xrtGetError())==XERR_STATE,"static synchronization failure does not inherit old OOM classification");Old=xrtGetError();
    testRequire(xrtNetStreamPollRetired(S)==XWAIT_OK && xrtGetError()==Old,"normal query preserves error after failed query");xrtClearError();
    testRequire(xrtAllocationBudgetLeave(&Scope),"poll failure scope leave");xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes && !I.LiveCount && I.Rejections,"poll failure refunds diagnostics");xrtAllocationBudgetFree(B);
}
#endif
static void retireAwait(retireContext* C,bool Drop,unsigned Waiters)
{
    xrtMutexLock(&C->Lock);
    while((Drop && !C->Entered) || C->Started<Waiters)testRequire(xrtCondWaitFor(&C->Ready,&C->Lock,5000)==XWAIT_OK,"retirement handshake");
    xrtMutexUnlock(&C->Lock);
}
static void retireCase(bool Held)
{
    retireContext C={0};testRequire(xrtMutexInit(&C.Lock) && xrtCondInit(&C.Ready),"context primitives");xrtAtomic32Init(&C.Refs,2);xrtAtomic32Init(&C.WorkerRejected,0);xrtAtomic32Init(&C.OtherRejected,0);C.Release=!Held;xrtAtomic32Init(&C.ReadyChecked,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);testRequire(Engine && xrtNetEngineStart(Engine),"retirement Engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistener* Listener=xrtNetListen(Engine,&L,NULL,NULL,NULL);xnetaddr Address;
    testRequire(Listener && xrtNetListenerLocal(Listener,&Address),"retirement Listener");
    static const xrtownershipops Ops={retireCount,retireTrace};static const xnetstreameventsownershipv1 Events={sizeof(Events),{NULL,NULL,NULL,NULL,NULL,NULL,retireClosed},retireDrop,&Ops};
    C.Stream=xrtNetStreamConnectOwnedV1(Engine,&Address,0,NULL,&C,&Events);testRequire(C.Stream!=NULL,"owned stream");double End=xrtTimer()+5;
    while(xrtNetStreamState(C.Stream)!=XNET_STREAM_OPEN){testRequire(xrtTimer()<End,"open timeout");xrtThreadYield();}
    xnetstream* Peer=NULL;while(!(Peer=xrtNetListenerAccept(Listener))){testRequire(xrtTimer()<End,"accept timeout");xrtThreadYield();}
    xrtSetErrorInfo(XERR_STATE,"retire.ambient",7,"preserve");const xerror* Ambient=xrtGetError();
    testRequire(xrtNetStreamPollRetired(C.Stream)==XWAIT_TIMEOUT && xrtGetError()==Ambient && xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_TIMEOUT && xrtGetError()==Ambient && xrtNetStreamState(C.Stream)==XNET_STREAM_OPEN,"poll neither closes nor changes ambient error");xrtClearError();
    testRequire(xrtNetStreamWaitRetired(C.Stream,-2)==XWAIT_ERROR,"invalid timeout");xrtClearError();
    xnetstream* Alias=xrtNetStreamRef(C.Stream);testRequire(Alias!=NULL && xrtNetStreamAbort(C.Stream),"alias and close");retireAwait(&C,true,0);
    testRequire(xrtNetStreamState(C.Stream)==XNET_STREAM_CLOSED && xrtNetStreamPollRetired(C.Stream)==XWAIT_TIMEOUT && xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_TIMEOUT,"CLOSED is not retired");
    if(Held){testRequire(xrtNetStreamWaitRetired(C.Stream,10)==XWAIT_TIMEOUT,"held Drop preserves timeout and owner");
        testRequire(xrtNetPostInit(&C.OtherPost) && xrtNetPost(xrtNetEngineWorker(Engine,1),&C.OtherPost,retireOtherWorker,&C),"other Worker check post");
        while(!xrtAtomic32Load(&C.OtherRejected,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"other Worker check");xrtThreadYield();}
    }
    retireWaiter W[2]={{&C,XWAIT_ERROR},{&C,XWAIT_ERROR}};xthread* Threads[2];
    for(unsigned I=0;I<2;++I){Threads[I]=xrtThreadCreate(retireWaitThread,W+I,0);testRequire(Threads[I]!=NULL,"concurrent retirement waiter");}retireAwait(&C,true,2);
    xrtMutexLock(&C.Lock);C.Release=true;xrtCondBroadcast(&C.Ready);while(!C.Returned)testRequire(xrtCondWaitFor(&C.Ready,&C.Lock,5000)==XWAIT_OK,"owned Drop finishes");
    testRequire(!C.Finished,"real alias prevents waiter success");xrtMutexUnlock(&C.Lock);
    testRequire(xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_TIMEOUT,"extra caller alias remains real");xrtNetStreamDestroy(Alias);
    testRequire(xrtNetStreamWaitRetired(C.Stream,5000)==XWAIT_OK,"caller-only physical retirement");
    for(unsigned I=0;I<2;++I){testRequire(xrtThreadWaitFor(Threads[I],5000)==XWAIT_OK && W[I].Result==XWAIT_OK,"all waiters wake without lost notification");xrtThreadDestroy(Threads[I]);}
    retirePollWorker(&C,Engine,0,true);retirePollWorker(&C,Engine,1,true);
#ifdef NET_TCP_RETIRE_SINGLE
    testRequire(xrtMutexLock(&C.Stream->RetirementLock),"hold the final notification predicate lock");
    testRequire(xrtNetStreamPollRetired(C.Stream)==XWAIT_TIMEOUT,"same-thread predicate owner returns pending without recursion");
    retirePollWorker(&C,Engine,1,false);testRequire(xrtMutexUnlock(&C.Stream->RetirementLock),"predicate owner release");
    testRequire(xrtNetStreamPollRetired(C.Stream)==XWAIT_OK,"ready only after final notification lock release");
#endif
    retirePollAmbient(C.Stream);
#ifdef NET_TCP_RETIRE_SINGLE
    if(Held)retirePollSyncFailure(C.Stream);
#endif
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"retirement allocation scope");
    testRequire(xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_OK && xrtAllocationBudgetLeave(&Scope),"ready wait allocates nothing");xallocationbudgetinfo B={0};B.Size=sizeof(B);testRequire(xrtAllocationBudgetInfo(Budget,&B) && !B.LiveBytes && !B.LiveCount,"no waiter allocation debt");xrtAllocationBudgetFree(Budget);
    testRequire(xrtAtomic32Load(&C.Refs,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&C.WorkerRejected,XMEMORY_ACQUIRE)==1,"owner and Worker boundaries");
    testRequire(xrtNetStreamAbort(Peer) && xrtNetStreamWaitRetired(Peer,5000)==XWAIT_OK,"peer physical close");xrtNetStreamDestroy(Peer);xrtNetListenerClose(Listener);xrtNetListenerDestroy(Listener);
    End=xrtTimer()+5;while(!xrtNetEngineStop(Engine)){testRequire(xrtTimer()<End,"listener runtime retirement");xrtClearError();xrtThreadYield();}testRequire(xrtNetEngineDestroy(Engine),"Engine destruction");
    testRequire(xrtNetStreamPollRetired(C.Stream)==XWAIT_OK && xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_OK,"inert Stream remains usable after Engine destruction");
    Alias=xrtNetStreamRef(C.Stream);testRequire(Alias && xrtNetStreamPollRetired(C.Stream)==XWAIT_TIMEOUT && xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_TIMEOUT,"post-Engine alias keeps real owner pending without stale Engine access");xrtNetStreamDestroy(Alias);
    testRequire(xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_OK,"post-Engine alias release wakes predicate");xrtNetStreamDestroy(C.Stream);
    testRequire(xrtCondUnit(&C.Ready) && xrtMutexUnit(&C.Lock),"context retirement");
}
int main(void)
{
    testRequire(xrtNetStreamPollRetired(NULL)==XWAIT_ERROR,"null retirement poll");xrtClearError();
    testRequire(xrtNetStreamWaitRetired(NULL,0)==XWAIT_ERROR,"null retirement wait");xrtClearError();retireCase(true);
    for(unsigned I=0;I<32;++I)retireCase(false);
    puts("[PASS] TCP physical retirement: callback tails, aliases, multiwaiters, deadlines, Worker rejection, OOM and 32 wake races");return 0;
}
