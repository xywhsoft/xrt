#include "../../../tests/test.h"
#include "../bench/http_measure.h"
#include "../bench/process_cpu.h"
#include <xrt/memory_stats.h>
typedef struct measureProducer {xpromise* Promise;xatomic32 Go,Done;double Before,After;} measureProducer;
static void measureWait(xatomic32* Flag,uint32 N)
{double End=xrtTimer()+10;while(xrtAtomic32Load(Flag,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<End,"measurement producer deadline");xrtThreadYield();}}
static int32 measurePublish(ptr Data)
{
    measureProducer* P=Data;measureWait(&P->Go,1);P->Before=xrtTimer();
    testRequire(xrtPromiseResolve(P->Promise,NULL),"measurement producer resolve");P->After=xrtTimer();
    xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);return 0;
}
static void measureDelayedCollector(void)
{
    xfuture* F=NULL;measureProducer P={0};P.Promise=xrtPromiseCreate(&F,NULL);testRequire(P.Promise && F,"observer promise");
    xrtAtomic32Init(&P.Go,0);xrtAtomic32Init(&P.Done,0);rpcBenchObservation O;
    testRequire(rpcBenchObserve(&O,F) && !O.Ready,"pending watch admission");
    xthread* T=xrtThreadCreate(measurePublish,&P,0);testRequire(T!=NULL,"measurement publisher thread");xrtAtomic32Store(&P.Go,1,XMEMORY_RELEASE);measureWait(&P.Done,1);
    xrtSleep(40);double At=0,Observed=xrtTimer();testRequire(rpcBenchObserved(&O,&At) && At>=P.Before && At<=P.After && Observed-At>=.03,"observer sleep contaminated notification timestamp");
    rpcBenchObservationUnit(&O);testRequire(xrtThreadWaitFor(T,10000)==XWAIT_OK,"publisher join");xrtThreadDestroy(T);xrtPromiseDestroy(P.Promise);xrtFutureDestroy(F);
}
static void measureReadyAndDetach(void)
{
    xfuture* F=NULL;xpromise* P=xrtPromiseCreate(&F,NULL);testRequire(P && xrtPromiseResolve(P,NULL),"ready fixture");
    xrtSleep(20);rpcBenchObservation O;double Before=xrtTimer(),At=0;
    testRequire(rpcBenchObserve(&O,F) && O.Ready && !O.Linked && rpcBenchObserved(&O,&At) && At>=Before && At<=xrtTimer(),"READY invented historical timestamp");
    rpcBenchObservationUnit(&O);xrtPromiseDestroy(P);xrtFutureDestroy(F);
    P=xrtPromiseCreate(&F,NULL);testRequire(P!=NULL,"detach fixture");rpcBenchObservation* Heap=xrtMalloc(sizeof(*Heap));testRequire(Heap && rpcBenchObserve(Heap,F) && !Heap->Ready,"detached observer init");
    testRequire(xrtFutureCancel(F) && !rpcBenchObserved(Heap,&At),"cancel request fabricated terminal notification");rpcBenchObservationUnit(Heap);xrtFree(Heap);
    testRequire(xrtPromiseResolve(P,NULL),"detached resolution");xrtPromiseDestroy(P);xrtFutureDestroy(F);
}
static void measureRaces(void)
{
    for(unsigned K=0;K<256;++K){xfuture* F=NULL;measureProducer P={0};P.Promise=xrtPromiseCreate(&F,NULL);testRequire(P.Promise!=NULL,"racing promise");
        xrtAtomic32Init(&P.Go,0);xrtAtomic32Init(&P.Done,0);xthread* T=xrtThreadCreate(measurePublish,&P,0);testRequire(T!=NULL,"racing publisher");
        rpcBenchObservation O;if(K%2)xrtAtomic32Store(&P.Go,1,XMEMORY_RELEASE);
        testRequire(rpcBenchObserve(&O,F),"racing observer add");xrtAtomic32Store(&P.Go,1,XMEMORY_RELEASE);
        if(K%3==0)rpcBenchObservationUnit(&O);
        measureWait(&P.Done,1);double At=0;
        if(K%3!=0)testRequire(rpcBenchObserved(&O,&At) && (O.Ready?At>=P.Before:At>=P.Before && At<=P.After),"racing READY/notification provenance");
        rpcBenchObservationUnit(&O);testRequire(xrtThreadWaitFor(T,10000)==XWAIT_OK,"racing publisher join");xrtThreadDestroy(T);xrtPromiseDestroy(P.Promise);xrtFutureDestroy(F);
    }
}
static void measureNoAllocationAndCpu(void)
{
    xfuture* F=NULL;xpromise* P=xrtPromiseCreate(&F,NULL);testRequire(P!=NULL,"budget promise");xallocationbudget* B=NULL;xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"observer one-byte budget");rpcBenchObservation O;double At;
    testRequire(rpcBenchObserve(&O,F) && xrtPromiseResolve(P,NULL) && rpcBenchObserved(&O,&At),"observer allocated under one-byte budget");rpcBenchObservationUnit(&O);
    testRequire(xrtAllocationBudgetLeave(&Scope),"observer budget leave");xallocationbudgetinfo I={0};I.Size=sizeof(I);
    testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes && !I.LiveCount && !I.Rejections,"observer allocation/owner charge");xrtAllocationBudgetFree(B);xrtPromiseDestroy(P);xrtFutureDestroy(F);
    double Start=0,End=0;testRequire(rpcBenchProcessCpu(&Start),"process CPU clock");volatile uint64 V=1;double Until=xrtTimer()+5;
    do{for(unsigned N=0;N<10000;++N)V=V*UINT64_C(6364136223846793005)+1;testRequire(rpcBenchProcessCpu(&End) && xrtTimer()<Until,"CPU progress deadline");}while(End-Start<1000.);
    testRequire(V!=0 && rpcBenchProcessCpu(&End) && End>Start,"process CPU counter did not observe busy work");
}
int main(void)
{
    measureNoAllocationAndCpu();measureDelayedCollector();measureReadyAndDetach();measureRaces();
    puts("[PASS] xrpc HTTP measurement: delayed-observer/pending/READY-upper-bound/cancel-request/detach-before-free/256-races/one-byte-budget/process-CPU");return 0;
}
