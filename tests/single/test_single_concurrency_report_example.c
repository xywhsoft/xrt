#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 复用同一份范例源码验证 TaskGroup、TaskPool、Future 与协程的单头组合。 */
#define main xrtConcurrencyReportExampleMain
#include "../../examples/concurrency/report/main.c"
#undef main



/* 运行被重命名的范例入口。 */
int main(void)
{
	return xrtConcurrencyReportExampleMain();
}
