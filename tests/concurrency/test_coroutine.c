#include "../test.h"
#include "../test_thread.h"

#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
	#define TEST_ROUND_DOWN 0x00000100u
	#define TEST_ROUND_UP 0x00000200u
	#define TEST_ROUND_ZERO 0x00000300u
	#define TEST_ROUND_MASK 0x00000300u
	unsigned int _controlfp(unsigned int iNew, unsigned int iMask);

	/* TinyCC Windows 读取 CRT 浮点舍入控制位。 */
	static int testCoroRoundGet(void)
	{
		return (int)(_controlfp(0, 0) & TEST_ROUND_MASK);
	}



	/* TinyCC Windows 更新 CRT 浮点舍入控制位。 */
	static int testCoroRoundSet(int iRound)
	{
		(void)_controlfp((unsigned int)iRound, TEST_ROUND_MASK);
		return 0;
	}
#else
	#include <fenv.h>
	#define TEST_ROUND_DOWN FE_DOWNWARD
	#define TEST_ROUND_UP FE_UPWARD
	#define TEST_ROUND_ZERO FE_TOWARDZERO
	#define testCoroRoundGet fegetround
	#define testCoroRoundSet fesetround
#endif



/* 基础切换用例记录当前协程、临时内存和恢复次数。 */
typedef struct testcorobasic {
	xcoro* Current;
	int Step;
	int Result;
	bool TempPreserved;
	bool StackPreserved;
} testcorobasic;



/* 验证两次让出、当前对象和协程私有临时内存。 */
static ptr testCoroBasicProc(ptr pData)
{
	testcorobasic* pState = (testcorobasic*)pData;
	char* sTemp = (char*)xrtTemp(16);
	volatile uint8 StackData[128];
	size_t i;

	pState->Current = xrtCoCurrent();
	pState->Step = 1;
	memcpy(sTemp, "private", 8);
	for ( i = 0; i < sizeof(StackData); i++ ) {
		StackData[i] = (uint8)(i ^ 0x5Au);
	}
	testRequire(xrtCoYield() == XWAIT_OK, "coroutine first yield failed");
	pState->TempPreserved = memcmp(sTemp, "private", 8) == 0;
	pState->StackPreserved = true;
	for ( i = 0; i < sizeof(StackData); i++ ) {
		if ( StackData[i] != (uint8)(i ^ 0x5Au) ) {
			pState->StackPreserved = false;
			break;
		}
	}
	pState->Step = 2;
	testRequire(xrtCoYield() == XWAIT_OK, "coroutine second yield failed");
	pState->Step = 3;
	pState->Result = 73;
	return &pState->Result;
}



/* 错误隔离用例在协程上下文中留下未处理错误。 */
static ptr testCoroErrorProc(ptr pData)
{
	xerror* pError = xrtErrorCreate(XERR_IO, "test.coroutine", 19, "coroutine error");

	(void)pData;
	testRequire(pError != NULL, "coroutine error allocation failed");
	xrtSetError(pError);
	xrtErrorFree(pError);
	testRequire(xrtCoYield() == XWAIT_OK, "error coroutine yield failed");
	testRequire(xrtErrorCode(xrtGetError()) == 19, "coroutine error context was not preserved");
	return NULL;
}



/* 浮点环境用例记录创建时继承和恢复后的舍入模式。 */
typedef struct testcorofenv {
	int Initial;
	int Resumed;
} testcorofenv;



/* 修改协程舍入模式并验证让出后能够恢复。 */
static ptr testCoroFenvProc(ptr pData)
{
	testcorofenv* pState = (testcorofenv*)pData;

	pState->Initial = testCoroRoundGet();
	testRequire(testCoroRoundSet(TEST_ROUND_UP) == 0, "coroutine rounding mode update failed");
	testRequire(xrtCoYield() == XWAIT_OK, "floating environment yield failed");
	pState->Resumed = testCoroRoundGet();
	return pState;
}



