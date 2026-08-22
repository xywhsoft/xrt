#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的可取消 Channel Select。 */
int main(void)
{
	xchannel tChannel;
	xcancel* pCancel;
	xchannelcase tCase;
	xchannelselectresult tResult;
	ptr pItem = NULL;
	int iResult = 0;

	if ( !xrtChannelInit(&tChannel, 0) ) {
		return 1;
	}
	pCancel = xrtCancelCreate();
	if ( pCancel == NULL ) {
		(void)xrtChannelUnit(&tChannel);
		return 2;
	}
	if ( !xrtCancelRequest(pCancel) ) {
		iResult = 3;
	} else {
		tCase = xrtChannelCaseRecv(&tChannel, &pItem);
		tResult = xrtChannelSelectUntilCancel(
			&tCase,
			1u,
			XRT_DEADLINE_NEVER,
			pCancel
		);
		if ( tResult.Wait != XWAIT_CANCELLED ) {
			iResult = 4;
		}
	}
	xrtCancelDestroy(pCancel);
	if ( !xrtChannelUnit(&tChannel) && (iResult == 0) ) {
		iResult = 5;
	}
	return iResult;
}
