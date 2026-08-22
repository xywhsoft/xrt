#include "../test.h"



/* 验证共同容量规则和错误分类。 */
int main(void)
{
	testRequire(xrtQueueCapacity(1u) == 1u, "queue capacity one mismatch");
	testRequire(xrtQueueCapacity(3u) == 4u, "queue capacity rounding mismatch");
	testRequire(
		xrtQueueCapacity(XRT_QUEUE_MAX_CAPACITY) == XRT_QUEUE_MAX_CAPACITY,
		"queue maximum capacity mismatch"
	);

	xrtClearError();
	testRequire(xrtQueueCapacity(0u) == 0u, "zero queue capacity should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "zero queue capacity error mismatch");
	xrtClearError();
	testRequire(
		xrtQueueCapacity(XRT_QUEUE_MAX_CAPACITY + 1u) == 0u,
		"oversized queue capacity should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "oversized queue capacity error mismatch");
	printf("[PASS] queue\n");
	return 0;
}
