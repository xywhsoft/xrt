#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留 CookieJar 存取和稳定快照。 */
int main(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	xcookiesnapshot* pSnapshot;
	str sValue;
	size_t iSize;

	if ( (pJar == NULL) || (xrtCookieJarStoreUrl(
		pJar, XRT_STR_LITERAL("https://example.com/"),
		XRT_STR_LITERAL("sid=1; Path=/; Secure"), NULL
	) != XCOOKIE_STORE_STORED) ) {
		return 1;
	}
	sValue = xrtCookieJarBuildUrl(
		pJar, XRT_STR_LITERAL("https://example.com/"), &iSize
	);
	pSnapshot = xrtCookieJarSnapshot(pJar, xrtNow());
	if ( (sValue == NULL) || (iSize != 5u) ||
		(memcmp(sValue, "sid=1", 5u) != 0) ||
		(pSnapshot == NULL) ||
		(xrtCookieSnapshotCount(pSnapshot) != 1u) ) {
		xrtFree(sValue);
		xrtCookieSnapshotDestroy(pSnapshot);
		xrtCookieJarRelease(pJar);
		return 1;
	}
	xrtFree(sValue);
	xrtCookieSnapshotDestroy(pSnapshot);
	xrtCookieJarRelease(pJar);
	return 0;
}
