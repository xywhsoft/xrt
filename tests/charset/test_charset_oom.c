#include "../test.h"
#include "../test_allocator.h"



/* 通用转码必须完整传播分配失败。 */
int main(void)
{
	bytes pText;

	testRequire(testInstallFailAllocator(), "failed to install charset OOM allocator");
	pText = xrtTranscode((xbytesview){ (cbytes)"text", 4 },
		XENCODING_UTF8, XENCODING_UTF16_LE, XUTF_STRICT, false, NULL);
	testRequire(pText == NULL, "transcode ignored allocation failure");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"transcode did not preserve OOM error");
	return 0;
}
