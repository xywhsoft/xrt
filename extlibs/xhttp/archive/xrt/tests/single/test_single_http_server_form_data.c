#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头 multipart 请求辅助入口。 */
int main(void)
{
	return xrtHttpServerRequestFormData(
		NULL,
		NULL,
		NULL,
		NULL
	) == NULL ? 0 : 1;
}
