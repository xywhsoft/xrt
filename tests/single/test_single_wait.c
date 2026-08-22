#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的截止时间契约。 */
int main(void)
{
	xdeadline iDeadline = xrtDeadlineAfter(1000);

	return (iDeadline != XRT_DEADLINE_NEVER) &&
		(xrtDeadlineRemaining(iDeadline) <= 1000) ? 0 : 1;
}
