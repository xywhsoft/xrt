#include "../test_budget_allocator.h"



/* 验证接收变换自身与惰性 Inflate 分配都能在 OOM 下干净失败。 */
int main(void)
{
	testbudgetallocator Allocator;
	xwsinflater* pInflater;

	testRequire(
		testInstallBudgetAllocator(&Allocator, 0u),
		"WebSocket Inflater failure allocator install failed"
	);
	testRequire(
		(xrtWsInflaterCreate(NULL) == NULL) &&
		(Allocator.Denied != 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"WebSocket Inflater create OOM mismatch"
	);
	xrtClearError();

	Allocator.Allow = 32u;
	pInflater = xrtWsInflaterCreate(NULL);
	testRequire(
		pInflater != NULL,
		"WebSocket Inflater OOM fixture create failed"
	);
	Allocator.Allow = 0u;
	testRequire(
		!xrtWsInflaterBegin(pInflater, true) &&
		(Allocator.Denied != 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"WebSocket Inflater lazy OOM mismatch"
	);
	xrtClearError();
	xrtWsInflaterDestroy(pInflater);
	printf("[PASS] websocket_inflater_oom\n");
	return 0;
}
