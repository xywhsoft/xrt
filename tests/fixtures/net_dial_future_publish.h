#include "../test.h"
#include <xrt/detail/wait.h>
#include "../../src/internal/xrt_dial_future.h"

/* Run the real shared constructor with a resident submit seam. Completion
 * must RETURN while Submit has not returned; the old wait gate deadlocks here. */
#define __xrtNetStreamDialFutureOps testPublishedDialOps
#define __xrtNetStreamDialFuturePayloadPolicy testPublishedDialPayloadPolicy
#define __xrtNetStreamDialFutureDrop testPublishedDialDrop
#define __xrtNetStreamDialFutureComplete testPublishedDialComplete
#define __xrtNetStreamDialFutureCreate testPublishedDialCreate
#define __xrtNetStreamDialFutureCreateOwned testPublishedDialCreateOwned
#define __xrtNetStreamDialFutureOwner testPublishedDialOwner
#define __xrtNetStreamDialFutureProducerPolicyV1Get testPublishedDialProducer
#define __xrtNetStreamDialFutureCancelPolicyV1Get testPublishedDialCancel
#define __xrtNetStreamDialFuturePayloadPolicyV1Get testPublishedDialPayload
#define __xrtNetStreamDialFutureOwnershipQuiescenceV1Get testPublishedDialQuiet
#define __xrtNetStreamDialFutureOwnershipAdapterV1 testPublishedDialAdapter
#include "../../src/network/dial_future.c"
#undef __xrtNetStreamDialFutureOwnershipAdapterV1
#undef __xrtNetStreamDialFutureOwnershipQuiescenceV1Get
#undef __xrtNetStreamDialFuturePayloadPolicyV1Get
#undef __xrtNetStreamDialFutureCancelPolicyV1Get
#undef __xrtNetStreamDialFutureProducerPolicyV1Get
#undef __xrtNetStreamDialFutureOwner
#undef __xrtNetStreamDialFutureCreateOwned
#undef __xrtNetStreamDialFutureCreate
#undef __xrtNetStreamDialFutureComplete
#undef __xrtNetStreamDialFutureDrop
#undef __xrtNetStreamDialFuturePayloadPolicy
#undef __xrtNetStreamDialFutureOps

static uint8 DialMarker;
static unsigned Released, Returned, OperationRefs;
static ptr CancelContext;
static ptr refDial(ptr dial) { testRequire(dial==&DialMarker&&OperationRefs,"wrong operation");OperationRefs++;return dial; }
static void releaseDial(ptr dial) { if(dial){testRequire(dial==&DialMarker&&OperationRefs,"wrong release");OperationRefs--;Released++;} }
static bool cancelDial(ptr dial) {
    testRequire(dial==&DialMarker&&CancelContext,"wrong cancel");
    testPublishedDialComplete(CancelContext,XNET_RESULT_CANCELLED,NULL,NULL);testPublishedDialDrop(CancelContext);CancelContext=NULL;return true;
}
static xrtownershipref ownershipDial(const void* dial) { (void)dial;return (xrtownershipref){0}; }
static void abortValue(ptr value) { (void)value; }
static void freeBytes(ptr value,ptr data){testRequire(!data,"unexpected payload context");xrtNetBytesDestroy(value);}
static bool traceBytes(const void* value,const void* data,xrtownershipvisitor visit,ptr arg)
{return value&&!data&&visit&&visit(xrtNetBytesOwnership(value),arg);}
static const xfuturepayloadownershipv1 bytesPayload={sizeof(bytesPayload),freeBytes,traceBytes};
static const xrt_net_dial_future_policy policy={refDial,releaseDial,cancelDial,ownershipDial,
    &bytesPayload,abortValue,NULL};
static int32 completeBeforePublish(ptr context)
{
    testPublishedDialComplete(context,XNET_RESULT_CLOSED,NULL,NULL);
    testPublishedDialDrop(context);Returned++;
    return 0;
}
static ptr submitBeforePublish(ptr context,const void* data)
{
    unsigned mode=*(const unsigned*)data;OperationRefs=1;
    if(mode==3){
        xrt_net_stream_dial_future* c=context;CancelContext=context;xcancel* token=xrtPromiseCancelToken(c->Promise);
        testRequire(token&&xrtCancelRequest(token),"prepublication cancel failed");xrtCancelDestroy(token);Returned++;
    }else if(mode==2){
        xnetbytes* bytes=xrtNetBytesCopy("a\0b",3);testRequire(bytes!=NULL,"bytes allocation");
        testPublishedDialComplete(context,XNET_RESULT_OK,bytes,NULL);testPublishedDialDrop(context);Returned++;
    }else if(mode==1){
        xthread* thread=xrtThreadCreate(completeBeforePublish,context,0);
        testRequire(thread&&xrtThreadWaitFor(thread,5000)==XWAIT_OK,
            "completion blocked waiting for the constructor");
        xrtThreadDestroy(thread);
    }else (void)completeBeforePublish(context);
    testRequire(Returned==1,"completion did not return before publication");
    return &DialMarker;
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"debug setup");
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    for(unsigned i=0;i<4;i++){
        Released=Returned=OperationRefs=0;CancelContext=NULL;
        xfuture* f=testPublishedDialCreate(&policy,submitBeforePublish,&i);
        testRequire(f&&Released==(i==3?2u:1u)&&!OperationRefs&&Returned==1&&
            xrtFutureState(f)==(i==2?XFUTURE_RESOLVED:i==3?XFUTURE_CANCELLED:XFUTURE_CLOSED),
            "publication lost its operation credit or terminal result");
        if(i==2){xbytesview bytes=xrtNetBytesView(xrtFutureValue(f));testRequire(bytes.Size==3&&!memcmp(bytes.Data,"a\0b",3),"owned early result lost bytes");}
        xrtFutureDestroy(f);xrtClearError();
    }
    xrtMemDebugSnapshot(&after);
    testRequire(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount,"publication leak");
    testRequire(xrtMemDebugReset(),"debug reset");
    puts("[PASS] shared Dial inline/threaded completion, early owned result and prepublication cancel; balanced");
    return 0;
}
