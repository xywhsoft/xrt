#define XJWT_MODULE_XJWT
#include "xjwt.h"
#include <string.h>

int test_jwt_package(void)
{
	xvalue* claims = xrtValueObject();
	xvalue* verified;
	char* token;
	if ( claims == NULL ) return 1;
	token = xjwtHs256(claims, "package-consumer-secret", 60);
	xrtValueRelease(claims);
	if ( token == NULL ) return 2;
	verified = xjwtVerify(token, "incorrect-package-secret", NULL);
	if ( (verified != NULL) || (xjwtLastError() == 0) ) {
		xrtValueRelease(verified); xrtFree(token); return 3;
	}
	/* 调用方与库必须观察同一个 Core 错误上下文，不能各自嵌入运行时。 */
	if ( (xrtGetError() == NULL) || (strcmp(xrtErrorDomain(xrtGetError()), "xrt.jwt") != 0) ||
		 (xrtErrorCode(xrtGetError()) != xjwtLastError()) ) {
		xrtFree(token); return 5;
	}
	verified = xjwtVerify(token, "package-consumer-secret", NULL);
	xrtFree(token);
	if ( verified == NULL ) return 4;
	xrtValueRelease(verified);
	return 0;
}

#if !defined(AUTH_PACKAGE_COMPOSED)
int main(void) { return test_jwt_package(); }
#endif
