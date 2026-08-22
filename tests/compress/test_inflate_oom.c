#include "../test_allocator.h"



/* 验证解码器的唯一常驻分配在 OOM 下干净失败。 */
int main(void)
{
	xinflateconfig Config;
	size_t iSize = 17;

	xrtInflateConfigInit(&Config);
	Config.Format = XINFLATE_GZIP;
	testRequire(
		testInstallFailAllocator(),
		"Inflate failure allocator install failed"
	);
	testRequire(
		xrtInflateCreate(&Config) == NULL,
		"Inflate create should fail under OOM"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Inflate create OOM error mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtInflateAll(
			(xbytesview){ NULL, 0 },
			&Config,
			&iSize
		) == NULL) &&
		(iSize == 17) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Inflate all OOM failure atomicity mismatch"
	);
	printf("[PASS] inflate_oom\n");
	return 0;
}
