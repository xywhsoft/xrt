#include "../test.h"



/* 判断固定长度缓冲是否全部为零。 */
static bool testX448AllZero(const uint8* pData, size_t iSize)
{
	uint8 iValue = 0;

	for ( size_t i = 0; i < iSize; i++ ) {
		iValue |= pData[i];
	}
	return iValue == 0;
}



/* 验证随机密钥对规范化、公钥派生和双方协商。 */
static void testX448KeyPairs(void)
{
	uint8 AlicePrivate[XRT_X448_PRIVATE_SIZE];
	uint8 AlicePublic[XRT_X448_PUBLIC_SIZE];
	uint8 BobPrivate[XRT_X448_PRIVATE_SIZE];
	uint8 BobPublic[XRT_X448_PUBLIC_SIZE];
	uint8 Derived[XRT_X448_PUBLIC_SIZE];
	uint8 AliceShared[XRT_X448_SHARED_SIZE];
	uint8 BobShared[XRT_X448_SHARED_SIZE];

	testRequire(xrtX448KeyPair(AlicePrivate, AlicePublic),
		"Alice X448 key-pair generation failed");
	testRequire(xrtX448KeyPair(BobPrivate, BobPublic),
		"Bob X448 key-pair generation failed");
	testRequire(((AlicePrivate[0] & 3u) == 0) &&
		((AlicePrivate[55] & 0x80u) != 0) &&
		!testX448AllZero(AlicePrivate, sizeof(AlicePrivate)) &&
		!testX448AllZero(AlicePublic, sizeof(AlicePublic)),
		"X448 generated key is not canonical or is all zero");
	testRequire(xrtX448Public(AlicePrivate, Derived) &&
		xrtConstTimeEqual(Derived, AlicePublic, sizeof(Derived)),
		"X448 generated public key does not match its private key");
	testRequire(xrtX448Shared(AlicePrivate, BobPublic, AliceShared) &&
		xrtX448Shared(BobPrivate, AlicePublic, BobShared) &&
		xrtConstTimeEqual(AliceShared, BobShared, sizeof(AliceShared)) &&
		!testX448AllZero(AliceShared, sizeof(AliceShared)),
		"X448 generated key agreement mismatch");
}



/* 失败时不得发布半个密钥对或覆盖相邻输出。 */
static void testX448KeyPairEdges(void)
{
	uint8 Buffer[112];
	uint8 Before[112];

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Before));
	xrtClearError();
	testRequire(!xrtX448KeyPair(Buffer, Buffer + 28u) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"X448 key-pair accepted overlapping outputs");
	testRequire(!xrtX448KeyPair(NULL, Buffer) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)),
		"X448 key-pair null private output changed public output");
	testRequire(!xrtX448KeyPair(Buffer, NULL) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Buffer)),
		"X448 key-pair null public output changed private output");
}



/* 执行 X448 随机密钥对功能和失败原子性测试。 */
int main(void)
{
	testX448KeyPairs();
	testX448KeyPairEdges();
	return 0;
}
