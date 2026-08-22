#include <stdio.h>

#include <xrt.h>



/* 创建可直接放入 URL 和 Cookie 的密码安全随机令牌。 */
int main(void)
{
	str sToken = xrtSecureString(32);

	if ( sToken == NULL ) {
		return 1;
	}
	printf("token: %s\n", sToken);
	xrtSecureZero(sToken, 33);
	xrtFree(sToken);
	return 0;
}
