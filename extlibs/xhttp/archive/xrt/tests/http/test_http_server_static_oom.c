#include "../test.h"
#include "../../src/internal/xrt_http_server.h"



#define TEST_HTTP_SERVER_STATIC_OOM_CLASSES	64u
#define TEST_HTTP_SERVER_STATIC_OOM_HELD		65536u
#define TEST_HTTP_SERVER_STATIC_OOM_LIMIT	32u



/* 故障分配器只在门开启时限制新的底层 span。 */
typedef struct test_http_server_static_oom_allocator {
	xatomic32 Gate;
	xatomic32 Allow;
	xatomic64 Denied;
} test_http_server_static_oom_allocator;



/* 测试状态保存故障分配器和临时占用的尺寸类块。 */
typedef struct test_http_server_static_oom {
	test_http_server_static_oom_allocator Allocator;
	ptr* Held;
	size_t HeldCount;
	size_t HeldCapacity;
} test_http_server_static_oom;



/* 阻塞唯一文件 Worker，使 Future 构造的同步分配点保持确定。 */
typedef struct test_http_server_static_oom_block {
	xmutex Lock;
	xcond Cond;
	bool Started;
	bool Release;
} test_http_server_static_oom_block;



/* 按门状态放行或拒绝一个新的底层分配。 */
static ptr testHttpServerStaticOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_server_static_oom_allocator* pAllocator =
		(test_http_server_static_oom_allocator*)pData;

	if ( xrtAtomic32Load(
		&pAllocator->Gate,
		XMEMORY_ACQUIRE
	) != 0 ) {
		uint32 iAllow = xrtAtomic32Load(
			&pAllocator->Allow,
			XMEMORY_ACQUIRE
		);

		if ( iAllow == 0 ) {
			(void)xrtAtomic64FetchAdd(
				&pAllocator->Denied,
				1,
				XMEMORY_RELAXED
			);
			return NULL;
		}
		(void)xrtAtomic32FetchSub(
			&pAllocator->Allow,
			1,
			XMEMORY_ACQ_REL
		);
	}
	return malloc(iSize);
}



/* 重分配服从相同的底层 span 故障门。 */
static ptr testHttpServerStaticOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_server_static_oom_allocator* pAllocator =
		(test_http_server_static_oom_allocator*)pData;

	if ( xrtAtomic32Load(
		&pAllocator->Gate,
		XMEMORY_ACQUIRE
	) != 0 ) {
		uint32 iAllow = xrtAtomic32Load(
			&pAllocator->Allow,
			XMEMORY_ACQUIRE
		);

		if ( iAllow == 0 ) {
			(void)xrtAtomic64FetchAdd(
				&pAllocator->Denied,
				1,
				XMEMORY_RELAXED
			);
			return NULL;
		}
		(void)xrtAtomic32FetchSub(
			&pAllocator->Allow,
			1,
			XMEMORY_ACQ_REL
		);
	}
	return realloc(pMemory, iSize);
}



/* 释放故障窗口外成功取得的底层内存。 */
static void testHttpServerStaticOomFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	free(pMemory);
}



/* 暂时占满常用尺寸类，迫使被测构造申请新的 backing span。 */
static void testHttpServerStaticOomExhaust(
	test_http_server_static_oom* pState
)
{
	size_t i;

	xrtAtomic32Store(
		&pState->Allocator.Allow,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pState->Allocator.Gate,
		1,
		XMEMORY_RELEASE
	);
	for ( i = 1; i <=
		TEST_HTTP_SERVER_STATIC_OOM_CLASSES; i++ ) {
		for ( ;; ) {
			ptr pMemory = xrtMalloc(i * 16u);

			if ( pMemory == NULL ) {
				xrtClearError();
				break;
			}
			testRequire(
				pState->HeldCount <
					pState->HeldCapacity,
				"HTTP static server OOM exhaustion overflowed"
			);
			pState->Held[pState->HeldCount++] =
				pMemory;
		}
	}
}



/* 关闭故障门并归还全部尺寸类块。 */
static void testHttpServerStaticOomRestore(
	test_http_server_static_oom* pState
)
{
	size_t i;

	xrtAtomic32Store(
		&pState->Allocator.Gate,
		0,
		XMEMORY_RELEASE
	);
	for ( i = 0; i < pState->HeldCount; i++ ) {
		xrtFree(pState->Held[i]);
	}
	pState->HeldCount = 0;
}



