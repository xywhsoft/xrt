/*
 * 范例：crypto/kdf_tour —— HKDF 两段式与组合式、PBKDF2-SHA384
 * ----------------------------------------------------------------
 * 演示 API：
 *   【HKDF-SHA256】 xrtHkdfSha256Extract + Expand（两段式）
 *                  与组合式 xrtHkdfSha256 逐位一致
 *   【HKDF-SHA512】 xrtHkdfSha512Extract + Expand（两段式）
 *                  与组合式 xrtHkdfSha512 逐位一致
 *   【HKDF-SHA384】 xrtHkdfSha384 组合式（两段式已在
 *                  hkdf_sha384 范例演示）
 *   【PBKDF2】      xrtPbkdf2Sha384（密码+salt+迭代派生）
 * 模块宏：XRT_MODULE_CRYPTO（HKDF_SHA256/384/512 +
 *         PBKDF2_SHA384）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/kdf_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   kdf: hkdf-sha256 extract+expand == combined
 *   kdf: hkdf-sha512 extract+expand == combined
 *   kdf: hkdf-sha384 combined deterministic
 *   kdf: pbkdf2-sha384 2 iters deterministic
 *
 * HKDF 两段式用于"提取一次、多次展开"的会话派生场景；
 *   组合式等价于 Extract 后紧跟一次 Expand。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	static const uint8 arrSalt[8] = "salt1234";
	static const uint8 arrIkm[10] = "input-key!";
	static const uint8 arrInfo[4] = "info";
	uint8 arrPrk[64];
	uint8 arrTwoStep[64];
	uint8 arrCombined[64];
	uint8 arrAgain[64];
	int iResult = 1;

	/* ---- HKDF-SHA256：两段式 vs 组合式（32 字节 OKM）。 ---- */
	if ( !xrtHkdfSha256Extract(arrSalt, sizeof(arrSalt),
			arrIkm, sizeof(arrIkm), arrPrk) ||
		!xrtHkdfSha256Expand(arrPrk, XRT_SHA256_SIZE,
			arrInfo, sizeof(arrInfo), arrTwoStep, 32u) ||
		!xrtHkdfSha256(arrSalt, sizeof(arrSalt), arrIkm,
			sizeof(arrIkm), arrInfo, sizeof(arrInfo),
			arrCombined, 32u) ||
		(memcmp(arrTwoStep, arrCombined, 32u) != 0) ) {
		goto Cleanup;
	}
	printf("kdf: hkdf-sha256 extract+expand == combined\n");

	/* ---- HKDF-SHA512：两段式 vs 组合式（64 字节 OKM）。 ---- */
	if ( !xrtHkdfSha512Extract(arrSalt, sizeof(arrSalt),
			arrIkm, sizeof(arrIkm), arrPrk) ||
		!xrtHkdfSha512Expand(arrPrk, XRT_SHA512_SIZE,
			arrInfo, sizeof(arrInfo), arrTwoStep, 64u) ||
		!xrtHkdfSha512(arrSalt, sizeof(arrSalt), arrIkm,
			sizeof(arrIkm), arrInfo, sizeof(arrInfo),
			arrCombined, 64u) ||
		(memcmp(arrTwoStep, arrCombined, 64u) != 0) ) {
		goto Cleanup;
	}
	printf("kdf: hkdf-sha512 extract+expand == combined\n");

	/* ---- HKDF-SHA384：组合式两次调用逐位一致（48 字节）。 ---- */
	if ( !xrtHkdfSha384(arrSalt, sizeof(arrSalt), arrIkm,
			sizeof(arrIkm), arrInfo, sizeof(arrInfo),
			arrCombined, 48u) ||
		!xrtHkdfSha384(arrSalt, sizeof(arrSalt), arrIkm,
			sizeof(arrIkm), arrInfo, sizeof(arrInfo),
			arrAgain, 48u) ||
		(memcmp(arrCombined, arrAgain, 48u) != 0) ) {
		goto Cleanup;
	}
	printf("kdf: hkdf-sha384 combined deterministic\n");

	/* ---- PBKDF2-SHA384：同参两次派生一致。 ---- */
	if ( !xrtPbkdf2Sha384("password", 8u, arrSalt, sizeof(arrSalt),
			2u, arrTwoStep, 32u) ||
		!xrtPbkdf2Sha384("password", 8u, arrSalt, sizeof(arrSalt),
			2u, arrAgain, 32u) ||
		(memcmp(arrTwoStep, arrAgain, 32u) != 0) ) {
		goto Cleanup;
	}
	printf("kdf: pbkdf2-sha384 2 iters deterministic\n");
	iResult = 0;

Cleanup:
	return iResult;
}
