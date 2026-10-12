#include "../test.h"



/* 无效参数必须同步失败，且不能安排完成回调。 */
static void testHttpServerUpgradeUnexpected(
	xhttpconn* pConnection,
	xnetresult Result,
	xhttpupgrade Upgrade,
	const xerror* pError,
	ptr pData
)
{
	(void)pConnection;
	(void)Result;
	(void)Upgrade;
	(void)pError;
	(void)pData;
	testRequire(false, "invalid HTTP Upgrade invoked completion");
}



/* 覆盖三个公开提交层与空 Upgrade 清理。 */
int main(void)
{
	unsigned char Storage[sizeof(xhttpupgrade) + 2u];
	xhttpupgrade Upgrade;
	const xerror* pSaved;

	memset(&Upgrade, 0, sizeof(Upgrade));
	xrtHttpUpgradeAbort(NULL);
	xrtHttpUpgradeAbort(&Upgrade);
	testRequire(
		(Upgrade.Tcp == NULL) &&
		(Upgrade.Tls == NULL) &&
		(Upgrade.Buffered == 0),
		"empty HTTP Upgrade abort changed state"
	);
	memset(Storage, 0xA5, sizeof(Storage));
	memcpy(Storage + 1u, &Upgrade, sizeof(Upgrade));
	xrtHttpUpgradeAbort(
		(xhttpupgrade*)(void*)(Storage + 1u)
	);
	memcpy(&Upgrade, Storage + 1u, sizeof(Upgrade));
	testRequire(
		(Storage[0] == 0xA5u) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5u) &&
		(Upgrade.Tcp == NULL) &&
		(Upgrade.Tls == NULL) &&
		(Upgrade.Buffered == 0),
		"unaligned HTTP Upgrade abort corrupted its range"
	);
	xrtClearError();
	xrtHttpUpgradeAbort(
		(xhttpupgrade*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"wrapping HTTP Upgrade cleanup range was accepted"
	);
	xrtClearError();
	testRequire(
		(xrtHttpConnUpgradeResponse(
			NULL,
			NULL,
			testHttpServerUpgradeUnexpected,
			NULL
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTP Upgrade response plan was accepted"
	);
	pSaved = xrtGetError();
	xrtHttpUpgradeAbort(&Upgrade);
	testRequire(
		xrtGetError() == pSaved,
		"HTTP Upgrade abort replaced the current error"
	);
	xrtClearError();
	testRequire(
		(xrtHttpConnUpgrade(
			NULL,
			NULL,
			testHttpServerUpgradeUnexpected,
			NULL
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTP Upgrade Reply was accepted"
	);
	xrtClearError();
	testRequire(
		(xrtHttpConnUpgradeRaw(
			NULL,
			(xbytesview){ NULL, 0 },
			testHttpServerUpgradeUnexpected,
			NULL
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null raw HTTP Upgrade was accepted"
	);
	xrtClearError();
	printf("[PASS] HTTP server Upgrade invalid arguments\n");
	return 0;
}
