#include <stdio.h>
#include <string.h>
#include <xrt.h>



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
