#include "../test.h"



/* 安全重置必须擦除可复用块，安全销毁则清空 arena 本体。 */
static void testTempSecure(void)
{
	xtemparena Arena;
	unsigned char* pFirst;
	unsigned char* pSecond;

	memset(&Arena, 0, sizeof(Arena));
	testRequire(xrtTempInit(&Arena, NULL),
		"secure temp arena initialization failed");
	pFirst = (unsigned char*)xrtTempAlloc(&Arena, 64u);
	testRequire(pFirst != NULL, "secure temp allocation failed");
	memset(pFirst, 0xA5, 64u);
	testRequire(xrtTempSecureReset(&Arena), "secure temp reset failed");
	pSecond = (unsigned char*)xrtTempAlloc(&Arena, 64u);
	testRequire(pSecond == pFirst, "secure temp reset did not reuse block");
	for ( size_t i = 0; i < 64u; i++ ) {
		testRequire(pSecond[i] == 0, "secure temp reset left old content");
	}
	xrtTempSecureUnit(&Arena);
	testRequire(
		(Arena.Blocks == NULL) && (Arena.Flags == 0),
		"secure temp unit did not clear arena"
	);
}



/* 验证旧版 arena 边界以及新增的有界保留契约。 */
int main(void)
{
	xtemparena tArena;
	xtempinfo tInfo;
	xtempmark tOuter;
	xtempmark tInner;
	xtempmark tStale;
	xtempmark tCurrent;
	ptr pA1;
	ptr pA2;
	ptr pA3;
	ptr pB1;
	ptr pB2;
	ptr pB3;
	ptr pPromotedData;
	char* sOuter;
	char* sInner;
	char* sPromoted;
	const unsigned char arrData[] = { 1, 2, 3, 4 };
	#if defined(XRT_FEATURE_MEMORY_STATS)
		xmemstats tStats;
	#endif
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xmemdebugsnapshot tDebug;
	#endif

	memset(&tArena, 0, sizeof(tArena));
	#if defined(XRT_FEATURE_MEMORY_STATS)
		xrtMemStatsEnable(true);
		xrtMemStatsReset();
	#endif
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		testRequire(xrtMemDebugReset(), "initial memory debug reset failed");
	#endif
	testRequire(xrtTempInit(&tArena, NULL), "temp arena initialization failed");
	pA1 = xrtTempAlloc(&tArena, 1800);
	pA2 = xrtTempAlloc(&tArena, 1800);
	pA3 = xrtTempAlloc(&tArena, 1800);
	testRequire((pA1 != NULL) && (pA2 != NULL) && (pA3 != NULL), "regular temp allocation failed");
	xrtTempGet(&tArena, &tInfo);
	testRequire(tInfo.BlockCount == 2, "regular allocations should use two blocks");
	testRequire(tInfo.SpillCount == 0, "regular allocations must not use spill blocks");

	testRequire(xrtTempReset(&tArena), "temp reset failed");
	pB1 = xrtTempAlloc(&tArena, 1800);
	pB2 = xrtTempAlloc(&tArena, 1800);
	pB3 = xrtTempAlloc(&tArena, 1800);
	testRequire((pB1 == pA1) && (pB2 == pA2) && (pB3 == pA3), "reset did not reuse regular blocks");
	testRequire(xrtTempAlloc(&tArena, 3000) != NULL, "spill allocation failed");
	xrtTempGet(&tArena, &tInfo);
	testRequire(tInfo.SpillCount == 1, "large allocation did not use a spill block");
	testRequire(xrtTempReset(&tArena), "spill reset failed");
	xrtTempGet(&tArena, &tInfo);
	testRequire((tInfo.SpillCount == 0) && (tInfo.ResetCount == 2), "spill reset state mismatch");

	sOuter = (char*)xrtTempAlloc(&tArena, 64);
	memcpy(sOuter, "outer", 6);
	tOuter = xrtTempBegin(&tArena);
	sInner = (char*)xrtTempAlloc(&tArena, 128);
	memcpy(sInner, "inner", 6);
	tInner = xrtTempBegin(&tArena);
	testRequire(xrtTempAlloc(&tArena, 256) != NULL, "nested scope allocation failed");
	testRequire(!xrtTempEnd(&tOuter), "out-of-order scope end must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "out-of-order scope must report state error");
	xrtClearError();
	testRequire(xrtTempEnd(&tInner), "inner scope end failed");
	testRequire(strcmp(sInner, "inner") == 0, "inner parent data was not preserved");
	testRequire(xrtTempEnd(&tOuter), "outer scope end failed");
	testRequire(strcmp(sOuter, "outer") == 0, "pre-scope data was not preserved");

	tStale = xrtTempBegin(&tArena);
	tOuter = tStale;
	testRequire(xrtTempAlloc(&tArena, 32) != NULL, "stale mark fixture allocation failed");
	testRequire(xrtTempEnd(&tOuter), "stale mark fixture end failed");
	tCurrent = xrtTempBegin(&tArena);
	testRequire(xrtTempAlloc(&tArena, 32) != NULL, "replacement scope allocation failed");
	testRequire(!xrtTempEnd(&tStale), "copied stale mark must not end a later scope");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "stale mark must report state error");
	xrtClearError();
	testRequire(xrtTempEnd(&tCurrent), "replacement scope end failed");

	pPromotedData = xrtTempDup(&tArena, arrData, sizeof(arrData));
	testRequire((pPromotedData != NULL) &&
		(memcmp(pPromotedData, arrData, sizeof(arrData)) == 0), "temp duplicate mismatch");
	tOuter = xrtTempBegin(&tArena);
	sInner = xrtTempStr(&tArena, XRT_STR_LITERAL("promoted"));
	testRequire(sInner != NULL, "temporary string copy failed");
	sPromoted = xrtTempEndStr(&tOuter, (xstrview){ sInner, 8 });
	testRequire((sPromoted != NULL) && (strcmp(sPromoted, "promoted") == 0),
		"promoted string mismatch");
	tOuter = xrtTempBegin(&tArena);
	pPromotedData = xrtTempEndDup(&tOuter, arrData, sizeof(arrData));
	testRequire((pPromotedData != NULL) &&
		(memcmp(pPromotedData, arrData, sizeof(arrData)) == 0), "promoted data mismatch");
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xrtMemDebugSnapshot(&tDebug);
		testRequire(tDebug.TempCurrentBytes != 0, "temp debug current bytes missing");
		testRequire(tDebug.TempPeakBytes >= tDebug.TempCurrentBytes, "temp debug peak bytes mismatch");
	#endif

	testRequire(!xrtTempTrim(&tArena, 0), "active arena trim must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "active trim must report state error");
	xrtClearError();
	testRequire(xrtTempReset(&tArena), "final reset failed");
	testRequire(xrtTempTrim(&tArena, 0), "idle arena trim failed");
	xrtTempGet(&tArena, &tInfo);
	testRequire((tInfo.BlockCount == 0) && (tInfo.RetainedBytes == 0), "trim did not release regular blocks");
	#if defined(XRT_FEATURE_MEMORY_STATS)
		xrtMemStatsGet(&tStats);
		testRequire((tStats.TempCalls == 16) && (tStats.TempBytes == 14338), "temp statistics mismatch");
		xrtMemStatsEnable(false);
	#endif
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xrtMemDebugSnapshot(&tDebug);
		testRequire((tDebug.TempCurrentBytes == 0) && (tDebug.TempResetCount >= 3), "temp debug reset mismatch");
		testRequire(xrtMemDebugReset(), "final memory debug reset failed");
	#endif
	xrtTempUnit(&tArena);
	testTempSecure();
	printf("[PASS] temp\n");
	return 0;
}
