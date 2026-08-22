#include "../test.h"
#include "test_crypto_digest.h"



/* 核对 RFC 7748 的两个独立 X448 标量乘法向量。 */
static void testX448Vectors(void)
{
	static const struct {
		cstr Scalar;
		cstr Point;
		cstr Expected;
	} Cases[] = {
		{
			"3d262fddf9ec8e88495266fea19a34d28882acef045104d0d1aae121"
			"700a779c984c24f8cdd78fbff44943eba368f54b29259a4f1c600ad3",
			"06fce640fa3487bfda5f6cf2d5263f8aad88334cbd07437f020f08f9"
			"814dc031ddbdc38c19c6da2583fa5429db94ada18aa7a7fb4ef8a086",
			"ce3e4ff95a60dc6697da1db1d85e6afbdf79b50a2412d7546d5f239f"
			"e14fbaadeb445fc66a01b0779d98223961111e21766282f73dd96b6f"
		},
		{
			"203d494428b8399352665ddca42f9de8fef600908e0d461cb021f8c5"
			"38345dd77c3e4806e25f46d3315c44e0a5b4371282dd2c8d5be3095f",
			"0fbcc2f993cd56d3305b0b7d9e55d4c1a8fb5dbb52f8e9a1e9b6201b"
			"165d015894e56c4d3570bee52fe205e28a78b91cdfbde71ce8d157db",
			"884a02576239ff7a2f2f63b2db6a9ff37047ac13568e1e30fe63c4a7"
			"ad1b3ee3a5700df34321d62077e63633c575c1c954514e99da7c179d"
		}
	};
	uint8 Scalar[XRT_X448_PRIVATE_SIZE];
	uint8 Point[XRT_X448_PUBLIC_SIZE];
	uint8 Output[XRT_X448_SHARED_SIZE];

	for ( size_t i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		testCryptoDecode(Scalar, sizeof(Scalar), Cases[i].Scalar,
			"X448 scalar vector has the wrong size");
		testCryptoDecode(Point, sizeof(Point), Cases[i].Point,
			"X448 point vector has the wrong size");
		testRequire(xrtX448(Scalar, Point, Output),
			"X448 RFC vector failed");
		testCryptoDigest(Output, sizeof(Output), Cases[i].Expected,
			"X448 RFC result mismatch");
	}
}



/* 核对 RFC 7748 的 Alice/Bob 公钥与共享秘密向量。 */
static void testX448Exchange(void)
{
	uint8 AlicePrivate[XRT_X448_PRIVATE_SIZE];
	uint8 AlicePublic[XRT_X448_PUBLIC_SIZE];
	uint8 BobPrivate[XRT_X448_PRIVATE_SIZE];
	uint8 BobPublic[XRT_X448_PUBLIC_SIZE];
	uint8 Shared[XRT_X448_SHARED_SIZE];

	testCryptoDecode(AlicePrivate, sizeof(AlicePrivate),
		"9a8f4925d1519f5775cf46b04b5800d4ee9ee8bae8bc5565d498c28d"
		"d9c9baf574a9419744897391006382a6f127ab1d9ac2d8c0a598726b",
		"Alice X448 private vector has the wrong size");
	testCryptoDecode(BobPrivate, sizeof(BobPrivate),
		"1c306a7ac2a0e2e0990b294470cba339e6453772b075811d8fad0d1d"
		"6927c120bb5ee8972b0d3e21374c9c921b09d1b0366f10b65173992d",
		"Bob X448 private vector has the wrong size");
	testRequire(xrtX448Public(AlicePrivate, AlicePublic),
		"Alice X448 public derivation failed");
	testRequire(xrtX448Public(BobPrivate, BobPublic),
		"Bob X448 public derivation failed");
	testCryptoDigest(AlicePublic, sizeof(AlicePublic),
		"9b08f7cc31b7e3e67d22d5aea121074a273bd2b83de09c63faa73d2c"
		"22c5d9bbc836647241d953d40c5b12da88120d53177f80e532c41fa0",
		"Alice X448 public key mismatch");
	testCryptoDigest(BobPublic, sizeof(BobPublic),
		"3eb7a829b0cd20f5bcfc0b599b6feccf6da4627107bdb0d4f345b430"
		"27d8b972fc3e34fb4232a13ca706dcb57aec3dae07bdc1c67bf33609",
		"Bob X448 public key mismatch");
	testRequire(xrtX448Shared(AlicePrivate, BobPublic, Shared),
		"Alice X448 agreement failed");
	testCryptoDigest(Shared, sizeof(Shared),
		"07fff4181ac6cc95ec1c16a94a0f74d12da232ce40a77552281d282b"
		"b60c0b56fd2464c335543936521c24403085d59a449a5037514a879d",
		"X448 shared secret mismatch");
	testRequire(xrtX448Shared(BobPrivate, AlicePublic, BobPrivate) &&
		xrtConstTimeEqual(Shared, BobPrivate, sizeof(Shared)),
		"X448 reverse or in-place agreement mismatch");
}



