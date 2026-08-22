#include "../test.h"



/* 判断固定长度区域是否全部为零。 */
static bool testEd25519AllZero(const uint8* pData, size_t iSize)
{
	uint8 iOr = 0;

	for ( size_t i = 0; i < iSize; i++ ) {
		iOr |= pData[i];
	}
	return iOr == 0;
}



/* 验证随机密钥对、公钥复算和输出边界。 */
int main(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE];
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];
	uint8 Derived[XRT_ED25519_PUBLIC_SIZE];
	uint8 Buffer[96];
	uint8 Before[96];

	testRequire(xrtEd25519KeyPair(Seed, Public),
		"Ed25519 key-pair generation failed");
	testRequire(!testEd25519AllZero(Seed, sizeof(Seed)) &&
		xrtEd25519Public(Seed, Derived) &&
		xrtConstTimeEqual(Public, Derived, sizeof(Public)),
		"Ed25519 generated key pair is inconsistent");
	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Before));
	testRequire(!xrtEd25519KeyPair(Buffer, Buffer + 16u) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)),
		"Ed25519 key pair accepted overlapping outputs or changed them");
	testRequire(!xrtEd25519KeyPair(NULL, Buffer) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)),
		"Ed25519 key pair null seed changed public output");
	testRequire(!xrtEd25519KeyPair(Buffer, NULL) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)),
		"Ed25519 key pair null public changed seed output");
	printf("[PASS] crypto_ed25519_keypair\n");
	return 0;
}
