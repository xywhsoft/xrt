#include "../test.h"



/* 一个操作在同一把外部锁下同时更新基础容器和固定对象池。 */
typedef struct testexternalsyncitem {
	int Key;
	int Value;
} testexternalsyncitem;



/* 基础容器本身保持无锁，组合共享策略由这一层显式表达。 */
typedef struct testexternalsyncstate {
	xmutex Lock;
	xarray Array;
	xmap Map;
	xintmap IntMap;
	xavltree Tree;
	xpool Pool;
} testexternalsyncstate;



/* 每个工作者使用互不重叠的键区间。 */
typedef struct testexternalsyncworker {
	testexternalsyncstate* State;
	int Base;
} testexternalsyncworker;



/* 按整数键比较查找键和树内对象。 */
static int testExternalSyncCompare(
	const void* pKey,
	const void* pItem,
	ptr pUserData
)
{
	int iKey = *(const int*)pKey;
	int iItemKey = ((const testexternalsyncitem*)pItem)->Key;

	(void)pUserData;
	if ( iKey < iItemKey ) {
		return -1;
	}
	if ( iKey > iItemKey ) {
		return 1;
	}
	return 0;
}



/* 在锁内完成一次组合更新，并保证临时池对象也在锁内归还。 */
static bool testExternalSyncStep(testexternalsyncstate* pState, int iKey)
{
	testexternalsyncitem Item;
	xbytesview Key;
	int* pPooled;
	bool bOk;

	if ( !xrtMutexLock(&pState->Lock) ) {
		return false;
	}

	Item.Key = iKey;
	Item.Value = iKey + 1;
	Key.Data = (cbytes)&iKey;
	Key.Size = sizeof(iKey);
	bOk =
		xrtArrayPush(&pState->Array, &Item) &&
		xrtMapSet(&pState->Map, Key, &Item.Value) &&
		xrtIntMapSet(&pState->IntMap, (int64)iKey, &Item.Value) &&
		(xrtAVLTreeAdd(&pState->Tree, &Item.Key, &Item, NULL) != NULL);

	pPooled = bOk ? (int*)xrtPoolAlloc(&pState->Pool) : NULL;
	if ( pPooled != NULL ) {
		*pPooled = iKey;
		bOk = xrtPoolFree(&pState->Pool, pPooled);
	} else {
		bOk = false;
	}

	if ( !xrtMutexUnlock(&pState->Lock) ) {
		return false;
	}
	return bOk;
}



/* 并发工作者反复走同一个外部同步组合入口。 */
static int32 testExternalSyncWorker(ptr pData)
{
	testexternalsyncworker* pWorker = (testexternalsyncworker*)pData;

	for ( int i = 0; i < 64; i++ ) {
		if ( !testExternalSyncStep(pWorker->State, pWorker->Base + i) ) {
			return 1;
		}
		xrtThreadYield();
	}
	return 0;
}



/* 验证外部同步替代旧容器 owner/shared 隐式锁模式。 */
int main(void)
{
	testexternalsyncstate State;
	testexternalsyncworker arrWorker[3];
	xthread* arrThread[3] = { NULL, NULL, NULL };
	xpoolinfo PoolInfo;
	size_t iExpected = 256u;

	memset(&State, 0, sizeof(State));
	testRequire(xrtMutexInit(&State.Lock), "external sync mutex init failed");
	testRequire(xrtArrayInit(&State.Array, sizeof(testexternalsyncitem)), "external sync array init failed");
	testRequire(xrtMapInit(&State.Map, sizeof(int)), "external sync map init failed");
	testRequire(xrtIntMapInit(&State.IntMap, sizeof(int)), "external sync int map init failed");
	testRequire(
		xrtAVLTreeInit(
			&State.Tree,
			sizeof(testexternalsyncitem),
			testExternalSyncCompare,
			NULL
		),
		"external sync AVL tree init failed"
	);
	testRequire(xrtPoolInit(&State.Pool, sizeof(int)), "external sync pool init failed");

	for ( int i = 0; i < 3; i++ ) {
		arrWorker[i].State = &State;
		arrWorker[i].Base = (i + 1) * 1000;
		arrThread[i] = xrtThreadCreate(testExternalSyncWorker, &arrWorker[i], 0);
		testRequire(arrThread[i] != NULL, "external sync worker create failed");
	}

	for ( int i = 0; i < 64; i++ ) {
		testRequire(testExternalSyncStep(&State, i + 1), "external sync main step failed");
		xrtThreadYield();
	}
	for ( int i = 0; i < 3; i++ ) {
		testRequire(xrtThreadWait(arrThread[i]) == XWAIT_OK, "external sync worker wait failed");
		testRequire(xrtThreadExitCode(arrThread[i]) == 0, "external sync worker operation failed");
		xrtThreadDestroy(arrThread[i]);
	}

	xrtPoolGet(&State.Pool, &PoolInfo);
	testRequire(State.Array.Count == iExpected, "external sync array count mismatch");
	testRequire(xrtMapCount(&State.Map) == iExpected, "external sync map count mismatch");
	testRequire(xrtIntMapCount(&State.IntMap) == iExpected, "external sync int map count mismatch");
	testRequire(xrtAVLTreeCount(&State.Tree) == iExpected, "external sync AVL tree count mismatch");
	testRequire(PoolInfo.LiveCount == 0u, "external sync pool retained live objects");
	testRequire(PoolInfo.AllocCount == iExpected, "external sync pool allocation count mismatch");
	testRequire(PoolInfo.FreeCount == iExpected, "external sync pool free count mismatch");

	xrtPoolUnit(&State.Pool);
	xrtAVLTreeUnit(&State.Tree);
	xrtIntMapUnit(&State.IntMap);
	xrtMapUnit(&State.Map);
	xrtArrayUnit(&State.Array);
	testRequire(xrtMutexUnit(&State.Lock), "external sync mutex unit failed");
	return 0;
}
