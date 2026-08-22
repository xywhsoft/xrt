#ifndef XRT_TEST_TEMPARENA_CORE_H
#define XRT_TEST_TEMPARENA_CORE_H

// 内部函数：__Test_TempArenaCoreRequire
static void __Test_TempArenaCoreRequire(bool bCond, const char* sMsg)
{
	if ( !bCond ) {
		fprintf(stderr, "[temparena-core] FAIL: %s\n", sMsg ? sMsg : "(no message)");
		exit(1);
	}
}


// 内部函数：__Test_TempArenaCoreFileContains
static bool __Test_TempArenaCoreFileContains(const char* sPath, const char* sNeedle)
{
	FILE* pFile;
	long iSize;
	char* sData;
	bool bRet;

	if ( sPath == NULL || sNeedle == NULL ) {
		return FALSE;
	}

	pFile = fopen(sPath, "rb");
	if ( pFile == NULL ) {
		return FALSE;
	}
	fseek(pFile, 0, SEEK_END);
	iSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	if ( iSize < 0 ) {
		fclose(pFile);
		return FALSE;
	}
	sData = (char*)malloc((size_t)iSize + 1);
	if ( sData == NULL ) {
		fclose(pFile);
		return FALSE;
	}
	if ( iSize > 0 ) {
		fread(sData, 1, (size_t)iSize, pFile);
	}
	sData[iSize] = '\0';
	fclose(pFile);
	bRet = strstr(sData, sNeedle) != NULL;
	free(sData);
	return bRet;
}


// 内部函数：__Test_TempArenaCoreCountBlocks
static uint32 __Test_TempArenaCoreCountBlocks(xrtTempArenaBlock* pBlock)
{
	uint32 iCount = 0;
	while ( pBlock ) {
		iCount++;
		pBlock = pBlock->pNext;
	}
	return iCount;
}


typedef struct {
	xrtTempArenaBlock* pBlocks;
	uint32 iDepth;
	bool bValuePreserved;
	bool bStatePreserved;
} __Test_TempArenaCoroutineCase;


// 内部函数：验证协程临时内存区跨 yield 保持且不污染宿主
static void __Test_TempArenaCoroutine(ptr pArg)
{
	__Test_TempArenaCoroutineCase* pCase = (__Test_TempArenaCoroutineCase*)pArg;
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	xrtTempScope tScope = xrtTempScopeBegin();
	char* sValue = (char*)xrtTempMemory(64);

	memcpy(sValue, "coroutine-temp", 15);
	pCase->pBlocks = pThreadData->tTemp.pBlocks;
	pCase->iDepth = pThreadData->tTemp.iScopeDepth;
	xrtCoYield();
	pCase->bValuePreserved = strcmp(sValue, "coroutine-temp") == 0;
	pCase->bStatePreserved = pThreadData->tTemp.pBlocks == pCase->pBlocks &&
		pThreadData->tTemp.iScopeDepth == pCase->iDepth;
	xrtTempScopeEnd(&tScope);
}


