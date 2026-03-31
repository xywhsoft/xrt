// 内部函数：__Test_XNet2_ReleaseRef
static void __Test_XNet2_ReleaseRef(ptr pCtx, const void* pData, size_t iLen)
{
	int* pCount = (int*)pCtx;
	(void)pData;
	(void)iLen;
	if ( pCount ) (*pCount)++;
}



// XNET2MEM测试
void Test_XNet2_Mem(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 Memory Chain Test:\n\n");
	
	{
		xnetchain tChain;
		xrtNetChainInit(&tChain);
		printf("  Init empty bytes == 0 : %s\n", xrtNetChainBytes(&tChain) == 0 ? "PASS" : "FAIL");
		printf("  Init empty spans == 0 : %s\n", xrtNetChainSpanCount(&tChain) == 0 ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);
	}
	
	{
		xnetchain tChain;
		char aPeek[16] = {0};
		xrtNetChainInit(&tChain);
		
		bool bOk = xrtNetChainAppendCopy(&tChain, "hello", 5);
		size_t iPeek = xrtNetChainPeek(&tChain, aPeek, sizeof(aPeek));
		size_t iPosL = xrtNetChainFindByte(&tChain, 'l', 0);
		
		printf("  Append small block : %s\n", bOk ? "PASS" : "FAIL");
		printf("  Bytes after append : %s\n", xrtNetChainBytes(&tChain) == 5 ? "PASS" : "FAIL");
		printf("  Span count == 1 : %s\n", xrtNetChainSpanCount(&tChain) == 1 ? "PASS" : "FAIL");
		printf("  Peek data match : %s\n", iPeek == 5 && memcmp(aPeek, "hello", 5) == 0 ? "PASS" : "FAIL");
		printf("  Find first 'l' at 2 : %s\n", iPosL == 2 ? "PASS" : "FAIL");
		
		xrtNetChainConsume(&tChain, 2);
		memset(aPeek, 0, sizeof(aPeek));
		iPeek = xrtNetChainPeek(&tChain, aPeek, sizeof(aPeek));
		printf("  Consume 2 leaves 'llo' : %s\n",
			iPeek == 3 && memcmp(aPeek, "llo", 3) == 0 ? "PASS" : "FAIL");
		
		xrtNetChainClear(&tChain);
	}
	
	{
		xnetchain tChain;
		char aA[300];
		char aB[50];
		char aPeek[8] = {0};
		xnetspan arrSpans[4];
		uint32 iSpanCap = (uint32)(sizeof(arrSpans) / sizeof(arrSpans[0]));
		uint32 iSpanCount = 0;
		memset(aA, 'A', sizeof(aA));
		memset(aB, 'B', sizeof(aB));
		
		xrtNetChainInit(&tChain);
		xrtNetChainAppendCopy(&tChain, aA, sizeof(aA));
		xrtNetChainAppendCopy(&tChain, aB, sizeof(aB));
		iSpanCount = xrtNetChainGetSpans(&tChain, arrSpans, iSpanCap);
		
		printf("  Multi-block bytes == 350 : %s\n", xrtNetChainBytes(&tChain) == 350 ? "PASS" : "FAIL");
		printf("  Multi-block spans >= 2 : %s\n", xrtNetChainSpanCount(&tChain) >= 2 ? "PASS" : "FAIL");
		printf("  GetSpans count >= 2 : %s\n", iSpanCount >= 2 ? "PASS" : "FAIL");
		printf("  First span starts with 'A' : %s\n",
			iSpanCount >= 1 && ((const char*)arrSpans[0].pData)[0] == 'A' ? "PASS" : "FAIL");
		printf("  Find first 'B' at 300 : %s\n", xrtNetChainFindByte(&tChain, 'B', 0) == 300 ? "PASS" : "FAIL");
		
		xrtNetChainConsume(&tChain, 300);
		size_t iPeek = xrtNetChainPeek(&tChain, aPeek, sizeof(aPeek));
		printf("  Consume first block -> B head : %s\n",
			iPeek == sizeof(aPeek) && memcmp(aPeek, "BBBBBBBB", 8) == 0 ? "PASS" : "FAIL");
		
		xrtNetChainConsume(&tChain, 1000);
		printf("  Consume all -> bytes == 0 : %s\n", xrtNetChainBytes(&tChain) == 0 ? "PASS" : "FAIL");
		printf("  Consume all -> spans == 0 : %s\n", xrtNetChainSpanCount(&tChain) == 0 ? "PASS" : "FAIL");
		
		xrtNetChainClear(&tChain);
	}
	
	{
		xnetmemconfig tCfg;
		xnetmemctx tCtx;
		xnetmemstats tStats;
		xnetchain tChain;
		xrtNetMemConfigInit(&tCfg);
		xrtNetMemCtxInit(&tCtx, &tCfg);
		
		xrtNetChainInitEx(&tChain, &tCtx);
		xrtNetChainAppendCopy(&tChain, "abc", 3);
		printf("  Small class block : %s\n",
			tChain.pHead && tChain.pHead->iClassId == XNET_MEM_CLASS_SMALL ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);
		
		xrtNetMemCtxGetStats(&tCtx, &tStats);
		printf("  Small cache after clear >= 1 : %s\n", tStats.iSmallCached >= 1 ? "PASS" : "FAIL");
		
		xrtNetChainInitEx(&tChain, &tCtx);
		xrtNetChainAppendCopy(&tChain, "xyz", 3);
		xrtNetMemCtxGetStats(&tCtx, &tStats);
		printf("  Small cache reuse counted : %s\n", tStats.iSmallReuseCount >= 1 ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);
		
		xrtNetMemCtxUnit(&tCtx);
	}
	
	{
		xnetmemconfig tCfg;
		xnetmemctx tCtx;
		xnetchain tChain;
		char aMedium[500];
		char aLarge[5000];
		char aDynamic[20000];
		memset(aMedium, 'M', sizeof(aMedium));
		memset(aLarge, 'L', sizeof(aLarge));
		memset(aDynamic, 'D', sizeof(aDynamic));
		
		xrtNetMemConfigInit(&tCfg);
		xrtNetMemCtxInit(&tCtx, &tCfg);
		
		xrtNetChainInitEx(&tChain, &tCtx);
		xrtNetChainAppendCopy(&tChain, aMedium, sizeof(aMedium));
		printf("  Medium class block : %s\n",
			tChain.pHead && tChain.pHead->iClassId == XNET_MEM_CLASS_MEDIUM ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);
		
		xrtNetChainInitEx(&tChain, &tCtx);
		xrtNetChainAppendCopy(&tChain, aLarge, sizeof(aLarge));
		printf("  Large class block : %s\n",
			tChain.pHead && tChain.pHead->iClassId == XNET_MEM_CLASS_LARGE ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);
		
		xrtNetChainInitEx(&tChain, &tCtx);
		xrtNetChainAppendCopy(&tChain, aDynamic, sizeof(aDynamic));
		printf("  Dynamic class block : %s\n",
			tChain.pHead && tChain.pHead->iClassId == XNET_MEM_CLASS_DYNAMIC ? "PASS" : "FAIL");
		printf("  Dynamic capacity >= payload : %s\n",
			tChain.pHead && tChain.pHead->iCapacity >= sizeof(aDynamic) ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);
		
		xrtNetMemCtxUnit(&tCtx);
	}

	{
		xnetchain tChain;
		xnetbufref tRef;
		xnetspan arrSpans[2];
		uint32 iSpanCap = (uint32)(sizeof(arrSpans) / sizeof(arrSpans[0]));
		char aRefData[] = "ref-data";
		char aPeek[16] = {0};
		uint32 iSpanCount = 0;
		int iReleaseCount = 0;

		xrtNetChainInit(&tChain);
		memset(&tRef, 0, sizeof(tRef));
		tRef.pData = aRefData;
		tRef.iLen = (uint32)strlen(aRefData);
		tRef.pfnRelease = __Test_XNet2_ReleaseRef;
		tRef.pReleaseCtx = &iReleaseCount;

		printf("  Append ref block : %s\n", xrtNetChainAppendRef(&tChain, &tRef) ? "PASS" : "FAIL");
		iSpanCount = xrtNetChainGetSpans(&tChain, arrSpans, iSpanCap);
		printf("  Ref class block : %s\n",
			tChain.pHead && tChain.pHead->iClassId == XNET_MEM_CLASS_REF ? "PASS" : "FAIL");
		printf("  Ref span count == 1 : %s\n", iSpanCount == 1 ? "PASS" : "FAIL");
		printf("  Ref span points to source : %s\n",
			iSpanCount == 1 && arrSpans[0].pData == aRefData ? "PASS" : "FAIL");
		printf("  Ref peek matches source : %s\n",
			xrtNetChainPeek(&tChain, aPeek, sizeof(aPeek)) == strlen(aRefData) &&
			memcmp(aPeek, aRefData, strlen(aRefData)) == 0 ? "PASS" : "FAIL");

		xrtNetChainConsume(&tChain, 4);
		memset(aPeek, 0, sizeof(aPeek));
		printf("  Ref partial consume keeps suffix : %s\n",
			xrtNetChainPeek(&tChain, aPeek, sizeof(aPeek)) == strlen("data") &&
			memcmp(aPeek, "data", strlen("data")) == 0 ? "PASS" : "FAIL");
		printf("  Ref release not fired early : %s\n", iReleaseCount == 0 ? "PASS" : "FAIL");

		xrtNetChainClear(&tChain);
		printf("  Ref release fired once on clear : %s\n", iReleaseCount == 1 ? "PASS" : "FAIL");
	}
}
