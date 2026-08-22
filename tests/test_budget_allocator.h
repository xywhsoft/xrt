#ifndef XRT_TEST_BUDGET_ALLOCATOR_H
#define XRT_TEST_BUDGET_ALLOCATOR_H

#include "test.h"



/* 分配预算用于精确触发第 N 次底层内存申请失败。 */
typedef struct testbudgetallocator {
	size_t Allow;
	size_t Denied;
} testbudgetallocator;



/* 在预算耗尽前使用系统分配器，耗尽后拒绝申请。 */
static ptr testBudgetAlloc(ptr pContext, size_t iSize)
{
	testbudgetallocator* pState = (testbudgetallocator*)pContext;

	if ( pState->Allow == 0u ) {
		pState->Denied++;
		return NULL;
	}

	pState->Allow--;
	return malloc(iSize);
}



/* 重分配与普通分配共享同一故障预算。 */
static ptr testBudgetRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testbudgetallocator* pState = (testbudgetallocator*)pContext;

	if ( pState->Allow == 0u ) {
		pState->Denied++;
		return NULL;
	}

	pState->Allow--;
	return realloc(pMemory, iSize);
}



/* 释放故障窗口前已成功取得的系统内存。 */
static void testBudgetFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 安装带有指定成功次数的进程级测试分配器。 */
static bool testInstallBudgetAllocator(
	testbudgetallocator* pState,
	size_t iAllow
)
{
	xallocator Allocator;

	pState->Allow = iAllow;
	pState->Denied = 0u;
	Allocator.Context = pState;
	Allocator.Alloc = testBudgetAlloc;
	Allocator.Realloc = testBudgetRealloc;
	Allocator.Free = testBudgetFree;
	return xrtSetAllocator(&Allocator);
}

#endif
