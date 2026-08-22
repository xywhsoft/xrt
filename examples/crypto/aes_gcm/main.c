#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 展示固定密钥状态上的 AES-GCM 原位封装与打开。 */
int main(void)
{
	uint8 Key[XRT_AES256_KEY_SIZE] = { 0 };
	uint8 Nonce[XRT_AES_GCM_NONCE_DEFAULT_SIZE] = { 0 };
	uint8 Buffer[64] = "hello from aes-gcm";
	const char* pAad = "message-v1";
	size_t iPlainSize = strlen((const char*)Buffer);
	xaesgcm State;

	if ( !xrtAesGcmInit(
		&State, Key, sizeof(Key), XRT_AES_GCM_TAG_DEFAULT_SIZE
	) ) {
		return 1;
	}
	if ( !xrtAesGcmSeal(
		&State,
		Nonce,
		sizeof(Nonce),
		pAad,
		strlen(pAad),
		Buffer,
		iPlainSize,
		Buffer,
		sizeof(Buffer)
	) ) {
		xrtAesGcmClear(&State);
		return 1;
	}
	if ( !xrtAesGcmOpen(
		&State,
		Nonce,
		sizeof(Nonce),
		pAad,
		strlen(pAad),
		Buffer,
		iPlainSize + XRT_AES_GCM_TAG_DEFAULT_SIZE,
		Buffer,
		sizeof(Buffer)
	) ) {
		xrtAesGcmClear(&State);
		return 1;
	}
	Buffer[iPlainSize] = '\0';
	printf("%s\n", Buffer);
	xrtAesGcmClear(&State);
	return 0;
}
