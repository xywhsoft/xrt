#include "../test.h"

#include "../test_fault_allocator.h"



#define TEST_TEMPLATE_FILE_OOM_PATH "xrt-template-file-oom.tpl"



/* 读取并编译同一文件，失败和成功都必须完整释放临时源码。 */
static bool testTemplateFileOomAttempt(void)
{
	xtemplate* pTemplate = xrtTemplateCompileFile(
		TEST_TEMPLATE_FILE_OOM_PATH
	);
	bool bComplete = pTemplate != NULL;

	xrtTemplateRelease(pTemplate);
	xrtClearError();
	return bComplete;
}



/* 扫描文件读取到模板编译组合路径的稳定分配点。 */
int main(void)
{
	static const char sPrefix[] = "prefix {$name} suffix ";
	char arrSource[8192];
	FILE* pFile;
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	size_t iBaseline;
	size_t iCalls;

	pFile = fopen(TEST_TEMPLATE_FILE_OOM_PATH, "wb");
	testRequire(pFile != NULL, "template file OOM fixture open failed");
	memcpy(arrSource, sPrefix, sizeof(sPrefix) - 1u);
	memset(
		arrSource + sizeof(sPrefix) - 1u,
		'x',
		sizeof(arrSource) - (sizeof(sPrefix) - 1u)
	);
	testRequire(
		fwrite(arrSource, 1u, sizeof(arrSource), pFile) ==
			sizeof(arrSource),
		"template file OOM fixture write failed"
	);
	testRequire(
		fclose(pFile) == 0,
		"template file OOM fixture close failed"
	);
	testRequire(
		xrtSetAllocator(&Allocator),
		"template file OOM allocator install failed"
	);
	testRequire(
		testTemplateFileOomAttempt(),
		"template file OOM warm-up failed"
	);
	testMemoryDebugDrain("template file OOM memory debug reset failed");
	iBaseline = State.Live;

	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(
		testTemplateFileOomAttempt(),
		"template file OOM baseline failed"
	);
	iCalls = State.Calls;
	testRequire(iCalls != 0, "template file reached no allocation");
	testMemoryDebugDrain("template file OOM baseline reset failed");
	testRequire(
		State.Live == iBaseline,
		"template file OOM baseline leaked storage"
	);

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testTemplateFileOomAttempt(),
			"template file unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "template file OOM target was not reached");
		testMemoryDebugDrain(
			"template file OOM memory debug reset failed"
		);
		testRequire(
			State.Live == iBaseline,
			"template file OOM path leaked storage"
		);
	}

	State.FailAt = SIZE_MAX;
	testRequire(
		testTemplateFileOomAttempt(),
		"template file did not recover after OOM"
	);
	testMemoryDebugDrain("template file OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "template file recovery leaked storage");
	testRequire(
		remove(TEST_TEMPLATE_FILE_OOM_PATH) == 0,
		"template file OOM fixture cleanup failed"
	);
	printf("[PASS] template file OOM (%zu allocation points)\n", iCalls);
	return 0;
}
