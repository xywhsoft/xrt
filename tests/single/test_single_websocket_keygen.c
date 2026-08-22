#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须使用安全随机源生成规范客户端握手密钥。 */
int main(void)
{
	char Key[XWS_KEY_CAPACITY];

	if ( !xrtWsKeyGenerate(Key, sizeof(Key)) ||
		!xrtWsKeyValid((xstrview){ Key, XWS_KEY_SIZE }) ) {
		return 1;
	}
	return 0;
}