typedef struct testcorocleanup testcorocleanup;



/* 单个清理参数同时携带结果对象和顺序标识。 */
typedef struct testcorocleanupitem {
	testcorocleanup* State;
	int Value;
} testcorocleanupitem;



/* 清理测试保存执行顺序和覆盖终态的调用方节点。 */
struct testcorocleanup {
	int Values[4];
	int Count;
	xcocleanup First;
	xcocleanup Second;
	testcorocleanupitem Items[3];
};



/* 将清理标识追加到结果数组。 */
static void testCoroCleanupRecord(ptr pData)
{
	testcorocleanupitem* pItem = (testcorocleanupitem*)pData;

	pItem->State->Values[pItem->State->Count++] = pItem->Value;
}



/* 验证无分配清理节点严格后进先出，并支持提前弹出执行。 */
static ptr testCoroCleanupProc(ptr pData)
{
	testcorocleanup* pState = (testcorocleanup*)pData;
	xcocleanup* pThird;

	pState->Items[0].State = pState;
	pState->Items[0].Value = 1;
	pState->Items[1].State = pState;
	pState->Items[1].Value = 2;
	pState->Items[2].State = pState;
	pState->Items[2].Value = 3;
	testRequire(xrtCoCleanupPush(&pState->First, testCoroCleanupRecord, &pState->Items[0]), "first cleanup push failed");
	testRequire(xrtCoCleanupPush(&pState->Second, testCoroCleanupRecord, &pState->Items[1]), "second cleanup push failed");
	testRequire(xrtCoCleanupPop(&pState->Second, true), "second cleanup pop failed");
	pThird = xrtCoDefer(testCoroCleanupRecord, &pState->Items[2]);
	testRequire(pThird != NULL, "managed cleanup defer failed");
	return NULL;
}



/* 取消测试记录过程是否实际启动以及恢复后的等待结果。 */
typedef struct testcorocancel {
	int Started;
	xwaitresult Wait;
	int Result;
	bool Confirm;
} testcorocancel;



/* 让出后观察协作取消。 */
static ptr testCoroCancelProc(ptr pData)
{
	testcorocancel* pState = (testcorocancel*)pData;

	pState->Started++;
	pState->Wait = xrtCoYield();
	if ( pState->Confirm && (pState->Wait == XWAIT_CANCELLED) ) {
		testRequire(xrtCoConfirmCancel(),
			"coroutine cancellation confirmation failed");
	}
	pState->Result = 83;
	return &pState->Result;
}



/* 深栈递归阻止编译器消除栈帧，并校验每层局部数据。 */
static int testCoroDeepStack(int iDepth, volatile int* pSum)
{
	volatile uint8 Data[256];

	memset((ptr)Data, iDepth & 0xFF, sizeof(Data));
	*pSum += Data[(size_t)iDepth & 0xFFu];
	if ( iDepth == 0 ) {
		return *pSum;
	}
	return testCoroDeepStack(iDepth - 1, pSum) + Data[0];
}



/* 在自定义栈上执行深递归。 */
static ptr testCoroDeepProc(ptr pData)
{
	volatile int* pSum = (volatile int*)pData;

	(void)testCoroDeepStack(80, pSum);
	return pData;
}



/* 跨线程误用用例验证所有权边界。 */
typedef struct testcoroowner {
	xcoro* Co;
	bool Resume;
	bool Destroy;
	xerrkind Error;
} testcoroowner;



/* 从非所属线程尝试恢复和销毁协程。 */
static int testCoroWrongOwner(ptr pData)
{
	testcoroowner* pState = (testcoroowner*)pData;

	pState->Resume = xrtCoResume(pState->Co);
	pState->Destroy = xrtCoDestroy(pState->Co);
	pState->Error = xrtErrorKind(xrtGetError());
	return 0;
}



