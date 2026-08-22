#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件启动器保存 Promise，并把新 Future 返回给任务组。 */
typedef struct testsingletaskgroup {
	xpromise* Promise;
	xfuture* Future;
} testsingletaskgroup;



/* 同步创建一个由测试代码稍后完成的 Future。 */
static xfuture* testSingleTaskGroupStart(ptr pData)
{
	testsingletaskgroup* pContext = (testsingletaskgroup*)pData;

	pContext->Promise = xrtPromiseCreate(&pContext->Future, NULL);
	return pContext->Promise != NULL ? pContext->Future : NULL;
}



/* 验证单头文件包含独立裁剪的结构化任务组。 */
int main(void)
{
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	testsingletaskgroup tContext;
	xfuture* pSource;
	xtaskgroupstats tStats;
	int iResult = 1;

	memset(&tContext, 0, sizeof(tContext));
	pSource = pGroup != NULL ? xrtTaskGroupStart(
		pGroup,
		testSingleTaskGroupStart,
		&tContext
	) : NULL;
	if ( (pGroup != NULL) && (pSource != NULL) &&
		xrtPromiseResolve(tContext.Promise, NULL) &&
		(xrtTaskGroupWait(pGroup) == XWAIT_OK) &&
		xrtTaskGroupGet(pGroup, &tStats) &&
		(tStats.Succeeded == 1) ) {
		iResult = 0;
	}
	xrtPromiseDestroy(tContext.Promise);
	xrtFutureDestroy(pSource);
	xrtTaskGroupDestroy(pGroup);
	return iResult;
}
