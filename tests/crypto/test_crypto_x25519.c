#include "../test.h"
#include "test_crypto_digest.h"

/* 核对 RFC 7748 的两个独立标量乘法向量和输入最高位屏蔽规则。 */
static void testX25519Vectors(void)
{
	static const struct {
		cstr Scalar;
		cstr Point;
		cstr Expected;
	} Cases[] = {
		{
			"a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4",
			"e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c",
			"c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552"
		},
		{
			"4b66e9d4d1b4673c5ad22691957d6af5c11b6421e0ea01d42ca4169e7918ba0d",
			"e5210f12786811d3f4b7959d0538ae2c31dbe7106fc03c3efc4cd549c715a493",
			"95cbde9476e8907d7aade45cb4b873f88b595a68799fa152e6f8f7647aac7957"
		}
	};
	uint8 Scalar[XRT_X25519_PRIVATE_SIZE];
	uint8 Point[XRT_X25519_PUBLIC_SIZE];
	uint8 Output[XRT_X25519_SHARED_SIZE];
	uint8 MaskedOutput[XRT_X25519_SHARED_SIZE];

	for ( size_t i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		testCryptoDecode(Scalar, sizeof(Scalar), Cases[i].Scalar,
			"X25519 scalar vector has the wrong size");
		testCryptoDecode(Point, sizeof(Point), Cases[i].Point,
			"X25519 point vector has the wrong size");
		testRequire(xrtX25519(Scalar, Point, Output),
			"X25519 RFC vector failed");
		testCryptoDigest(Output, sizeof(Output), Cases[i].Expected,
			"X25519 RFC result mismatch");
	}

	Point[31] &= 0x7Fu;
	testRequire(xrtX25519(Scalar, Point, MaskedOutput) &&
		xrtConstTimeEqual(Output, MaskedOutput, sizeof(Output)),
		"X25519 did not mask the point's final input bit");
}



/* 核对 RFC 7748 的 Alice/Bob 公钥与共享秘密向量。 */
static void testX25519Exchange(void)
{
	uint8 AlicePrivate[XRT_X25519_PRIVATE_SIZE];
	uint8 AlicePublic[XRT_X25519_PUBLIC_SIZE];
	uint8 BobPrivate[XRT_X25519_PRIVATE_SIZE];
	uint8 BobPublic[XRT_X25519_PUBLIC_SIZE];
	uint8 Shared[XRT_X25519_SHARED_SIZE];

	testCryptoDecode(AlicePrivate, sizeof(AlicePrivate),
		"77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a",
		"Alice X25519 private vector has the wrong size");
	testCryptoDecode(BobPrivate, sizeof(BobPrivate),
		"5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb",
		"Bob X25519 private vector has the wrong size");
	testRequire(xrtX25519Public(AlicePrivate, AlicePublic),
		"Alice X25519 public derivation failed");
	testRequire(xrtX25519Public(BobPrivate, BobPublic),
		"Bob X25519 public derivation failed");
	testCryptoDigest(AlicePublic, sizeof(AlicePublic),
		"8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a",
		"Alice X25519 public key mismatch");
	testCryptoDigest(BobPublic, sizeof(BobPublic),
		"de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f",
		"Bob X25519 public key mismatch");
	testRequire(xrtX25519Shared(AlicePrivate, BobPublic, Shared),
		"Alice X25519 agreement failed");
	testCryptoDigest(Shared, sizeof(Shared),
		"4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742",
		"X25519 shared secret mismatch");
	testRequire(xrtX25519Shared(BobPrivate, AlicePublic, BobPrivate) &&
		xrtConstTimeEqual(Shared, BobPrivate, sizeof(Shared)),
		"X25519 reverse or in-place agreement mismatch");
}



/* 核对 RFC 7748 的一次和一千次迭代结果。 */
static void testX25519Iterations(void)
{
	uint8 Scalar[XRT_X25519_PRIVATE_SIZE] = { 9 };
	uint8 Point[XRT_X25519_PUBLIC_SIZE] = { 9 };
	uint8 Output[XRT_X25519_SHARED_SIZE];

	for ( size_t i = 0; i < 1000u; i++ ) {
		testRequire(xrtX25519(Scalar, Point, Output),
			"X25519 iteration failed");
		memcpy(Point, Scalar, sizeof(Point));
		memcpy(Scalar, Output, sizeof(Scalar));
		if ( i == 0 ) {
			testCryptoDigest(Scalar, sizeof(Scalar),
				"422c8e7a6227d7bca1350b3e2bb7279f7897b87bb6854b783c60e80311ae3079",
				"X25519 first iteration mismatch");
		}
	}
	testCryptoDigest(Scalar, sizeof(Scalar),
		"684cf59ba83309552800ef566f2f4d3c1c3887c49360e3875f2eb94d99532c51",
		"X25519 thousandth iteration mismatch");
}



