#include "../test.h"
#include "../fixtures/http_call.h"
#include "../../../src/internal/xrt_memory.h"



#define TEST_HTTP_DECOMPRESS_DRAIN_MAX 8192u



/*
	底层分配器只拒绝命中尺寸范围的下一次分配。
	首次失败后立即恢复，保证错误对象仍能完整构造。
*/
typedef struct test_http_decompress_allocator {
	size_t Min;
	size_t Max;
	bool Armed;
	bool Failed;
} test_http_decompress_allocator;



/*
	转发普通分配，并在命中当前故障窗口时注入一次 OOM。
*/
static ptr testHttpDecompressAlloc(ptr pContext, size_t iSize)
{
	test_http_decompress_allocator* pState =
		(test_http_decompress_allocator*)pContext;

	if ( pState->Armed && !pState->Failed &&
		(iSize >= pState->Min) &&
		(iSize <= pState->Max) ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/*
	转发普通重分配，并与初始分配共享同一个故障点。
*/
static ptr testHttpDecompressRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_decompress_allocator* pState =
		(test_http_decompress_allocator*)pContext;

	if ( pState->Armed && !pState->Failed &&
		(iSize >= pState->Min) &&
		(iSize <= pState->Max) ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/*
	释放测试分配器产生的原始内存。
*/
static void testHttpDecompressFree(
	ptr pContext,
	ptr pMemory
)
{
	(void)pContext;
	free(pMemory);
}



/*
	配置下一次底层分配的拒绝范围。
*/
static void testHttpDecompressArm(
	test_http_decompress_allocator* pState,
	size_t iMin,
	size_t iMax
)
{
	pState->Min = iMin;
	pState->Max = iMax;
	pState->Armed = true;
	pState->Failed = false;
}



/*
	耗尽指定小尺寸类的空闲块，并让下一次同类分配必须补充 Span。
*/
static size_t testHttpDecompressDrain(
	test_http_decompress_allocator* pState,
	size_t iSize,
	ptr Blocks[TEST_HTTP_DECOMPRESS_DRAIN_MAX]
)
{
	size_t iCount = 0;

	testHttpDecompressArm(pState, 0, SIZE_MAX);
	while ( iCount < TEST_HTTP_DECOMPRESS_DRAIN_MAX ) {
		ptr pBlock = xrtMalloc(iSize);

		if ( pBlock == NULL ) {
			break;
		}
		Blocks[iCount] = pBlock;
		iCount++;
	}
	testRequire(
		pState->Failed,
		"HTTP decompression OOM could not exhaust a heap class"
	);
	pState->Armed = false;
	xrtClearError();
	return iCount;
}



/*
	归还为尺寸类故障注入暂时占用的全部块。
*/
static void testHttpDecompressUndrain(
	ptr Blocks[TEST_HTTP_DECOMPRESS_DRAIN_MAX],
	size_t iCount
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		xrtFree(Blocks[i]);
	}
}



/*
	创建带 Content-Encoding 的可填充响应。
*/
static xhttpresponse* testHttpDecompressResponse(
	xstrview Encoding,
	bool bStreamed
)
{
	xhttpresponse* pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		200,
		XRT_STR_LITERAL("OK"),
		NULL
	);

	testRequire(
		(pResponse != NULL) &&
		__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Content-Encoding"),
			Encoding
		),
		"HTTP decompression OOM response fixture failed"
	);
	if ( bStreamed ) {
		__xrtHttpResponseSetFlags(
			pResponse,
			(uint32)XHTTP_RESPONSE_STREAMED
		);
	}
	return pResponse;
}



/*
	初始化解码事件所需的最小 Call，并安装统一终态字段。
*/
static void testHttpDecompressCall(
	xhttpcall* pCall,
	xhttprequest* pRequest,
	xhttpcallproc pDone,
	ptr pData
)
{
	memset(pCall, 0, sizeof(*pCall));
	pCall->Request = pRequest;
	pCall->DecompressEnabled = true;
	pCall->DecompressLimit =
		XHTTP_DECOMPRESS_BODY_DEFAULT;
	pCall->DecompressMaxCodings =
		XHTTP_DECOMPRESS_CODINGS_DEFAULT;
	testHttpCallStateInit(pCall, pDone, pData);
}



