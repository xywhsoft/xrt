#include <xrt/core.h>

#include <string.h>



/* 验证发布库公开符号可以从独立消费者链接并执行。 */
int main(void)
{
	if ( strcmp(xrtVersion(), XRT_VERSION_TEXT) != 0 ) {
		return 1;
	}
	return 0;
}
