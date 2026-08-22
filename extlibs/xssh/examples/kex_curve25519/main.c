#include <stdio.h>
#include <xssh.h>



/* 从显式私钥导出 Curve25519 临时公钥。 */
int main(void)
{
	unsigned char arrPrivate[32] = { 1u };
	unsigned char arrPublic[32];

	if ( xrtSshCurve25519Public(arrPrivate, arrPublic) != XSSH_OK ) {
		return 1;
	}
	printf("public-prefix=%02x%02x\n", arrPublic[0], arrPublic[1]);
	return 0;
}