/* 占住任务池唯一 Worker，直到测试关闭故障门。 */
static xtaskoutcome testHttpServerStaticOomBlockTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	test_http_server_static_oom_block* pBlock =
		(test_http_server_static_oom_block*)pData;
	bool bResult = true;

	(void)pCancel;
	(void)pResult;
	if ( !xrtMutexLock(&pBlock->Lock) ) {
		return XTASK_FAILED;
	}
	pBlock->Started = true;
	bResult = xrtCondBroadcast(&pBlock->Cond);
	while ( bResult && !pBlock->Release ) {
		bResult = xrtCondWait(
			&pBlock->Cond,
			&pBlock->Lock
		) == XWAIT_OK;
	}
	(void)xrtMutexUnlock(&pBlock->Lock);
	return bResult ? XTASK_SUCCESS : XTASK_FAILED;
}



/* 等待阻塞任务稳定进入唯一 Worker。 */
static bool testHttpServerStaticOomBlockStarted(
	test_http_server_static_oom_block* pBlock
)
{
	bool bResult = xrtMutexLock(&pBlock->Lock);

	while ( bResult && !pBlock->Started ) {
		bResult = xrtCondWaitFor(
			&pBlock->Cond,
			&pBlock->Lock,
			UINT64_C(2000000)
		) == XWAIT_OK;
	}
	(void)xrtMutexUnlock(&pBlock->Lock);
	return bResult;
}



/* 放行阻塞任务并允许后续文件准备执行。 */
static bool testHttpServerStaticOomBlockRelease(
	test_http_server_static_oom_block* pBlock
)
{
	bool bResult = xrtMutexLock(&pBlock->Lock);

	if ( bResult ) {
		pBlock->Release = true;
		bResult = xrtCondBroadcast(&pBlock->Cond);
		(void)xrtMutexUnlock(&pBlock->Lock);
	}
	return bResult;
}



/* 创建带多范围字段的拥有型请求快照。 */
static xhttpserverrequest* testHttpServerStaticOomRequest(void)
{
	static const char sWire[] =
		"GET /asset.txt HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Range: bytes=0-2,7-9\r\n\r\n";
	xhttpfield Fields[8];
	xhttp1bodyplan Plan;
	xhttp1head Head;

	xrtHttp1HeadInit(&Head, Fields, 8);
	if ( xrtHttp1RequestParse(
		XRT_BYTES_LITERAL(sWire),
		&Head,
		NULL,
		NULL
	) != XHTTP1_READY ) {
		return NULL;
	}
	if ( !xrtHttp1RequestBodyPlan(
		&Head,
		&Plan
	) ) {
		return NULL;
	}
	return __xrtHttpServerRequestCreate(
		&Head,
		&Plan,
		XHTTP_SERVER_REQUEST_KEEP_ALIVE
	);
}



/* 创建请求指定的拥有型快照。 */
static xhttpserverrequest* testHttpServerStaticOomPathRequest(
	cstr sTarget
)
{
	char Wire[256];
	int iSize = snprintf(
		Wire,
		sizeof(Wire),
		"GET %s HTTP/1.1\r\n"
		"Host: example.test\r\n\r\n",
		sTarget
	);
	xhttpfield Fields[8];
	xhttp1bodyplan Plan;
	xhttp1head Head;

	if ( (iSize <= 0) ||
		((size_t)iSize >= sizeof(Wire)) ) {
		return NULL;
	}
	xrtHttp1HeadInit(&Head, Fields, 8);
	if ( xrtHttp1RequestParse(
		(xbytesview){
			(cbytes)Wire,
			(size_t)iSize
		},
		&Head,
		NULL,
		NULL
	) != XHTTP1_READY ) {
		return NULL;
	}
	if ( !xrtHttp1RequestBodyPlan(
		&Head,
		&Plan
	) ) {
		return NULL;
	}
	return __xrtHttpServerRequestCreate(
		&Head,
		&Plan,
		XHTTP_SERVER_REQUEST_KEEP_ALIVE
	);
}



/* 在文件根内创建一个固定短资源。 */
static void testHttpServerStaticOomWrite(
	xroot Root,
	cstr sPath,
	cstr sText
)
{
	xfileoptions Options;
	xfile File;

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE |
		XFILE_CREATE |
		XFILE_TRUNCATE;
	File = xrtRootFileOpen(
		Root,
		sPath,
		&Options
	);
	testRequire(
		(File != NULL) &&
		xrtWriteFull(
			File,
			sText,
			strlen(sText),
			NULL
		) &&
		xrtClose(File),
		"HTTP static server OOM fixture write failed"
	);
}



