#include "../test.h"

#include <xrt/http_cache_store.h>



/* 每个线程反复替换自己的 Partition，并读取共享 Cache。 */
typedef struct test_http_cache_thread {
	xhttpcache* Cache;
	int Index;
} test_http_cache_thread;



/* 条件操作竞争夹具让两个线程在同一快照上同时提交。 */
typedef enum test_http_cache_race_operation {
	TEST_HTTP_CACHE_RACE_INSERT = 0,
	TEST_HTTP_CACHE_RACE_REPLACE,
	TEST_HTTP_CACHE_RACE_REMOVE
} test_http_cache_race_operation;



typedef struct test_http_cache_race {
	xhttpcache* Cache;
	const xhttpcacherecord* Expected;
	xhttpcacherecord* Records[2];
	xmutex Lock;
	uint32 Ready;
	uint32 Start;
	uint32 Succeeded;
	uint32 Conflicted;
	uint32 Failed;
	test_http_cache_race_operation Operation;
} test_http_cache_race;



typedef struct test_http_cache_race_thread {
	test_http_cache_race* Race;
	size_t Index;
} test_http_cache_race_thread;



/* 创建当前迭代的短正文完整 Record。 */
static xhttpcacherecord* testHttpCacheThreadRecord(
	xstrview Partition,
	xstrview Body,
	xtime iTime
)
{
	xhttpcachepart Part = {
		0, { (cbytes)Body.Data, Body.Size }
	};
	xhttpcachekey Key;
	xhttpcacherecordinput Input;

	if ( !xrtHttpCacheKeyInit(
		&Key,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/threaded")
	) ) {
		return NULL;
	}
	Key.Partition = Partition;
	if ( !xrtHttpCacheRecordInputInit(
		&Input, &Key, XHTTP_STATUS_OK
	) ) {
		return NULL;
	}
	Input.Flags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;
	Input.Parts = &Part;
	Input.PartCount = 1;
	Input.Length = Body.Size;
	Input.ResponseTime = iTime;
	Input.RequestClock = (uint64)iTime;
	Input.ResponseClock = (uint64)iTime + 1u;
	return xrtHttpCacheRecordCreate(&Input);
}



/* 执行共享 Cache 的并发替换、命中和引用释放循环。 */
static int32 testHttpCacheThreadEntry(ptr pData)
{
	test_http_cache_thread* pThread =
		(test_http_cache_thread*)pData;
	char Partition[32];
	char Body[64];
	size_t iPartition;
	size_t i;

	iPartition = (size_t)snprintf(
		Partition,
		sizeof(Partition),
		"worker-%d",
		pThread->Index
	);
	for ( i = 0; i < 800u; i++ ) {
		xhttpcacherecord* pRecord;
		xhttpcacherecord* pHit = NULL;
		xhttpcachekey Key;
		size_t iBody;
		xhttpcacheput Put;

		iBody = (size_t)snprintf(
			Body,
			sizeof(Body),
			"worker=%d iteration=%u",
			pThread->Index,
			(unsigned)i
		);
		pRecord = testHttpCacheThreadRecord(
			(xstrview){ Partition, iPartition },
			(xstrview){ Body, iBody },
			(xtime)(i + 1u)
		);
		if ( pRecord == NULL ) {
			return 1;
		}
		Put = xrtHttpCachePut(pThread->Cache, pRecord);
		xrtHttpCacheRecordRelease(pRecord);
		if ( (Put != XHTTP_CACHE_PUT_STORED) &&
			(Put != XHTTP_CACHE_PUT_REPLACED) ) {
			return 2;
		}
		if ( !xrtHttpCacheKeyInit(
			&Key,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL(
				"https://example.test/threaded"
			)
		) ) {
			return 3;
		}
		Key.Partition = (xstrview){
			Partition, iPartition
		};
		if ( xrtHttpCacheGet(
			pThread->Cache, &Key, &pHit
		) != XHTTP_CACHE_LOOKUP_HIT ) {
			return 4;
		}
		if ( (xrtHttpCacheRecordPartCount(pHit) != 1) ||
			(xrtHttpCacheRecordPartAt(pHit, 0) == NULL) ) {
			xrtHttpCacheRecordRelease(pHit);
			return 5;
		}
		xrtHttpCacheRecordRelease(pHit);
	}
	return 0;
}



/* 把条件操作结果归并为成功、冲突或异常三类。 */
static void testHttpCacheRaceResult(
	test_http_cache_race* pRace,
	bool bSucceeded,
	bool bConflicted
)
{
	testRequire(
		xrtMutexLock(&pRace->Lock),
		"HTTP cache race result lock failed"
	);
	if ( bSucceeded ) {
		pRace->Succeeded++;
	} else if ( bConflicted ) {
		pRace->Conflicted++;
	} else {
		pRace->Failed++;
	}
	testRequire(
		xrtMutexUnlock(&pRace->Lock),
		"HTTP cache race result unlock failed"
	);
}



