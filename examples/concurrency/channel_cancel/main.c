#include <stdio.h>

#include <xrt.h>



/* 演示用一个取消令牌中断无限 Channel 接收。 */
int main(void)
{
	xchannel tChannel;
	xcancel* pCancel;
	ptr pItem = NULL;
	xwaitresult iResult;

	if ( !xrtChannelInit(&tChannel, 1u) ) {
		return 1;
	}
	pCancel = xrtCancelCreate();
	if ( pCancel == NULL ) {
		(void)xrtChannelUnit(&tChannel);
		return 2;
	}
	if ( !xrtCancelRequest(pCancel) ) {
		xrtCancelDestroy(pCancel);
		(void)xrtChannelUnit(&tChannel);
		return 3;
	}

	iResult = xrtChannelRecvCancel(&tChannel, &pItem, pCancel);
	printf("cancelled: %s\n", iResult == XWAIT_CANCELLED ? "yes" : "no");

	xrtCancelDestroy(pCancel);
	if ( !xrtChannelUnit(&tChannel) ) {
		return 4;
	}
	return iResult == XWAIT_CANCELLED ? 0 : 5;
}
