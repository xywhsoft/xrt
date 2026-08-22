#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件包含可裁剪临时内存实现。 */
int main(void)
{
	char* sMemory = (char*)xrtTemp(32);

	if ( sMemory == NULL ) {
		return 1;
	}
	sMemory[0] = 'X';
	return xrtTempClear() ? 0 : 2;
}
