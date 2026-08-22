#ifndef XRT_TEST_FAULT_ALLOCATOR_H
#define XRT_TEST_FAULT_ALLOCATOR_H



/* 故障注入分配器记录调用、命中点和仍存活的底层块。 */
typedef struct testfaultallocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
	bool Hit;
} testfaultallocator;



/* 在指定分配序号失败，其余请求交给 C 运行库。 */
static ptr testFaultAlloc(ptr pContext, size_t iSize)
{
	testfaultallocator* pState = (testfaultallocator*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Hit = true;
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 重分配失败时保留原块，成功创建新块时更新存活计数。 */
static ptr testFaultRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testfaultallocator* pState = (testfaultallocator*)pContext;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Hit = true;
		return NULL;
	}
	if ( iSize == 0 ) {
		if ( pMemory != NULL ) {
			testRequire(
				pState->Live != 0,
				"fault allocator live counter underflow"
			);
			pState->Live--;
		}
		free(pMemory);
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放底层块并维护存活计数。 */
static void testFaultFree(ptr pContext, ptr pMemory)
{
	testfaultallocator* pState = (testfaultallocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(
		pState->Live != 0,
		"fault allocator live counter underflow"
	);
	pState->Live--;
	free(pMemory);
}



/* 构造使用故障注入状态的 XRT 分配器。 */
static xallocator testFaultAllocator(testfaultallocator* pState)
{
	xallocator Allocator;

	Allocator.Context = pState;
	Allocator.Alloc = testFaultAlloc;
	Allocator.Realloc = testFaultRealloc;
	Allocator.Free = testFaultFree;
	return Allocator;
}

#endif
