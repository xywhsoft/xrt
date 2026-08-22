#include <stdio.h>
#include <string.h>

#include <xrt.h>



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