/* 等待两个竞争线程都取得同一代快照后再同时放行。 */
static int32 testHttpCacheRaceEntry(ptr pData)
{
	test_http_cache_race_thread* pThread =
		(test_http_cache_race_thread*)pData;
	test_http_cache_race* pRace = pThread->Race;
	bool bStart = false;

	testRequire(
		xrtMutexLock(&pRace->Lock),
		"HTTP cache race barrier lock failed"
	);
	pRace->Ready++;
	testRequire(
		xrtMutexUnlock(&pRace->Lock),
		"HTTP cache race barrier unlock failed"
	);
	while ( !bStart ) {
		testRequire(
			xrtMutexLock(&pRace->Lock),
			"HTTP cache race start lock failed"
		);
		bStart = pRace->Start != 0;
		testRequire(
			xrtMutexUnlock(&pRace->Lock),
			"HTTP cache race start unlock failed"
		);
		xrtThreadYield();
	}

	if ( pRace->Operation ==
		TEST_HTTP_CACHE_RACE_INSERT ) {
		xhttpcacheput Put = xrtHttpCacheInsert(
			pRace->Cache,
			pRace->Records[pThread->Index]
		);

		testHttpCacheRaceResult(
			pRace,
			Put == XHTTP_CACHE_PUT_STORED,
			Put == XHTTP_CACHE_PUT_CONFLICT
		);
	} else if ( pRace->Operation ==
		TEST_HTTP_CACHE_RACE_REPLACE ) {
		xhttpcacheput Put = xrtHttpCacheReplace(
			pRace->Cache,
			pRace->Expected,
			pRace->Records[pThread->Index]
		);

		testHttpCacheRaceResult(
			pRace,
			Put == XHTTP_CACHE_PUT_REPLACED,
			Put == XHTTP_CACHE_PUT_CONFLICT
		);
	} else {
		xhttpcachechange Change =
			xrtHttpCacheRemoveRecord(
				pRace->Cache,
				pRace->Expected
			);

		testHttpCacheRaceResult(
			pRace,
			Change == XHTTP_CACHE_CHANGE_APPLIED,
			Change == XHTTP_CACHE_CHANGE_CONFLICT
		);
	}
	return 0;
}



/* 启动一对竞争线程，并要求条件提交只能有一个胜者。 */
static void testHttpCacheRaceRun(
	test_http_cache_race* pRace,
	test_http_cache_race_operation Operation
)
{
	test_http_cache_race_thread Contexts[2];
	xthread* Threads[2];
	xdeadline Deadline = xrtDeadlineAfter(5000000u);
	uint32 iReady = 0;
	size_t i;

	pRace->Ready = 0;
	pRace->Start = 0;
	pRace->Succeeded = 0;
	pRace->Conflicted = 0;
	pRace->Failed = 0;
	pRace->Operation = Operation;
	testRequire(
		xrtMutexInit(&pRace->Lock),
		"HTTP cache race mutex init failed"
	);
	for ( i = 0; i < 2u; i++ ) {
		Contexts[i].Race = pRace;
		Contexts[i].Index = i;
		Threads[i] = xrtThreadCreate(
			testHttpCacheRaceEntry,
			&Contexts[i],
			0
		);
		testRequire(
			Threads[i] != NULL,
			"HTTP cache race worker create failed"
		);
	}
	while ( iReady != 2u ) {
		testRequire(
			xrtMutexLock(&pRace->Lock),
			"HTTP cache race ready lock failed"
		);
		iReady = pRace->Ready;
		testRequire(
			xrtMutexUnlock(&pRace->Lock),
			"HTTP cache race ready unlock failed"
		);
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP cache race workers did not reach barrier"
		);
		xrtThreadYield();
	}
	testRequire(
		xrtMutexLock(&pRace->Lock),
		"HTTP cache race open lock failed"
	);
	pRace->Start = 1;
	testRequire(
		xrtMutexUnlock(&pRace->Lock),
		"HTTP cache race open unlock failed"
	);
	for ( i = 0; i < 2u; i++ ) {
		testRequire(
			(xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0),
			"HTTP cache race worker failed"
		);
		xrtThreadDestroy(Threads[i]);
	}
	testRequire(
		(pRace->Succeeded == 1u) &&
		(pRace->Conflicted == 1u) &&
		(pRace->Failed == 0),
		"HTTP cache conditional race was not atomic"
	);
	testRequire(
		xrtMutexUnit(&pRace->Lock),
		"HTTP cache race mutex unit failed"
	);
}



