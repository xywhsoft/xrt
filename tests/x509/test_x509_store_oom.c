#include "../test.h"
#include "../fixtures/x509_path_legacy.h"



typedef struct test_x509_store_allocator {
	size_t Calls;
	size_t Allow;
} test_x509_store_allocator;



/* 只允许前几次分配成功，随后稳定注入 OOM。 */
static ptr testX509StoreAlloc(ptr pContext, size_t iSize)
{
	test_x509_store_allocator* pState =
		(test_x509_store_allocator*)pContext;

	if ( pState->Calls >= pState->Allow ) {
		return NULL;
	}
	pState->Calls++;
	return malloc(iSize);
}



/* 本测试没有需要成功的重分配路径。 */
static ptr testX509StoreRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	(void)pContext;
	(void)pMemory;
	(void)iSize;
	return NULL;
}



/* 释放由测试分配器前几次请求产生的内存。 */
static void testX509StoreFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证 DER 和 PEM 导入在 OOM 下不改变信任库与输出。 */
int main(void)
{
	static const char Pem[] =
		"-----BEGIN CERTIFICATE-----\n"
		"QQ==\n"
		"-----END CERTIFICATE-----\n";
	test_x509_store_allocator State = { 0, 2u };
	xallocator Allocator;
	xx509store* pStore;
	size_t iAdded = 77;

	Allocator.Context = &State;
	Allocator.Alloc = testX509StoreAlloc;
	Allocator.Realloc = testX509StoreRealloc;
	Allocator.Free = testX509StoreFree;
	testRequire(xrtSetAllocator(&Allocator),
		"X.509 store staged allocator install failed");
	pStore = xrtX509StoreCreate();
	testRequire(pStore != NULL, "X.509 store OOM fixture creation failed");
	testRequire((xrtX509StoreAdd(
		pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
	) == X509_ERROR) && (xrtX509StoreCount(pStore) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"X.509 store DER OOM changed the store");
	xrtClearError();
	testRequire(!xrtX509StoreAddPem(
		pStore, Pem, sizeof(Pem) - 1u, &iAdded
	) && (iAdded == 77) && (xrtX509StoreCount(pStore) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"X.509 store PEM OOM changed the store");
	xrtX509StoreFree(pStore);
	printf("[PASS] x509_store_oom\n");
	return 0;
}
