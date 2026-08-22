#include "../test.h"
#include "test_crypto_digest.h"



/* 验证 SHA-384 与 SHA-512 HKDF 的提取、展开和组合入口。 */
static void testHkdfSha512Vectors(void)
{
	uint8 arrIkm[22];
	uint8 arrSalt[13];
	uint8 arrInfo[10];
	uint8 arrPrk[XRT_SHA512_SIZE];
	uint8 arrOkm[42];
	uint8 arrCombined[42];

	memset(arrIkm, 0x0B, sizeof(arrIkm));
	for ( size_t i = 0; i < sizeof(arrSalt); i++ ) {
		arrSalt[i] = (uint8)i;
	}
	for ( size_t i = 0; i < sizeof(arrInfo); i++ ) {
		arrInfo[i] = (uint8)(0xF0u + i);
	}
	testRequire(xrtHkdfSha384Extract(
			arrSalt, sizeof(arrSalt), arrIkm, sizeof(arrIkm), arrPrk
		), "HKDF-SHA384 extract failed");
	testCryptoDigest(arrPrk, XRT_SHA384_SIZE,
		"704b39990779ce1dc548052c7dc39f303570dd13fb39f7acc564680bef80e8dec70ee9a7e1f3e293ef68eceb072a5ade",
		"HKDF-SHA384 PRK mismatch");
	testRequire(xrtHkdfSha384Expand(
			arrPrk, XRT_SHA384_SIZE, arrInfo, sizeof(arrInfo),
			arrOkm, sizeof(arrOkm)
		), "HKDF-SHA384 expand failed");
	testCryptoDigest(arrOkm, sizeof(arrOkm),
		"9b5097a86038b805309076a44b3a9f38063e25b516dcbf369f394cfab43685f748b6457763e4f0204fc5",
		"HKDF-SHA384 OKM mismatch");
	testRequire(xrtHkdfSha384(
			arrSalt, sizeof(arrSalt), arrIkm, sizeof(arrIkm),
			arrInfo, sizeof(arrInfo), arrCombined, sizeof(arrCombined)
		) && xrtConstTimeEqual(arrOkm, arrCombined, sizeof(arrOkm)),
		"HKDF-SHA384 combined result mismatch");

	testRequire(xrtHkdfSha512Extract(
			arrSalt, sizeof(arrSalt), arrIkm, sizeof(arrIkm), arrPrk
		), "HKDF-SHA512 extract failed");
	testCryptoDigest(arrPrk, XRT_SHA512_SIZE,
		"665799823737ded04a88e47e54a5890bb2c3d247c7a4254a8e61350723590a26c36238127d8661b88cf80ef802d57e2f7cebcf1e00e083848be19929c61b4237",
		"HKDF-SHA512 PRK mismatch");
	testRequire(xrtHkdfSha512Expand(
			arrPrk, XRT_SHA512_SIZE, arrInfo, sizeof(arrInfo),
			arrOkm, sizeof(arrOkm)
		), "HKDF-SHA512 expand failed");
	testCryptoDigest(arrOkm, sizeof(arrOkm),
		"832390086cda71fb47625bb5ceb168e4c8e26a1a16ed34d9fc7fe92c1481579338da362cb8d9f925d7cb",
		"HKDF-SHA512 OKM mismatch");
}



/* 实际生成 SHA-384/512 的 255 个输出块并验证首尾。 */
static void testHkdfSha512Limits(void)
{
	uint8 arrIkm[22];
	uint8 arrSalt[13];
	uint8 arrInfo[10];
	uint8 arrPrk[XRT_SHA512_SIZE];
	uint8 arrOkm[255u * XRT_SHA512_SIZE];

	memset(arrIkm, 0x0B, sizeof(arrIkm));
	for ( size_t i = 0; i < sizeof(arrSalt); i++ ) {
		arrSalt[i] = (uint8)i;
	}
	for ( size_t i = 0; i < sizeof(arrInfo); i++ ) {
		arrInfo[i] = (uint8)(0xF0u + i);
	}
	testRequire(xrtHkdfSha384Extract(
			arrSalt, sizeof(arrSalt), arrIkm, sizeof(arrIkm), arrPrk
		) && xrtHkdfSha384Expand(
			arrPrk, XRT_SHA384_SIZE, arrInfo, sizeof(arrInfo),
			arrOkm, 255u * XRT_SHA384_SIZE
		), "HKDF-SHA384 maximum output failed");
	testCryptoDigest(arrOkm + (254u * XRT_SHA384_SIZE), XRT_SHA384_SIZE,
		"51c32bc5254cef4332ed63af77508e0b5c6c2638baf7fcf25cd354f9760cab2d71e079bb1ef9867117de2b2b16cb4a4e",
		"HKDF-SHA384 last block mismatch");
	testRequire(xrtHkdfSha512Extract(
			arrSalt, sizeof(arrSalt), arrIkm, sizeof(arrIkm), arrPrk
		) && xrtHkdfSha512Expand(
			arrPrk, XRT_SHA512_SIZE, arrInfo, sizeof(arrInfo),
			arrOkm, sizeof(arrOkm)
		), "HKDF-SHA512 maximum output failed");
	testCryptoDigest(arrOkm + sizeof(arrOkm) - XRT_SHA512_SIZE,
		XRT_SHA512_SIZE,
		"6f4c862c43cc05f02bbc375f8e523cf7a8148162f9266a8e90e9de9261973f47931623de9936c096438e9f180736960acc54a28763012fec34d40b7c12ee8560",
		"HKDF-SHA512 last block mismatch");
	xrtClearError();
	testRequire(!xrtHkdfSha384Expand(
			arrPrk, XRT_SHA384_SIZE, NULL, 0,
			arrOkm, (255u * XRT_SHA384_SIZE) + 1u
		) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HKDF-SHA384 accepted a 256th block");
	xrtClearError();
	testRequire(!xrtHkdfSha512Expand(
			arrPrk, XRT_SHA512_SIZE, NULL, 0,
			arrOkm, sizeof(arrOkm) + 1u
		) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HKDF-SHA512 accepted a 256th block");
}



/* 验证零输出、info 重叠和非法指针契约。 */
static void testHkdfSha512Edges(void)
{
	uint8 arrPrk[XRT_SHA512_SIZE];
	uint8 arrBuffer[96];

	testRequire(xrtHkdfSha512Extract(NULL, 0, NULL, 0, arrPrk) &&
		xrtHkdfSha512Expand(arrPrk, sizeof(arrPrk), NULL, 0, NULL, 0),
		"HKDF-SHA512 empty inputs failed");
	memcpy(arrBuffer, arrPrk, sizeof(arrPrk));
	testRequire(xrtHkdfSha512Expand(
			arrBuffer, sizeof(arrPrk), NULL, 0, arrBuffer, sizeof(arrBuffer)
		), "HKDF-SHA512 PRK/output overlap failed");
	xrtClearError();
	testRequire(!xrtHkdfSha512Expand(
			arrPrk, sizeof(arrPrk), arrBuffer, 10,
			arrBuffer, sizeof(arrBuffer)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HKDF-SHA512 accepted info/output overlap");
	xrtClearError();
	testRequire(!xrtHkdfSha384Extract(NULL, 0, NULL, 1, arrPrk) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HKDF-SHA384 accepted invalid IKM");
}



/* 执行 HKDF-SHA384/512 向量、上限与参数边界测试。 */
int main(void)
{
	testHkdfSha512Vectors();
	testHkdfSha512Limits();
	testHkdfSha512Edges();
	return 0;
}
