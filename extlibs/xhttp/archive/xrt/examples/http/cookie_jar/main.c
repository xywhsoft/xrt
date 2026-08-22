#include <stdio.h>

#include <xrt.h>



/* 展示普通 HTTP 客户端的一行存储和一行构建路径。 */
int main(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	str sCookie;
	size_t iSize;

	if ( (pJar == NULL) || (xrtCookieJarStoreUrl(
		pJar, XRT_STR_LITERAL("https://api.example.com/login"),
		XRT_STR_LITERAL("sid=abc123; Path=/; Secure; HttpOnly"),
		NULL
	) != XCOOKIE_STORE_STORED) ) {
		return 1;
	}
	sCookie = xrtCookieJarBuildUrl(
		pJar, XRT_STR_LITERAL("https://api.example.com/data"), &iSize
	);
	if ( sCookie == NULL ) {
		xrtCookieJarRelease(pJar);
		return 1;
	}
	printf("Cookie: %.*s\n", (int)iSize, sCookie);
	xrtFree(sCookie);
	xrtCookieJarRelease(pJar);
	return 0;
}
