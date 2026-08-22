#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件可独立完成 AES-GCM 原位 Seal/Open。 */
int main(void)
{
	uint8 Key[XRT_AES128_KEY_SIZE] = { 0 };
	uint8 Nonce[XRT_AES_GCM_NONCE_DEFAULT_SIZE] = { 0 };
	uint8 Buffer[5u + XRT_AES_GCM_TAG_DEFAULT_SIZE] = {
		'h', 'e', 'l', 'l', 'o'
	};
	xaesgcm State;

	return (!xrtAesGcmInit(
			&State, Key, sizeof(Key), XRT_AES_GCM_TAG_DEFAULT_SIZE
		) || !xrtAesGcmSeal(
			&State, Nonce, sizeof(Nonce), NULL, 0,
			Buffer, 5u, Buffer, sizeof(Buffer)
		) || !xrtAesGcmOpen(
			&State, Nonce, sizeof(Nonce), NULL, 0,
			Buffer, sizeof(Buffer), Buffer, sizeof(Buffer)
		) || (memcmp(Buffer, "hello", 5u) != 0)) ? 1 : 0;
}
