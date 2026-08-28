#include <xrt.h>


/* 验证 XLang 的 C/TCC 入口可以消费当前 XRT 单头声明和核心 ABI。 */
int main(void)
{
	xrtClearError();
	return xrtGetError() == NULL ? 0 : 1;
}
