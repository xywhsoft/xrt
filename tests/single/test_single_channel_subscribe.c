#define XRT_MODULE_CHANNEL_SUBSCRIBE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

static int testNotified,testDropped;
static void testNotify(xchannelsubscription* pSubscription,ptr pData)
{
	xchannelselectresult tResult;
	ptr pValue;
	(void)pData;
	if(xrtChannelSubscribeResult(pSubscription,&tResult,&pValue) &&
		tResult.Wait==XWAIT_OK && tResult.Result==XCHANNEL_OK && !pValue) testNotified++;
}
static void testDrop(const void* pData) { (void)pData; testDropped++; }

int main(void)
{
	const xchannelsubscribepolicyv1 tPolicy={sizeof(tPolicy),testNotify,testDrop};
	xchannel tChannel;
	xchannelsubscribecase tCase;
	xchannelsubscription* pSubscription;
	if(!xrtChannelInit(&tChannel,0u)) return 1;
	tCase=(xchannelsubscribecase){&tChannel,XCHANNEL_OP_RECV,NULL};
	pSubscription=xrtChannelSubscribeCreateV1(&tCase,1u,NULL,&tPolicy);
	if(!pSubscription || !xrtChannelSubscribeStart(pSubscription)) return 2;
	if(xrtChannelTrySend(&tChannel,NULL)!=XCHANNEL_OK) return 3;
	xrtChannelSubscribeDestroy(pSubscription);
	if(testNotified!=1 || testDropped!=1 || !xrtChannelUnit(&tChannel)) return 4;
	return 0;
}
