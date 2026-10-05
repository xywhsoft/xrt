/* 验证 xjwt 模块清单的最小依赖闭包能独立构建并完成一次签验。 */
#if defined(XJWT_FEATURE_XJWT) || defined(XJWT_SINGLE_HEADER)
#include "../src/internal/xjwt_internal.h"
#else
#define XRT_IMPLEMENTATION
#include "support/runtime.h"
#include "support/implementation.c"
#endif

int main(void)
{
	xvalue* claims = xrtValueObject();
	char* token;
	xvalue* verified;
	if(claims == NULL)
		return 1;
	token = xjwtHs256(claims, "minimal-test-secret", 60);
	xrtValueRelease(claims);
	if(token == NULL)
		return 2;
	verified = xjwtVerify(token, "minimal-test-secret", NULL);
	xrtFree(token);
	if(verified == NULL)
		return 3;
	xrtValueRelease(verified);
	return 0;
}
