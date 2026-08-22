#include <stdio.h>
#include <xssh.h>



/* 生成生产用 Curve25519 临时密钥对。 */
int main(void)
{
	unsigned char arrPrivate[32];
	unsigned char arrPublic[32];

	if ( xrtSshCurve25519KeyPair(arrPrivate, arrPublic) != XSSH_OK ) {
		return 1;
	}
	xrtSecureZero(arrPrivate, sizeof(arrPrivate));
	printf("ephemeral-public-size=%zu\n", sizeof(arrPublic));
	return 0;
}
