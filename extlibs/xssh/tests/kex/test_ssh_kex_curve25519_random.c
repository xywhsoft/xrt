#include "../test.h"



/* 验证两个安全随机临时密钥对得到相同共享秘密。 */
int main(void)
{
	unsigned char arrPrivateA[32];
	unsigned char arrPublicA[32];
	unsigned char arrPrivateB[32];
	unsigned char arrPublicB[32];
	unsigned char arrSharedA[32];
	unsigned char arrSharedB[32];

	testRequire((xrtSshCurve25519KeyPair(
		arrPrivateA,
		arrPublicA
	) == XSSH_OK) && (xrtSshCurve25519KeyPair(
		arrPrivateB,
		arrPublicB
	) == XSSH_OK) && (xrtSshCurve25519Shared(
		arrPrivateA,
		arrPublicB,
		arrSharedA
	) == XSSH_OK) && (xrtSshCurve25519Shared(
		arrPrivateB,
		arrPublicA,
		arrSharedB
	) == XSSH_OK) &&
		(memcmp(arrSharedA, arrSharedB, sizeof(arrSharedA)) == 0),
		"ssh curve25519 random key exchange failed");
	return 0;
}