/*
	验证自动解压错误稳定保留内存分类、客户端错误码和底层原因。
*/
static void testHttpDecompressError(
	const xerror* pError,
	cstr sOperation
)
{
	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_MEMORY) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client"
		) == 0) &&
		(xrtErrorCode(pError) ==
		 (int32)XHTTP_CLIENT_ERROR_DECOMPRESSION) &&
		(strcmp(
			xrtErrorOperation(pError),
			sOperation
		) == 0) &&
		(xrtErrorIs(pError, XERR_MEMORY) != NULL),
		"HTTP decompression OOM lost its stable error"
	);
}



/*
	确认一次故障场景中的所有逻辑堆块已经配对释放。
*/
static void testHttpDecompressMemoryBalanced(
	const xmemstats* pBefore
)
{
	xmemstats After;

	xrtMemStatsGet(&After);
	testRequire(
		(After.BlockAllocCalls - pBefore->BlockAllocCalls) ==
		(After.BlockFreeCalls - pBefore->BlockFreeCalls),
		"HTTP decompression OOM leaked a heap block"
	);
}



/*
	模拟完成回调，借用错误对象并记录稳定终态。
*/
typedef struct test_http_decompress_done {
	bool Called;
	xnetresult Result;
	const xerror* Error;
} test_http_decompress_done;



/*
	记录由高层 Call 发布的自动解压失败。
*/
static void testHttpDecompressDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_decompress_done* pState =
		(test_http_decompress_done*)pData;

	testRequire(
		(pCall != NULL) && (pResult != NULL),
		"HTTP decompression OOM completion arguments were invalid"
	);
	pState->Called = true;
	pState->Result = pResult->Result;
	pState->Error = pResult->Error;
}



/*
	验证小型解码器指针数组在堆尺寸类补充失败时原子回滚。
*/
static void testHttpDecompressArrayOom(
	test_http_decompress_allocator* pAllocator
)
{
	ptr Blocks[TEST_HTTP_DECOMPRESS_DRAIN_MAX];
	xmemstats Before;
	xhttprequest* pRequest;
	xhttpresponse* pResponse;
	xhttpcall Call;
	xhttp1exchangeevents Next;
	const xhttp1exchangeevents* pEvents;
	xerror* pError;
	size_t iBlocks;

	xrtMemStatsGet(&Before);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://oom.test/array")
	);
	pResponse = testHttpDecompressResponse(
		XRT_STR_LITERAL("gzip"),
		false
	);
	testRequire(
		pRequest != NULL,
		"HTTP decompression array OOM request failed"
	);
	testHttpDecompressCall(&Call, pRequest, NULL, NULL);
	memset(&Next, 0, sizeof(Next));
	pEvents = __xrtHttpDecompressEvents(&Call, &Next);
	iBlocks = testHttpDecompressDrain(
		pAllocator,
		sizeof(xinflate*),
		Blocks
	);
	testHttpDecompressArm(pAllocator, 0, SIZE_MAX);
	testRequire(
		!pEvents->Headers(pResponse, pEvents->Data) &&
		pAllocator->Failed &&
		Call.DecompressFailed &&
		(Call.Inflaters == NULL) &&
		(Call.InflaterCount == 0),
		"HTTP decoder array allocation survived OOM"
	);
	pAllocator->Armed = false;
	pError = xrtTakeError();
	testHttpDecompressError(
		pError,
		"create-http-content-decoders"
	);
	xrtErrorFree(pError);
	testHttpDecompressUndrain(Blocks, iBlocks);
	__xrtHttpDecompressReset(&Call);
	testHttpCallStateUnit(&Call);
	xrtHttpResponseDestroy(pResponse);
	xrtHttpRequestDestroy(pRequest);
	testHttpDecompressMemoryBalanced(&Before);
}