// TEMPARENA核心测试
static void Test_TempArenaCore(void)
{
	xrtThreadData* pThreadData;
	ptr pA1;
	ptr pA2;
	ptr pA3;
	ptr pB1;
	ptr pB2;
	ptr pB3;
	ptr pSpill;
	ptr pOuter;
	ptr pInner;
	str sPromoted;
	xrtTempScope tOuterScope;
	xrtTempScope tInnerScope;
	uint32 iBlockCountBefore;
	uint32 iBlockCountAfter;
	uint64 iResetBefore;

	printf("[temparena-core] start\n");

	pThreadData = xrtThreadGetCurrent();
	__Test_TempArenaCoreRequire(pThreadData != NULL, "current thread must be attached");

	xrtFreeTempMemory();

	pA1 = xrtTempMemory(1800);
	pA2 = xrtTempMemory(1800);
	pA3 = xrtTempMemory(1800);
	__Test_TempArenaCoreRequire(pA1 != NULL && pA2 != NULL && pA3 != NULL, "temp arena allocations failed");
	__Test_TempArenaCoreRequire(pThreadData->tTemp.pBlocks != NULL, "temp arena block list missing");
	__Test_TempArenaCoreRequire(pThreadData->tTemp.pSpill == NULL, "small temp allocations should not use spill");
	iBlockCountBefore = __Test_TempArenaCoreCountBlocks(pThreadData->tTemp.pBlocks);
	__Test_TempArenaCoreRequire(iBlockCountBefore >= 2, "expected multiple arena blocks after repeated allocations");

	iResetBefore = pThreadData->tTemp.iResetCount;
	xrtFreeTempMemory();
	__Test_TempArenaCoreRequire(pThreadData->tTemp.pCurrent == pThreadData->tTemp.pBlocks, "arena reset should rewind current block");
	__Test_TempArenaCoreRequire(pThreadData->tTemp.pSpill == NULL, "arena reset should clear spill list");
	__Test_TempArenaCoreRequire(pThreadData->tTemp.iResetCount == iResetBefore + 1, "arena reset count mismatch");

	pB1 = xrtTempMemory(1800);
	pB2 = xrtTempMemory(1800);
	pB3 = xrtTempMemory(1800);
	__Test_TempArenaCoreRequire(pB1 != NULL && pB2 != NULL && pB3 != NULL, "arena reuse allocations failed");
	iBlockCountAfter = __Test_TempArenaCoreCountBlocks(pThreadData->tTemp.pBlocks);
	__Test_TempArenaCoreRequire(iBlockCountAfter == iBlockCountBefore, "arena reuse should not expand block chain");
	__Test_TempArenaCoreRequire(pB1 == pA1 && pB2 == pA2 && pB3 == pA3, "arena reset should reuse existing block memory");

	pSpill = xrtTempMemory(3000);
	__Test_TempArenaCoreRequire(pSpill != NULL, "spill allocation failed");
	__Test_TempArenaCoreRequire(pThreadData->tTemp.pSpill != NULL, "spill allocation should populate spill list");
	__Test_TempArenaCoreRequire(pThreadData->tTemp.pSpill->iFlags == 1u, "spill block flag mismatch");

	xrtFreeTempMemory();
	__Test_TempArenaCoreRequire(pThreadData->tTemp.pSpill == NULL, "spill list should be released on reset");

	// 嵌套作用域只回收自身分配，外层指针和内容必须保持有效。
	pOuter = xrtTempMemory(64);
	memcpy(pOuter, "outer", 6);
	tOuterScope = xrtTempScopeBegin();
	pInner = xrtTempMemory(128);
	memcpy(pInner, "inner", 6);
	tInnerScope = xrtTempScopeBegin();
	__Test_TempArenaCoreRequire(xrtTempMemory(256) != NULL, "nested temp scope allocation failed");
	xrtTempScopeEnd(&tInnerScope);
	__Test_TempArenaCoreRequire(strcmp((const char*)pInner, "inner") == 0, "ending child scope must preserve parent scope data");
	xrtTempScopeEnd(&tOuterScope);
	__Test_TempArenaCoreRequire(strcmp((const char*)pOuter, "outer") == 0, "ending outer scope must preserve pre-scope data");
	__Test_TempArenaCoreRequire(pThreadData->tTemp.iScopeDepth == 0, "temp scope depth should return to zero");

	// 字符串提升后应位于父作用域，并在子作用域结束后继续有效。
	tOuterScope = xrtTempScopeBegin();
	pInner = xrtTempMemory(32);
	memcpy(pInner, "promoted", 9);
	sPromoted = xrtTempScopeEndString(&tOuterScope, (str)pInner, 8);
	__Test_TempArenaCoreRequire(strcmp(sPromoted, "promoted") == 0, "promoted temp string content mismatch");

	// 宿主与 coroutine 必须各自维护临时内存区，且 coroutine 数据可跨 yield 使用。
	{
		__Test_TempArenaCoroutineCase tCase;
		xrtTempArenaBlock* pHostBlocks;
		xrtTempScope tHostScope;
		char* sHostValue;
		xcoro pCo;

		memset(&tCase, 0, sizeof(tCase));
		xrtFreeTempMemory();
		tHostScope = xrtTempScopeBegin();
		sHostValue = (char*)xrtTempMemory(64);
		memcpy(sHostValue, "host-temp", 10);
		pCo = xrtCoCreate(__Test_TempArenaCoroutine, &tCase, 0);
		__Test_TempArenaCoreRequire(pCo != NULL, "coroutine temp arena fixture create failed");
		pHostBlocks = pThreadData->tTemp.pBlocks;
		__Test_TempArenaCoreRequire(xrtCoResume(pCo), "coroutine temp arena first resume failed");
		__Test_TempArenaCoreRequire(strcmp(sHostValue, "host-temp") == 0, "coroutine must preserve host temp data");
		__Test_TempArenaCoreRequire(pThreadData->tTemp.pBlocks == pHostBlocks, "coroutine must restore host temp blocks");
		__Test_TempArenaCoreRequire(pThreadData->tTemp.iScopeDepth == 1, "coroutine must restore host temp scope depth");
		__Test_TempArenaCoreRequire(tCase.pBlocks != NULL && tCase.pBlocks != pHostBlocks, "coroutine must own distinct temp blocks");
		__Test_TempArenaCoreRequire(xrtTempMemory(512) != NULL, "host temp allocation after coroutine yield failed");
		__Test_TempArenaCoreRequire(xrtCoResume(pCo), "coroutine temp arena second resume failed");
		__Test_TempArenaCoreRequire(xrtCoGetState(pCo) == XRT_CO_DEAD, "coroutine temp arena fixture must finish");
		__Test_TempArenaCoreRequire(tCase.bValuePreserved, "coroutine temp value must survive yield");
		__Test_TempArenaCoreRequire(tCase.bStatePreserved, "coroutine temp state must survive yield");
		xrtCoDestroy(pCo);
		xrtTempScopeEnd(&tHostScope);
	}

	#ifdef XRT_MEM_DEBUG
	{
		const char* sTextPath = "dev/temparena_report.txt";
		const char* sJsonPath = "dev/temparena_report.json";

		xrtMemDebugReset();
		xrtMemDebugEnable(TRUE);
		xrtFreeTempMemory();
		__Test_TempArenaCoreRequire(xrtTempMemory(64) != NULL, "debug small temp allocation failed");
		__Test_TempArenaCoreRequire(xrtTempMemory(3000) != NULL, "debug spill temp allocation failed");
		xrtFreeTempMemory();
		__Test_TempArenaCoreRequire(xrtMemDebugDumpText((str)sTextPath), "temp text report dump failed");
		__Test_TempArenaCoreRequire(xrtMemDebugDumpJson((str)sJsonPath), "temp json report dump failed");
		__Test_TempArenaCoreRequire(__Test_TempArenaCoreFileContains(sTextPath, "TEMP_ALLOC"), "text report missing TEMP_ALLOC");
		__Test_TempArenaCoreRequire(__Test_TempArenaCoreFileContains(sTextPath, "TEMP_RESET"), "text report missing TEMP_RESET");
		__Test_TempArenaCoreRequire(__Test_TempArenaCoreFileContains(sTextPath, "temp_current_bytes"), "text report missing temp_current_bytes");
		__Test_TempArenaCoreRequire(__Test_TempArenaCoreFileContains(sTextPath, "temp_peak_bytes"), "text report missing temp_peak_bytes");
		__Test_TempArenaCoreRequire(__Test_TempArenaCoreFileContains(sTextPath, "temp_reset_count"), "text report missing temp_reset_count");
		__Test_TempArenaCoreRequire(__Test_TempArenaCoreFileContains(sJsonPath, "\"TEMP_ALLOC\""), "json report missing TEMP_ALLOC");
		__Test_TempArenaCoreRequire(__Test_TempArenaCoreFileContains(sJsonPath, "\"TEMP_RESET\""), "json report missing TEMP_RESET");
		__Test_TempArenaCoreRequire(__Test_TempArenaCoreFileContains(sJsonPath, "\"temp_current_bytes\""), "json report missing temp_current_bytes");
		__Test_TempArenaCoreRequire(__Test_TempArenaCoreFileContains(sJsonPath, "\"temp_peak_bytes\""), "json report missing temp_peak_bytes");
		__Test_TempArenaCoreRequire(__Test_TempArenaCoreFileContains(sJsonPath, "\"temp_reset_count\""), "json report missing temp_reset_count");
		xrtMemDebugReset();
	}
	#endif

	xrtFreeTempMemory();

	printf("[temparena-core] PASS\n");
}

#endif
