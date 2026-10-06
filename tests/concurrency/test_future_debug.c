#ifdef FUTURE_DEBUG_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"

static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    testRequire(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes,
        "metadata physical lifetime leaked");
    testRequire(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount,
        "metadata allocation ledger mismatch");
}
static xstrview copy(xfuture* future)
{
    xstrview result = {0};
    testRequire(xrtFutureDebugNameCopy(future,&result), "copy debug name");
    testRequire(result.Data && result.Data[result.Size] == '\0', "copy terminator");
    return result;
}
static void lifecycle(void)
{
    static const char exact[] = "a\0\xe4\xbd\xa0";
    xmemdebugsnapshot before, named, after;
    xfuture* future = NULL; xpromise* promise;
    xfuturedebugsnapshot first, again;
    xstrview owned;
    xrtMemDebugSnapshot(&before);
    promise=xrtPromiseCreate(&future,NULL); testRequire(promise!=NULL,"create debug Future");
    testRequire(xrtFutureDebugSnapshot(future,&first) && first.FirstTerminalObserved==0 &&
        first.PendingWatches==0,"pending first observation");
    owned=copy(future); testRequire(owned.Size==0,"empty debug name"); xrtFree((void*)owned.Data);
    testRequire(xrtFutureDebugSetNameN(future,exact,sizeof(exact)-1),"exact debug name");
    owned=copy(future);
    testRequire(owned.Size==sizeof(exact)-1 && memcmp(owned.Data,exact,owned.Size)==0,"embedded NUL preserved");
    xrtMemDebugSnapshot(&named);
    for(unsigned i=0;i<100;++i) testRequire(xrtFutureDebugSetNameN(future,exact,sizeof(exact)-1),"rename");
    xrtMemDebugSnapshot(&after);
    testRequire(named.LiveCount==after.LiveCount && named.LiveBytes==after.LiveBytes,"rename must not retain history");
    testRequire(xrtFutureDebugSetNameN(future,NULL,0),"NULL empty name");
    testRequire(xrtFutureDebugSnapshot(future,&again) && again.FirstObserved==first.FirstObserved,
        "first observation changed");
    testRequire(xrtPromiseResolve(promise,NULL),"resolve observed Future");
    testRequire(xrtFutureDebugSnapshot(future,&again) && again.FirstTerminalObserved>=first.FirstObserved,
        "first terminal observation");
    first=again;
    testRequire(xrtFutureDebugSnapshot(future,&again) && memcmp(&first,&again,sizeof(first))==0,
        "repeat snapshot changed");
    xrtPromiseDestroy(promise); xrtFutureDestroy(future);
    testRequire(memcmp(owned.Data,exact,owned.Size)==0,"copy must outlive Future");
    xrtFree((void*)owned.Data); balance(&before);
}
static unsigned prefix(unsigned mode)
{
    unsigned attempts=0;
    for(uint64 point=0;point<16;++point) {
        xmemdebugsnapshot before; xfuture* future=NULL; xpromise* promise;
        xfuturedebugsnapshot output, sentinel; xstrview name, nameSentinel;
        bool ok, hit;
        xrtMemDebugSnapshot(&before);
        promise=xrtPromiseCreate(&future,NULL); testRequire(promise!=NULL,"OOM fixture");
        if(mode>=3) testRequire(xrtFutureDebugSetNameN(future,"original",8),"prepared name");
        memset(&output,0xA5,sizeof(output)); sentinel=output;
        name=(xstrview){(cstr)(uintptr_t)1,999}; nameSentinel=name;
        testRequire(xrtMemDebugFailAfter(point),"enable metadata OOM");
        if(mode==0) ok=xrtFutureDebugSnapshot(future,&output);
        else if(mode==1 || mode==3) ok=xrtFutureDebugSetNameN(future,"replace\0x",9);
        else ok=xrtFutureDebugNameCopy(future,&name);
        hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); ++attempts;
        testRequire(ok==!hit,"metadata OOM status/diagnostic");
        if(!ok) {
            testRequire(xrtErrorKind(xrtGetError())==XERR_MEMORY,"metadata OOM error");
            testRequire(memcmp(&output,&sentinel,sizeof(output))==0 &&
                name.Data==nameSentinel.Data && name.Size==nameSentinel.Size,"failure modified output");
        } else if(mode==2 || mode==4) xrtFree((void*)name.Data);
        xrtClearError();
        if(!ok) {
            name=copy(future);
            testRequire(mode>=3 ? name.Size==8 && memcmp(name.Data,"original",8)==0 : name.Size==0,
                "failed operation changed name");
            xrtFree((void*)name.Data);
        }
        testRequire(xrtFutureState(future)==XFUTURE_PENDING,"OOM cancelled input");
        xrtPromiseDestroy(promise); xrtFutureDestroy(future); balance(&before);
        if(!hit) return attempts;
    }
    testRequire(false,"OOM prefix did not reach success"); return 0;
}
static unsigned notified;
static void notify(ptr data) { (void)data; ++notified; }
static void watches(void)
{
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL);
    xfuturewatch a={0},b={0}; xfuturedebugsnapshot snapshot;
    testRequire(promise && xrtFutureWatchInit(&a,notify,NULL,NULL) &&
        xrtFutureWatchInit(&b,notify,NULL,NULL),"init observed Watches");
    testRequire(xrtFutureWatchAdd(future,&a)==XFUTURE_WATCH_PENDING &&
        xrtFutureWatchAdd(future,&b)==XFUTURE_WATCH_PENDING,"register observed Watches");
    testRequire(xrtFutureDebugSnapshot(future,&snapshot) && snapshot.PendingWatches==2,"actual Watch count");
    xrtFutureWatchRemove(future,&a);
    testRequire(xrtFutureDebugSnapshot(future,&snapshot) && snapshot.PendingWatches==1,"removed Watch count");
    testRequire(xrtPromiseResolve(promise,NULL),"notify observed Watches");
    testRequire(xrtFutureDebugSnapshot(future,&snapshot) && snapshot.PendingWatches==0 && notified==1,
        "completed Watch count");
    xrtFutureWatchRemove(future,&b); xrtPromiseDestroy(promise); xrtFutureDestroy(future);
}
static int32 concurrent(ptr data)
{
    xfuture* future=(xfuture*)data;
    for(unsigned i=0;i<1000;++i) {
        const char* text=(i&1)?"left\0x":"right"; size_t size=(i&1)?6:5;
        xstrview name;
        testRequire(xrtFutureDebugSetNameN(future,text,size),"concurrent setter");
        name=copy(future);
        testRequire((name.Size==6 && memcmp(name.Data,"left\0x",6)==0) ||
            (name.Size==5 && memcmp(name.Data,"right",5)==0),"concurrent snapshot torn");
        xrtFree((void*)name.Data);
    }
    xrtFutureDestroy(future); return 0;
}
int main(void)
{
    unsigned attempts=0;
    xmemdebugsnapshot before; xfuture* future=NULL; xpromise* promise; xthread* a; xthread* b;
    testRequire(xrtMemDebugEnable(true),"enable metadata ledger");
    xrtClearError();
    for(unsigned i=0;i<1000;++i) lifecycle();
    for(unsigned mode=0;mode<5;++mode) attempts+=prefix(mode);
    xrtMemDebugSnapshot(&before); watches(); balance(&before);
    xrtMemDebugSnapshot(&before);
    promise=xrtPromiseCreate(&future,NULL); testRequire(promise!=NULL,"concurrent Future");
    a=xrtThreadCreate(concurrent,xrtFutureRef(future),0);
    b=xrtThreadCreate(concurrent,xrtFutureRef(future),0);
    testRequire(a && b,"concurrent threads");
    testRequire(xrtThreadWaitFor(a,INT64_C(10000))==XWAIT_OK &&
        xrtThreadWaitFor(b,INT64_C(10000))==XWAIT_OK,"concurrent deadline");
    xrtThreadDestroy(a); xrtThreadDestroy(b); xrtPromiseDestroy(promise); xrtFutureDestroy(future); balance(&before);
    printf("[PASS] Future debug: 1000 physical lifetimes, bounded rename storage, exact owned names, 5 complete OOM prefixes/%u positions, 2000 concurrent copies, actual Watches\n",attempts);
    return 0;
}