/*
	验证按需创建的 32 KiB 级解码状态分配失败不会泄漏数组。
*/
static void testHttpDecompressInflaterOom(
	test_http_decompress_allocator* pAllocator
)
{
	xmemstats Before;
	xhttprequest* pRequest;
	xhttpresponse* pResponse;
	xhttpcall Call;
	xhttp1exchangeevents Next;
	const xhttp1exchangeevents* pEvents;
	xerror* pError;

	xrtMemStatsGet(&Before);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://oom.test/inflater")
	);
	pResponse = testHttpDecompressResponse(
		XRT_STR_LITERAL("gzip"),
		false
	);
	testRequire(
		pRequest != NULL,
		"HTTP decompression inflater OOM request failed"
	);
	testHttpDecompressCall(&Call, pRequest, NULL, NULL);
	memset(&Next, 0, sizeof(Next));
	pEvents = __xrtHttpDecompressEvents(&Call, &Next);
	testHttpDecompressArm(
		pAllocator,
		UINT32_C(32768),
		SIZE_MAX
	);
	testRequire(
		!pEvents->Headers(pResponse, pEvents->Data) &&
		pAllocator->Failed &&
		Call.DecompressFailed &&
		(Call.Inflaters == NULL) &&
		(Call.InflaterCount == 0),
		"HTTP content decoder creation survived OOM"
	);
	pAllocator->Armed = false;
	pError = xrtTakeError();
	testHttpDecompressError(
		pError,
		"create-http-content-decoders"
	);
	xrtErrorFree(pError);
	__xrtHttpDecompressReset(&Call);
	testHttpCallStateUnit(&Call);
	xrtHttpResponseDestroy(pResponse);
	xrtHttpRequestDestroy(pRequest);
	testHttpDecompressMemoryBalanced(&Before);
}



/*
	验证长 Content-Encoding 元数据复制失败时释放已经创建的解码链。
*/
static void testHttpDecompressEncodingOom(
	test_http_decompress_allocator* pAllocator
)
{
	char Encoding[2048];
	size_t iTarget;
	xmemstats Before;
	xhttprequest* pRequest;
	xhttpresponse* pResponse;
	xhttpcall Call;
	xhttp1exchangeevents Next;
	const xhttp1exchangeevents* pEvents;
	xerror* pError;

	memcpy(Encoding, "gzip", 4u);
	memset(Encoding + 4u, ' ', sizeof(Encoding) - 4u);
	xrtMemStatsGet(&Before);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://oom.test/encoding")
	);
	pResponse = testHttpDecompressResponse(
		(xstrview){ Encoding, sizeof(Encoding) },
		false
	);
	testRequire(
		pRequest != NULL,
		"HTTP decompression encoding OOM request failed"
	);
	testHttpDecompressCall(&Call, pRequest, NULL, NULL);
	memset(&Next, 0, sizeof(Next));
	pEvents = __xrtHttpDecompressEvents(&Call, &Next);
	iTarget = (sizeof(Encoding) + 1u) +
		__xrtHeapHeaderSize() +
		__xrtMemDebugTailSize() +
		(XRT_HEAP_ALIGNMENT - 1u);
	testHttpDecompressArm(
		pAllocator,
		iTarget,
		iTarget
	);
	testRequire(
		!pEvents->Headers(pResponse, pEvents->Data) &&
		pAllocator->Failed &&
		Call.DecompressFailed &&
		(Call.Inflaters == NULL) &&
		(Call.InflaterCount == 0),
		"HTTP Content-Encoding copy survived OOM"
	);
	pAllocator->Armed = false;
	pError = xrtTakeError();
	testHttpDecompressError(
		pError,
		"copy-http-content-encoding"
	);
	xrtErrorFree(pError);
	__xrtHttpDecompressReset(&Call);
	testHttpCallStateUnit(&Call);
	xrtHttpResponseDestroy(pResponse);
	xrtHttpRequestDestroy(pRequest);
	testHttpDecompressMemoryBalanced(&Before);
}



