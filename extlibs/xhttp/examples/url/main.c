#include <stdio.h>

#include <xhttp.h>



/* 展示零拷贝解析、HTTP target 写出和 RFC 3986 引用解析。 */
int main(void)
{
	xurl Url;
	char Target[128];
	str sResolved;
	size_t iSize;

	if ( !xrtUrlParse(
		XRT_STR_LITERAL("https://example.test/api/items?page=2#result"),
		&Url
	) || !xrtUrlTargetWrite(
		&Url, Target, sizeof(Target), &iSize
	) ) {
		return 1;
	}
	printf("host: %.*s\ntarget: %.*s\n",
		(int)Url.Host.Size, Url.Host.Data,
		(int)iSize, Target);

	sResolved = xrtUrlResolveBuild(
		&Url, XRT_STR_LITERAL("../health?full=1"), &iSize
	);
	if ( sResolved == NULL ) {
		return 1;
	}
	printf("resolved: %s\n", sResolved);
	xrtFree(sResolved);
	return 0;
}
