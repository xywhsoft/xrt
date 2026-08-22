#include "../test.h"



/* 可调失败分配器扫描 Reply 的全部按需存储路径。 */
typedef struct test_http_reply_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_reply_allocator;



/* 在指定分配序号失败，其余请求交给 C 运行库。 */
static ptr testHttpReplyAlloc(ptr pContext, size_t iSize)
{
	test_http_reply_allocator* pState =
		(test_http_reply_allocator*)pContext;
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



/* 重分配失败保留原块，首次成功分配时增加存活计数。 */
static ptr testHttpReplyRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_reply_allocator* pState =
		(test_http_reply_allocator*)pContext;
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



/* 释放底层块并防止存活计数下溢。 */
static void testHttpReplyFree(ptr pContext, ptr pMemory)
{
	test_http_reply_allocator* pState =
		(test_http_reply_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"HTTP Reply OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 在一个失败点下推进原因、字段、正文和克隆路径。 */
static bool testHttpReplyOomAttempt(void)
{
	char Large[16384];
	xhttpreply* pReply;
	xhttpreply* pClone = NULL;
	xstrview OldReason;
	xhttpbody* pOldBody;
	size_t iOldHeaders;
	bool bComplete = false;

	memset(Large, 'x', sizeof(Large));
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	if ( pReply == NULL ) {
		return false;
	}
	if ( !xrtHttpReplySetReason(
		pReply, XRT_STR_LITERAL("old")
	) || !xrtHttpReplySetBytes(
		pReply,
		(xbytesview){ (cbytes)"old", 3 },
		XRT_STR_LITERAL("text/plain")
	) ) {
		goto done;
	}

	OldReason = xrtHttpReplyReason(pReply);
	if ( !xrtHttpReplySetReason(
		pReply, (xstrview){ Large, sizeof(Large) }
	) ) {
		testRequire(
			(xrtHttpReplyReason(pReply).Size ==
			 OldReason.Size) &&
			(memcmp(
				xrtHttpReplyReason(pReply).Data,
				OldReason.Data,
				OldReason.Size
			) == 0),
			"HTTP Reply reason OOM changed visible state"
		);
		goto done;
	}

	iOldHeaders = xrtHttpReplyHeaderCount(pReply);
	if ( !xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("X-Large"),
		(xstrview){ Large, sizeof(Large) }
	) ) {
		testRequire(
			xrtHttpReplyHeaderCount(pReply) == iOldHeaders,
			"HTTP Reply Header OOM exposed a partial field"
		);
		goto done;
	}
	if ( !xrtHttpReplyAddTrailer(
		pReply,
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("sha-256=:AA==:")
	) ) {
		testRequire(xrtHttpReplyTrailerCount(pReply) == 0,
			"HTTP Reply Trailer OOM exposed a partial field");
		goto done;
	}

	pOldBody = xrtHttpReplyBody(pReply);
	if ( !xrtHttpReplySetBytes(
		pReply,
		(xbytesview){ (cbytes)Large, sizeof(Large) },
		XRT_STR_LITERAL("application/octet-stream")
	) ) {
		testRequire(
			xrtHttpReplyBody(pReply) == pOldBody,
			"HTTP Reply body OOM replaced the old body"
		);
		goto done;
	}
	pClone = xrtHttpReplyClone(pReply);
	if ( pClone == NULL ) {
		testRequire(
			(xrtHttpReplyHeaderCount(pReply) == 2) &&
			(xrtHttpReplyTrailerCount(pReply) == 1) &&
			(xrtHttpBodyLength(
				xrtHttpReplyBody(pReply)
			) == sizeof(Large)),
			"HTTP Reply Clone OOM changed the source"
		);
		goto done;
	}
	bComplete = true;

done:
	xrtHttpReplyDestroy(pClone);
	xrtHttpReplyDestroy(pReply);
	xrtClearError();
	return bComplete;
}



/* 扫描失败序号并要求每次回到稳定内存基线。 */
int main(void)
{
	test_http_reply_allocator State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpReplyAlloc,
		testHttpReplyRealloc,
		testHttpReplyFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP Reply OOM allocator install failed");
	testRequire(testHttpReplyOomAttempt(),
		"HTTP Reply OOM warm-up failed");
	testMemoryDebugDrain(
		"HTTP Reply OOM memory debug reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= 160; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpReplyOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"HTTP Reply OOM memory debug reset failed"
		);
		testRequire(State.Live == iBaseline,
			"HTTP Reply OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"HTTP Reply OOM sweep missed failure or success");
	printf("[PASS] HTTP server Reply OOM\n");
	return 0;
}
