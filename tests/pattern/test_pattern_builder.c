#include "../test.h"



/* 验证追加编译不改变旧快照，并在无修改时复用缓存。 */
static void testPatternBuilderSnapshots(void)
{
	xpatternbuilder* pBuilder = xrtPatternBuilderCreate();
	xpatternspec Spec;
	xpatternid iFirst;
	xpatternid iSecond;
	xpattern* pV1;
	xpattern* pSame;
	xpattern* pV2;
	xpatternmatch Match;
	uint64 iVersion;

	testRequire(pBuilder != NULL, "pattern builder create failed");
	memset(&Spec, 0, sizeof(Spec));
	Spec.Pattern = XRT_STR_LITERAL("/first/{id}");
	Spec.Value = (ptr)(uintptr_t)1u;
	iFirst = xrtPatternBuilderAdd(pBuilder, &Spec);
	testRequire(iFirst != XPATTERN_ID_INVALID, "first builder add failed");
	testRequire(xrtPatternBuilderCount(pBuilder) == 1u, "builder count mismatch");
	testRequire(xrtPatternBuilderDirty(pBuilder), "new builder entry is not dirty");
	iVersion = xrtPatternBuilderVersion(pBuilder);
	pV1 = xrtPatternBuilderCompile(pBuilder);
	testRequire(pV1 != NULL, "first builder compile failed");
	testRequire(!xrtPatternBuilderDirty(pBuilder), "compiled builder remained dirty");
	pSame = xrtPatternBuilderCompile(pBuilder);
	testRequire(pSame == pV1, "unchanged builder did not reuse cached snapshot");
	xrtPatternRelease(pSame);

	Spec.Pattern = XRT_STR_LITERAL("/second/pre{name}suf");
	Spec.Value = (ptr)(uintptr_t)2u;
	iSecond = xrtPatternBuilderAdd(pBuilder, &Spec);
	testRequire(iSecond != XPATTERN_ID_INVALID, "second builder add failed");
	testRequire(
		xrtPatternBuilderVersion(pBuilder) == iVersion + 1u,
		"builder version did not advance"
	);
	pV2 = xrtPatternBuilderCompile(pBuilder);
	testRequire(pV2 != NULL && pV2 != pV1, "changed builder reused stale snapshot");
	testRequire(
		xrtPatternTest(pV1, XRT_STR_LITERAL("/second/prevaluesuf")) ==
			XPATTERN_NONE,
		"old snapshot observed a later append"
	);
	testRequire(
		xrtPatternLookup(pV2, XRT_STR_LITERAL("/second/prevaluesuf"), &Match) ==
			XPATTERN_MATCH && Match.Id == iSecond,
		"new snapshot missed appended pattern"
	);
	testRequire(
		xrtPatternLookup(pV2, XRT_STR_LITERAL("/first/7"), &Match) ==
			XPATTERN_MATCH && Match.Id == iFirst,
		"new snapshot lost original pattern"
	);
	xrtPatternRelease(pV1);
	xrtPatternRelease(pV2);
	xrtPatternBuilderFree(pBuilder);
}



/* 验证 Set、Remove、槽代际和 Clear 不会令陈旧 ID 重新生效。 */
static void testPatternBuilderMutation(void)
{
	xpatternbuilder* pBuilder = xrtPatternBuilderCreate();
	xpatternspec Spec;
	xpatternid Id;
	xpatternid iReused;
	xpattern* pPattern;
	xpatternmatch Match;

	testRequire(pBuilder != NULL, "mutation builder create failed");
	memset(&Spec, 0, sizeof(Spec));
	Spec.Pattern = XRT_STR_LITERAL("/old");
	Spec.Value = (ptr)(uintptr_t)1u;
	Id = xrtPatternBuilderAdd(pBuilder, &Spec);
	testRequire(Id != XPATTERN_ID_INVALID, "mutation builder add failed");
	Spec.Pattern = XRT_STR_LITERAL("/new/{id}");
	Spec.Value = (ptr)(uintptr_t)2u;
	testRequire(xrtPatternBuilderSet(pBuilder, Id, &Spec), "builder set failed");
	pPattern = xrtPatternBuilderCompile(pBuilder);
	testRequire(pPattern != NULL, "mutation builder compile failed");
	testRequire(
		xrtPatternTest(pPattern, XRT_STR_LITERAL("/old")) == XPATTERN_NONE &&
		xrtPatternLookup(pPattern, XRT_STR_LITERAL("/new/4"), &Match) ==
			XPATTERN_MATCH && Match.Id == Id,
		"builder set changed ID or retained old source"
	);
	xrtPatternRelease(pPattern);
	testRequire(xrtPatternBuilderRemove(pBuilder, Id), "builder remove failed");
	testRequire(!xrtPatternBuilderRemove(pBuilder, Id), "stale ID removed twice");
	Spec.Pattern = XRT_STR_LITERAL("/reused");
	iReused = xrtPatternBuilderAdd(pBuilder, &Spec);
	testRequire(
		(iReused != XPATTERN_ID_INVALID) && (iReused != Id),
		"reused slot did not change generation"
	);
	testRequire(!xrtPatternBuilderSet(pBuilder, Id, &Spec), "stale ID replaced new slot");
	xrtPatternBuilderClear(pBuilder);
	testRequire(xrtPatternBuilderCount(pBuilder) == 0, "builder clear kept entries");
	testRequire(!xrtPatternBuilderRemove(pBuilder, iReused), "clear kept ID valid");
	pPattern = xrtPatternBuilderCompile(pBuilder);
	testRequire(
		pPattern != NULL && xrtPatternCount(pPattern) == 0,
		"cleared builder did not compile to empty snapshot"
	);
	xrtPatternRelease(pPattern);
	xrtPatternBuilderFree(pBuilder);
}