/* 验证底层原语允许零结果，而协议便利层拒绝它且不修改输出。 */
static void testX25519LowOrder(void)
{
	static const cstr NonCanonical[] = {
		"edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
		"eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f"
	};
	uint8 Private[XRT_X25519_PRIVATE_SIZE] = { 7 };
	uint8 LowOrder[XRT_X25519_PUBLIC_SIZE] = { 0 };
	uint8 Output[XRT_X25519_SHARED_SIZE];
	uint8 Before[XRT_X25519_SHARED_SIZE];
	uint8 Zero[XRT_X25519_SHARED_SIZE] = { 0 };

	memset(Output, 0xA5, sizeof(Output));
	testRequire(xrtX25519(Private, LowOrder, Output) &&
		xrtConstTimeEqual(Output, Zero, sizeof(Output)),
		"raw X25519 rejected or changed an all-zero result");
	for ( size_t i = 0; i < (sizeof(NonCanonical) / sizeof(NonCanonical[0])); i++ ) {
		testCryptoDecode(LowOrder, sizeof(LowOrder), NonCanonical[i],
			"X25519 non-canonical vector has the wrong size");
		memset(Output, 0xA5, sizeof(Output));
		testRequire(xrtX25519(Private, LowOrder, Output) &&
			xrtConstTimeEqual(Output, Zero, sizeof(Output)),
			"X25519 did not reduce a non-canonical low-order point");
	}
	memset(LowOrder, 0, sizeof(LowOrder));
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Before));
	xrtClearError();
	testRequire(!xrtX25519Shared(Private, LowOrder, Output) &&
		xrtConstTimeEqual(Output, Before, sizeof(Output)) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.crypto") == 0) &&
		(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_KEY_AGREEMENT) &&
		(strcmp(xrtErrorOperation(xrtGetError()), "x25519-shared") == 0),
		"X25519 low-order rejection contract failed");
}



/* 验证输入输出重叠能力和所有空指针失败原子性。 */
static void testX25519Edges(void)
{
	uint8 Private[XRT_X25519_PRIVATE_SIZE] = { 3 };
	uint8 Point[XRT_X25519_PUBLIC_SIZE] = { 9 };
	uint8 Expected[XRT_X25519_SHARED_SIZE];
	uint8 Buffer[96];
	uint8 Before[XRT_X25519_SHARED_SIZE];

	testRequire(xrtX25519(Private, Point, Expected),
		"X25519 edge reference failed");
	memcpy(Buffer, Private, sizeof(Private));
	memcpy(Buffer + 32u, Point, sizeof(Point));
	testRequire(xrtX25519(Buffer, Buffer + 32u, Buffer + 16u) &&
		xrtConstTimeEqual(Buffer + 16u, Expected, sizeof(Expected)),
		"X25519 partial overlap failed");
	memcpy(Buffer, Private, sizeof(Private));
	testRequire(xrtX25519Public(Buffer, Buffer) &&
		xrtConstTimeEqual(Buffer, Expected, sizeof(Expected)),
		"X25519 public in-place path failed");

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Before));
	xrtClearError();
	testRequire(!xrtX25519(NULL, Point, Buffer) &&
		xrtConstTimeEqual(Buffer, Before, sizeof(Before)) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"X25519 null scalar changed output or error contract");
	testRequire(!xrtX25519(Private, NULL, Buffer),
		"X25519 accepted a null point");
	testRequire(!xrtX25519(Private, Point, NULL),
		"X25519 accepted a null output");
}



/* 执行 X25519 标准向量、安全边界和缓冲契约测试。 */
int main(void)
{
	testX25519Vectors();
	testX25519Exchange();
	testX25519Iterations();
	testX25519LowOrder();
	testX25519Edges();
	return 0;
}
