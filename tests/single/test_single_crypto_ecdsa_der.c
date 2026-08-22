#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件可以独立完成 ECDSA DER 表示转换。 */
int main(void)
{
	const uint8 Raw[2] = { 1, 2 };
	uint8 Der[8];
	uint8 Decoded[2];
	size_t iSize = 0;

	return (!xrtEcdsaDerEncode(Raw, 1, Der, sizeof(Der), &iSize) ||
		(iSize != sizeof(Der)) ||
		!xrtEcdsaDerDecode(Der, iSize, Decoded, 1) ||
		!xrtConstTimeEqual(Raw, Decoded, sizeof(Raw))) ? 1 : 0;
}
