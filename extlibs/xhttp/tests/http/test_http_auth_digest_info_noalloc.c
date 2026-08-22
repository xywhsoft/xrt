#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* Authentication-Info 的查询、解析与写出不得依赖堆分配。 */
int main(void)
{
	xhttpdigestinfo Info;
	char Decoded[96];
	char Value[192];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"HTTP Digest info failure allocator install failed");
	testRequire(xrtHttpDigestInfoRead(
		XRT_STR_LITERAL(
			"nextnonce=\"next\", qop=auth, "
			"rspauth=\"0123456789abcdef0123456789abcdef"
			"0123456789abcdef0123456789abcdef\", "
			"cnonce=\"client\", nc=00000001"
		),
		XHTTP_DIGEST_ALGORITHM_SHA256,
		NULL, 0, &iSize, &Info
	), "HTTP Digest info query allocated");
	testRequire(xrtHttpDigestInfoRead(
		XRT_STR_LITERAL(
			"nextnonce=\"next\", qop=auth, "
			"rspauth=\"0123456789abcdef0123456789abcdef"
			"0123456789abcdef0123456789abcdef\", "
			"cnonce=\"client\", nc=00000001"
		),
		XHTTP_DIGEST_ALGORITHM_SHA256,
		Decoded, sizeof(Decoded), &iSize, &Info
	), "HTTP Digest info reader allocated");
	testRequire(xrtHttpDigestInfoWrite(
		&Info, Value, sizeof(Value), &iSize
	), "HTTP Digest info writer allocated");
	puts("[PASS] HTTP Digest Authentication-Info no allocation");
	return 0;
}
