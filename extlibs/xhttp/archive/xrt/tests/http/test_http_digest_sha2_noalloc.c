#include "../test_allocator.h"

#include <xrt/http_digest.h>



/* RFC 9530 SHA-2 连续内容生成和验证必须保持零分配。 */
int main(void)
{
	xhttpdigestcursor Cursor;
	xhttpdigest Digest;
	char arrOutput[128];
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"HTTP SHA-2 digest failure allocator install failed"
	);
	testRequire(
		xrtHttpDigestSha256Write(
			"body", 4u, arrOutput, sizeof(arrOutput), &iSize
		),
		"HTTP SHA-2 digest writer allocated"
	);
	xrtHttpDigestCursorInit(&Cursor);
	testRequire(
		(xrtHttpDigestNext(
			(xstrview){ arrOutput, iSize }, &Cursor, &Digest
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpDigestSha2Verify(
			&Digest, "body", 4u
		) == XHTTP_DIGEST_MATCH_OK),
		"HTTP SHA-2 digest verifier allocated"
	);
	printf("[PASS] http_digest_sha2_noalloc\n");
	return 0;
}
