#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 Channel 取消路径。 */
int main(void)
{
	xchannel tChannel;
	xcancel* pCancel;
	ptr pItem = NULL;
	int iResult = 0;

	if ( !xrtChannelInit(&tChannel, 1u) ) {
		return 1;
	}
	pCancel = xrtCancelCreate();
	if ( pCancel == NULL ) {
		(void)xrtChannelUnit(&tChannel);
		return 2;
	}
	if ( !xrtCancelRequest(pCancel) ) {
		iResult = 3;
	} else if (
		xrtChannelRecvCancel(
			&tChannel,
			&pItem,
			pCancel
		) != XWAIT_CANCELLED
	) {
		iResult = 4;
	}
	xrtCancelDestroy(pCancel);
	if ( !xrtChannelUnit(&tChannel) && (iResult == 0) ) {
		iResult = 5;
	}
	return iResult;
}
