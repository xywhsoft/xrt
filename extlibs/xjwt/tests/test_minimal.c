/* 验证 xjwt-xrt.h 的最小模块闭包能独立构建并完成一次签验。 */
#define XRT_IMPLEMENTATION
#include "../xjwt.c"

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
