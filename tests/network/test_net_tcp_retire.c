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
    xnetpost OtherPost;xnetstream* Stream;
} retireContext;
typedef struct retireWaiter {retireContext* Context;xwaitresult Result;} retireWaiter;
static bool retireCount(const void* Data,size_t* Count)
{*Count=xrtAtomic32Load(&((const retireContext*)Data)->Refs,XMEMORY_ACQUIRE);return true;}
static bool retireTrace(const void* Data,xrtownershipvisitor Visit,ptr User)
{(void)Data;(void)Visit;(void)User;return false;}
static void retireClosed(xnetstream* Stream,xnetresult Result,const xerror* Error,ptr Data)
{
    (void)Result;(void)Error;retireContext* C=Data;
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
    testRequire(xrtNetStreamWaitRetired(C->Stream,0)==XWAIT_ERROR,"other Worker of same Engine rejected");xrtClearError();
    xrtAtomic32Store(&C->OtherRejected,1,XMEMORY_RELEASE);
}
static int32 retireWaitThread(ptr Data)
{
    retireWaiter* W=Data;retireContext* C=W->Context;xrtMutexLock(&C->Lock);++C->Started;xrtCondBroadcast(&C->Ready);xrtMutexUnlock(&C->Lock);
    W->Result=xrtNetStreamWaitRetired(C->Stream,5000);
    xrtMutexLock(&C->Lock);testRequire(C->Returned,"no early success during owned Drop");++C->Finished;xrtCondBroadcast(&C->Ready);xrtMutexUnlock(&C->Lock);return 0;
}
static void retireAwait(retireContext* C,bool Drop,unsigned Waiters)
{
    xrtMutexLock(&C->Lock);
    while((Drop && !C->Entered) || C->Started<Waiters)testRequire(xrtCondWaitFor(&C->Ready,&C->Lock,5000)==XWAIT_OK,"retirement handshake");
    xrtMutexUnlock(&C->Lock);
}
static void retireCase(bool Held)
{
    retireContext C={0};testRequire(xrtMutexInit(&C.Lock) && xrtCondInit(&C.Ready),"context primitives");xrtAtomic32Init(&C.Refs,2);xrtAtomic32Init(&C.WorkerRejected,0);xrtAtomic32Init(&C.OtherRejected,0);C.Release=!Held;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);testRequire(Engine && xrtNetEngineStart(Engine),"retirement Engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistener* Listener=xrtNetListen(Engine,&L,NULL,NULL,NULL);xnetaddr Address;
    testRequire(Listener && xrtNetListenerLocal(Listener,&Address),"retirement Listener");
    static const xrtownershipops Ops={retireCount,retireTrace};static const xnetstreameventsownershipv1 Events={sizeof(Events),{NULL,NULL,NULL,NULL,NULL,NULL,retireClosed},retireDrop,&Ops};
    C.Stream=xrtNetStreamConnectOwnedV1(Engine,&Address,0,NULL,&C,&Events);testRequire(C.Stream!=NULL,"owned stream");double End=xrtTimer()+5;
    while(xrtNetStreamState(C.Stream)!=XNET_STREAM_OPEN){testRequire(xrtTimer()<End,"open timeout");xrtThreadYield();}
    xnetstream* Peer=NULL;while(!(Peer=xrtNetListenerAccept(Listener))){testRequire(xrtTimer()<End,"accept timeout");xrtThreadYield();}
    xrtSetErrorInfo(XERR_STATE,"retire.ambient",7,"preserve");const xerror* Ambient=xrtGetError();
    testRequire(xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_TIMEOUT && xrtGetError()==Ambient && xrtNetStreamState(C.Stream)==XNET_STREAM_OPEN,"poll neither closes nor changes ambient error");xrtClearError();
    testRequire(xrtNetStreamWaitRetired(C.Stream,-2)==XWAIT_ERROR,"invalid timeout");xrtClearError();
    xnetstream* Alias=xrtNetStreamRef(C.Stream);testRequire(Alias!=NULL && xrtNetStreamAbort(C.Stream),"alias and close");retireAwait(&C,true,0);
    testRequire(xrtNetStreamState(C.Stream)==XNET_STREAM_CLOSED && xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_TIMEOUT,"CLOSED is not retired");
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
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"retirement allocation scope");
    testRequire(xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_OK && xrtAllocationBudgetLeave(&Scope),"ready wait allocates nothing");xallocationbudgetinfo B={0};B.Size=sizeof(B);testRequire(xrtAllocationBudgetInfo(Budget,&B) && !B.LiveBytes && !B.LiveCount,"no waiter allocation debt");xrtAllocationBudgetFree(Budget);
    testRequire(xrtAtomic32Load(&C.Refs,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&C.WorkerRejected,XMEMORY_ACQUIRE)==1,"owner and Worker boundaries");
    testRequire(xrtNetStreamAbort(Peer) && xrtNetStreamWaitRetired(Peer,5000)==XWAIT_OK,"peer physical close");xrtNetStreamDestroy(Peer);xrtNetListenerClose(Listener);xrtNetListenerDestroy(Listener);
    End=xrtTimer()+5;while(!xrtNetEngineStop(Engine)){testRequire(xrtTimer()<End,"listener runtime retirement");xrtClearError();xrtThreadYield();}testRequire(xrtNetEngineDestroy(Engine),"Engine destruction");
    testRequire(xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_OK,"inert Stream remains usable after Engine destruction");
    Alias=xrtNetStreamRef(C.Stream);testRequire(Alias && xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_TIMEOUT,"post-Engine alias keeps real owner pending without stale Engine access");xrtNetStreamDestroy(Alias);
    testRequire(xrtNetStreamWaitRetired(C.Stream,0)==XWAIT_OK,"post-Engine alias release wakes predicate");xrtNetStreamDestroy(C.Stream);
    testRequire(xrtCondUnit(&C.Ready) && xrtMutexUnit(&C.Lock),"context retirement");
}
int main(void)
{
    testRequire(xrtNetStreamWaitRetired(NULL,0)==XWAIT_ERROR,"null retirement wait");xrtClearError();retireCase(true);
    for(unsigned I=0;I<32;++I)retireCase(false);
    puts("[PASS] TCP physical retirement: callback tails, aliases, multiwaiters, deadlines, Worker rejection, OOM and 32 wake races");return 0;
}
