#ifndef XRPC_BENCH_HTTP_MEASURE_H
#define XRPC_BENCH_HTTP_MEASURE_H
#include <xrt/future.h>
#include <xrt/atomic.h>
#include <xrt/time.h>
#include <xrt/thread.h>
#include <string.h>
/* Benchmark-only observer. Caller owns Future and this storage through Unit.
 * Notify is nonblocking: clock read, one double store, release publication.
 * NotifiedAt is a terminal-watch observation, not an internal publication time.
 * READY cannot recover past completion: it records a caller observation upper
 * bound and is counted separately. No extra Future ref or heap allocation. */
typedef struct rpcBenchObservation {
    xfuturewatch Watch;
    xfuture* Future;
    xatomic32 Notified;
    double NotifiedAt;
    bool Ready,Linked;
} rpcBenchObservation;
static inline void rpcBenchNotified(ptr Data)
{
    rpcBenchObservation* O=Data;O->NotifiedAt=xrtTimer();
    xrtAtomic32Store(&O->Notified,1,XMEMORY_RELEASE);
}
static inline bool rpcBenchObserve(rpcBenchObservation* O,xfuture* Future)
{
    memset(O,0,sizeof(*O));xrtAtomic32Init(&O->Notified,0);O->Future=Future;
    if(!xrtFutureWatchInit(&O->Watch,rpcBenchNotified,NULL,O)){O->Future=NULL;return false;}
    xfuturewatchresult R=xrtFutureWatchAdd(Future,&O->Watch);
    if(R==XFUTURE_WATCH_ERROR){O->Future=NULL;return false;}
    O->Linked=R==XFUTURE_WATCH_PENDING;O->Ready=R==XFUTURE_WATCH_READY;
    if(O->Ready)rpcBenchNotified(O);
    return true;
}
static inline bool rpcBenchObserved(const rpcBenchObservation* O,double* At)
{
    if(!xrtAtomic32Load(&O->Notified,XMEMORY_ACQUIRE))return false;
    *At=O->NotifiedAt;return true;
}
static inline void rpcBenchObservationUnit(rpcBenchObservation* O)
{
    /* Remove also joins an in-flight callback. Never use the flag as proof
     * that the Core callback frame no longer references Watch storage. */
    if(O->Future && O->Linked)xrtFutureWatchRemove(O->Future,&O->Watch);
    O->Future=NULL;O->Linked=false;
}
#endif
