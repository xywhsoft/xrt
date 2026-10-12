#include "../test.h"



/* 单点故障注入状态。 */
typedef struct test_http_static_multipart_body_oom {
	size_t Calls;
	size_t FailAt;
} test_http_static_multipart_body_oom;



/* 在指定分配调用拒绝一次请求。 */
static ptr testHttpStaticMultipartBodyOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_static_multipart_body_oom* pState =
		(test_http_static_multipart_body_oom*)pData;

	if ( pState->Calls++ == pState->FailAt ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 在指定重分配调用拒绝一次请求。 */
static ptr testHttpStaticMultipartBodyOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_static_multipart_body_oom* pState =
		(test_http_static_multipart_body_oom*)pData;

	if ( pState->Calls++ == pState->FailAt ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放故障注入分配器创建的内存。 */
static void testHttpStaticMultipartBodyOomFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	free(pMemory);
}



/* 在当前目录创建用于资源所有权测试的短文件。 */
static void testHttpStaticMultipartBodyOomWrite(xroot Root)
{
	xfileoptions Options;
	xfile File;

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE |
		XFILE_CREATE |
		XFILE_TRUNCATE;
	File = xrtRootFileOpen(
		Root,
		"asset.txt",
		&Options
	);
	testRequire(
		(File != NULL) &&
		xrtWriteFull(File, "0123", 4, NULL) &&
		xrtClose(File),
		"HTTP static multipart OOM fixture write failed"
	);
}



/* 遍历正文工厂与正文对象分配点并验证失败不消费静态文件。 */
int main(void)
{
	test_http_static_multipart_body_oom State;
	xallocator Allocator;
	xtaskpoolconfig PoolConfig = { 1, 16, 0 };
	xhttpbyterange Ranges[2] = {
		{ 0, 0 },
		{ 3, 3 }
	};
	char sDirectory[96];
	xtaskpool* pPool;
	xhttpstaticfile* pFile;
	xhttpbody* pBody;
	xroot Parent;
	xroot Root;
	size_t i;

	memset(&State, 0, sizeof(State));
	State.FailAt = SIZE_MAX;
	Allocator.Context = &State;
	Allocator.Alloc = testHttpStaticMultipartBodyOomAlloc;
	Allocator.Realloc =
		testHttpStaticMultipartBodyOomRealloc;
	Allocator.Free = testHttpStaticMultipartBodyOomFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP static multipart OOM allocator install failed"
	);
	testRequire(
		(snprintf(
			sDirectory,
			sizeof(sDirectory),
			".xrt-http-static-multipart-oom-%lld",
			(long long)xrtNow()
		) > 0),
		"HTTP static multipart OOM directory name failed"
	);
	Parent = xrtRootOpen(".");
	testRequire(
		Parent != NULL,
		"HTTP static multipart OOM parent failed"
	);
	if ( !xrtRootRemove(Parent, sDirectory) ) {
		xrtClearError();
	}
	testRequire(xrtRootDirCreate(
		Parent,
		sDirectory,
		0700u
	), "HTTP static multipart OOM directory failed");
	Root = xrtRootOpenIn(Parent, sDirectory);
	testRequire(
		Root != NULL,
		"HTTP static multipart OOM root failed"
	);
	testHttpStaticMultipartBodyOomWrite(Root);
	pPool = xrtTaskPoolCreate(&PoolConfig);
	testRequire(
		pPool != NULL,
		"HTTP static multipart OOM pool failed"
	);

	for ( i = 0; i < 4u; i++ ) {
		State.FailAt = SIZE_MAX;
		pFile = xrtHttpStaticFileOpen(
			pPool,
			Root,
			"asset.txt"
		);
		testRequire(
			pFile != NULL,
			"HTTP static multipart OOM file open failed"
		);
		State.Calls = 0;
		State.FailAt = i;
		pBody = xrtHttpStaticFileTakeMultipartBody(
			pFile,
			Ranges,
			2,
			XRT_STR_LITERAL("text/plain"),
			XRT_STR_LITERAL("oom")
		);
		State.FailAt = SIZE_MAX;
		if ( pBody != NULL ) {
			xrtHttpBodyDestroy(pBody);
		} else {
			testRequire(
				(xrtGetError() != NULL) &&
				(xrtErrorKind(xrtGetError()) ==
				 XERR_MEMORY),
				"HTTP static multipart OOM error mismatch"
			);
			xrtClearError();
			pBody = xrtHttpStaticFileTakeBodyAll(
				pFile
			);
			testRequire(
				pBody != NULL,
				"HTTP static multipart OOM consumed file"
			);
			xrtHttpBodyDestroy(pBody);
		}
		xrtHttpStaticFileDestroy(pFile);
	}

	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP static multipart OOM pool cleanup failed"
	);
	testRequire(
		xrtRootRemove(Root, "asset.txt") &&
		xrtRootClose(Root) &&
		xrtRootRemove(Parent, sDirectory) &&
		xrtRootClose(Parent),
		"HTTP static multipart OOM cleanup failed"
	);
	printf("[PASS] http_static_multipart_body_oom\n");
	return 0;
}
