#include <stdio.h>
#include <string.h>
#include <xrt.h>



/*
 * 范例：crypto/chacha20 —— 裸 ChaCha20 流密码（教学层）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtChaCha20   (密钥, nonce, 计数器, 输入, 输出) 流式异或
 * 模块宏：XRT_MODULE_CRYPTO（CHACHA20 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/chacha20/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   round trip: yes
 *
 * 裸流密码无认证——加密与解密是同一运算（密钥流异或），
 *   示范"为什么不能直接用"：攻击者可随意翻转密文位。
 *   实际数据一律用 AEAD 变体 chacha20_poly1305。
 *   同密钥复用 nonce 会泄漏两消息的异或——范例注释
 *   特别强调，生产代码计数器必须严格递增。
 */


/* 展示裸 ChaCha20 的分离输出和原位反向变换。 */
int main(void)
{
	static const char Message[] = "xrt chacha20 stream";
	uint8 Key[XRT_CHACHA20_KEY_SIZE];
	uint8 Nonce[XRT_CHACHA20_NONCE_SIZE];
	uint8 Buffer[sizeof(Message)];
	bool bValid = false;

	/* 固定材料只用于可重复示例，真实协议不得在同一密钥下复用 nonce。 */
	for ( size_t i = 0; i < sizeof(Key); i++ ) {
		Key[i] = (uint8)i;
	}
	for ( size_t i = 0; i < sizeof(Nonce); i++ ) {
		Nonce[i] = (uint8)(0xA0u + i);
	}

	/* 流密码使用同一密钥流异或两次即可恢复原文。 */
	if ( xrtChaCha20(
			Key, Nonce, 1u, Message, Buffer, sizeof(Message)
		) && xrtChaCha20(
			Key, Nonce, 1u, Buffer, Buffer, sizeof(Buffer)
		) ) {
		bValid = memcmp(Buffer, Message, sizeof(Message)) == 0;
	}
	xrtSecureZero(Key, sizeof(Key));
	xrtSecureZero(Buffer, sizeof(Buffer));

	printf("round trip: %s\n", bValid ? "yes" : "no");
	return bValid ? 0 : 1;
}