/* 要求一次尝试的全部逻辑堆块最终配对释放。 */
static void testHttpServerStaticOomBalanced(
	const xmemstats* pBefore
)
{
	xmemstats After;

	xrtMemStatsGet(&After);
	testRequire(
		(After.BlockAllocCalls -
		 pBefore->BlockAllocCalls) ==
		(After.BlockFreeCalls -
		 pBefore->BlockFreeCalls),
		"HTTP static server OOM leaked a heap block"
	);
}



/* 验证排队期间取消会跳过 Reply 组合，并完整回收文件任务资源。 */
static void testHttpServerStaticFutureCancel(xroot Root)
{
	xtaskpoolconfig PoolConfig = { 1, 8, 0 };
	test_http_server_static_oom_block Block;
	xhttpserverrequest* pRequest;
	xtaskpool* pPool;
	xfuture* pBlockFuture;
	xfuture* pReplyFuture;
	xtaskpoolstats Stats;
	xmemstats Before;

	xrtMemStatsGet(&Before);
	memset(&Block, 0, sizeof(Block));
	memset(&Stats, 0, sizeof(Stats));
	testRequire(
		xrtMutexInit(&Block.Lock) &&
		xrtCondInit(&Block.Cond),
		"HTTP static server cancellation blocker init failed"
	);
	pPool = xrtTaskPoolCreate(&PoolConfig);
	pRequest = testHttpServerStaticOomPathRequest(
		"/asset.txt"
	);
	testRequire(
		(pPool != NULL) &&
		(pRequest != NULL),
		"HTTP static server cancellation setup failed"
	);
	pBlockFuture = xrtTaskSubmit(
		pPool,
		testHttpServerStaticOomBlockTask,
		&Block,
		NULL
	);
	testRequire(
		(pBlockFuture != NULL) &&
		testHttpServerStaticOomBlockStarted(&Block),
		"HTTP static server cancellation blocker failed"
	);
	pReplyFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pRequest,
		NULL
	);
	testRequire(
		(pReplyFuture != NULL) &&
		xrtFutureCancel(pReplyFuture),
		"HTTP static server cancellation request failed"
	);
	testRequire(
		testHttpServerStaticOomBlockRelease(&Block) &&
		(xrtFutureWaitFor(
			pBlockFuture,
			UINT64_C(2000000)
		 ) == XWAIT_OK) &&
		(xrtFutureWaitFor(
			pReplyFuture,
			UINT64_C(3000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pReplyFuture) ==
		 XFUTURE_CANCELLED) &&
		xrtTaskPoolClose(pPool) &&
		(xrtTaskPoolWaitFor(
			pPool,
			UINT64_C(2000000)
		 ) == XWAIT_OK) &&
		xrtTaskPoolGet(pPool, &Stats) &&
		(Stats.Submitted == 2) &&
		(Stats.Succeeded == 1) &&
		(Stats.Cancelled == 1),
		"HTTP static server cancellation did not cancel the queued file task"
	);
	xrtFutureDestroy(pReplyFuture);
	xrtFutureDestroy(pBlockFuture);
	xrtHttpServerRequestDestroy(pRequest);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP static server cancellation pool cleanup failed"
	);
	testRequire(
		xrtCondDestroy(&Block.Cond) &&
		xrtMutexDestroy(&Block.Lock),
		"HTTP static server cancellation blocker cleanup failed"
	);
	testHttpServerStaticOomBalanced(&Before);
}



