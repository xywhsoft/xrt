#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 范例：crypto/aes_gcm —— AES-256-GCM：认证加密（AEAD）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtAesGcmInit / Seal / Open / Clear   AEAD 状态机
 *   XRT_AES_GCM_NONCE/TAG_DEFAULT_SIZE    推荐参数
 * 模块宏：XRT_MODULE_CRYPTO（AES 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/aes_gcm/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   hello from aes-gcm
 *
 * AEAD 三件套一次到位：机密性 + 完整性 + 认证。
 *   Seal 原位加密并追加认证标签；Open 验签解密一体——
 *   标签不符整体失败，绝不返回"可能被篡改的明文"。
 *   AAD（"message-v1"）只认证不加密：版本头/上下文绑定。
 *   实现自动走 AES-NI 硬件加速路径。
 */


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
