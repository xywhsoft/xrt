#include "../test.h"

static int testNotified,testDropped;

#if !defined(XRT_FEATURE_MEMORY_DEBUG)
static size_t testCalls,testFail,testLive;
static ptr testAlloc(ptr pContext,size_t iSize)
{
	ptr pMemory;
	(void)pContext;
	if(++testCalls==testFail) return NULL;
	pMemory=malloc(iSize);
	if(pMemory) testLive++;
	return pMemory;
}
static ptr testRealloc(ptr pContext,ptr pMemory,size_t iSize)
{
	(void)pContext;
	if(++testCalls==testFail) return NULL;
	if(!pMemory) return testAlloc(NULL,iSize);
	return realloc(pMemory,iSize);
}
static void testFree(ptr pContext,ptr pMemory)
{
	(void)pContext;
	if(pMemory) { testRequire(testLive>0u,"allocator credit underflow"); testLive--; free(pMemory); }
}
#endif
static void testNotify(xchannelsubscription* pSubscription,ptr pData)
{ (void)pSubscription; (void)pData; testNotified++; }
static void testDrop(const void* pData) { (void)pData; testDropped++; }
static const xchannelsubscribepolicyv1 testPolicy={sizeof(xchannelsubscribepolicyv1),testNotify,testDrop};

int main(void)
{
	#if !defined(XRT_FEATURE_MEMORY_DEBUG)
	xallocator tAllocator={NULL,testAlloc,testRealloc,testFree};
	xchannel tChannel;
	xchannelsubscribecase tCases[64];
	size_t iBaseline;
	testRequire(xrtSetAllocator(&tAllocator),"OOM allocator installation failed");
	testRequire(xrtChannelInit(&tChannel,0u),"OOM channel init failed");
	/* Both setup blocks exceed the small-block cache cutoff, so this backing
	 * prefix really reaches both allocation sites, independent of warmed pools. */
	for(size_t i=0;i<64u;i++)
		tCases[i]=(xchannelsubscribecase){&tChannel,XCHANNEL_OP_RECV,NULL};
	xrtClearError();
	iBaseline=testLive;
	for(size_t i=1;i<=2;i++) {
		xchannelsubscription* pSubscription;
		testCalls=0; testFail=i;
		pSubscription=xrtChannelSubscribeCreateV1(tCases,64u,NULL,&testPolicy);
		testRequire(!pSubscription && !testNotified && !testDropped,"OOM consumed Data or notified");
		testFail=0;
		xrtClearError();
		testRequire(testLive==iBaseline,"Create failure retained storage");
	}
	{
		xchannelsubscription* pSubscription=xrtChannelSubscribeCreateV1(tCases,64u,NULL,&testPolicy);
		testRequire(pSubscription!=NULL,"successful setup failed");
		testCalls=0; testFail=1;
		testRequire(xrtChannelSubscribeStart(pSubscription),"Start allocated after successful Create");
		testRequire(xrtChannelTrySend(&tChannel,NULL)==XCHANNEL_OK,"wake allocated after successful Create");
		xrtChannelSubscribeDestroy(pSubscription);
		testRequire(testCalls==0u && testNotified==1 && testDropped==1,"completion/cleanup allocated or lost Data");
	}
	testFail=0;
	xrtClearError();
	testRequire(xrtChannelUnit(&tChannel),"OOM unit found residual waiter");
	testRequire(testLive==iBaseline,"successful subscription retained storage");
	#else
		xchannel tChannel;
		xchannelsubscribecase tCases[1];
		testRequire(xrtMemDebugEnable(true),"logical OOM debug enable failed");
		testRequire(xrtChannelInit(&tChannel,0u),"logical OOM channel init failed");
		tCases[0]=(xchannelsubscribecase){&tChannel,XCHANNEL_OP_RECV,NULL};
		/* Also cover both logical allocations for the common one-case shape;
		 * backing failure alone cannot see already-cached small blocks. */
		xmemdebugsnapshot tBefore,tAfter;
		xrtMemDebugSnapshot(&tBefore);
		for(uint64 i=0;i<2u;i++) {
			xchannelsubscription* pSubscription;
			testRequire(xrtMemDebugFailAfter(i),"logical OOM injection failed");
			pSubscription=xrtChannelSubscribeCreateV1(tCases,1u,NULL,&testPolicy);
			testRequire(!pSubscription && xrtMemDebugFailTriggered(),"logical OOM site not reached");
			xrtMemDebugFailClear();
			xrtClearError();
			xrtMemDebugSnapshot(&tAfter);
			testRequire(tAfter.LiveCount==tBefore.LiveCount && tAfter.LiveBytes==tBefore.LiveBytes &&
				!testNotified && !testDropped,"logical OOM rollback lost ownership");
		}
		{
			xchannelsubscription* pSubscription=xrtChannelSubscribeCreateV1(tCases,1u,NULL,&testPolicy);
			xmemdebugsnapshot tCreated,tFinished;
			testRequire(pSubscription!=NULL,"logical OOM success setup failed");
			xrtMemDebugSnapshot(&tCreated);
			testRequire(xrtMemDebugFailAfter(0u),"logical OOM wake injection failed");
			testRequire(xrtChannelSubscribeStart(pSubscription) &&
				xrtChannelTrySend(&tChannel,NULL)==XCHANNEL_OK,"logical OOM wake failed");
			xrtChannelSubscribeDestroy(pSubscription);
			testRequire(!xrtMemDebugFailTriggered(),"wait/wake/retirement attempted a logical allocation");
			xrtMemDebugFailClear();
			xrtMemDebugSnapshot(&tFinished);
			testRequire(tCreated.AllocCount==tFinished.AllocCount &&
				testNotified==1 && testDropped==1,"logical allocation-free completion failed");
		}
		testRequire(xrtChannelUnit(&tChannel),"logical OOM residual waiter");
		xrtMemDebugSnapshot(&tAfter);
		testRequire(tAfter.LiveCount==tBefore.LiveCount && tAfter.LiveBytes==tBefore.LiveBytes,
			"logical OOM acceptance retained storage");
		testMemoryDebugDrain("logical OOM quarantine drain failed");
	#endif
	printf("[PASS] channel subscription complete Create allocation prefix; zero-allocation wait/wake/cleanup\n");
	return 0;
}
