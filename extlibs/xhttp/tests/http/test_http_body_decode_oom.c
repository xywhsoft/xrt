#include "../test.h"



/* 逐个拒绝指定序号的分配，用于覆盖多层包装回滚。 */
typedef struct test_http_body_decode_oom {
	size_t Calls;
	size_t FailAt;
	size_t Denied;
} test_http_body_decode_oom;



/* 在指定序号拒绝分配，其余请求交给系统堆。 */
static ptr testHttpBodyDecodeOomAlloc(
	ptr pContext,
	size_t iSize
)
{
	test_http_body_decode_oom* pState =
		(test_http_body_decode_oom*)pContext;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Denied++;
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配与普通分配共享同一个故障序号。 */
static ptr testHttpBodyDecodeOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_body_decode_oom* pState =
		(test_http_body_decode_oom*)pContext;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Denied++;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放所有成功取得的系统堆块。 */
static void testHttpBodyDecodeOomFree(
	ptr pContext,
	ptr pMemory
)
{
	(void)pContext;
	free(pMemory);
}



/* 验证包装链的底层分配失败不泄漏链条或损坏来源。 */
int main(void)
{
	static const uint8 Encoded[] = {
		0x78, 0x9c, 0x93, 0xef, 0xe6, 0x60, 0x00,
		0x03, 0xe6, 0xd3, 0x4c, 0x9d, 0x5d, 0x4e,
		0x0d, 0x61, 0x37, 0x9f, 0x32, 0xa7, 0x24,
		0xb5, 0xb8, 0x24, 0xb3, 0x20, 0x35, 0x27,
		0xb9, 0x44, 0x8f, 0x0b, 0x00, 0x9d, 0x4d,
		0x0c, 0x74
	};
	test_http_body_decode_oom State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpBodyDecodeOomAlloc,
		testHttpBodyDecodeOomRealloc,
		testHttpBodyDecodeOomFree
	};
	size_t iFail;
	size_t iFailures = 0;
	bool bSucceeded = false;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP Body decode OOM allocator install failed");
	for ( iFail = 1; iFail <= 16; iFail++ ) {
		xhttpbody* pSource;
		xhttpbody* pBody = NULL;
		xbytesview View;
		xhttpbodydecoderesult Result;

		State.FailAt = 0;
		State.Calls = 0;
		pSource = xrtHttpBodyBorrow(
			(xbytesview){ Encoded, sizeof(Encoded) }
		);
		testRequire(pSource != NULL,
			"HTTP Body decode OOM source setup failed");
		State.FailAt = iFail;
		State.Calls = 0;
		Result = xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL("gzip, deflate"),
			NULL,
			&pBody
		);
		State.FailAt = 0;
		if ( Result == XHTTP_BODY_DECODE_ERROR ) {
			iFailures++;
			testRequire(
				(pBody == NULL) &&
				(xrtErrorKind(xrtGetError()) ==
				 XERR_MEMORY) &&
				xrtHttpBodyView(pSource, &View) &&
				(View.Size == sizeof(Encoded)) &&
				(memcmp(
					View.Data,
					Encoded,
					sizeof(Encoded)
				) == 0),
				"HTTP Body decode OOM rollback mismatch"
			);
			xrtClearError();
		} else {
			testRequire(
				(Result == XHTTP_BODY_DECODE_APPLIED) &&
				(pBody != NULL),
				"HTTP Body decode OOM success state mismatch"
			);
			bSucceeded = true;
			xrtHttpBodyDestroy(pBody);
		}
		xrtHttpBodyDestroy(pSource);
		testMemoryDebugDrain(
			"HTTP Body decode OOM leaked a logical allocation"
		);
		if ( bSucceeded ) {
			break;
		}
	}
	testRequire(
		bSucceeded &&
		(iFailures >= 1) &&
		(State.Denied == iFailures),
		"HTTP Body decode OOM did not cover its backing allocation"
	);
	printf("[PASS] HTTP Body content decode OOM\n");
	return 0;
}

