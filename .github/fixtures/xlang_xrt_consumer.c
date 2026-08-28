#include <xrt.h>

#include <string.h>



/* 验证 XLang 的 C/TCC 入口可以消费当前 XRT 单头声明和核心 ABI。 */
int main(void)
{
	return strcmp(xrtVersion(), XRT_VERSION_TEXT) == 0 ? 0 : 1;
}
