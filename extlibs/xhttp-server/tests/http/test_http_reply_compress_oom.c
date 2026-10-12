#include "../test.h"

#include <xrt/http_compress.h>



/* 故障分配器记录分配序号和当前存活块。 */
typedef struct test_http_reply_compress_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_reply_compress_allocator;



/* 在指定序号拒绝分配，其余请求交给 C 运行库。 */
static ptr testHttpReplyCompressAlloc(
	ptr pContext,
	size_t iSize
)
{
	test_http_reply_compress_allocator* pState =
		(test_http_reply_compress_allocator*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 重分配失败保留原块，首次成功分配增加存活计数。 */
static ptr testHttpReplyCompressRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_reply_compress_allocator* pState =
		(test_http_reply_compress_allocator*)pContext;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放一个底层块并维护存活计数。 */
static void testHttpReplyCompressFree(
	ptr pContext,
	ptr pMemory
)
{
	test_http_reply_compress_allocator* pState =
		(test_http_reply_compress_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"Reply compression OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 在一个失败点下完成 Reply 构建和 eager 压缩。 */
static bool testHttpReplyCompressOomAttempt(void)
{
	static uint8 Input[16384];
	xhttpacceptencoding Accept;
	xhttpreply* pReply = NULL;
	xhttpreply* pOutput = NULL;
	xhttpbody* pOriginal;
	xhttpreplycompressstatus Status;
	bool bComplete = false;

	memset(Input, 'z', sizeof(Input));
	xrtHttpAcceptEncodingInit(&Accept);
	if ( !xrtHttpAcceptEncodingAdd(
		&Accept, XRT_STR_LITERAL("gzip")
	) ) {
		goto done;
	}
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	if ( (pReply == NULL) ||
		!xrtHttpReplySetBytes(
			pReply,
			(xbytesview){ Input, sizeof(Input) },
			XRT_STR_LITERAL("application/json")
		) ||
		!xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Vary"),
			XRT_STR_LITERAL("User-Agent")
		) ||
		!xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Content-Digest"),
			XRT_STR_LITERAL("sha-256=:AAAA:")
		) ) {
		goto done;
	}
	pOriginal = xrtHttpReplyBody(pReply);
	Status = xrtHttpReplyCompress(
		&Accept,
		XRT_STR_LITERAL("GET"),
		pReply,
		NULL,
		&pOutput
	);
	if ( Status == XHTTP_REPLY_COMPRESS_ERROR ) {
		testRequire((pOutput == NULL) &&
			(xrtHttpReplyBody(pReply) == pOriginal) &&
			(xrtHttpReplyHeader(
				pReply,
				XRT_STR_LITERAL("Content-Encoding")
			 ) == NULL) &&
			(xrtHttpReplyHeader(
				pReply,
				XRT_STR_LITERAL("Content-Digest")
			 ) != NULL),
			"Reply compression OOM changed source Reply");
		goto done;
	}
	testRequire(
		(Status == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL),
		"Reply compression OOM success status mismatch"
	);
	bComplete = true;

done:
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
	xrtClearError();
	return bComplete;
}



/* 扫描全部分配失败点并要求每次回到稳定内存基线。 */
int main(void)
{
	test_http_reply_compress_allocator State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpReplyCompressAlloc,
		testHttpReplyCompressRealloc,
		testHttpReplyCompressFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"Reply compression OOM allocator install failed");
	testRequire(testHttpReplyCompressOomAttempt(),
		"Reply compression OOM warm-up failed");
	testMemoryDebugDrain(
		"Reply compression OOM warm-up leaked storage"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= 320; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpReplyCompressOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"Reply compression OOM attempt leaked storage"
		);
		testRequire(State.Live == iBaseline,
			"Reply compression OOM live allocation leak");
	}
	testRequire((iFailures != 0) && bSuccess,
		"Reply compression OOM sweep missed failure or success");
	printf("[PASS] HTTP Reply compression OOM\n");
	return 0;
}
