#include "../test.h"



/* 不应进入非法 TLS 参数测试的完成过程。 */
static void testHttpClientTlsUnexpectedDone(
	xhttp1call* pCall,
	const xhttp1callresult* pResult,
	ptr pData
)
{
	(void)pCall;
	(void)pResult;
	(void)pData;
	testRequire(false,
		"invalid HTTPS call unexpectedly completed");
}



/* 验证 HTTPS 驱动入口拒绝空 TLS Stream。 */
int main(void)
{
	xhttp1callevents Events;

	xrtHttp1CallEventsInit(&Events);
	Events.Done = testHttpClientTlsUnexpectedDone;
	xrtClearError();
	testRequire(!xrtHttp1CallTls(
		NULL,
		NULL,
		NULL,
		&Events
	), "null HTTPS call unexpectedly succeeded");
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTPS call error mismatch"
	);
	printf("[PASS] HTTP/1 TLS call invalid inputs\n");
	return 0;
}
