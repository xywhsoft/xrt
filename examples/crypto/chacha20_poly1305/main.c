#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 范例：crypto/chacha20_poly1305 —— ChaCha20-Poly1305 AEAD 便捷层
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtChaCha20Poly1305Seal / Open   无状态一对入口
 *   XRT_CHACHA20_POLY1305_OVERHEAD   密文相对明文的固定开销
 * 模块宏：XRT_MODULE_CRYPTO（CHACHA20_POLY1305 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/chacha20_poly1305/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   authenticated message
 *
 * 与 AES-GCM 同级的 AEAD：无硬件加速平台上更快、
 *   常量时间实现——移动端/物联网首选。
 *   无状态便捷层密钥即用即走；高频同钥场景有
 *   带状态的 Sealer/Opener 变体（TLS 内部即用）。
 */


/* 使用 packed 便捷层完成一次可认证的原位加解密。 */
int main(void)
{
	uint8 Key[XRT_CHACHA20_POLY1305_KEY_SIZE];
	uint8 Nonce[XRT_CHACHA20_POLY1305_NONCE_SIZE];
	uint8 Message[64] = "authenticated message";
	size_t iPlainSize = strlen((cstr)Message);
	size_t iSealedSize = iPlainSize + XRT_CHACHA20_POLY1305_OVERHEAD;

	/* 固定材料仅用于复现示例；生产中同一密钥下的 nonce 必须唯一。 */
	for ( size_t i = 0; i < sizeof(Key); i++ ) {
		Key[i] = (uint8)(0x40u + i);
	}
	for ( size_t i = 0; i < sizeof(Nonce); i++ ) {
		Nonce[i] = (uint8)i;
	}
	if ( !xrtChaCha20Poly1305Seal(
			Key, Nonce, "header", 6,
			Message, iPlainSize, Message, sizeof(Message)
		) || !xrtChaCha20Poly1305Open(
			Key, Nonce, "header", 6,
			Message, iSealedSize, Message, sizeof(Message)
		) ) {
		xrtSecureZero(Key, sizeof(Key));
		return 1;
	}
	Message[iPlainSize] = 0;
	printf("%s\n", (cstr)Message);
	xrtSecureZero(Key, sizeof(Key));
	return 0;
}