/* 验证基础状态机、结果和宿主错误隔离。 */
static void testCoroutineCore(void)
{
	testcorobasic tState;
	testcorofenv tFenv;
	xcoro* pCo;
	xerror* pHostError;
	int iOriginalRound;

	memset(&tState, 0, sizeof(tState));
	pCo = xrtCoCreate(testCoroBasicProc, &tState, NULL);
	testRequire(pCo != NULL, "coroutine create failed");
	testRequire(xrtCoState(pCo) == XCORO_READY, "coroutine initial state mismatch");
	testRequire(xrtCoResume(pCo), "coroutine first resume failed");
	testRequire(tState.Step == 1, "coroutine first step mismatch");
	testRequire(tState.Current == pCo, "current coroutine mismatch");
	testRequire(xrtCoState(pCo) == XCORO_SUSPENDED, "coroutine suspended state mismatch");
	testRequire(xrtCoResume(pCo), "coroutine second resume failed");
	testRequire(tState.Step == 2, "coroutine second step mismatch");
	testRequire(tState.TempPreserved, "coroutine temporary memory was not preserved");
	testRequire(
		tState.StackPreserved,
		"coroutine stack memory was not preserved"
	);
	testRequire(xrtCoResume(pCo), "coroutine final resume failed");
	testRequire(xrtCoState(pCo) == XCORO_DONE, "coroutine final state mismatch");
	testRequire(xrtCoTerm(pCo) == XCORO_TERM_RETURNED, "coroutine term mismatch");
	testRequire(xrtCoResult(pCo) == &tState.Result, "coroutine result mismatch");
	testRequire(xrtCoDestroy(pCo), "coroutine destroy failed");

	pHostError = xrtErrorCreate(XERR_STATE, "test.host", 7, "host error");
	testRequire(pHostError != NULL, "host error allocation failed");
	xrtSetError(pHostError);
	xrtErrorFree(pHostError);
	pCo = xrtCoCreate(testCoroErrorProc, NULL, NULL);
	testRequire(pCo != NULL, "error coroutine create failed");
	testRequire(xrtCoResume(pCo), "error coroutine first resume failed");
	testRequire(xrtErrorCode(xrtGetError()) == 7, "host error was replaced by coroutine error");
	testRequire(xrtCoResume(pCo), "error coroutine final resume failed");
	testRequire(xrtCoTerm(pCo) == XCORO_TERM_ERROR, "error coroutine term mismatch");
	testRequire(xrtErrorCode(xrtCoError(pCo)) == 19, "coroutine structured error mismatch");
	testRequire(xrtErrorCode(xrtGetError()) == 7, "host error was not restored");
	testRequire(xrtCoDestroy(pCo), "error coroutine destroy failed");
	xrtClearError();

	iOriginalRound = testCoroRoundGet();
	testRequire(testCoroRoundSet(TEST_ROUND_DOWN) == 0, "host rounding mode setup failed");
	memset(&tFenv, 0, sizeof(tFenv));
	pCo = xrtCoCreate(testCoroFenvProc, &tFenv, NULL);
	testRequire(pCo != NULL, "floating environment coroutine create failed");
	testRequire(xrtCoResume(pCo), "floating environment first resume failed");
	testRequire(tFenv.Initial == TEST_ROUND_DOWN, "coroutine did not inherit floating environment");
	testRequire(testCoroRoundGet() == TEST_ROUND_DOWN, "coroutine changed host floating environment");
	testRequire(testCoroRoundSet(TEST_ROUND_ZERO) == 0, "host rounding mode replacement failed");
	testRequire(xrtCoResume(pCo), "floating environment final resume failed");
	testRequire(tFenv.Resumed == TEST_ROUND_UP, "coroutine floating environment was not restored");
	testRequire(testCoroRoundGet() == TEST_ROUND_ZERO, "host floating environment was not restored");
	testRequire(xrtCoDestroy(pCo), "floating environment coroutine destroy failed");
	testRequire(testCoroRoundSet(iOriginalRound) == 0, "host rounding mode restore failed");
}



