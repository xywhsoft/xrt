#include "../test.h"
#include "../../src/internal/xrt_tls_stream.h"
static uint8 DialMarker;
static unsigned Released,Returned,Dropped;
static xtlsdial* mockRef(xtlsdial* d){testRequire((ptr)d==&DialMarker,"wrong retain");return d;}
static void mockDestroy(xtlsdial* d){if(d){testRequire((ptr)d==&DialMarker,"wrong release");Released++;}}
static bool mockCancel(xtlsdial* d){testRequire((ptr)d==&DialMarker,"wrong cancel");return true;}
static xrtownershipref mockOwnership(const xtlsdial* d){(void)d;return (xrtownershipref){0};}
static xtlsdial* mockLegacy(xnetengine* e,xnetresolver* r,cstr h,uint16 p,const xtlsclientconfig* t,
    const xtlsdialconfig* c,const xtlsstreamevents* v,ptr data,xtlsdialproc done,ptr context)
{
    (void)e;(void)r;(void)h;(void)p;(void)t;(void)c;(void)v;(void)data;
    done((xtlsdial*)&DialMarker,XNET_RESULT_CLOSED,NULL,NULL,context);Returned++;
    return (xtlsdial*)&DialMarker;
}
static xtlsdial* mockOwned(xnetengine* e,xnetresolver* r,cstr h,uint16 p,const xtlsclientconfig* t,
    const xtlsdialconfig* c,ptr context,const xtlsdialownershipv1* policy)
{
    (void)e;(void)r;(void)h;(void)p;(void)t;(void)c;
    policy->Done((xtlsdial*)&DialMarker,XNET_RESULT_CLOSED,NULL,NULL,context);
    policy->Drop(context);Returned++;return (xtlsdial*)&DialMarker;
}
#define xrtTlsDialRef mockRef
#define xrtTlsDialDestroy mockDestroy
#define xrtTlsDialCancel mockCancel
#define xrtTlsDialOwnership mockOwnership
#define xrtTlsDial mockLegacy
#define xrtTlsDialOwnedV1 mockOwned
#define xrtTlsDialAsync testTlsFutureAsync
#define xrtTlsDialAsyncOwnedV1 testTlsFutureAsyncOwned
#define xrtTlsDialProxyAsyncOwnedV1 testTlsProxyFutureAsyncOwned
#define xrtTlsDialFutureDialPolicyV1Get testTlsFutureDialPolicy
#define xrtTlsDialFutureDialPushPolicyV1Get testTlsFutureDialPushPolicy
#define xrtTlsDialFutureProducerPolicyV1Get testTlsFutureProducer
#define xrtTlsDialFutureCancelPolicyV1Get testTlsFutureCancel
#define xrtTlsDialFuturePayloadPolicyV1Get testTlsFuturePayload
#define xrtTlsDialFutureOwnershipQuiescenceV1Get testTlsFutureQuiet
#define xrtTlsDialFutureOwnershipAdapterV1 testTlsFutureAdapter
#define xrtTlsDialFutureContextOwnershipQuiescenceV1Get testTlsFutureOwnerQuiet
#define xrtTlsDialFutureContextOwnershipAdapterV1 testTlsFutureOwnerAdapter
#include "../../src/tls/stream_dial_future.c"
#undef xrtTlsDialRef
#undef xrtTlsDialDestroy
#undef xrtTlsDialCancel
#undef xrtTlsDialOwnership
#undef xrtTlsDial
#undef xrtTlsDialOwnedV1
#undef xrtTlsDialAsync
#undef xrtTlsDialAsyncOwnedV1
#undef xrtTlsDialProxyAsyncOwnedV1
#undef xrtTlsDialFutureDialPolicyV1Get
#undef xrtTlsDialFutureDialPushPolicyV1Get
#undef xrtTlsDialFutureProducerPolicyV1Get
#undef xrtTlsDialFutureCancelPolicyV1Get
#undef xrtTlsDialFuturePayloadPolicyV1Get
#undef xrtTlsDialFutureOwnershipQuiescenceV1Get
#undef xrtTlsDialFutureOwnershipAdapterV1
#undef xrtTlsDialFutureContextOwnershipQuiescenceV1Get
#undef xrtTlsDialFutureContextOwnershipAdapterV1
static bool ownerCount(const void* data,size_t* count){if(!data||!count)return false;*count=1;return true;}
static bool ownerTrace(const void* data,xrtownershipvisitor visit,ptr context){(void)data;(void)visit;(void)context;return true;}
static void ownerDrop(const void* data){testRequire(data==&DialMarker,"wrong Data");Dropped++;}
static const xrtownershipops ownerOps={ownerCount,ownerTrace};
static const xtlsstreamownershipv1 ownerPolicy={sizeof(ownerPolicy),{0},ownerDrop,&ownerOps};
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"debug setup");xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    for(unsigned i=0;i<2;i++){
        Released=Returned=Dropped=0;
        xfuture* f=i?testTlsFutureAsyncOwned(NULL,NULL,"local",1,NULL,NULL,&DialMarker,&ownerPolicy):
            testTlsFutureAsync(NULL,NULL,"local",1,NULL,NULL,NULL,NULL);
        testRequire(f&&Returned==1&&Released==1&&Dropped==i&&xrtFutureState(f)==XFUTURE_CLOSED,
            "TLS completion blocked, lost a credit or double-dropped Data");
        xrtFutureDestroy(f);xrtClearError();
    }
    xrtMemDebugSnapshot(&after);testRequire(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount,"TLS publication leak");
    testRequire(xrtMemDebugReset(),"debug reset");
    puts("[PASS] real TLS legacy/owned adapters return before constructor publication; balanced");return 0;
}
