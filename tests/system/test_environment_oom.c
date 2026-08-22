#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../test_allocator.h"

#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
	#if defined(__TINYC__)
		#include <winapi/windows.h>
	#else
		#include <windows.h>
	#endif
#endif



#define TEST_ENV_OOM_NAME "XRT_TEST_ENVIRONMENT_OOM_7F4C2B19"



/* 不使用 XRT 分配器预置测试变量，保留首次分配故障注入能力。 */
static void testEnvironmentPrepare(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		testRequire(SetEnvironmentVariableA(TEST_ENV_OOM_NAME, "value") != 0,
			"environment OOM fixture setup failed");
	#else
		testRequire(setenv(TEST_ENV_OOM_NAME, "value", 1) == 0,
			"environment OOM fixture setup failed");
	#endif
}



/* 拥有副本分配失败必须原样发布统一内存错误。 */
int main(void)
{
	str sValue = (str)(uintptr_t)1u;

	testEnvironmentPrepare();
	testRequire(testInstallFailAllocator(),
		"failure allocator install failed");
	testRequire(!xrtEnvLookup(TEST_ENV_OOM_NAME, &sValue),
		"environment lookup ignored allocation failure");
	testRequire(sValue == NULL,
		"failed environment lookup published a value");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"environment allocation failure error mismatch");
	return 0;
}
