#include "../test_budget_allocator.h"



/* 验证发送变换自身与惰性 Deflate 分配都能在 OOM 下干净失败。 */
int main(void)
{
	testbudgetallocator Allocator;
	xwsdeflater* pDeflater;

	testRequire(
		testInstallBudgetAllocator(&Allocator, 0u),
		"WebSocket Deflater failure allocator install failed"
	);
	testRequire(
		(xrtWsDeflaterCreate(NULL) == NULL) &&
		(Allocator.Denied != 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"WebSocket Deflater create OOM mismatch"
	);
	xrtClearError();

	Allocator.Allow = 32u;
	pDeflater = xrtWsDeflaterCreate(NULL);
	testRequire(
		pDeflater != NULL,
		"WebSocket Deflater OOM fixture create failed"
	);
	Allocator.Allow = 0u;
	testRequire(
		!xrtWsDeflaterBegin(pDeflater, true) &&
		(Allocator.Denied != 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"WebSocket Deflater lazy OOM mismatch"
	);
	xrtClearError();
	xrtWsDeflaterDestroy(pDeflater);
	printf("[PASS] websocket_deflater_oom\n");
	return 0;
}
