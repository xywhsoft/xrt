#include <xrt/crypto.h>



/* 生成双方临时密钥，验证共享秘密并在所有返回路径清理敏感栈数据。 */
int main(void)
{
	uint8 PrivateA[XRT_P256_PRIVATE_SIZE] = { 0 };
	uint8 PublicA[XRT_P256_PUBLIC_SIZE];
	uint8 PrivateB[XRT_P256_PRIVATE_SIZE] = { 0 };
	uint8 PublicB[XRT_P256_PUBLIC_SIZE];
	uint8 SharedA[XRT_P256_SHARED_SIZE] = { 0 };
	uint8 SharedB[XRT_P256_SHARED_SIZE] = { 0 };
	bool bSame = false;

	if ( !xrtP256KeyPair(PrivateA, PublicA) ||
		!xrtP256KeyPair(PrivateB, PublicB) ||
		!xrtP256Shared(PrivateA, PublicB, SharedA) ||
		!xrtP256Shared(PrivateB, PublicA, SharedB) ) {
		goto cleanup;
	}
	bSame = xrtConstTimeEqual(SharedA, SharedB, sizeof(SharedA));

cleanup:
	xrtSecureZero(PrivateA, sizeof(PrivateA));
	xrtSecureZero(PrivateB, sizeof(PrivateB));
	xrtSecureZero(SharedA, sizeof(SharedA));
	xrtSecureZero(SharedB, sizeof(SharedB));
	return bSame ? 0 : 1;
}