/* 在故障门下构建路径、配置副本、文件任务和 continuation。 */
static bool testHttpServerStaticFutureOomAttempt(
	test_http_server_static_oom* pState,
	xroot Root,
	uint32 iAllow
)
{
	xtaskpoolconfig PoolConfig = { 1, 8, 0 };
	test_http_server_static_oom_block Block;
	xhttpstaticserveconfig Config;
	xhttpserverrequest* pRequest;
	xtaskpool* pPool;
	xfuture* pBlockFuture;
	xfuture* pReplyFuture;
	xmemstats Before;
	uint64 iDenied;

	xrtMemStatsGet(&Before);
	memset(&Block, 0, sizeof(Block));
	testRequire(
		xrtMutexInit(&Block.Lock) &&
		xrtCondInit(&Block.Cond),
		"HTTP static server Future OOM blocker init failed"
	);
	pPool = xrtTaskPoolCreate(&PoolConfig);
	pRequest = testHttpServerStaticOomPathRequest("/");
	testRequire(
		(pPool != NULL) &&
		(pRequest != NULL),
		"HTTP static server Future OOM setup failed"
	);
	pBlockFuture = xrtTaskSubmit(
		pPool,
		testHttpServerStaticOomBlockTask,
		&Block,
		NULL
	);
	testRequire(
		(pBlockFuture != NULL) &&
		testHttpServerStaticOomBlockStarted(&Block),
		"HTTP static server Future OOM blocker failed"
	);
	xrtHttpStaticServeConfigInit(&Config);
	Config.Reply.ContentType =
		XRT_STR_LITERAL("text/example");
	Config.Reply.CacheControl =
		XRT_STR_LITERAL("public, max-age=60");
	Config.Reply.Boundary =
		XRT_STR_LITERAL("future-oom");

	testHttpServerStaticOomExhaust(pState);
	iDenied = xrtAtomic64Load(
		&pState->Allocator.Denied,
		XMEMORY_ACQUIRE
	);
	xrtAtomic32Store(
		&pState->Allocator.Allow,
		iAllow,
		XMEMORY_RELEASE
	);
	pReplyFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pRequest,
		&Config
	);
	if ( pReplyFuture == NULL ) {
		testRequire(
			(xrtAtomic64Load(
				&pState->Allocator.Denied,
				XMEMORY_ACQUIRE
			 ) > iDenied) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) ==
			 XERR_MEMORY),
			"HTTP static server Future OOM failure mismatch"
		);
	}
	testHttpServerStaticOomRestore(pState);
	xrtClearError();

	testRequire(
		testHttpServerStaticOomBlockRelease(&Block) &&
		(xrtFutureWaitFor(
			pBlockFuture,
			UINT64_C(2000000)
		 ) == XWAIT_OK),
		"HTTP static server Future OOM blocker release failed"
	);
	if ( pReplyFuture != NULL ) {
		testRequire(
			(xrtFutureWaitFor(
				pReplyFuture,
				UINT64_C(3000000)
			 ) == XWAIT_OK) &&
			(xrtFutureState(pReplyFuture) ==
			 XFUTURE_RESOLVED),
			"HTTP static server Future did not recover"
		);
		xrtFutureDestroy(pReplyFuture);
	}
	xrtFutureDestroy(pBlockFuture);
	xrtHttpServerRequestDestroy(pRequest);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP static server Future OOM pool cleanup failed"
	);
	testRequire(
		xrtCondDestroy(&Block.Cond) &&
		xrtMutexDestroy(&Block.Lock),
		"HTTP static server Future OOM blocker cleanup failed"
	);
	testHttpServerStaticOomBalanced(&Before);
	return pReplyFuture != NULL;
}



/* 在指定 span 放行量下构建多范围 Reply，并验证失败不消费文件。 */
static bool testHttpServerStaticOomAttempt(
	test_http_server_static_oom* pState,
	xroot Root,
	uint32 iAllow
)
{
	xtaskpoolconfig PoolConfig = { 1, 8, 0 };
	xhttpstaticreplyconfig Config;
	xhttpserverrequest* pRequest;
	xhttpstaticfile* pFile;
	xhttpreply* pReply;
	xhttpbody* pBody;
	xtaskpool* pPool;
	xmemstats Before;
	uint64 iDenied;

	xrtMemStatsGet(&Before);
	pPool = xrtTaskPoolCreate(&PoolConfig);
	pRequest = testHttpServerStaticOomRequest();
	pFile = xrtHttpStaticFileOpen(
		pPool,
		Root,
		"asset.txt"
	);
	testRequire(
		(pPool != NULL) &&
		(pRequest != NULL) &&
		(pFile != NULL),
		"HTTP static server OOM setup failed"
	);
	xrtHttpStaticReplyConfigInit(&Config);
	Config.Boundary = XRT_STR_LITERAL("oom");

	testHttpServerStaticOomExhaust(pState);
	iDenied = xrtAtomic64Load(
		&pState->Allocator.Denied,
		XMEMORY_ACQUIRE
	);
	xrtAtomic32Store(
		&pState->Allocator.Allow,
		iAllow,
		XMEMORY_RELEASE
	);
	pReply = xrtHttpReplyStatic(
		pRequest,
		pFile,
		XRT_STR_LITERAL("asset.txt"),
		&Config
	);
	if ( pReply == NULL ) {
		testRequire(
			(xrtAtomic64Load(
				&pState->Allocator.Denied,
				XMEMORY_ACQUIRE
			 ) > iDenied) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) ==
			 XERR_MEMORY),
			"HTTP static server OOM failure mismatch"
		);
	}
	testHttpServerStaticOomRestore(pState);
	xrtClearError();

	if ( pReply != NULL ) {
		xrtHttpReplyDestroy(pReply);
	} else {
		pBody = xrtHttpStaticFileTakeBodyAll(
			pFile
		);
		testRequire(
			pBody != NULL,
			"HTTP static server OOM consumed the file"
		);
		xrtHttpBodyDestroy(pBody);
	}
	xrtHttpStaticFileDestroy(pFile);
	xrtHttpServerRequestDestroy(pRequest);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP static server OOM pool cleanup failed"
	);
	testHttpServerStaticOomBalanced(&Before);
	return pReply != NULL;
}



