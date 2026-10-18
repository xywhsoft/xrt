#include "../test.h"

typedef struct testsubscription {
	int Notified;
	int Dropped;
	bool InNotify;
	bool ReleaseInNotify;
	xchannel* Channel;
	xchannelselectresult Result;
	ptr Value;
} testsubscription;

static void testSubscriptionNotify(xchannelsubscription* pSubscription,ptr pData)
{
	testsubscription* pState=(testsubscription*)pData;
	pState->InNotify=true;
	testRequire(++pState->Notified==1,"subscription notified twice");
	testRequire(xrtChannelSubscribeResult(pSubscription,&pState->Result,&pState->Value),
		"notification preceded result/unlink publication");
	/* SRW/pthread Channel mutexes are nonrecursive; this proves lock-free callback delivery. */
	(void)xrtChannelCount(pState->Channel);
	testRequire(!xrtChannelSubscribeStart(pSubscription),"subscription restarted");
	if(pState->ReleaseInNotify) {
		xrtChannelSubscribeDestroy(pSubscription);
		testRequire(!pState->Dropped,"Drop ran inside a still-active callback");
	}
	pState->InNotify=false;
}

static void testSubscriptionDrop(const void* pData)
{
	testsubscription* pState=(testsubscription*)pData;
	testRequire(!pState->InNotify,"Drop preceded callback tail");
	testRequire(++pState->Dropped==1,"subscription Data dropped twice");
}

static const xchannelsubscribepolicyv1 testPolicy={
	sizeof(xchannelsubscribepolicyv1),testSubscriptionNotify,testSubscriptionDrop
};

static xchannelsubscription* testCreate(testsubscription* pState,
	xchannelsubscribecase* pCases,size_t iCount)
{
	xchannelsubscription* pSubscription=xrtChannelSubscribeCreateV1(pCases,iCount,pState,&testPolicy);
	testRequire(pSubscription!=NULL,"subscription Create failed");
	return pSubscription;
}

static void testBuffered(void)
{
	xchannel tChannel;
	xchannelsubscribecase tCase;
	xchannelsubscription* pSubscription;
	testsubscription tState={0};
	xchannelselectresult tSentinel={XWAIT_CLOSED,99u,XCHANNEL_FULL};
	ptr pSentinel=(ptr)(uintptr_t)99u;
	testRequire(xrtChannelInit(&tChannel,1u),"buffered channel init failed");
	tState.Channel=&tChannel;
	tCase=(xchannelsubscribecase){&tChannel,XCHANNEL_OP_RECV,NULL};
	pSubscription=testCreate(&tState,&tCase,1u);
	tCase.Channel=NULL; /* Create owns the descriptor copy. */
	testRequire(!xrtChannelSubscribeResult(pSubscription,&tSentinel,&pSentinel) &&
		tSentinel.Index==99u && (uintptr_t)pSentinel==99u,"pending query changed outputs");
	testRequire(xrtChannelSubscribeStart(pSubscription),"buffered subscribe Start failed");
	testRequire(!tState.Notified,"empty channel completed prematurely");
	testRequire(!xrtChannelUnit(&tChannel),"active subscription did not protect Unit");
	testRequire(xrtChannelTrySend(&tChannel,(ptr)(uintptr_t)73u)==XCHANNEL_OK,"buffered send failed");
	testRequire(tState.Notified==1 && tState.Result.Wait==XWAIT_OK &&
		tState.Result.Index==0u && tState.Result.Result==XCHANNEL_OK &&
		(uintptr_t)tState.Value==73u,"buffered subscription result mismatch");
	testRequire(xrtChannelSubscribeRef(pSubscription)==pSubscription,"subscription Ref failed");
	xrtChannelSubscribeDestroy(pSubscription);
	testRequire(!tState.Dropped,"nonlast external Destroy dropped Data");
	xrtChannelSubscribeDestroy(pSubscription);
	testRequire(tState.Dropped==1,"buffered Data reference leaked");
	testRequire(xrtChannelUnit(&tChannel),"buffered unit failed");
}