/* 核对 RFC 7748 的一次和一千次迭代结果。 */
static void testX448Iterations(void)
{
	uint8 Scalar[XRT_X448_PRIVATE_SIZE] = { 5 };
	uint8 Point[XRT_X448_PUBLIC_SIZE] = { 5 };
	uint8 Output[XRT_X448_SHARED_SIZE];

	for ( size_t i = 0; i < 1000u; i++ ) {
		testRequire(xrtX448(Scalar, Point, Output),
			"X448 iteration failed");
		memcpy(Point, Scalar, sizeof(Point));
		memcpy(Scalar, Output, sizeof(Scalar));
		if ( i == 0 ) {
			testCryptoDigest(Scalar, sizeof(Scalar),
				"3f482c8a9f19b01e6c46ee9711d9dc14fd4bf67af30765c2ae2b846a"
				"4d23a8cd0db897086239492caf350b51f833868b9bc2b3bca9cf4113",
				"X448 first iteration mismatch");
		}
	}
	testCryptoDigest(Scalar, sizeof(Scalar),
		"aa3b4749d55b9daf1e5b00288826c467274ce3ebbdd5c17b975e09d4"
		"af6c67cf10d087202db88286e2b79fceea3ec353ef54faa26e219f38",
		"X448 thousandth iteration mismatch");
}



/* 验证非规范输入归约、全零底层结果和共享秘密失败原子性。 */
static void testX448LowOrder(void)
{
	static const cstr NonCanonical[] = {
		"ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
		"feffffffffffffffffffffffffffffffffffffffffffffffffffffff",
		"00000000000000000000000000000000000000000000000000000000"
		"ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
	};
	uint8 Private[XRT_X448_PRIVATE_SIZE] = { 7 };
	uint8 LowOrder[XRT_X448_PUBLIC_SIZE] = { 0 };
	uint8 Output[XRT_X448_SHARED_SIZE];
	uint8 Before[XRT_X448_SHARED_SIZE];
	uint8 Zero[XRT_X448_SHARED_SIZE] = { 0 };

	memset(Output, 0xA5, sizeof(Output));
	testRequire(xrtX448(Private, LowOrder, Output) &&
		xrtConstTimeEqual(Output, Zero, sizeof(Output)),
		"raw X448 rejected or changed an all-zero result");
	for ( size_t i = 0; i < (sizeof(NonCanonical) / sizeof(NonCanonical[0])); i++ ) {
		testCryptoDecode(LowOrder, sizeof(LowOrder), NonCanonical[i],
			"X448 non-canonical vector has the wrong size");
		memset(Output, 0xA5, sizeof(Output));
		testRequire(xrtX448(Private, LowOrder, Output) &&
			xrtConstTimeEqual(Output, Zero, sizeof(Output)),
			"X448 did not reduce a non-canonical low-order point");
	}
	memset(LowOrder, 0, sizeof(LowOrder));
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Before));
	xrtClearError();
	testRequire(!xrtX448Shared(Private, LowOrder, Output) &&
		xrtConstTimeEqual(Output, Before, sizeof(Output)) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.crypto") == 0) &&
		(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_KEY_AGREEMENT) &&
		(strcmp(xrtErrorOperation(xrtGetError()), "x448-shared") == 0),
		"X448 low-order rejection contract failed");
}



/* 验证输入输出任意重叠和空指针失败原子性。 */
static void testX448Edges(void)
{
	uint8 Private[XRT_X448_PRIVATE_SIZE] = { 3 };
	uint8 Point[XRT_X448_PUBLIC_SIZE] = { 5 };
	uint8 Expected[XRT_X448_SHARED_SIZE];
	uint8 Buffer[140];
	uint8 Before[XRT_X448_SHARED_SIZE];

	testRequire(xrtX448(Private, Point, Expected),
		"X448 edge reference failed");
	memcpy(Buffer, Private, sizeof(Private));
	memcpy(Buffer + 56u, Point, sizeof(Point));
	testRequire(xrtX448(Buffer, Buffer + 56u, Buffer + 28u) &&
		xrtConstTimeEqual(Buffer + 28u, Expected, sizeof(Expected)),
		"X448 partial overlap failed");
	memcpy(Buffer, Private, sizeof(Private));
	testRequire(xrtX448Public(Buffer, Buffer) &&
		xrtConstTimeEqual(Buffer, Expected, sizeof(Expected)),
		"X448 public in-place path failed");

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Before));
	xrtClearError();
	testRequire(!xrtX448(NULL, Point, Buffer) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Before)) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"X448 null scalar changed output or error contract");
	testRequire(!xrtX448(Private, NULL, Buffer),
		"X448 accepted a null point");
	testRequire(!xrtX448(Private, Point, NULL),
		"X448 accepted a null output");
}



/* 执行 X448 标准向量、安全边界和缓冲契约测试。 */
int main(void)
{
	testX448Vectors();
	testX448Exchange();
	testX448Iterations();
	testX448LowOrder();
	testX448Edges();
	return 0;
}
