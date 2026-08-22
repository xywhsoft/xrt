#include <stdio.h>

#include <xrt/http_digest.h>



/* 为连续 HTTP 内容生成 SHA-256 Content-Digest 字段值。 */
int main(void)
{
	static const char Body[] = "{\"ok\":true}";
	char arrValue[96];
	size_t iSize;

	if ( !xrtHttpDigestSha256Write(
		Body, sizeof(Body) - 1u,
		arrValue, sizeof(arrValue), &iSize
	) ) {
		return 1;
	}
	printf("Content-Digest: %.*s\n", (int)iSize, arrValue);
	return 0;
}
