#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件可独立完成 AES 原位加解密。 */
int main(void)
{
	uint8 Key[XRT_AES128_KEY_SIZE] = { 0 };
	uint8 Block[XRT_AES_BLOCK_SIZE] = { 1, 2, 3, 4 };
	uint8 Plain[XRT_AES_BLOCK_SIZE];
	xaes State;

	memcpy(Plain, Block, sizeof(Plain));
	return (!xrtAesInit(&State, Key, sizeof(Key)) ||
		!xrtAesEncrypt(&State, Block, Block) ||
		!xrtAesDecrypt(&State, Block, Block) ||
		(memcmp(Block, Plain, sizeof(Block)) != 0)) ? 1 : 0;
}