static void testStopAndReady(void)
{
	for(int i=0;i<5;i++) {
		xchannel tChannel;
		xchannelsubscription* pSubscription;
		testsubscription tState={0};
		xchannelsubscribecase tCase;
		testRequire(xrtChannelInit(&tChannel,1u),"stop channel init failed");
		tState.Channel=&tChannel;
		tCase=(xchannelsubscribecase){&tChannel,XCHANNEL_OP_RECV,NULL};
		if(i==1) testRequire(xrtChannelTrySend(&tChannel,NULL)==XCHANNEL_OK,"NULL message setup failed");
		if(i==2) xrtChannelClose(&tChannel);
		pSubscription=testCreate(&tState,&tCase,1u);
		testRequire(xrtChannelSubscribeStop(pSubscription,i==3?XWAIT_TIMEOUT:XWAIT_CANCELLED),
			"pre-Start stop request failed");
		testRequire(!tState.Notified,"unstarted subscription notified");
		if(i==4) {
			xrtChannelSubscribeDestroy(pSubscription);
			testRequire(!tState.Notified && tState.Dropped==1,"unstarted release contract failed");
		} else {
			tState.ReleaseInNotify=true;
			testRequire(xrtChannelSubscribeStart(pSubscription),"pre-stopped Start failed");
			testRequire(tState.Notified==1 && tState.Dropped==1,"inline callback release leaked");
			testRequire(tState.Result.Wait==(i==0?XWAIT_CANCELLED:i==3?XWAIT_TIMEOUT:XWAIT_OK),
				"ready/closed versus stop precedence mismatch");
			if(i==1) testRequire(tState.Result.Result==XCHANNEL_OK && !tState.Value,"NULL payload not preserved");
			if(i==2) testRequire(tState.Result.Result==XCHANNEL_CLOSED,"closed result mismatch");
		}
		testRequire(xrtChannelUnit(&tChannel),"stop channel unit failed");
	}
}

static void testRendezvous(void)
{
	for(int i=0;i<4;i++) {
		xchannel tChannel;
		testsubscription tState={0};
		xchannelsubscribecase tCase;
		xchannelsubscription* pSubscription;
		ptr pValue=NULL;
		testRequire(xrtChannelInit(&tChannel,0u),"rendezvous init failed");
		tState.Channel=&tChannel;
		tCase=(xchannelsubscribecase){&tChannel,i&1?XCHANNEL_OP_SEND:XCHANNEL_OP_RECV,(ptr)(uintptr_t)42u};
		pSubscription=testCreate(&tState,&tCase,1u);
		testRequire(xrtChannelSubscribeStart(pSubscription),"rendezvous Start failed");
		if(i==0) testRequire(xrtChannelTrySend(&tChannel,(ptr)(uintptr_t)42u)==XCHANNEL_OK,"rendezvous TrySend failed");
		if(i==1) testRequire(xrtChannelTryRecv(&tChannel,&pValue)==XCHANNEL_OK,"rendezvous TryRecv failed");
		if(i==2) testRequire(xrtChannelSendFor(&tChannel,(ptr)(uintptr_t)42u,1000)==XWAIT_OK,"sync sender failed");
		if(i==3) testRequire(xrtChannelRecvFor(&tChannel,&pValue,1000)==XWAIT_OK,"sync receiver failed");
		testRequire(tState.Notified==1 && tState.Result.Wait==XWAIT_OK &&
			tState.Result.Result==XCHANNEL_OK,"rendezvous did not complete");
		testRequire((uintptr_t)(i&1?pValue:tState.Value)==42u,"rendezvous value mismatch");
		xrtChannelSubscribeDestroy(pSubscription);
		testRequire(tState.Dropped==1,"rendezvous Data leaked");
		testRequire(xrtChannelUnit(&tChannel),"rendezvous unit failed");
	}
}

