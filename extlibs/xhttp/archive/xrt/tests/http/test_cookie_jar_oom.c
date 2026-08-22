#include "../test.h"



#define TEST_COOKIE_VALUE_SIZE 1200u
#define TEST_COOKIE_FIELD_SIZE (TEST_COOKIE_VALUE_SIZE + 32u)
#define TEST_COOKIE_OUTPUT_SIZE 16384u



/* 可调失败点分配器扫描 CookieJar 的全部拥有型路径。 */
typedef struct test_cookie_jar_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_cookie_jar_allocator;



/* 在目标分配序号失败，其余请求交给 C 运行库。 */
static ptr testCookieJarAlloc(ptr pContext, size_t iSize)
{
	test_cookie_jar_allocator* pState =
		(test_cookie_jar_allocator*)pContext;
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



/* 重分配失败时保持原块有效并维持存活计数。 */
static ptr testCookieJarRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_cookie_jar_allocator* pState =
		(test_cookie_jar_allocator*)pContext;
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



/* 释放成功分配的底层内存并维护存活计数。 */
static void testCookieJarFree(ptr pContext, ptr pMemory)
{
	test_cookie_jar_allocator* pState =
		(test_cookie_jar_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"CookieJar OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 生成超过小对象池上限的有效 Cookie 字段。 */
static size_t testCookieJarField(
	char* sField,
	char cName,
	char cValue,
	bool bDelete
)
{
	size_t iSize = 2u + TEST_COOKIE_VALUE_SIZE;

	sField[0] = cName;
	sField[1] = '=';
	memset(sField + 2u, cValue, TEST_COOKIE_VALUE_SIZE);
	if ( bDelete ) {
		memcpy(sField + iSize, "; Max-Age=0; Path=/", 19u);
		iSize += 19u;
	} else {
		memcpy(sField + iSize, "; Path=/", 8u);
		iSize += 8u;
	}
	return iSize;
}



/* 验证 Cookie 字段的名称顺序和完整大值。 */
static bool testCookieJarWire(
	cbytes pData,
	size_t iSize,
	cstr sNames,
	cstr sValues
)
{
	size_t iCount = strlen(sNames);
	size_t iExpected = iCount * (TEST_COOKIE_VALUE_SIZE + 2u);
	size_t iOffset = 0;
	size_t i;

	if ( iCount != 0 ) {
		iExpected += (iCount - 1u) * 2u;
	}
	if ( (strlen(sValues) != iCount) || (iSize != iExpected) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( i != 0 ) {
			if ( (pData[iOffset] != ';') ||
				(pData[iOffset + 1u] != ' ') ) {
				return false;
			}
			iOffset += 2u;
		}
		if ( (pData[iOffset] != (uint8)sNames[i]) ||
			(pData[iOffset + 1u] != '=') ) {
			return false;
		}
		iOffset += 2u;
		for ( size_t j = 0; j < TEST_COOKIE_VALUE_SIZE; j++ ) {
			if ( pData[iOffset + j] != (uint8)sValues[i] ) {
				return false;
			}
		}
		iOffset += TEST_COOKIE_VALUE_SIZE;
	}
	return true;
}



/* 在故障已经命中后读取 Jar，验证失败操作没有改变可见内容。 */
static bool testCookieJarText(
	xcookiejar* pJar,
	cstr sNames,
	cstr sValues,
	xtime iNow
)
{
	xcookierequestcontext Request;
	uint8 Output[TEST_COOKIE_OUTPUT_SIZE];
	size_t iSize;

	memset(&Request, 0, sizeof(Request));
	Request.Flags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_HAS_NOW | XCOOKIE_REQUEST_SAME_SITE |
		XCOOKIE_REQUEST_SAFE_METHOD;
	Request.URL = XRT_STR_LITERAL("https://example.com/path/item");
	Request.Now = iNow;
	return xrtCookieJarWrite(
		pJar, &Request, Output, sizeof(Output), &iSize
	) && testCookieJarWire(Output, iSize, sNames, sValues);
}



/* 验证故障结果是内存错误，并清除错误供后续状态检查使用。 */
static void testCookieJarOomError(cstr sMessage)
{
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, sMessage);
	xrtClearError();
}



/* 比较两个快照的稳定字段，确认失败构建没有更新访问状态。 */
static bool testCookieJarSnapshotsEqual(
	const xcookiesnapshot* pLeft,
	const xcookiesnapshot* pRight
)
{
	size_t iCount = xrtCookieSnapshotCount(pLeft);
	size_t i;

	if ( iCount != xrtCookieSnapshotCount(pRight) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		const xcookieinfo* pA = xrtCookieSnapshotAt(pLeft, i);
		const xcookieinfo* pB = xrtCookieSnapshotAt(pRight, i);

		if ( (pA == NULL) || (pB == NULL) ||
			(pA->Flags != pB->Flags) ||
			(pA->SameSite != pB->SameSite) ||
			(pA->Priority != pB->Priority) ||
			(pA->Expires != pB->Expires) ||
			(pA->Created != pB->Created) ||
			(pA->Accessed != pB->Accessed) ||
			(pA->Name.Size != pB->Name.Size) ||
			(pA->Value.Size != pB->Value.Size) ||
			(memcmp(pA->Name.Data, pB->Name.Data, pA->Name.Size) != 0) ||
			(memcmp(pA->Value.Data, pB->Value.Data, pA->Value.Size) != 0) ) {
			return false;
		}
	}
	return true;
}



/* 存储一个有效同站 Cookie，并在 OOM 时验证预期旧状态。 */
static bool testCookieJarStore(
	xcookiejar* pJar,
	char cName,
	char cValue,
	bool bDelete,
	xtime iNow,
	size_t iExpectedCount,
	cstr sExpectedNames,
	cstr sExpectedValues
)
{
	xcookiestorecontext Context;
	xcookiestorestatus Status;
	char Field[TEST_COOKIE_FIELD_SIZE];
	size_t iField;

	iField = testCookieJarField(Field, cName, cValue, bDelete);
	memset(&Context, 0, sizeof(Context));
	Context.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_HAS_NOW |
		XCOOKIE_STORE_SAME_SITE;
	Context.URL = XRT_STR_LITERAL("https://example.com/path/item");
	Context.Now = iNow;
	Status = xrtCookieJarStore(
		pJar, &Context, (xstrview){ Field, iField }, NULL
	);
	if ( (!bDelete && (Status == XCOOKIE_STORE_STORED)) ||
		(bDelete && (Status == XCOOKIE_STORE_REMOVED)) ) {
		return true;
	}
	testRequire(Status == XCOOKIE_STORE_ERROR,
		"CookieJar OOM valid store returned unexpected status");
	testCookieJarOomError("CookieJar store did not publish OOM");
	testRequire((xrtCookieJarCount(pJar) == iExpectedCount) &&
		testCookieJarText(
			pJar, sExpectedNames, sExpectedValues, iNow + 1
		), "CookieJar store OOM changed visible state");
	return false;
}



/* 在一个故障点下覆盖添加、替换、预留、构建、快照、淘汰和删除。 */
static bool testCookieJarOomAttempt(void)
{
	xcookiejarconfig Config;
	xcookierequestcontext Request;
	xcookiejar* pJar = NULL;
	xcookiesnapshot* pBefore = NULL;
	xcookiesnapshot* pAfter = NULL;
	str sBuilt = NULL;
	size_t iSize = 0;
	bool bComplete = false;

	xrtCookieJarConfigInit(&Config);
	Config.InitialCookies = 8u;
	Config.MaxCookies = 8u;
	Config.MaxCookiesPerDomain = 8u;
	pJar = xrtCookieJarCreate(&Config);
	if ( pJar == NULL ) {
		testCookieJarOomError("CookieJar create did not publish OOM");
		goto done;
	}
	if ( !testCookieJarStore(pJar, 'a', '1', false, 1, 0u, "", "") ||
		!testCookieJarStore(pJar, 'b', '2', false, 2, 1u, "a", "1") ||
		!testCookieJarStore(pJar, 'a', 'u', false, 3, 2u, "ab", "12") ||
		!testCookieJarStore(pJar, 'c', '3', false, 4, 2u, "ab", "u2") ) {
		goto done;
	}

	pBefore = xrtCookieJarSnapshot(pJar, 5);
	if ( pBefore == NULL ) {
		testCookieJarOomError("CookieJar snapshot did not publish OOM");
		testRequire(testCookieJarText(pJar, "abc", "u23", 6),
			"CookieJar snapshot OOM changed visible state");
		goto done;
	}
	memset(&Request, 0, sizeof(Request));
	Request.Flags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_HAS_NOW | XCOOKIE_REQUEST_SAME_SITE |
		XCOOKIE_REQUEST_SAFE_METHOD;
	Request.URL = XRT_STR_LITERAL("https://example.com/path/item");
	Request.Now = 10;
	sBuilt = xrtCookieJarBuild(pJar, &Request, &iSize);
	if ( sBuilt == NULL ) {
		testCookieJarOomError("CookieJar build did not publish OOM");
		pAfter = xrtCookieJarSnapshot(pJar, 11);
		testRequire((pAfter != NULL) &&
			testCookieJarSnapshotsEqual(pBefore, pAfter),
			"CookieJar build OOM changed access or content state");
		goto done;
	}
	testRequire(testCookieJarWire(
		(cbytes)sBuilt, iSize, "abc", "u23"
	), "CookieJar build produced unexpected content");
	xrtFree(sBuilt);
	sBuilt = NULL;
	xrtCookieSnapshotDestroy(pBefore);
	pBefore = NULL;

	if ( !testCookieJarStore(pJar, 'd', '4', false, 20, 3u, "abc", "u23") ||
		!testCookieJarStore(pJar, 'e', '5', false, 21, 4u, "abcd", "u234") ||
		!testCookieJarStore(pJar, 'f', '6', false, 22, 5u, "abcde", "u2345") ||
		!testCookieJarStore(pJar, 'g', '7', false, 23, 6u, "abcdef", "u23456") ||
		!testCookieJarStore(pJar, 'h', '8', false, 24, 7u, "abcdefg", "u234567") ||
		!testCookieJarStore(pJar, 'i', '9', false, 25, 8u, "abcdefgh", "u2345678") ) {
		goto done;
	}
	testRequire((xrtCookieJarCount(pJar) == 8u) &&
		testCookieJarText(pJar, "bcdefghi", "23456789", 30),
		"CookieJar full-capacity eviction mismatch");

	if ( !testCookieJarStore(
		pJar, 'b', 'x', true, 40, 8u, "bcdefghi", "23456789"
	) ) {
		goto done;
	}
	testRequire((xrtCookieJarCount(pJar) == 7u) &&
		testCookieJarText(pJar, "cdefghi", "3456789", 41),
		"CookieJar deletion mismatch");
	pAfter = xrtCookieJarSnapshot(pJar, 50);
	if ( pAfter == NULL ) {
		testCookieJarOomError("CookieJar final snapshot did not publish OOM");
		testRequire(testCookieJarText(pJar, "cdefghi", "3456789", 51),
			"CookieJar final snapshot OOM changed visible state");
		goto done;
	}
	testRequire(xrtCookieSnapshotCount(pAfter) == 7u,
		"CookieJar final snapshot count mismatch");
	bComplete = true;

done:
	xrtCookieSnapshotDestroy(pAfter);
	xrtCookieSnapshotDestroy(pBefore);
	xrtFree(sBuilt);
	xrtCookieJarRelease(pJar);
	xrtClearError();
	return bComplete;
}



/* 扫描实际 backing 分配序号并要求失败路径回到同一存活基线。 */
int main(void)
{
	test_cookie_jar_allocator State = { 0, 0, 0 };
	xallocator Allocator = {
		&State,
		testCookieJarAlloc,
		testCookieJarRealloc,
		testCookieJarFree
	};
	size_t iBaseline;
	size_t iMaxCalls;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"CookieJar OOM allocator install failed");
	testRequire(testCookieJarOomAttempt(),
		"CookieJar OOM warm-up failed");
	iMaxCalls = State.Calls + 1u;
	for ( iFail = 1; iFail <= iMaxCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		(void)testCookieJarOomAttempt();
	}
	testMemoryDebugDrain(
		"CookieJar OOM memory debug fault warm-up reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= iMaxCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testCookieJarOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"CookieJar OOM memory debug reset failed"
		);
		if ( State.Live != iBaseline ) {
			printf(
				"[DIAG] CookieJar OOM fail_at=%u live=%u baseline=%u\n",
				(unsigned)iFail,
				(unsigned)State.Live,
				(unsigned)iBaseline
			);
		}
		testRequire(State.Live == iBaseline,
			"CookieJar OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"CookieJar OOM sweep missed failure or success paths");
	printf(
		"[PASS] CookieJar OOM fault_points=%u\n",
		(unsigned)iFailures
	);
	return 0;
}
