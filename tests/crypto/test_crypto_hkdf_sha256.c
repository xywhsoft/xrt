#include "../test.h"
#include "test_crypto_digest.h"



/* 验证 RFC 5869 test case 1 的 Extract、Expand 和组合入口。 */
static void testHkdfSha256Vector(void)
{
	uint8 arrIkm[22];
	uint8 arrSalt[13];
	uint8 arrInfo[10];
	uint8 arrPrk[XRT_SHA256_SIZE];
	uint8 arrOkm[42];
	uint8 arrCombined[42];

	memset(arrIkm, 0x0B, sizeof(arrIkm));
	for ( size_t i = 0; i < sizeof(arrSalt); i++ ) {
		arrSalt[i] = (uint8)i;
	}
	for ( size_t i = 0; i < sizeof(arrInfo); i++ ) {
		arrInfo[i] = (uint8)(0xF0u + i);
	}
	testRequire(xrtHkdfSha256Extract(
			arrSalt, sizeof(arrSalt), arrIkm, sizeof(arrIkm), arrPrk
		), "HKDF-SHA256 extract failed");
	testCryptoDigest(arrPrk, sizeof(arrPrk),
		"077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5",
		"HKDF-SHA256 PRK mismatch");
	testRequire(xrtHkdfSha256Expand(
			arrPrk, sizeof(arrPrk), arrInfo, sizeof(arrInfo),
			arrOkm, sizeof(arrOkm)
		), "HKDF-SHA256 expand failed");
	testCryptoDigest(arrOkm, sizeof(arrOkm),
		"3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865",
		"HKDF-SHA256 OKM mismatch");
	testRequire(xrtHkdfSha256(
			arrSalt, sizeof(arrSalt), arrIkm, sizeof(arrIkm),
			arrInfo, sizeof(arrInfo), arrCombined, sizeof(arrCombined)
		) && xrtConstTimeEqual(arrOkm, arrCombined, sizeof(arrOkm)),
		"HKDF-SHA256 combined result mismatch");
}



/* 实际生成 255 个块，并核对上限输出的首尾块。 */
static void testHkdfSha256Limit(void)
{
	uint8 arrIkm[22];
	uint8 arrSalt[13];
	uint8 arrInfo[10];
	uint8 arrPrk[XRT_SHA256_SIZE];
	uint8 arrOkm[255u * XRT_SHA256_SIZE];

	memset(arrIkm, 0x0B, sizeof(arrIkm));
	for ( size_t i = 0; i < sizeof(arrSalt); i++ ) {
		arrSalt[i] = (uint8)i;
	}
	for ( size_t i = 0; i < sizeof(arrInfo); i++ ) {
		arrInfo[i] = (uint8)(0xF0u + i);
	}
	testRequire(xrtHkdfSha256Extract(
			arrSalt, sizeof(arrSalt), arrIkm, sizeof(arrIkm), arrPrk
		) && xrtHkdfSha256Expand(
			arrPrk, sizeof(arrPrk), arrInfo, sizeof(arrInfo),
			arrOkm, sizeof(arrOkm)
		), "HKDF-SHA256 maximum output failed");
	testCryptoDigest(arrOkm, XRT_SHA256_SIZE,
		"3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf",
		"HKDF-SHA256 first block mismatch");
	testCryptoDigest(arrOkm + sizeof(arrOkm) - XRT_SHA256_SIZE,
		XRT_SHA256_SIZE,
		"76a3f78bcffe95fecf91923c22ad6ee64d48a6d1b981d7e523d5c0f22154ee88",
		"HKDF-SHA256 last block mismatch");
	xrtClearError();
	testRequire(!xrtHkdfSha256Expand(
			arrPrk, sizeof(arrPrk), NULL, 0,
			arrOkm, sizeof(arrOkm) + 1
		) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HKDF-SHA256 accepted a 256th block");
}



/* 验证空 salt、零输出、允许的 PRK 重叠和被拒绝的 info 重叠。 */
static void testHkdfSha256Edges(void)
{
	uint8 arrPrk[XRT_SHA256_SIZE];
	uint8 arrBuffer[64];

	testRequire(xrtHkdfSha256Extract(NULL, 0, NULL, 0, arrPrk),
		"HKDF-SHA256 empty extract failed");
	testCryptoDigest(arrPrk, sizeof(arrPrk),
		"b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad",
		"HKDF-SHA256 empty extract mismatch");
	testRequire(xrtHkdfSha256Expand(
			arrPrk, sizeof(arrPrk), NULL, 0, NULL, 0
		), "HKDF-SHA256 zero output failed");
	memcpy(arrBuffer, arrPrk, sizeof(arrPrk));
	testRequire(xrtHkdfSha256Expand(
			arrBuffer, sizeof(arrPrk), NULL, 0, arrBuffer, 42
		), "HKDF-SHA256 PRK/output overlap failed");
	xrtClearError();
	testRequire(!xrtHkdfSha256Expand(
			arrPrk, sizeof(arrPrk), arrBuffer, 10, arrBuffer, 42
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HKDF-SHA256 accepted info/output overlap");
	xrtClearError();
	testRequire(!xrtHkdfSha256Extract(NULL, 1, NULL, 0, arrPrk) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HKDF-SHA256 accepted invalid salt");
	xrtClearError();
	testRequire(!xrtHkdfSha256Extract(NULL, 0, NULL, 0, NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HKDF-SHA256 accepted null PRK output");
}



/* 执行 HKDF-SHA256 向量、上限与参数边界测试。 */
int main(void)
{
	testHkdfSha256Vector();
	testHkdfSha256Limit();
	testHkdfSha256Edges();
	return 0;
}
