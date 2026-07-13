static int64 __Test_ValueRecursiveGraphFibDict(int iN, xvalue pParent)
{
	xvalue pFrame;
	xvalue pN;
	int64 iLeft;
	int64 iRight;

	if ( iN < 2 ) {
		xvoUnref(pParent);
		return iN;
	}

	// 每层建立一个指向父层的字典，模拟解释器作用域和 AST 父子关系。
	pFrame = xvoCreateTable();
	if ( pFrame == NULL ) {
		xvoUnref(pParent);
		return -1;
	}
	pN = xvoCreateInt(iN);
	if ( pN == NULL || !xvoTableSetValue(pFrame, "n", 1, pN, FALSE) ) {
		xvoUnref(pN);
		xvoUnref(pFrame);
		xvoUnref(pParent);
		return -1;
	}
	xvoUnref(pN);
	if ( !xvoTableSetValue(pFrame, "parent", 6, pParent, FALSE) ) {
		xvoUnref(pFrame);
		xvoUnref(pParent);
		return -1;
	}

	// 递归函数取得实参所有权，因此两个调用各传递一个独立引用。
	xvoAddRef(pFrame);
	iLeft = __Test_ValueRecursiveGraphFibDict(iN - 1, pFrame);
	xvoAddRef(pFrame);
	iRight = __Test_ValueRecursiveGraphFibDict(iN - 2, pFrame);

	xvoUnref(pFrame);
	xvoUnref(pParent);
	if ( iLeft < 0 || iRight < 0 ) {
		return -1;
	}
	return iLeft + iRight;
}


static int __Test_ValueRecursiveGraphRound(
	int iN,
	int iRepeats,
	xrtMemTelemetrySnapshot* pSnapshot,
	double* pElapsed)
{
	int i;
	int64 iResult = 0;
	double fStart;

	xrtMemTelemetryReset();
	fStart = xrtTimer();
	for ( i = 0; i < iRepeats; i++ ) {
		iResult = __Test_ValueRecursiveGraphFibDict(iN, xvoCreateNull());
		if ( iResult < 0 ) {
			return FALSE;
		}
	}
	*pElapsed = xrtTimer() - fStart;
	memset(pSnapshot, 0, sizeof(*pSnapshot));
	xrtMemTelemetryGetSnapshot(pSnapshot);
	return iResult == 6765;
}


static int Test_ValueRecursiveGraph(xrtGlobalData* pCore)
{
	xrtMemTelemetrySnapshot tFirst;
	xrtMemTelemetrySnapshot tSecond;
	double fFirst;
	double fSecond;
	int iFailed = 0;

	(void)pCore;
	printf("[value_recursive_graph] begin\n");
	xrtMemTelemetryEnable(TRUE);

	if ( !__Test_ValueRecursiveGraphRound(20, 12, &tFirst, &fFirst) ) {
		printf("[value_recursive_graph] FAIL: first result\n");
		iFailed++;
	}
	if ( !__Test_ValueRecursiveGraphRound(20, 12, &tSecond, &fSecond) ) {
		printf("[value_recursive_graph] FAIL: second result\n");
		iFailed++;
	}

	// 固定内存池的底层分配不全部进入公开 malloc 计数，不能用 alloc-free
	// 推导存活对象数；这里比较两轮完整画像，确保后轮没有持续积累资源。
	if ( tFirst.iMallocCalls != tSecond.iMallocCalls ||
		 tFirst.iCallocCalls != tSecond.iCallocCalls ||
		 tFirst.iReallocCalls != tSecond.iReallocCalls ||
		 tFirst.iFreeCalls != tSecond.iFreeCalls ) {
		printf("[value_recursive_graph] FAIL: allocation profile changed between rounds\n");
		iFailed++;
	}
	if ( fSecond > (fFirst * 4.0) && fSecond > 1.0 ) {
		printf("[value_recursive_graph] FAIL: second round degraded unexpectedly\n");
		iFailed++;
	}

	printf("[value_recursive_graph] first=%.3fms second=%.3fms alloc=%llu free=%llu\n",
		fFirst * 1000.0,
		fSecond * 1000.0,
		(unsigned long long)tSecond.iMallocCalls,
		(unsigned long long)tSecond.iFreeCalls);
	xrtMemTelemetryReset();
	xrtMemTelemetryEnable(FALSE);
	printf("[value_recursive_graph] %s\n", iFailed == 0 ? "PASS" : "FAIL");
	return iFailed == 0 ? 0 : 1;
}
