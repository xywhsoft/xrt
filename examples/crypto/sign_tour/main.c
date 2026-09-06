/*
 * 范例：crypto/sign_tour —— Ed25519 三域与 ECDSA 双曲线签名
 * ----------------------------------------------------------------
 * 演示 API：
 *   【Ed25519】    xrtEd25519Sign（纯消息，种子直接签）
 *                  xrtEd25519SignMode / VerifyMode
 *                  （CONTEXT 与 PREHASH 两个互不兼容域）
 *   【ECDSA-P256】 xrtEcdsaP256Sign（RFC 6979 确定性 low-S）
 *                  xrtEcdsaP256Verify / VerifyDer
 *   【ECDSA-P384】 xrtEcdsaP384Sign / Verify / VerifyDer
 *   【DER】        xrtEcdsaDerEncode / DerDecode
 *                  （定宽 r||s ↔ 规范 DER 往返）
 * 模块宏：XRT_MODULE_CRYPTO（ED25519 + ECDSA_P256/P384 + DER）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/sign_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   sign: ed25519 pure sign->verify ok
 *   sign: ed25519 context + prehash modes ok
 *   sign: ecdsa-p256 sign->verify(der) roundtrip ok
 *   sign: ecdsa-p384 sign->verify(der) roundtrip ok
 *   sign: der r||s <-> der roundtrip ok
 *
 * Ed25519 三域互不兼容：PURE 签名不能被 CONTEXT 验证；
 *   PREHASH 要求消息恰为 64 字节 SHA-512 预哈希。
 * ECDSA Sign 收"摘要"而非消息——Hash 枚举声明摘要算法。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	static const uint8 arrSeed[32] = {
		1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
		9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u,
		17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u,
		25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u
	};
	static const uint8 arrPriv256[32] = {
		0xA1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
		9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u,
		17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u,
		25u, 26u, 27u, 28u, 29u, 30u, 31u, 0xB0u
	};
	static const uint8 arrPriv384[48] = {
		0xC1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
		9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u,
		17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u,
		25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u,
		33u, 34u, 35u, 36u, 37u, 38u, 39u, 40u,
		41u, 42u, 43u, 44u, 45u, 46u, 47u, 0xD0u
	};
	static const uint8 arrContext[4] = "ctx1";
	uint8 arrMessage[13] = "payload-bytes";
	uint8 arrPublic[64];
	uint8 arrSignature[96];
	uint8 arrSignature2[128]; /* 第二次 DER 编码输出（约 102 字节）。 */
	uint8 arrDer[128];
	uint8 arrRaw2[96];
	uint8 arrDigest[64];
	uint8 arrPub256[65];
	uint8 arrPub384[97];
	size_t iDerSize = 0;
	xed25519key Key;
	int iResult = 1;

	/* ---- Ed25519 纯消息：种子签 → 公钥验。 ---- */
	if ( !xrtEd25519Public(arrSeed, arrPublic) ||
		!xrtEd25519Sign(arrSeed, arrMessage, sizeof(arrMessage) - 1u,
			arrSignature) ||
		!xrtEd25519Verify(arrPublic, arrMessage,
			sizeof(arrMessage) - 1u, arrSignature) ) {
		goto Cleanup;
	}
	arrSignature[0] ^= 1u;
	if ( xrtEd25519Verify(arrPublic, arrMessage,
			sizeof(arrMessage) - 1u, arrSignature) ) {
		goto Cleanup;
	}
	printf("sign: ed25519 pure sign->verify ok\n");

	/* ---- Ed25519 CONTEXT / PREHASH 域：展开密钥一次复用。 ---- */
	if ( !xrtEd25519KeyInit(&Key, arrSeed) ||
		!xrtEd25519SignMode(&Key, XED25519_CONTEXT, arrContext,
			sizeof(arrContext), arrMessage,
			sizeof(arrMessage) - 1u, arrSignature) ||
		!xrtEd25519VerifyMode(arrPublic, XED25519_CONTEXT,
			arrContext, sizeof(arrContext), arrMessage,
			sizeof(arrMessage) - 1u, arrSignature) ) {
		goto Cleanup;
	}
	/* PREHASH：消息须先取 64 字节 SHA-512。 */
	if ( !xrtSha512(arrMessage, sizeof(arrMessage) - 1u, arrDigest) ||
		!xrtEd25519SignMode(&Key, XED25519_PREHASH, NULL, 0u,
			arrDigest, XRT_ED25519_PREHASH_SIZE, arrSignature) ||
		!xrtEd25519VerifyMode(arrPublic, XED25519_PREHASH, NULL, 0u,
			arrDigest, XRT_ED25519_PREHASH_SIZE, arrSignature) ) {
		goto Cleanup;
	}
	xrtEd25519KeyClear(&Key);
	printf("sign: ed25519 context + prehash modes ok\n");

	/* ---- ECDSA-P256：签摘要 → raw 验证 → DER 验证。 ---- */
	if ( !xrtSha256(arrMessage, sizeof(arrMessage) - 1u, arrDigest) ||
		!xrtP256Public(arrPriv256, arrPub256) ||
		!xrtEcdsaP256Sign(XCRYPTO_HASH_SHA256, arrDigest, arrPriv256,
			arrSignature) ||
		!xrtEcdsaP256Verify(arrDigest, XRT_SHA256_SIZE, arrSignature,
			arrPub256) ) {
		goto Cleanup;
	}
	/* DerEncode：空输出先查询所需长度，再实际编码。 */
	if ( !xrtEcdsaDerEncode(arrSignature, 32u, NULL, 0u, &iDerSize) ||
		(iDerSize == 0u) ||
		(iDerSize > sizeof(arrDer)) ||
		!xrtEcdsaDerEncode(arrSignature, 32u, arrDer, iDerSize,
			&iDerSize) ||
		!xrtEcdsaP256VerifyDer(arrDigest, XRT_SHA256_SIZE, arrDer,
			iDerSize, arrPub256) ) {
		goto Cleanup;
	}
	printf("sign: ecdsa-p256 sign->verify(der) roundtrip ok\n");

	/* ---- ECDSA-P384：SHA-384 摘要同构闭环。 ---- */
	if ( !xrtSha384(arrMessage, sizeof(arrMessage) - 1u, arrDigest) ||
		!xrtP384Public(arrPriv384, arrPub384) ||
		!xrtEcdsaP384Sign(XCRYPTO_HASH_SHA384, arrDigest, arrPriv384,
			arrSignature) ||
		!xrtEcdsaP384Verify(arrDigest, XRT_SHA384_SIZE, arrSignature,
			arrPub384) ||
		!xrtEcdsaDerEncode(arrSignature, 48u, arrDer, sizeof(arrDer),
			&iDerSize) ||
		!xrtEcdsaP384VerifyDer(arrDigest, XRT_SHA384_SIZE, arrDer,
			iDerSize, arrPub384) ) {
		goto Cleanup;
	}
	printf("sign: ecdsa-p384 sign->verify(der) roundtrip ok\n");

	/* ---- DER 往返：DerDecode(Encode(x)) == x（P-384 签名）。 ---- */
	if ( !xrtEcdsaDerDecode(arrDer, iDerSize, arrRaw2, 48u) ||
		(memcmp(arrRaw2, arrSignature, 96u) != 0) ||
		/* DER（约 102 字节）比 96 字节 raw 更长——独立缓冲。 */
		!xrtEcdsaDerEncode(arrRaw2, 48u, arrSignature2,
			sizeof(arrDer), &iDerSize) ||
		!xrtEcdsaDerDecode(arrSignature2, iDerSize, arrRaw2, 48u) ||
		(memcmp(arrRaw2, arrSignature, 96u) != 0) ) {
		goto Cleanup;
	}
	printf("sign: der r||s <-> der roundtrip ok\n");
	iResult = 0;

Cleanup:
	return iResult;
}
