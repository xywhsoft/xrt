#include "../test.h"
#include "../test_thread.h"

typedef struct testsubrace {
	xchannel Channels[2];
	xchannelsubscription* Subscription;
	xevent Start;
	xevent Entered;
	xevent Leave;
	bool BlockNotify;
	xatomic32 Notified;
	xatomic32 Dropped;
	xchannelselectresult Result;
	ptr Value;
	xchannelresult Sends[2];
} testsubrace;

typedef struct testsubworker { testsubrace* Race; int Index; } testsubworker;

static void testRaceNotify(xchannelsubscription* pSubscription,ptr pData)
{
	testsubrace* pRace=(testsubrace*)pData;
	testRequire(xrtChannelSubscribeResult(pSubscription,&pRace->Result,&pRace->Value),"race result not ready");
	if(pRace->BlockNotify) {
		testRequire(xrtEventSet(&pRace->Entered),"callback entry event failed");
		testRequire(xrtEventWaitFor(&pRace->Leave,5000)==XWAIT_OK,"callback release event timed out");
	}
	(void)xrtAtomic32FetchAdd(&pRace->Notified,1u,XMEMORY_RELEASE);
}

static void testRaceDrop(const void* pData)
{
	testsubrace* pRace=(testsubrace*)pData;
	(void)xrtAtomic32FetchAdd(&pRace->Dropped,1u,XMEMORY_RELEASE);
}

static const xchannelsubscribepolicyv1 testRacePolicy={sizeof(xchannelsubscribepolicyv1),testRaceNotify,testRaceDrop};

static int testRaceWorker(ptr pData)
{
	testsubworker* pWorker=(testsubworker*)pData;
	testsubrace* pRace=pWorker->Race;
	if(xrtEventWait(&pRace->Start)!=XWAIT_OK) return 1;
	if(pWorker->Index<2) pRace->Sends[pWorker->Index]=xrtChannelTrySend(
		&pRace->Channels[pWorker->Index],(ptr)(uintptr_t)(pWorker->Index+20));
	else if(pWorker->Index==2) (void)xrtChannelSubscribeStop(pRace->Subscription,XWAIT_CANCELLED);
	else if(!xrtChannelSubscribeStart(pRace->Subscription)) return 2;
	return 0;
}

static void testDestroyDuringCallback(void)
{
	testsubrace tRace;
	testsubworker tWorker;
	testthread tThread;
	xchannelsubscribecase tCase;
	memset(&tRace,0,sizeof(tRace));
	tRace.BlockNotify=true;
	xrtAtomic32Init(&tRace.Notified,0u);
	xrtAtomic32Init(&tRace.Dropped,0u);
	testRequire(xrtEventInit(&tRace.Start,true,true) &&
		xrtEventInit(&tRace.Entered,true,false) && xrtEventInit(&tRace.Leave,true,false),"tail events init failed");
	testRequire(xrtChannelInit(&tRace.Channels[0],0u),"tail channel init failed");
	tCase=(xchannelsubscribecase){&tRace.Channels[0],XCHANNEL_OP_RECV,NULL};
	tRace.Subscription=xrtChannelSubscribeCreateV1(&tCase,1u,&tRace,&testRacePolicy);
	testRequire(tRace.Subscription && xrtChannelSubscribeStart(tRace.Subscription),"tail setup failed");
	tWorker=(testsubworker){&tRace,0};
	tThread.Proc=testRaceWorker;
	tThread.Data=&tWorker;
	testThreadsStart(&tThread,1u);
	testRequire(xrtEventWaitFor(&tRace.Entered,2000)==XWAIT_OK,"callback never entered");
	xrtChannelSubscribeDestroy(tRace.Subscription);
	testRequire(!xrtAtomic32Load(&tRace.Dropped,XMEMORY_ACQUIRE),"last release dropped an active callback context");
	testRequire(xrtEventSet(&tRace.Leave),"callback leave event failed");
	testThreadsJoin(&tThread,1u);
	testRequire(xrtAtomic32Load(&tRace.Dropped,XMEMORY_ACQUIRE)==1u &&
		xrtAtomic32Load(&tRace.Notified,XMEMORY_ACQUIRE)==1u,"physical callback tail leaked");
	testRequire(xrtChannelUnit(&tRace.Channels[0]) && xrtEventUnit(&tRace.Start) &&
		xrtEventUnit(&tRace.Entered) && xrtEventUnit(&tRace.Leave),"tail teardown failed");
}

int main(void)
{
	for(int iteration=0;iteration<300;iteration++) {
		testsubrace tRace;
		testsubworker tWorkers[4];
		testthread tThreads[4];
		size_t iThreads=iteration<150 ? 3u : 4u;
		xchannelsubscribecase tCases[2];
		memset(&tRace,0,sizeof(tRace));
		xrtAtomic32Init(&tRace.Notified,0u);
		xrtAtomic32Init(&tRace.Dropped,0u);
		testRequire(xrtEventInit(&tRace.Start,true,false),"race event init failed");
		for(int i=0;i<2;i++) {
			testRequire(xrtChannelInit(&tRace.Channels[i],0u),"race channel init failed");
			tCases[i]=(xchannelsubscribecase){&tRace.Channels[i],XCHANNEL_OP_RECV,NULL};
		}
		tRace.Subscription=xrtChannelSubscribeCreateV1(tCases,2u,&tRace,&testRacePolicy);
		testRequire(tRace.Subscription!=NULL,"race Create failed");
		if(iThreads==3u) testRequire(xrtChannelSubscribeStart(tRace.Subscription),"race Start failed");
		for(int i=0;i<(int)iThreads;i++) {
			tWorkers[i]=(testsubworker){&tRace,i};
			tThreads[i].Proc=testRaceWorker;
			tThreads[i].Data=&tWorkers[i];
		}
		testThreadsStart(tThreads,iThreads);
		testRequire(xrtEventSet(&tRace.Start),"race start event failed");
		testThreadsJoin(tThreads,iThreads);
		testRequire(xrtAtomic32Load(&tRace.Notified,XMEMORY_ACQUIRE)==1u,"race duplicate/lost completion");
		if(tRace.Result.Wait==XWAIT_OK) {
			testRequire(tRace.Result.Index<2u && tRace.Result.Result==XCHANNEL_OK &&
				(uintptr_t)tRace.Value==20u+tRace.Result.Index &&
				tRace.Sends[tRace.Result.Index]==XCHANNEL_OK,"race winner/value inconsistent");
			testRequire(tRace.Sends[1u-tRace.Result.Index]==XCHANNEL_FULL,"race committed twice");
		} else testRequire(tRace.Result.Wait==XWAIT_CANCELLED &&
			tRace.Sends[0]==XCHANNEL_FULL && tRace.Sends[1]==XCHANNEL_FULL,"race stop lost a message");
		xrtChannelSubscribeDestroy(tRace.Subscription);
		testRequire(xrtAtomic32Load(&tRace.Dropped,XMEMORY_ACQUIRE)==1u,"race Data credit leaked");
		for(int i=0;i<2;i++) testRequire(xrtChannelUnit(&tRace.Channels[i]),"race retained waiter");
		testRequire(xrtEventUnit(&tRace.Start),"race event unit failed");
	}
	testDestroyDuringCallback();
	printf("[PASS] channel subscription 300 producer/cancel/Start races and active-callback retirement\n");
	return 0;
}
