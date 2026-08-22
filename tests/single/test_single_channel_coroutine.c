#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件接收协程等待一个由发送协程提交的值。 */
static ptr testSingleChannelRecv(ptr pData)
{
	ptr pItem = NULL;

	if (
		xrtChannelRecvAwait((xchannel*)pData, &pItem) !=
		XWAIT_OK
	) {
		return NULL;
	}
	return pItem;
}



/* 单头文件发送协程向同步 Channel 提交一个值。 */
static ptr testSingleChannelSend(ptr pData)
{
	xchannel* pChannel = (xchannel*)pData;

	return xrtChannelSendAwait(
		pChannel,
		(ptr)(uintptr_t)61u
	) == XWAIT_OK ? pData : NULL;
}



/* 验证单头文件接通 Channel 与协程调度器。 */
int main(void)
{
	xchannel tChannel;
	xcosched* pSched;
	xcoro* pRecv;
	xcoro* pSend;
	int iResult = 1;

	if ( !xrtChannelInit(&tChannel, 0) ) {
		return 1;
	}
	pSched = xrtCoSchedCreate();
	if ( pSched != NULL ) {
		pRecv = xrtCoSpawn(
			pSched,
			testSingleChannelRecv,
			&tChannel,
			NULL
		);
		pSend = xrtCoSpawn(
			pSched,
			testSingleChannelSend,
			&tChannel,
			NULL
		);
		if (
			(pRecv != NULL) &&
			(pSend != NULL) &&
			xrtCoSchedRun(pSched) &&
			((uintptr_t)xrtCoResult(pRecv) == 61u) &&
			(xrtCoResult(pSend) == &tChannel)
		) {
			iResult = 0;
		}
		xrtCoDestroy(pRecv);
		xrtCoDestroy(pSend);
	}
	xrtCoSchedDestroy(pSched);
	xrtCoThreadDetach();
	xrtChannelUnit(&tChannel);
	return iResult;
}
