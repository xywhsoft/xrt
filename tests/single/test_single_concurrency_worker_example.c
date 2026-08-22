#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 复用同一份范例源码验证 Channel、Thread 与 Future 的单头组合。 */
#define main xrtConcurrencyWorkerExampleMain
#include "../../examples/concurrency/worker/main.c"
#undef main



/* 运行被重命名的范例入口。 */
int main(void)
{
	return xrtConcurrencyWorkerExampleMain();
}
