#include <xrt.h>

#include <string.h>


/* 验证编译器可以消费当前 XRT 稳定头并链接核心版本 ABI。 */
int main(void)
{
	return strcmp(xrtVersion(), XRT_VERSION_TEXT) == 0 ? 0 : 1;
}