/*
	验证解码后正文缓冲 OOM 不会被误判为用户 Body 回调拒绝。
*/
static void testHttpDecompressBodyOom(
	test_http_decompress_allocator* pAllocator
)
{
	static const uint8 Gzip[] = {
		0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0xFF, 0x4B, 0x2B, 0x2D, 0x29, 0x2D, 0x4A,
		0x55, 0x48, 0x49, 0x4D, 0xCE, 0x4F, 0x49, 0x4D,
		0x51, 0x48, 0xCA, 0x4F, 0xA9, 0xE4, 0x02, 0x00,
		0x34, 0x32, 0x86, 0xF3, 0x14, 0x00, 0x00, 0x00
	};
	ptr Blocks[TEST_HTTP_DECOMPRESS_DRAIN_MAX];
	xmemstats Before;
	xhttprequest* pRequest;
	xhttpresponse* pResponse;
	xhttpclient Client;
	xhttpcall Call;
	xhttp1exchangeevents Next;
	const xhttp1exchangeevents* pEvents;
	test_http_decompress_done Done;
	xerror* pCause;
	size_t iBlocks;

	xrtMemStatsGet(&Before);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://oom.test/body")
	);
	pResponse = testHttpDecompressResponse(
		XRT_STR_LITERAL("gzip"),
		true
	);
	testRequire(
		pRequest != NULL,
		"HTTP decompression body OOM request failed"
	);
	memset(&Client, 0, sizeof(Client));
	testHttpClientStateInit(&Client);
	memset(&Done, 0, sizeof(Done));
	testHttpDecompressCall(
		&Call,
		pRequest,
		testHttpDecompressDone,
		&Done
	);
	Call.Client = &Client;
	memset(&Next, 0, sizeof(Next));
	pEvents = __xrtHttpDecompressEvents(&Call, &Next);
	testRequire(
		pEvents->Headers(pResponse, pEvents->Data),
		"HTTP decompression body OOM setup failed"
	);
	iBlocks = testHttpDecompressDrain(
		pAllocator,
		20u,
		Blocks
	);
	testHttpDecompressArm(pAllocator, 0, SIZE_MAX);
	testRequire(
		!pEvents->Body(
			pResponse,
			(xbytesview){ Gzip, sizeof(Gzip) },
			pEvents->Data
		) &&
		pAllocator->Failed &&
		Call.DecompressFailed &&
		!Call.DecompressForwardFailed,
		"HTTP decoded response buffering survived OOM"
	);
	pAllocator->Armed = false;
	pCause = xrtTakeError();
	testRequire(
		(pCause != NULL) &&
		(xrtErrorIs(pCause, XERR_MEMORY) != NULL) &&
		__xrtHttpDecompressFail(&Call, pCause) &&
		Done.Called &&
		(Done.Result == XNET_RESULT_ERROR) &&
		(Done.Error == Call.Error),
		"HTTP decoded body OOM did not reach the Call boundary"
	);
	testHttpDecompressError(
		Call.Error,
		"decode-http-response"
	);
	xrtErrorFree(pCause);
	xrtErrorFree(Call.Error);
	Call.Error = NULL;
	testHttpDecompressUndrain(Blocks, iBlocks);
	__xrtHttpDecompressReset(&Call);
	testHttpCallStateUnit(&Call);
	testHttpClientStateUnit(&Client);
	xrtHttpResponseDestroy(pResponse);
	xrtHttpRequestDestroy(pRequest);
	testHttpDecompressMemoryBalanced(&Before);
}



/*
	覆盖自动解压四条按需分配路径，并以逻辑块统计验证失败原子性。
*/
int main(void)
{
	test_http_decompress_allocator State;
	xallocator Allocator;

	memset(&State, 0, sizeof(State));
	Allocator.Context = &State;
	Allocator.Alloc = testHttpDecompressAlloc;
	Allocator.Realloc = testHttpDecompressRealloc;
	Allocator.Free = testHttpDecompressFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP decompression OOM allocator install failed"
	);
	xrtMemStatsEnable(true);
	xrtMemStatsReset();

	testHttpDecompressArrayOom(&State);
	testHttpDecompressInflaterOom(&State);
	testHttpDecompressEncodingOom(&State);
	testHttpDecompressBodyOom(&State);

	State.Armed = false;
	printf("[PASS] HTTP automatic decompression OOM\n");
	return 0;
}
