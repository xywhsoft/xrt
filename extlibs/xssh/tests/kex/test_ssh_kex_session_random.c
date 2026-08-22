#include "../test.h"



/* 随机层只验证生产入口和依赖闭包；确定性状态机由核心测试穷举。 */
int main(void)
{
	uint8 arrPrivate[XSSH_CURVE25519_PRIVATE_SIZE];
	uint8 arrPublic[XSSH_CURVE25519_PUBLIC_SIZE];

	testRequire((xrtSshCurve25519KeyPair(
		arrPrivate,
		arrPublic
	) == XSSH_OK), "ssh KEX session random source failed");
	xrtSecureZero(arrPrivate, sizeof(arrPrivate));
	xrtSecureZero(arrPublic, sizeof(arrPublic));
	puts("ssh KEX session secure ephemeral key enabled");
	return 0;
}
