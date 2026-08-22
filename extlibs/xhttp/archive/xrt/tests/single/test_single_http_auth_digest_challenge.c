#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留 Digest challenge 的严格解析。 */
int main(void)
{
	xhttpdigestchallenge Challenge;
	char Output[32];
	size_t iSize;

	if ( !xrtHttpDigestChallengeRead(
		XRT_STR_LITERAL(
			"Digest realm=\"api\", nonce=\"n\", "
			"algorithm=SHA-256, qop=\"auth\""
		),
		Output, sizeof(Output), &iSize, &Challenge
	) || (Challenge.Algorithm != XHTTP_DIGEST_ALGORITHM_SHA256) ||
		(Challenge.Realm.Size != 3u) ||
		(memcmp(Challenge.Realm.Data, "api", 3u) != 0) ) {
		return 1;
	}
	return 0;
}