/* 验证同一变体上的 Insert、Replace 和 RemoveRecord 竞争。 */
static void testHttpCacheCurrentRace(void)
{
	xhttpcache* pCache = xrtHttpCacheCreate(NULL);
	xhttpcacherecord* pInsert[2];
	xhttpcacherecord* pReplace[2];
	xhttpcacherecord* pCurrent = NULL;
	xhttpcachekey Key;
	xhttpcachestats Stats;
	test_http_cache_race Race;
	size_t i;

	testRequire(
		pCache != NULL,
		"HTTP cache race create failed"
	);
	for ( i = 0; i < 2u; i++ ) {
		pInsert[i] = testHttpCacheThreadRecord(
			XRT_STR_LITERAL("race"),
			i == 0 ?
				XRT_STR_LITERAL("insert-a") :
				XRT_STR_LITERAL("insert-b"),
			(xtime)(i + 1u)
		);
		pReplace[i] = testHttpCacheThreadRecord(
			XRT_STR_LITERAL("race"),
			i == 0 ?
				XRT_STR_LITERAL("replace-a") :
				XRT_STR_LITERAL("replace-b"),
			(xtime)(i + 3u)
		);
		testRequire(
			(pInsert[i] != NULL) &&
			(pReplace[i] != NULL),
			"HTTP cache race record create failed"
		);
	}
	memset(&Race, 0, sizeof(Race));
	Race.Cache = pCache;
	Race.Records[0] = pInsert[0];
	Race.Records[1] = pInsert[1];
	testHttpCacheRaceRun(
		&Race,
		TEST_HTTP_CACHE_RACE_INSERT
	);

	testRequire(
		xrtHttpCacheKeyInit(
			&Key,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL(
				"https://example.test/threaded"
			)
		),
		"HTTP cache race key init failed"
	);
	Key.Partition = XRT_STR_LITERAL("race");
	testRequire(
		(xrtHttpCacheGet(
			pCache,
			&Key,
			&pCurrent
		 ) == XHTTP_CACHE_LOOKUP_HIT) &&
		((pCurrent == pInsert[0]) ||
		 (pCurrent == pInsert[1])),
		"HTTP cache insert race lost its winner"
	);

	Race.Expected = pCurrent;
	Race.Records[0] = pReplace[0];
	Race.Records[1] = pReplace[1];
	testHttpCacheRaceRun(
		&Race,
		TEST_HTTP_CACHE_RACE_REPLACE
	);
	xrtHttpCacheRecordRelease(pCurrent);
	pCurrent = NULL;
	testRequire(
		(xrtHttpCacheGet(
			pCache,
			&Key,
			&pCurrent
		 ) == XHTTP_CACHE_LOOKUP_HIT) &&
		((pCurrent == pReplace[0]) ||
		 (pCurrent == pReplace[1])),
		"HTTP cache replace race lost its winner"
	);

	Race.Expected = pCurrent;
	testHttpCacheRaceRun(
		&Race,
		TEST_HTTP_CACHE_RACE_REMOVE
	);
	xrtHttpCacheRecordRelease(pCurrent);
	pCurrent = NULL;
	testRequire(
		(xrtHttpCacheGet(
			pCache,
			&Key,
			&pCurrent
		 ) == XHTTP_CACHE_LOOKUP_MISS) &&
		(pCurrent == NULL) &&
		xrtHttpCacheStats(pCache, &Stats) &&
		(Stats.Entries == 0) &&
		(Stats.Stores == 2) &&
		(Stats.Replacements == 1) &&
		(Stats.Conflicts == 3) &&
		(Stats.Removals == 1),
		"HTTP cache conditional race statistics mismatch"
	);

	for ( i = 0; i < 2u; i++ ) {
		xrtHttpCacheRecordRelease(pReplace[i]);
		xrtHttpCacheRecordRelease(pInsert[i]);
	}
	xrtHttpCacheRelease(pCache);
}



/* 验证共享缓存的 Mutex、替换和锁外快照生命周期。 */
int main(void)
{
	xhttpcache* pCache = xrtHttpCacheCreate(NULL);
	test_http_cache_thread Contexts[4];
	xthread* Threads[4];
	xhttpcachestats Stats;
	size_t i;

	testRequire(pCache != NULL,
		"HTTP cache thread create failed");
	for ( i = 0; i < 4u; i++ ) {
		Contexts[i].Cache = pCache;
		Contexts[i].Index = (int)i;
		Threads[i] = xrtThreadCreate(
			testHttpCacheThreadEntry,
			&Contexts[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"HTTP cache worker create failed");
	}
	for ( i = 0; i < 4u; i++ ) {
		testRequire(
			(xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0),
			"HTTP cache worker failed"
		);
		xrtThreadDestroy(Threads[i]);
	}
	testRequire(
		xrtHttpCacheStats(pCache, &Stats) &&
		(Stats.Entries == 4) &&
		(Stats.Stores == 3200) &&
		(Stats.Replacements == 3196) &&
		(Stats.Hits == 3200),
		"HTTP cache concurrent stats mismatch"
	);
	xrtHttpCacheRelease(pCache);
	testHttpCacheCurrentRace();
	printf("[PASS] HTTP cache store threads\n");
	return 0;
}
