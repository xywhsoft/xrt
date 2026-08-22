#include "../test.h"
#include "../fixtures/x509_path_legacy.h"

#include <stdio.h>



#define TEST_X509_STORE_FILE_OOM_SIZE (64u * 1024u)



typedef struct test_x509_store_file_allocator {
	size_t Calls;
	bool Armed;
	bool Failed;
} test_x509_store_file_allocator;



/* 武装后只拒绝第一份达到测试文件规模的直接分配。 */
static ptr testX509StoreFileAlloc(ptr pContext, size_t iSize)
{
	test_x509_store_file_allocator* pState =
		(test_x509_store_file_allocator*)pContext;

	pState->Calls++;
	if ( pState->Armed && !pState->Failed &&
		(iSize >= TEST_X509_STORE_FILE_OOM_SIZE) ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配与普通分配共享同一语义故障点。 */
static ptr testX509StoreFileRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_x509_store_file_allocator* pState =
		(test_x509_store_file_allocator*)pContext;

	pState->Calls++;
	if ( pState->Armed && !pState->Failed &&
		(iSize >= TEST_X509_STORE_FILE_OOM_SIZE) ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器已经成功分配的对象。 */
static void testX509StoreFileFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 文件层 OOM 必须包装原始原因并保持信任库和输出不变。 */
int main(void)
{
	static const char Path[] = "xrt-x509-store-file-oom.der";
	static test_x509_store_file_allocator State;
	xallocator Allocator;
	xx509store* pStore;
	FILE* pFile = fopen(Path, "wb");
	size_t iAdded = 77u;

	/*
		保留有效 DER 前缀并扩展文件，确保整文件读取使用非池化缓冲。
		故障发生在解析前，因此尾部填充不会进入证书协议路径。
	*/
	testRequire(
		(pFile != NULL) &&
		(fwrite(
			X509_PATH_ROOT,
			1u,
			sizeof(X509_PATH_ROOT),
			pFile
		) == sizeof(X509_PATH_ROOT)) &&
		(fseek(
			pFile,
			(long)TEST_X509_STORE_FILE_OOM_SIZE - 1l,
			SEEK_SET
		) == 0) &&
		(fputc(0, pFile) != EOF) &&
		(fclose(pFile) == 0),
		"X.509 store file OOM fixture creation failed"
	);
	Allocator.Context = &State;
	Allocator.Alloc = testX509StoreFileAlloc;
	Allocator.Realloc = testX509StoreFileRealloc;
	Allocator.Free = testX509StoreFileFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"X.509 store file allocator install failed"
	);
	pStore = xrtX509StoreCreate();
	testRequire(
		pStore != NULL,
		"X.509 store file OOM store creation failed"
	);

	/* Store 已稳定存在后才武装文件缓冲故障。 */
	State.Armed = true;
	testRequire(
		!xrtX509StoreAddFile(pStore, Path, &iAdded) &&
		(iAdded == 77u) && (xrtX509StoreCount(pStore) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		State.Failed &&
		(State.Calls != 0),
		"X.509 store file OOM changed state or error category"
	);
	xrtX509StoreFree(pStore);
	(void)remove(Path);
	printf("[PASS] x509_store_file_oom\n");
	return 0;
}