/* 批量追加在任意模式无效时必须完全回滚。 */
static void testPatternBuilderBatch(void)
{
	xpatternbuilder* pBuilder = xrtPatternBuilderCreate();
	xpatternspec arrSpec[3];
	xpatternid arrId[3];

	testRequire(pBuilder != NULL, "batch builder create failed");
	memset(arrSpec, 0, sizeof(arrSpec));
	arrSpec[0].Pattern = XRT_STR_LITERAL("/a");
	arrSpec[1].Pattern = XRT_STR_LITERAL("/{bad-name}");
	arrSpec[2].Pattern = XRT_STR_LITERAL("/c");
	testRequire(
		!xrtPatternBuilderAddMany(pBuilder, arrSpec, 3u, arrId),
		"invalid builder batch was accepted"
	);
	testRequire(
		xrtPatternBuilderCount(pBuilder) == 0,
		"failed builder batch partially committed"
	);
	arrSpec[1].Pattern = XRT_STR_LITERAL("/b/{name}");
	testRequire(
		xrtPatternBuilderAddMany(pBuilder, arrSpec, 3u, arrId),
		"valid builder batch failed"
	);
	testRequire(
		xrtPatternBuilderCount(pBuilder) == 3u &&
		arrId[0] != XPATTERN_ID_INVALID &&
		arrId[1] != XPATTERN_ID_INVALID &&
		arrId[2] != XPATTERN_ID_INVALID,
		"valid builder batch result mismatch"
	);
	xrtPatternBuilderFree(pBuilder);
}



/* 编译期冲突不能破坏已发布快照；修正 Builder 后可以继续生成新快照。 */
static void testPatternBuilderCompileFailure(void)
{
	xpatternbuilder* pBuilder = xrtPatternBuilderCreate();
	xpatternspec Spec;
	xpatternid iSecond;
	xpattern* pStable;
	xpattern* pFailed;
	xpattern* pRecovered;

	testRequire(pBuilder != NULL, "failure builder create failed");
	memset(&Spec, 0, sizeof(Spec));
	Spec.Pattern = XRT_STR_LITERAL("/same/{first}");
	testRequire(
		xrtPatternBuilderAdd(pBuilder, &Spec) != XPATTERN_ID_INVALID,
		"failure builder first add failed"
	);
	pStable = xrtPatternBuilderCompile(pBuilder);
	testRequire(pStable != NULL, "stable builder snapshot compile failed");
	Spec.Pattern = XRT_STR_LITERAL("/same/{second}");
	iSecond = xrtPatternBuilderAdd(pBuilder, &Spec);
	testRequire(iSecond != XPATTERN_ID_INVALID, "conflicting builder add failed");
	pFailed = xrtPatternBuilderCompile(pBuilder);
	testRequire(pFailed == NULL, "conflicting builder unexpectedly compiled");
	testRequire(
		xrtErrorCode(xrtGetError()) == XPATTERN_ERROR_CONFLICT &&
		xrtPatternBuilderDirty(pBuilder),
		"failed compile lost conflict or dirty state"
	);
	testRequire(
		xrtPatternTest(pStable, XRT_STR_LITERAL("/same/value")) ==
			XPATTERN_MATCH,
		"failed compile damaged the old snapshot"
	);
	Spec.Priority = 1;
	testRequire(
		xrtPatternBuilderSet(pBuilder, iSecond, &Spec),
		"conflicting builder repair failed"
	);
	pRecovered = xrtPatternBuilderCompile(pBuilder);
	testRequire(pRecovered != NULL, "repaired builder did not compile");
	testRequire(
		!xrtPatternBuilderDirty(pBuilder),
		"repaired builder remained dirty"
	);
	xrtPatternRelease(pStable);
	xrtPatternRelease(pRecovered);
	xrtPatternBuilderFree(pBuilder);
}



int main(void)
{
	testPatternBuilderSnapshots();
	testPatternBuilderMutation();
	testPatternBuilderBatch();
	testPatternBuilderCompileFailure();
	printf("[PASS] pattern builder\n");
	return 0;
}