/* 遍历静态 Reply 组合层的同步分配边界。 */
int main(void)
{
	test_http_server_static_oom State;
	xallocator Allocator = {
		&State.Allocator,
		testHttpServerStaticOomAlloc,
		testHttpServerStaticOomRealloc,
		testHttpServerStaticOomFree
	};
	char sDirectory[96];
	xroot Parent;
	xroot Root;
	size_t iFailures = 0;
	size_t iFutureFailures = 0;
	bool bSuccess = false;
	bool bFutureSuccess = false;
	uint32 i;
	int iSize;

	memset(&State, 0, sizeof(State));
	State.HeldCapacity =
		TEST_HTTP_SERVER_STATIC_OOM_HELD;
	State.Held = (ptr*)malloc(
		State.HeldCapacity * sizeof(ptr)
	);
	testRequire(
		State.Held != NULL,
		"HTTP static server OOM held array failed"
	);
	xrtAtomic32Init(&State.Allocator.Gate, 0);
	xrtAtomic32Init(&State.Allocator.Allow, 0);
	xrtAtomic64Init(&State.Allocator.Denied, 0);
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP static server OOM allocator install failed"
	);
	xrtMemStatsEnable(true);
	xrtMemStatsReset();

	iSize = snprintf(
		sDirectory,
		sizeof(sDirectory),
		".xrt-http-server-static-oom-%lld",
		(long long)xrtNow()
	);
	testRequire(
		(iSize > 0) &&
		((size_t)iSize < sizeof(sDirectory)),
		"HTTP static server OOM directory name failed"
	);
	Parent = xrtRootOpen(".");
	testRequire(
		Parent != NULL,
		"HTTP static server OOM parent root failed"
	);
	if ( !xrtRootRemove(
		Parent,
		sDirectory
	) ) {
		xrtClearError();
	}
	testRequire(
		xrtRootDirCreate(
			Parent,
			sDirectory,
			0700u
		),
		"HTTP static server OOM directory failed"
	);
	Root = xrtRootOpenIn(
		Parent,
		sDirectory
	);
	testRequire(
		Root != NULL,
		"HTTP static server OOM root failed"
	);
	testHttpServerStaticOomWrite(
		Root,
		"asset.txt",
		"0123456789"
	);
	testHttpServerStaticOomWrite(
		Root,
		"index.html",
		"index"
	);
	testHttpServerStaticFutureCancel(Root);

	for ( i = 0;
		(i <= TEST_HTTP_SERVER_STATIC_OOM_LIMIT) &&
		!bSuccess;
		i++ ) {
		bSuccess = testHttpServerStaticOomAttempt(
			&State,
			Root,
			i
		);
		if ( !bSuccess ) {
			iFailures++;
		}
	}
	testRequire(
		bSuccess && (iFailures != 0),
		"HTTP static server OOM traversal incomplete"
	);
	for ( i = 0;
		(i <= TEST_HTTP_SERVER_STATIC_OOM_LIMIT) &&
		!bFutureSuccess;
		i++ ) {
		bFutureSuccess =
			testHttpServerStaticFutureOomAttempt(
				&State,
				Root,
				i
			);
		if ( !bFutureSuccess ) {
			iFutureFailures++;
		}
	}
	testRequire(
		bFutureSuccess && (iFutureFailures != 0),
		"HTTP static server Future OOM traversal incomplete"
	);
	testRequire(
		xrtRootRemove(Root, "asset.txt") &&
		xrtRootRemove(Root, "index.html") &&
		xrtRootClose(Root) &&
		xrtRootRemove(Parent, sDirectory) &&
		xrtRootClose(Parent),
		"HTTP static server OOM fixture cleanup failed"
	);
	free(State.Held);
	printf("[PASS] http_server_static_oom\n");
	return 0;
}
