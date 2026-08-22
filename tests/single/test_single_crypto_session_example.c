#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件聚合选择能完成密钥交换、派生和 AEAD。 */
int main(void)
{
	uint8 PrivateA[XRT_X25519_PRIVATE_SIZE];
	uint8 PublicA[XRT_X25519_PUBLIC_SIZE];
	uint8 PrivateB[XRT_X25519_PRIVATE_SIZE];
	uint8 PublicB[XRT_X25519_PUBLIC_SIZE];
	uint8 Shared[XRT_X25519_SHARED_SIZE];
	uint8 Key[XRT_CHACHA20_POLY1305_KEY_SIZE];
	uint8 Nonce[XRT_CHACHA20_POLY1305_NONCE_SIZE] = { 0 };
	uint8 Buffer[17] = { 'x' };

	return (!xrtX25519KeyPair(PrivateA, PublicA) ||
		!xrtX25519KeyPair(PrivateB, PublicB) ||
		!xrtX25519Shared(PrivateA, PublicB, Shared) ||
		!xrtHkdfSha256(
			NULL, 0, Shared, sizeof(Shared),
			"session", 7, Key, sizeof(Key)
		) || !xrtChaCha20Poly1305Seal(
			Key, Nonce, NULL, 0, Buffer, 1, Buffer, sizeof(Buffer)
		) || !xrtChaCha20Poly1305Open(
			Key, Nonce, NULL, 0, Buffer, sizeof(Buffer),
			Buffer, sizeof(Buffer)
		) || (Buffer[0] != 'x')) ? 1 : 0;
}
