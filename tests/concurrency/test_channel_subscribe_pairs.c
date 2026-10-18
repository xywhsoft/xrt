#include "../test.h"
#include "../test_thread.h"

typedef struct pairside {
    xchannelsubscription* subscription;
    xevent* start;
    xchannelselectresult result;
    ptr value;
    unsigned notified,dropped;
} pairside;

static void notify(xchannelsubscription* subscription,ptr data)
{
    pairside* side=data;
    testRequire(xrtChannelSubscribeResult(subscription,&side->result,&side->value),"pair result not ready");
    ++side->notified;
}
static void drop(const void* data) { ++((pairside*)data)->dropped; }
static const xchannelsubscribepolicyv1 policy={sizeof(policy),notify,drop};
static int worker(ptr data)
{
    pairside* side=data;
    if(xrtEventWait(side->start)!=XWAIT_OK) return 1;
    return xrtChannelSubscribeStart(side->subscription) ? 0 : 2;
}

int main(void)
{
    for(unsigned round=0;round<300u;round++) {
        xchannel channels[3];
        xevent start;
        pairside sides[2]={{0}};
        testthread threads[2]={{0}};
        xchannelsubscribecase cases[2][2];
        for(size_t i=0;i<3u;i++) testRequire(xrtChannelInit(&channels[i],0u),"pair channel init failed");
        testRequire(xrtEventInit(&start,true,false),"pair event init failed");
        cases[0][0]=(xchannelsubscribecase){&channels[0],XCHANNEL_OP_RECV,NULL};
        cases[0][1]=(xchannelsubscribecase){&channels[1],XCHANNEL_OP_RECV,NULL};
        cases[1][0]=(xchannelsubscribecase){&channels[0],XCHANNEL_OP_SEND,(ptr)(uintptr_t)77u};
        cases[1][1]=(xchannelsubscribecase){&channels[2],XCHANNEL_OP_RECV,NULL};
        for(size_t i=0;i<2u;i++) {
            sides[i].start=&start;
            sides[i].subscription=xrtChannelSubscribeCreateV1(cases[i],2u,&sides[i],&policy);
            testRequire(sides[i].subscription!=NULL,"pair subscription create failed");
            threads[i].Proc=worker; threads[i].Data=&sides[i];
        }
        testThreadsStart(threads,2u);
        testRequire(xrtEventSet(&start),"pair start failed");
        testThreadsJoin(threads,2u);
        for(size_t i=0;i<2u;i++) testRequire(sides[i].notified==1u &&
            sides[i].result.Wait==XWAIT_OK && sides[i].result.Result==XCHANNEL_OK &&
            sides[i].result.Index==0u,"multiplexed peers lost availability notification");
        testRequire((uintptr_t)sides[0].value==77u,"pair delivered wrong value");
        for(size_t i=0;i<2u;i++) {
            xrtChannelSubscribeDestroy(sides[i].subscription);
            testRequire(sides[i].dropped==1u,"pair context leaked");
        }
        for(size_t i=0;i<3u;i++) testRequire(xrtChannelUnit(&channels[i]),"pair left a native node");
        testRequire(xrtEventUnit(&start),"pair event unit failed");
    }
    puts("[PASS] 300 simultaneous multiplexed subscription peer handshakes");
    return 0;
}
