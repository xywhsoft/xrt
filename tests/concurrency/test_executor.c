#include "../test.h"



/* 测试上下文记录执行和析构次数，并提供取消阻塞门。 */
typedef struct test_executor_context {
	xatomic32 Gate;
	xatomic32 Started;
	xatomic64 Executed;
	xatomic64 Destroyed;
} test_executor_context;



/* 普通工作只增加执行计数。 */
static void testExecutorRun(ptr pData)
{
	test_executor_context* pContext =
		(test_executor_context*)pData;

	(void)xrtAtomic64FetchAdd(
		&pContext->Executed,
		1,
		XMEMORY_RELAXED
	);
}



/* 阻塞工作等待主线程打开门，用于稳定建立取消窗口。 */
static void testExecutorBlock(ptr pData)
{
	test_executor_context* pContext =
		(test_executor_context*)pData;

	xrtAtomic32Store(&pContext->Started, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(
		&pContext->Gate,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	testExecutorRun(pData);
}



/* 成功提交的每项数据都必须在执行或取消后析构一次。 */
static void testExecutorDestroy(ptr pData, ptr pContext)
{
	test_executor_context* pOwner =
		(test_executor_context*)pContext;

	(void)pData;
	(void)xrtAtomic64FetchAdd(
		&pOwner->Destroyed,
		1,
		XMEMORY_RELAXED
	);
}



/* 验证批量原子提交、并发执行、背压、关闭和取消所有权。 */
int main(void)
{
	xexecutorconfig Config = { 4, 32, 0 };
	test_executor_context Context;
	xexecutoritem Items[16];
	xexecutorstats Stats;
	xexecutor* pExecutor;

	memset(&Context, 0, sizeof(Context));
	memset(&Stats, 0, sizeof(Stats));
	xrtAtomic32Init(&Context.Gate, 0);
	xrtAtomic32Init(&Context.Started, 0);
	xrtAtomic64Init(&Context.Executed, 0);
	xrtAtomic64Init(&Context.Destroyed, 0);
	for ( size_t i = 0; i < (sizeof(Items) / sizeof(Items[0])); i++ ) {
		Items[i].Proc = testExecutorRun;
		Items[i].Data = &Context;
		Items[i].Destroy = testExecutorDestroy;
		Items[i].DestroyContext = &Context;
	}
	pExecutor = xrtExecutorCreate(&Config);
	testRequire(pExecutor != NULL, "executor create failed");
	testRequire(
		xrtExecutorSubmitBatch(
			pExecutor,
			Items,
			sizeof(Items) / sizeof(Items[0])
		),
		"executor batch submit failed"
	);
	for ( size_t i = 0; i < 48; i++ ) {
		testRequire(
			xrtExecutorSubmit(
				pExecutor,
				testExecutorRun,
				&Context,
				testExecutorDestroy,
				&Context
			),
			"executor scalar submit failed"
		);
	}
	testRequire(xrtExecutorClose(pExecutor), "executor close failed");
	testRequire(
		xrtExecutorWaitFor(pExecutor, UINT64_C(3000000)) == XWAIT_OK,
		"executor drain failed"
	);
	testRequire(
		xrtExecutorGet(pExecutor, &Stats) &&
		(Stats.Submitted == 64) &&
		(Stats.Completed == 64) &&
		(Stats.Executed == 64) &&
		(Stats.Cancelled == 0) &&
		(Stats.Queued == 0) &&
		(Stats.Running == 0) &&
		Stats.Closed &&
		(xrtAtomic64Load(
			&Context.Executed,
			XMEMORY_ACQUIRE
		 ) == 64) &&
		(xrtAtomic64Load(
			&Context.Destroyed,
			XMEMORY_ACQUIRE
		 ) == 64),
		"executor drain statistics mismatch"
	);
	testRequire(
		!xrtExecutorSubmit(
			pExecutor,
			testExecutorRun,
			&Context,
			testExecutorDestroy,
			&Context
		) && (xrtErrorKind(xrtGetError()) == XERR_CLOSED) &&
		(xrtAtomic64Load(
			&Context.Destroyed,
			XMEMORY_ACQUIRE
		 ) == 64),
		"closed executor consumed rejected ownership"
	);
	testRequire(xrtExecutorDestroy(pExecutor), "executor destroy failed");

	/* 单 Worker 先被运行中工作占用，确保后续排队工作由 Cancel 丢弃。 */
	Config.Threads = 1;
	Config.QueueLimit = 8;
	xrtAtomic32Store(&Context.Gate, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.Started, 0, XMEMORY_RELEASE);
	xrtAtomic64Store(&Context.Executed, 0, XMEMORY_RELEASE);
	xrtAtomic64Store(&Context.Destroyed, 0, XMEMORY_RELEASE);
	pExecutor = xrtExecutorCreate(&Config);
	testRequire(pExecutor != NULL, "cancel executor create failed");
	testRequire(
		xrtExecutorSubmit(
			pExecutor,
			testExecutorBlock,
			&Context,
			testExecutorDestroy,
			&Context
		),
		"executor blocker submit failed"
	);
	while ( xrtAtomic32Load(
		&Context.Started,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	for ( size_t i = 0; i < 8; i++ ) {
		testRequire(
			xrtExecutorSubmit(
				pExecutor,
				testExecutorRun,
				&Context,
				testExecutorDestroy,
				&Context
			),
			"executor cancellation queue submit failed"
		);
	}
	testRequire(xrtExecutorCancel(pExecutor), "executor cancel failed");
	xrtAtomic32Store(&Context.Gate, 1, XMEMORY_RELEASE);
	testRequire(
		xrtExecutorWaitFor(pExecutor, UINT64_C(3000000)) == XWAIT_OK,
		"cancelled executor drain failed"
	);
	testRequire(
		xrtExecutorGet(pExecutor, &Stats) &&
		(Stats.Submitted == 9) &&
		(Stats.Completed == 9) &&
		(Stats.Executed == 1) &&
		(Stats.Cancelled == 8) &&
		(xrtAtomic64Load(
			&Context.Destroyed,
			XMEMORY_ACQUIRE
		 ) == 9),
		"executor cancellation ownership mismatch"
	);
	testRequire(xrtExecutorDestroy(pExecutor), "cancel executor destroy failed");
	printf("[PASS] high-throughput executor\n");
	return 0;
}
