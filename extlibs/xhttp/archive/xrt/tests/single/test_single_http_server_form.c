#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头 urlencoded 请求辅助入口。 */
int main(void)
{
	return xrtHttpServerRequestForm(
		NULL,
		NULL,
		NULL
	) == NULL ? 0 : 1;
}