static void testManyAndRelease(void)
{
	xchannel tFirst,tSecond;
	testsubscription tState={0};
	xchannelsubscribecase tCases[2];
	xchannelsubscription* pSubscription;
	testRequire(xrtChannelInit(&tFirst,0u) && xrtChannelInit(&tSecond,1u),"multi channel init failed");
	tState.Channel=&tSecond;
	tCases[0]=(xchannelsubscribecase){&tFirst,XCHANNEL_OP_SEND,(ptr)(uintptr_t)7u};
	tCases[1]=(xchannelsubscribecase){&tSecond,XCHANNEL_OP_RECV,NULL};
	pSubscription=testCreate(&tState,tCases,2u);
	testRequire(xrtChannelSubscribeStart(pSubscription),"multi Start failed");
	testRequire(xrtChannelTrySend(&tSecond,(ptr)(uintptr_t)9u)==XCHANNEL_OK,"multi send failed");
	testRequire(tState.Result.Index==1u && (uintptr_t)tState.Value==9u,"multi selection mismatch");
	testRequire(xrtChannelUnit(&tFirst) && xrtChannelUnit(&tSecond),"losing waiters not physically removed");
	xrtChannelSubscribeDestroy(pSubscription);
	memset(&tState,0,sizeof(tState));
	testRequire(xrtChannelInit(&tFirst,0u),"last release init failed");
	tState.Channel=&tFirst;
	tCases[0]=(xchannelsubscribecase){&tFirst,XCHANNEL_OP_RECV,NULL};
	pSubscription=testCreate(&tState,tCases,1u);
	testRequire(xrtChannelSubscribeStart(pSubscription),"last release Start failed");
	xrtChannelSubscribeDestroy(pSubscription);
	testRequire(tState.Notified==1 && tState.Dropped==1 &&
		tState.Result.Wait==XWAIT_CANCELLED,"last external release left an immortal registration");
	testRequire(xrtChannelUnit(&tFirst),"last release unit failed");
}

static void testSubscriptionsPair(void)
{
	xchannel tChannel;
	testsubscription tReceiver={0},tSender={0};
	xchannelsubscription *pReceive,*pSend;
	xchannelsubscribecase tCase;
	testRequire(xrtChannelInit(&tChannel,0u),"subscription pair init failed");
	tReceiver.Channel=&tChannel;
	tSender.Channel=&tChannel;
	tCase=(xchannelsubscribecase){&tChannel,XCHANNEL_OP_RECV,NULL};
	pReceive=testCreate(&tReceiver,&tCase,1u);
	tCase=(xchannelsubscribecase){&tChannel,XCHANNEL_OP_SEND,(ptr)(uintptr_t)81u};
	pSend=testCreate(&tSender,&tCase,1u);
	testRequire(xrtChannelSubscribeStart(pReceive),"pair receive Start failed");
	testRequire(xrtChannelSubscribeStart(pSend),"pair send Start failed");
	testRequire(tReceiver.Notified==1 && tSender.Notified==1 &&
		tReceiver.Result.Result==XCHANNEL_OK && tSender.Result.Result==XCHANNEL_OK &&
		(uintptr_t)tReceiver.Value==81u,"two subscriptions did not rendezvous");
	xrtChannelSubscribeDestroy(pReceive);
	xrtChannelSubscribeDestroy(pSend);
	testRequire(tReceiver.Dropped==1 && tSender.Dropped==1,"subscription pair retained Data");
	testRequire(xrtChannelUnit(&tChannel),"subscription pair unit failed");
}

int main(void)
{
	testBuffered();
	testStopAndReady();
	testRendezvous();
	testManyAndRelease();
	testSubscriptionsPair();
	printf("[PASS] channel subscription ownership, reentry, select, stop and rendezvous\n");
	return 0;
}
