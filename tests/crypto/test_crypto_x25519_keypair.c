#include "../test.h"



/* 判断固定长度缓冲是否全部为零。 */
static bool testX25519AllZero(const uint8* pData, size_t iSize)
{
	uint8 iValue = 0;

	for ( size_t i = 0; i < iSize; i++ ) {
		iValue |= pData[i];
	}
	return iValue == 0;
}



/* 验证随机密钥对规范化、公钥派生和双方协商。 */
static void testX25519KeyPairs(void)
{
	uint8 AlicePrivate[XRT_X25519_PRIVATE_SIZE];
	uint8 AlicePublic[XRT_X25519_PUBLIC_SIZE];
	uint8 BobPrivate[XRT_X25519_PRIVATE_SIZE];
	uint8 BobPublic[XRT_X25519_PUBLIC_SIZE];
	uint8 Derived[XRT_X25519_PUBLIC_SIZE];
	uint8 AliceShared[XRT_X25519_SHARED_SIZE];
	uint8 BobShared[XRT_X25519_SHARED_SIZE];

	testRequire(xrtX25519KeyPair(AlicePrivate, AlicePublic),
		"Alice X25519 key-pair generation failed");
	testRequire(xrtX25519KeyPair(BobPrivate, BobPublic),
		"Bob X25519 key-pair generation failed");
	testRequire(((AlicePrivate[0] & 7u) == 0) &&
		((AlicePrivate[31] & 0x80u) == 0) &&
		((AlicePrivate[31] & 0x40u) != 0) &&
		!testX25519AllZero(AlicePrivate, sizeof(AlicePrivate)) &&
		!testX25519AllZero(AlicePublic, sizeof(AlicePublic)),
		"X25519 generated key is not canonical or is all zero");
	testRequire(xrtX25519Public(AlicePrivate, Derived) &&
		xrtConstTimeEqual(Derived, AlicePublic, sizeof(Derived)),
		"X25519 generated public key does not match its private key");
	testRequire(xrtX25519Shared(AlicePrivate, BobPublic, AliceShared) &&
		xrtX25519Shared(BobPrivate, AlicePublic, BobShared) &&
		xrtConstTimeEqual(AliceShared, BobShared, sizeof(AliceShared)) &&
		!testX25519AllZero(AliceShared, sizeof(AliceShared)),
		"X25519 generated key agreement mismatch");
}



/* 失败时不得发布半个密钥对或覆盖相邻输出。 */
static void testX25519KeyPairEdges(void)
{
	uint8 Buffer[64];
	uint8 Before[64];

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Before));
	xrtClearError();
	testRequire(!xrtX25519KeyPair(Buffer, Buffer + 16u) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"X25519 key-pair accepted overlapping outputs");
	testRequire(!xrtX25519KeyPair(NULL, Buffer) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)),
		"X25519 key-pair null private output changed public output");
	testRequire(!xrtX25519KeyPair(Buffer, NULL) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)),
		"X25519 key-pair null public output changed private output");
}



/* 执行 X25519 随机密钥对功能和失败原子性测试。 */
int main(void)
{
	testX25519KeyPairs();
	testX25519KeyPairEdges();
	return 0;
}
