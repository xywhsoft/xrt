#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* 超长且带 quoted-pair 的 error_uri 分配失败时不得发布部分结果。 */
int main(void)
{
	char Value[512];
	char Output[384];
	char Before[384];
	xhttpbearerchallenge Challenge;
	size_t iOffset;
	size_t iSize = 71u;

	testRequire(testInstallFailAllocator(),
		"HTTP Bearer challenge failure allocator install failed");
	memcpy(Value, "Bearer error_uri=\"https:\\/\\/example.com\\/", 41u);
	iOffset = 41u;
	memset(Value + iOffset, 'a', 300u);
	iOffset += 300u;
	Value[iOffset++] = '"';
	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	memset(&Challenge, 0xA5, sizeof(Challenge));
	testRequire(!xrtHttpBearerChallengeRead(
		(xstrview){ Value, iOffset },
		Output, sizeof(Output), &iSize, &Challenge
	) && (iSize == 71u) && (Challenge.Flags == 0) &&
		(Challenge.Realm.Data == NULL) &&
		(Challenge.ErrorUri.Data == NULL) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP Bearer challenge OOM was not atomic");
	puts("[PASS] HTTP Bearer challenge OOM");
	return 0;
}
