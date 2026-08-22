#include "../test.h"



/* 每个工作线程反复覆盖自己的 Cookie 并并发读取共享 Jar。 */
typedef struct test_cookie_thread {
	xcookiejar* Jar;
	int Index;
} test_cookie_thread;



/* 执行并发存取循环。 */
static int32 testCookieJarThread(ptr pData)
{
	test_cookie_thread* pThread = (test_cookie_thread*)pData;
	char Field[64];
	char Output[512];
	size_t i;

	for ( i = 0; i < 800u; i++ ) {
		xcookiestorecontext Store;
		xcookierequestcontext Request;
		size_t iField;
		size_t iOutput;

		iField = (size_t)snprintf(
			Field, sizeof(Field), "worker%d=%u; Path=/",
			pThread->Index, (unsigned)i
		);
		memset(&Store, 0, sizeof(Store));
		Store.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_HAS_NOW |
			XCOOKIE_STORE_SAME_SITE;
		Store.URL = XRT_STR_LITERAL("https://example.test/");
		Store.Now = (xtime)i * XRT_TIME_SECOND;
		if ( xrtCookieJarStore(
			pThread->Jar, &Store, (xstrview){ Field, iField }, NULL
		) != XCOOKIE_STORE_STORED ) {
			return 1;
		}
		memset(&Request, 0, sizeof(Request));
		Request.Flags = XCOOKIE_REQUEST_HTTP_API |
			XCOOKIE_REQUEST_HAS_NOW | XCOOKIE_REQUEST_SAME_SITE;
		Request.URL = Store.URL;
		Request.Now = Store.Now;
		if ( !xrtCookieJarWrite(
			pThread->Jar, &Request, Output, sizeof(Output), &iOutput
		) ) {
			return 2;
		}
	}
	return 0;
}



/* 验证共享 Jar 的系统 mutex 契约和最终主键完整性。 */
int main(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	test_cookie_thread Contexts[4];
	xthread* Threads[4];
	xcookiesnapshot* pSnapshot;
	size_t i;

	testRequire(pJar != NULL, "cookie jar thread create failed");
	for ( i = 0; i < 4u; i++ ) {
		Contexts[i].Jar = pJar;
		Contexts[i].Index = (int)i;
		Threads[i] = xrtThreadCreate(
			testCookieJarThread, &Contexts[i], 0
		);
		testRequire(Threads[i] != NULL,
			"cookie jar worker create failed");
	}
	for ( i = 0; i < 4u; i++ ) {
		testRequire((xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0),
			"cookie jar worker failed");
		xrtThreadDestroy(Threads[i]);
	}
	pSnapshot = xrtCookieJarSnapshot(
		pJar, INT64_C(1000000000000)
	);
	testRequire((pSnapshot != NULL) &&
		(xrtCookieSnapshotCount(pSnapshot) == 4u),
		"cookie jar concurrent overwrite lost a storage key");
	xrtCookieSnapshotDestroy(pSnapshot);
	xrtCookieJarRelease(pJar);
	printf("[PASS] cookie_jar_threads\n");
	return 0;
}
