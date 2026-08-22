#include "../test_allocator.h"



/* 验证字符串创建与构建器在 OOM 下保持原状态。 */
int main(void)
{
	xstrbuf tBuffer;

	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtStrDup("text") == NULL, "string duplicate should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "duplicate OOM error mismatch");
	xrtClearError();
	testRequire(xrtStrFilter(XRT_STR_LITERAL("text"), XRT_STR_LITERAL("x")) == NULL,
		"string filter should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"filter OOM error mismatch");
	xrtClearError();

	xrtStrBufInit(&tBuffer);
	testRequire(!xrtStrBufAppend(&tBuffer, XRT_STR_LITERAL("text")), "builder append should fail");
	testRequire((tBuffer.Data == NULL) && (tBuffer.Size == 0) && (tBuffer.Capacity == 0),
		"failed builder append changed state");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "builder OOM error mismatch");
	xrtClearError();
	printf("[PASS] string-oom\n");
	return 0;
}
