#include "../test.h"



/* 记录共享 Ref 的唯一释放，并归还测试负载。 */
static void testWsGroupFutureRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	size_t* pReleased = (size_t*)pContext;

	(void)iSize;
	(*pReleased)++;
	free((ptr)pData);
}



/* 穷举所有前置分配失败点，验证失败不接管 Ref，成功只释放一次。 */
int main(void)
{
	bool bSucceeded = false;

	for ( size_t iFail = 0; iFail < 64u; iFail++ ) {
		xwsgroup* pGroup = xrtWsGroupCreate(0);
		xwsgroupop* pOperation;
		xnetref Ref;
		bytes pPayload = (bytes)malloc(1u);
		size_t iReleased = 0;

		testRequire(
			(pGroup != NULL) && (pPayload != NULL),
			"WebSocket group Future OOM setup failed"
		);
		pPayload[0] = UINT8_C(0x5A);
		Ref = (xnetref) {
			pPayload,
			1u,
			testWsGroupFutureRelease,
			&iReleased
		};
		testRequire(
			xrtMemDebugFailAfter(iFail),
			"WebSocket group Future OOM injection failed"
		);
		pOperation = xrtWsGroupBinaryRefAsync(pGroup, &Ref);
		if ( pOperation == NULL ) {
			testRequire(
				xrtMemDebugFailTriggered() &&
				(iReleased == 0) &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"WebSocket group Future OOM took caller Ref"
			);
			xrtMemDebugFailClear();
			xrtClearError();
			testWsGroupFutureRelease(
				Ref.Context,
				Ref.Data,
				Ref.Size
			);
		} else {
			testRequire(
				!xrtMemDebugFailTriggered() &&
				(iReleased == 1) &&
				(xrtWsGroupOpWaitFor(pOperation, 0) == XWAIT_OK),
				"WebSocket empty group did not take Ref exactly once"
			);
			xrtMemDebugFailClear();
			xrtWsGroupOpDestroy(pOperation);
			bSucceeded = true;
		}
		testRequire(
			iReleased == 1,
			"WebSocket group Future Ref release count mismatch"
		);
		xrtWsGroupDestroy(pGroup);
		testMemoryDebugDrain(
			"WebSocket group Future OOM leaked memory"
		);
		if ( bSucceeded ) {
			break;
		}
	}
	testRequire(
		bSucceeded,
		"WebSocket group Future OOM sweep never reached success"
	);
	return 0;
}
