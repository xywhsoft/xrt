/*
 * 范例：crypto/aead_tour —— AEAD 分离输出与 GMAC 认证
 * ----------------------------------------------------------------
 * 演示 API：
 *   【AES-GCM】    xrtAesGcmEncrypt / Decrypt（密文与标签分离）
 *                  xrtAesGcmTagSize（状态绑定的标签长度）
 *                  Init 第四参是标签长（本例 16 字节）
 *   【GMAC】       xrtAesGmac（只认证不加密）
 *                  xrtAesGmacVerify（常量时间校验）
 *   【ChaCha20】    xrtChaCha20Poly1305Encrypt / Decrypt
 * 模块宏：XRT_MODULE_CRYPTO（AES_GCM + CHACHA20_POLY1305）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/aead_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   aead: aes-gcm encrypt/decrypt roundtrip + tamper rejected
 *   aead: gmac compute+verify ok tamper rejected
 *   aead: chacha20poly1305 roundtrip + tamper rejected
 *
 * 分离式 Encrypt 把标签写到独立缓冲（与拼接式 Seal/Open
 *   相反）；Decrypt 在认证失败时不触碰明文输出。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	static const uint8 arrKey[16] = {
		0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u,
		8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u
	};
	static const uint8 arrNonce[12] = {
		0xAu, 0xBu, 0xCu, 0xDu, 0xEu, 0xFu,
		1u, 2u, 3u, 4u, 5u, 6u
	};
	static const uint8 arrPlain[7] = "secret";
	/* ChaCha20 密钥固定 32 字节，与 AES-128 的 16 字节不同。 */
	static const uint8 arrKey32[32] = {
		0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u,
		8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u,
		16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u,
		24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u
	};
	uint8 arrAad[3] = "hdr";
	uint8 arrCipher[16];
	uint8 arrTag[16];
	uint8 arrTagBad[16];
	uint8 arrOut[16];
	xaesgcm State;
	int iResult = 1;

	/* ---- AES-GCM：状态机 Init 后用分离式加解密。 ---- */
	if ( !xrtAesGcmInit(&State, arrKey, sizeof(arrKey), 16u) ||
		(xrtAesGcmTagSize(&State) != 16u) ||
		!xrtAesGcmEncrypt(&State, arrNonce, sizeof(arrNonce),
			arrAad, sizeof(arrAad), arrPlain, 6u, arrCipher,
			arrTag) ||
		!xrtAesGcmDecrypt(&State, arrNonce, sizeof(arrNonce),
			arrAad, sizeof(arrAad), arrCipher, 6u, arrTag,
			arrOut) ||
		(memcmp(arrOut, arrPlain, 6u) != 0) ) {
		goto Cleanup;
	}
	/* 篡改一个密文字节 → 认证失败且明文输出不被修改。 */
	memcpy(arrTagBad, arrTag, sizeof(arrTag));
	arrTagBad[0] ^= 0x80u;
	memset(arrOut, 0x5Au, sizeof(arrOut));
	if ( xrtAesGcmDecrypt(&State, arrNonce, sizeof(arrNonce),
			arrAad, sizeof(arrAad), arrCipher, 6u, arrTagBad,
			arrOut) ||
		(arrOut[0] != 0x5Au) ) {
		goto Cleanup;
	}
	printf("aead: aes-gcm encrypt/decrypt roundtrip + tamper rejected\n");

	/* ---- GMAC：空明文 GCM，只认证 AAD 等价数据。 ---- */
	if ( !xrtAesGmac(&State, arrNonce, sizeof(arrNonce), arrAad,
			sizeof(arrAad), arrTag) ||
		!xrtAesGmacVerify(&State, arrNonce, sizeof(arrNonce),
			arrAad, sizeof(arrAad), arrTag) ) {
		goto Cleanup;
	}
	arrTag[15] ^= 1u;
	if ( xrtAesGmacVerify(&State, arrNonce, sizeof(arrNonce),
			arrAad, sizeof(arrAad), arrTag) ) {
		goto Cleanup;
	}
	printf("aead: gmac compute+verify ok tamper rejected\n");

	/* ---- ChaCha20-Poly1305：密钥/nonce 直传，无需状态机。 ---- */
	if ( !xrtChaCha20Poly1305Encrypt(arrKey32, arrNonce, arrAad,
			sizeof(arrAad), arrPlain, 6u, arrCipher, arrTag) ||
		!xrtChaCha20Poly1305Decrypt(arrKey32, arrNonce, arrAad,
			sizeof(arrAad), arrCipher, 6u, arrTag, arrOut) ||
		(memcmp(arrOut, arrPlain, 6u) != 0) ) {
		goto Cleanup;
	}
	arrCipher[0] ^= 1u;
	if ( xrtChaCha20Poly1305Decrypt(arrKey32, arrNonce, arrAad,
			sizeof(arrAad), arrCipher, 6u, arrTag, arrOut) ) {
		goto Cleanup;
	}
	printf("aead: chacha20poly1305 roundtrip + tamper rejected\n");
	iResult = 0;

Cleanup:
	xrtAesGcmClear(&State);
	return iResult;
}
