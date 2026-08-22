#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件可独立构建无算法后端的 TLS 记录骨架。 */
int main(void)
{
	uint8 Iv[12] = { 0 };
	uint8 Nonce[12];

	__xrtTlsRecordNonce(Nonce, Iv, 1u);
	return Nonce[11] == 1u ? 0 : 1;
}
