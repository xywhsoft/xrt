#include "../test.h"



typedef struct test_x509_store_system_allocator {
	size_t Calls;
	bool Armed;
	bool Failed;
} test_x509_store_system_allocator;



/* 武装后只拒绝系统信任枚举遇到的第一份新内存。 */
static ptr testX509StoreSystemAlloc(ptr pContext, size_t iSize)
{
	test_x509_store_system_allocator* pState =
		(test_x509_store_system_allocator*)pContext;

	pState->Calls++;
	if ( pState->Armed && !pState->Failed ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配与普通分配共享同一个单次故障点。 */
static ptr testX509StoreSystemRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_x509_store_system_allocator* pState =
		(test_x509_store_system_allocator*)pContext;

	pState->Calls++;
	if ( pState->Armed && !pState->Failed ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器成功创建的全部底层内存。 */
static void testX509StoreSystemFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 系统枚举中任意 OOM 都必须回滚全部锚并保持输出不变。 */
int main(void)
{
	static test_x509_store_system_allocator State;
	xallocator Allocator;
	xx509store* pStore;
	size_t iAdded = 77u;

	Allocator.Context = &State;
	Allocator.Alloc = testX509StoreSystemAlloc;
	Allocator.Realloc = testX509StoreSystemRealloc;
	Allocator.Free = testX509StoreSystemFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"system X.509 store allocator install failed"
	);
	pStore = xrtX509StoreCreate();
	testRequire(
		pStore != NULL,
		"system X.509 OOM store creation failed"
	);

	/* 空 Store 稳定存在后才把故障边界移入系统枚举。 */
	State.Armed = true;
	testRequire(
		!xrtX509StoreAddSystem(pStore, &iAdded) &&
		(iAdded == 77u) && (xrtX509StoreCount(pStore) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		State.Failed &&
		(State.Calls != 0),
		"system X.509 store OOM violated failure atomicity"
	);
	xrtX509StoreFree(pStore);
	printf("[PASS] x509_store_system_oom\n");
	return 0;
}