/* 验证取消、清理、深栈和严格生命周期。 */
static void testCoroutineBoundaries(void)
{
	testcorocleanup tCleanup;
	testcorocancel tCancel;
	xcoroargs tArgs;
	xcancel* pParent;
	xcancel* pToken;
	xcoro* pCo;
	volatile int iSum = 0;

	memset(&tCleanup, 0, sizeof(tCleanup));
	pCo = xrtCoCreate(testCoroCleanupProc, &tCleanup, NULL);
	testRequire(pCo != NULL, "cleanup coroutine create failed");
	testRequire(xrtCoResume(pCo), "cleanup coroutine resume failed");
	testRequire(tCleanup.Count == 3, "cleanup callback count mismatch");
	testRequire(
		(tCleanup.Values[0] == 2) && (tCleanup.Values[1] == 3) && (tCleanup.Values[2] == 1),
		"cleanup callback order mismatch"
	);
	testRequire(xrtCoDestroy(pCo), "cleanup coroutine destroy failed");

	memset(&tCancel, 0, sizeof(tCancel));
	pCo = xrtCoCreate(testCoroCancelProc, &tCancel, NULL);
	testRequire(pCo != NULL, "cancel coroutine create failed");
	testRequire(xrtCoResume(pCo), "cancel coroutine first resume failed");
	xrtClearError();
	testRequire(!xrtCoDestroy(pCo), "suspended coroutine destroy succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "suspended destroy error mismatch");
	testRequire(xrtCoCancel(pCo), "coroutine cancel failed");
	testRequire(xrtCoResume(pCo), "cancel coroutine final resume failed");
	testRequire(tCancel.Wait == XWAIT_CANCELLED, "yield did not observe cancellation");
	testRequire((xrtCoTerm(pCo) == XCORO_TERM_RETURNED) &&
		(xrtCoResult(pCo) == &tCancel.Result),
		"handled cancellation did not preserve normal return");
	testRequire(xrtCoDestroy(pCo), "cancel coroutine destroy failed");

	memset(&tCancel, 0, sizeof(tCancel));
	tCancel.Confirm = true;
	pCo = xrtCoCreate(testCoroCancelProc, &tCancel, NULL);
	testRequire(pCo != NULL, "confirmed cancel coroutine create failed");
	testRequire(xrtCoResume(pCo),
		"confirmed cancel coroutine first resume failed");
	testRequire(xrtCoCancel(pCo),
		"confirmed cancel coroutine request failed");
	testRequire(xrtCoResume(pCo),
		"confirmed cancel coroutine final resume failed");
	testRequire((tCancel.Wait == XWAIT_CANCELLED) &&
		(xrtCoTerm(pCo) == XCORO_TERM_CANCELLED) &&
		(xrtCoResult(pCo) == NULL),
		"confirmed coroutine cancellation term mismatch");
	testRequire(xrtCoDestroy(pCo),
		"confirmed cancel coroutine destroy failed");

	pParent = xrtCancelCreate();
	testRequire(pParent != NULL, "coroutine parent cancel create failed");
	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Cancel = pParent;
	memset(&tCancel, 0, sizeof(tCancel));
	pCo = xrtCoCreate(testCoroCancelProc, &tCancel, &tArgs);
	testRequire(pCo != NULL, "parented coroutine create failed");
	testRequire(xrtCancelRequest(pParent), "parent cancellation failed");
	testRequire(xrtCoResume(pCo), "pre-cancelled coroutine resume failed");
	testRequire(tCancel.Started == 0, "pre-cancelled coroutine executed user code");
	testRequire(xrtCoTerm(pCo) == XCORO_TERM_CANCELLED, "pre-cancelled term mismatch");
	testRequire(xrtCoDestroy(pCo), "pre-cancelled coroutine destroy failed");
	xrtCancelDestroy(pParent);

	pCo = xrtCoCreate(testCoroDeepProc, (ptr)&iSum, NULL);
	testRequire(pCo != NULL, "cancel token coroutine create failed");
	pToken = xrtCoCancelToken(pCo);
	testRequire(pToken != NULL, "coroutine cancel token reference failed");
	testRequire(xrtCoDestroy(pCo), "cancel token coroutine destroy failed");
	testRequire(
		xrtCancelRequest(pToken) && xrtCancelRequested(pToken),
		"referenced coroutine cancel token did not survive coroutine destruction"
	);
	xrtCancelDestroy(pToken);

	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.StackSize = XRT_CORO_STACK_MIN - 1;
	xrtClearError();
	testRequire(
		xrtCoCreate(testCoroDeepProc, (ptr)&iSum, &tArgs) == NULL,
		"below-minimum coroutine stack was accepted"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"below-minimum coroutine stack error mismatch"
	);

	tArgs.StackSize = XRT_CORO_STACK_MAX + 1;
	xrtClearError();
	testRequire(
		xrtCoCreate(testCoroDeepProc, (ptr)&iSum, &tArgs) == NULL,
		"above-maximum coroutine stack was accepted"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"above-maximum coroutine stack error mismatch"
	);
	xrtClearError();

	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.StackSize = 128u * 1024u;
	pCo = xrtCoCreate(testCoroDeepProc, (ptr)&iSum, &tArgs);
	testRequire(pCo != NULL, "deep stack coroutine create failed");
	testRequire(xrtCoResume(pCo), "deep stack coroutine resume failed");
	testRequire((iSum != 0) && (xrtCoResult(pCo) == (ptr)&iSum), "deep stack result mismatch");
	testRequire(xrtCoDestroy(pCo), "deep stack coroutine destroy failed");
}



/* 验证跨线程所有权和无当前协程错误。 */
static void testCoroutineOwnership(void)
{
	testcorobasic tBasic;
	testcoroowner tOwner;
	testthread tThread;
	xcoro* pCo;

	memset(&tBasic, 0, sizeof(tBasic));
	memset(&tOwner, 0, sizeof(tOwner));
	pCo = xrtCoCreate(testCoroBasicProc, &tBasic, NULL);
	testRequire(pCo != NULL, "owner coroutine create failed");
	tOwner.Co = pCo;
	tThread.Proc = testCoroWrongOwner;
	tThread.Data = &tOwner;
	testThreadsStart(&tThread, 1);
	testThreadsJoin(&tThread, 1);
	testRequire(!tOwner.Resume && !tOwner.Destroy, "cross-thread coroutine operation succeeded");
	testRequire(tOwner.Error == XERR_STATE, "cross-thread coroutine error mismatch");
	testRequire(xrtCoDestroy(pCo), "owner coroutine destroy failed");

	xrtClearError();
	testRequire(xrtCoYield() == XWAIT_ERROR, "yield outside coroutine succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "outside yield error mismatch");
	testRequire(xrtCoCurrent() == NULL, "current coroutine outside execution was not null");
	testRequire(strlen(xrtCoBackend()) != 0, "coroutine backend name was empty");
	xrtClearError();
}



/* 终结测试记录清理顺序、最终终态和可见结果。 */
typedef struct testcorofinal {
	int Started;
	int Cleaned;
	int Finalized;
	int ErrorCode;
	int ErrorCodeAfterClear;
	bool Restrict;
	xwaitresult Yield;
	xerrkind YieldError;
	xcocleanup* Defer;
	xerrkind DeferError;
	bool Confirm;
	xerrkind ConfirmError;
	bool CleanupPush;
	xerrkind CleanupError;
	xcoroterm Term;
	ptr Result;
	const xerror* Error;
	xcocleanup Cleanup;
} testcorofinal;



/* 协程清理栈必须先于创建参数中的终结过程执行。 */
static void testCoroFinalCleanup(ptr pData)
{
	testcorofinal* pContext = (testcorofinal*)pData;

	pContext->Cleaned++;
}



/* 正常协程注册栈上清理节点并返回上下文。 */
static ptr testCoroFinalRun(ptr pData)
{
	testcorofinal* pContext = (testcorofinal*)pData;

	pContext->Started++;
	testRequire(
		xrtCoCleanupPush(&pContext->Cleanup, testCoroFinalCleanup, pContext),
		"coroutine final cleanup push failed"
	);
	if ( pContext->Restrict ) {
		testRequire(
			xrtCoCancel(xrtCoCurrent()),
			"coroutine finalizer restriction cancel setup failed"
		);
	}
	return pContext;
}



/* 构造一个未处理错误，验证终结器不能破坏终态错误快照。 */
static ptr testCoroFinalErrorRun(ptr pData)
{
	testcorofinal* pContext = (testcorofinal*)pData;
	xerror* pError = xrtErrorCreate(
		XERR_STATE,
		"test.coroutine",
		41,
		"terminal coroutine error"
	);

	testRequire(pError != NULL, "terminal coroutine error allocation failed");
	pContext->Started++;
	testRequire(
		xrtCoCleanupPush(&pContext->Cleanup, testCoroFinalCleanup, pContext),
		"terminal error cleanup push failed"
	);
	xrtSetError(pError);
	xrtErrorFree(pError);
	return pData;
}



/* 终结过程接收清理完成后的不可变终态快照。 */
static void testCoroFinalProc(
	xcoroterm Term,
	ptr pResult,
	const xerror* pError,
	ptr pData
)
{
	testcorofinal* pContext = (testcorofinal*)pData;
	xcocleanup tCleanup;

	pContext->Finalized++;
	pContext->Term = Term;
	pContext->Result = pResult;
	pContext->Error = pError;
	if ( Term == XCORO_TERM_ERROR ) {
		xerror* pFinalizeError;

		pContext->ErrorCode = xrtErrorCode(pError);
		xrtClearError();
		pContext->ErrorCodeAfterClear = xrtErrorCode(pError);
		pFinalizeError = xrtErrorCreate(
			XERR_VALUE,
			"test.finalizer",
			42,
			"finalizer local error"
		);
		testRequire(pFinalizeError != NULL, "finalizer local error allocation failed");
		xrtSetError(pFinalizeError);
		xrtErrorFree(pFinalizeError);
	}
	testRequire(
		(Term == XCORO_TERM_CANCELLED) || (pContext->Cleaned == 1),
		"coroutine finalizer ran before cleanup stack"
	);
	if ( !pContext->Restrict ) {
		return;
	}

	xrtClearError();
	pContext->Yield = xrtCoYield();
	pContext->YieldError = xrtErrorKind(xrtGetError());

	xrtClearError();
	pContext->Defer = xrtCoDefer(testCoroFinalCleanup, pContext);
	pContext->DeferError = xrtErrorKind(xrtGetError());

	xrtClearError();
	pContext->Confirm = xrtCoConfirmCancel();
	pContext->ConfirmError = xrtErrorKind(xrtGetError());

	memset(&tCleanup, 0, sizeof(tCleanup));
	xrtClearError();
	pContext->CleanupPush = xrtCoCleanupPush(
		&tCleanup,
		testCoroFinalCleanup,
		pContext
	);
	pContext->CleanupError = xrtErrorKind(xrtGetError());
	xrtClearError();
}



/* 验证正常返回和首次运行前取消都会执行一次终结过程。 */
static void testCoroutineFinalizer(void)
{
	testcorofinal tContext;
	xcoroargs tArgs;
	xcancel* pParent;
	xcoro* pCo;
	xerror* pHostError;

	memset(&tContext, 0, sizeof(tContext));
	tContext.Restrict = true;
	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Finalize = testCoroFinalProc;
	tArgs.FinalizeData = &tContext;
	pCo = xrtCoCreate(testCoroFinalRun, &tContext, &tArgs);
	testRequire(pCo != NULL, "finalized coroutine create failed");
	testRequire(xrtCoResume(pCo), "finalized coroutine resume failed");
	testRequire(
		(tContext.Started == 1) && (tContext.Cleaned == 1) &&
		(tContext.Finalized == 1) &&
		(tContext.Term == XCORO_TERM_RETURNED) &&
		(tContext.Result == &tContext) && (tContext.Error == NULL),
		"normal coroutine finalizer snapshot mismatch"
	);
	testRequire(
		(tContext.Yield == XWAIT_ERROR) &&
		(tContext.YieldError == XERR_STATE) &&
		(tContext.Defer == NULL) &&
		(tContext.DeferError == XERR_STATE) &&
		!tContext.Confirm &&
		(tContext.ConfirmError == XERR_STATE) &&
		!tContext.CleanupPush &&
		(tContext.CleanupError == XERR_STATE),
		"coroutine finalizer accepted a restricted operation"
	);
	testRequire(xrtCoDestroy(pCo), "finalized coroutine destroy failed");
	testRequire(tContext.Finalized == 1, "coroutine finalizer ran more than once");

	memset(&tContext, 0, sizeof(tContext));
	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Finalize = testCoroFinalProc;
	tArgs.FinalizeData = &tContext;
	pCo = xrtCoCreate(testCoroFinalErrorRun, &tContext, &tArgs);
	testRequire(pCo != NULL, "error finalized coroutine create failed");
	testRequire(xrtCoResume(pCo), "error finalized coroutine resume failed");
	testRequire(
		(tContext.Finalized == 1) &&
		(tContext.Term == XCORO_TERM_ERROR) &&
		(tContext.ErrorCode == 41) &&
		(tContext.ErrorCodeAfterClear == 41) &&
		(xrtErrorCode(xrtCoError(pCo)) == 41),
		"coroutine finalizer changed the terminal error"
	);
	testRequire(
		xrtGetError() == NULL,
		"coroutine finalizer leaked its private error to the host"
	);
	testRequire(xrtCoDestroy(pCo), "error finalized coroutine destroy failed");

	memset(&tContext, 0, sizeof(tContext));
	pParent = xrtCancelCreate();
	testRequire(pParent != NULL, "pre-cancel finalizer parent create failed");
	pHostError = xrtErrorCreate(
		XERR_STATE,
		"test.host",
		31,
		"host finalizer error"
	);
	testRequire(pHostError != NULL, "pre-cancel host error allocation failed");
	xrtSetError(pHostError);
	xrtErrorFree(pHostError);
	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Cancel = pParent;
	tArgs.Finalize = testCoroFinalProc;
	tArgs.FinalizeData = &tContext;
	pCo = xrtCoCreate(testCoroFinalRun, &tContext, &tArgs);
	testRequire(pCo != NULL, "pre-cancel finalized coroutine create failed");
	testRequire(xrtCancelRequest(pParent), "pre-cancel finalizer request failed");
	testRequire(xrtCoResume(pCo), "pre-cancel finalized coroutine resume failed");
	testRequire(
		(tContext.Started == 0) && (tContext.Cleaned == 0) &&
		(tContext.Finalized == 1) &&
		(tContext.Term == XCORO_TERM_CANCELLED) &&
		(tContext.Result == NULL) && (tContext.Error == NULL),
		"pre-cancel coroutine finalizer snapshot mismatch"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == 31,
		"pre-cancel finalizer replaced the host error context"
	);
	testRequire(xrtCoDestroy(pCo), "pre-cancel finalized coroutine destroy failed");
	xrtCancelDestroy(pParent);
	xrtClearError();
}



/* 运行协程核心的状态、上下文和安全边界测试。 */
int main(void)
{
	testCoroutineCore();
	testCoroutineBoundaries();
	testCoroutineOwnership();
	testCoroutineFinalizer();
	testRequire(xrtCoThreadDetach(), "coroutine runtime detach failed");

	printf("[PASS] coroutine\n");
	return 0;
}
