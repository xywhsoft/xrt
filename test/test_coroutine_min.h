#ifndef TEST_COROUTINE_MIN_H
#define TEST_COROUTINE_MIN_H

#include "../xrt.h"

static int __g_test_coroutine_min_done = 0;


// 内部函数：__Test_CoroutineMin_Main
static void __Test_CoroutineMin_Main(ptr pArg)
{
	(void)pArg;
	__g_test_coroutine_min_done = 1;
}


// 协程最小集测试
static int Test_CoroutineMin(void)
{
	xcosched* pSched = NULL;

	__g_test_coroutine_min_done = 0;

	pSched = xrtCoSchedCreate();
	if ( !pSched ) {
		return 11;
	}

	if ( !xrtCoSchedSpawn(pSched, __Test_CoroutineMin_Main, NULL, 0u) ) {
		xrtCoSchedDestroy(pSched);
		return 12;
	}

	xrtCoSchedRun(pSched);
	xrtCoSchedDestroy(pSched);

	return __g_test_coroutine_min_done ? 0 : 13;
}

#endif
