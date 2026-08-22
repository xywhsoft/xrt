#include <stdio.h>

#include <xrt.h>



/* 仅在必须兼容历史端点时选择 MD5 Digest 模块。 */
int main(void)
{
	char Digest[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;

	if ( !xrtHttpDigestHash(
		XHTTP_DIGEST_ALGORITHM_MD5,
		"legacy", 6u,
		Digest, sizeof(Digest), &iSize
	) ) {
		return 1;
	}
	printf("MD5=%.*s\n", (int)iSize, Digest);
	return 0;
}
