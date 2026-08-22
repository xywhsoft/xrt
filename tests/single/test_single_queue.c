#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供共同队列容量规则。 */
int main(void)
{
	return xrtQueueCapacity(17u) == 32u ? 0 : 1;
}
