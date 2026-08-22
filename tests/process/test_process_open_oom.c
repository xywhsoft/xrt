#include "../test.h"
#include "test_process_oom_allocator.h"



/* 验证默认程序入口在首个动态分配失败时保留内存错误。 */
int main(void)
{
	testprocessoomallocator Allocator;

	testRequire(
		testProcessOomInstall(&Allocator),
		"process open OOM allocator install failed"
	);
	testProcessOomFailStore(&Allocator, true);
	testRequire(
		!xrtProcessOpen("xrt-process-open-oom-target"),
		"process open succeeded under OOM"
	);
	testRequire(
		(testProcessOomDeniedLoad(&Allocator) != 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"process open OOM error mismatch"
	);
	return 0;
}
