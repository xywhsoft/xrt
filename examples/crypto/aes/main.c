#include <stdio.h>
#include <xrt.h>



/*
 * 范例：crypto/aes —— AES-128 块加密（FIPS 197 固定向量验证）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtAesInit / Encrypt / Decrypt / Clear   块密码状态机
 *   XRT_AES128_KEY_SIZE / XRT_AES_BLOCK_SIZE 尺寸常量
 * 模块宏：XRT_MODULE_CRYPTO（AES 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/aes/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   AES-128 vector: valid
 *
 * 期望密文来自 FIPS 197 附录 C.1——对上即证明实现正确。
 *   裸块加密仅是底座：真实数据一律走 AEAD（aes_gcm），
 *   绝不手工 ECB 拼块。展开密钥状态可复用（多块同钥），
 *   用完 AesClear 抹除密钥材料。
 */


/* 使用 FIPS 197 固定向量展示 AES 块加密、解密和状态清理。 */
int main(void)
{
	static const uint8 Key[XRT_AES128_KEY_SIZE] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
	};
	static const uint8 Plain[XRT_AES_BLOCK_SIZE] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
	};
	static const uint8 Expected[XRT_AES_BLOCK_SIZE] = {
		0x69, 0xC4, 0xE0, 0xD8, 0x6A, 0x7B, 0x04, 0x30,
		0xD8, 0xCD, 0xB7, 0x80, 0x70, 0xB4, 0xC5, 0x5A
	};
	uint8 Block[XRT_AES_BLOCK_SIZE];
	xaes State;
	bool bValid = false;

	/* 同一展开密钥状态可以只读复用，结束后必须显式清理。 */
	if ( xrtAesInit(&State, Key, sizeof(Key)) &&
		 xrtAesEncrypt(&State, Plain, Block) &&
		 xrtConstTimeEqual(Block, Expected, sizeof(Block)) &&
		 xrtAesDecrypt(&State, Block, Block) ) {
		bValid = xrtConstTimeEqual(Block, Plain, sizeof(Block));
	}
	xrtAesClear(&State);
	xrtSecureZero(Block, sizeof(Block));

	printf("AES-128 vector: %s\n", bValid ? "valid" : "invalid");
	return bValid ? 0 : 1;
}
