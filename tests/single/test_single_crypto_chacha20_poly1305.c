#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 AEAD 的原位 Seal/Open。 */
int main(void)
{
	uint8 Key[XRT_CHACHA20_POLY1305_KEY_SIZE] = { 0 };
	uint8 Nonce[XRT_CHACHA20_POLY1305_NONCE_SIZE] = { 0 };
	uint8 Buffer[21] = { 'h', 'e', 'l', 'l', 'o' };

	return (!xrtChaCha20Poly1305Seal(
			Key, Nonce, NULL, 0, Buffer, 5, Buffer, sizeof(Buffer)
		) || !xrtChaCha20Poly1305Open(
			Key, Nonce, NULL, 0, Buffer, sizeof(Buffer), Buffer, sizeof(Buffer)
		) || (memcmp(Buffer, "hello", 5) != 0)) ? 1 : 0;
}
