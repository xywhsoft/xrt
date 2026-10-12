#include "../test.h"
#include "../../src/internal/xrt_http_client_runtime.h"



/* 终态故障测试在进程第一次逻辑分配时拒绝底层申请。 */
typedef struct test_http_client_terminal_oom {
	size_t Denied;
} test_http_client_terminal_oom;



/* 拒绝全部申请并记录终态错误构建确实尝试过分配。 */
static ptr testHttpClientTerminalOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_client_terminal_oom* pState =
		(test_http_client_terminal_oom*)pData;

	(void)iSize;
	pState->Denied++;
	return NULL;
}



/* 拒绝全部重分配。 */
static ptr testHttpClientTerminalOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	(void)pMemory;
	return testHttpClientTerminalOomAlloc(
		pData,
		iSize
	);
}



/* 故障分配器不可能取得内存。 */
static void testHttpClientTerminalOomFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	(void)pMemory;
}



/* 验证一个终态代码在分配失败后仍映射为稳定通用类别。 */
static void testHttpClientTerminalOomCase(
	xhttpclienterror Code,
	xerrkind Kind
)
{
	xerror* pError = __xrtHttpClientTerminalError(
		NULL,
		Code
	);

	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == Kind) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.core"
		) == 0),
		"HTTP client terminal error lost its category under OOM"
	);
	xrtErrorFree(pError);
	xrtClearError();
}



/* 取消、总超时和 idle 超时都必须拥有无分配兜底。 */
int main(void)
{
	test_http_client_terminal_oom State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpClientTerminalOomAlloc,
		testHttpClientTerminalOomRealloc,
		testHttpClientTerminalOomFree
	};

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP client terminal OOM allocator install failed");
	testHttpClientTerminalOomCase(
		XHTTP_CLIENT_ERROR_CANCELLED,
		XERR_CANCELLED
	);
	testHttpClientTerminalOomCase(
		XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL,
		XERR_TIMEOUT
	);
	testHttpClientTerminalOomCase(
		XHTTP_CLIENT_ERROR_TIMEOUT_IDLE,
		XERR_TIMEOUT
	);
	testRequire(State.Denied >= 3,
		"HTTP client terminal OOM did not reject allocations");
	printf("[PASS] high-level HTTP client terminal OOM\n");
	return 0;
}
