#include <stdio.h>
#include <xrt.h>



/* 用取消令牌中止一个尚未就绪的 Channel Select。 */
int main(void)
{
	xchannel Channel;
	xchannelcase Case;
	xchannelselectresult Result;
	xcancel* pCancel;
	ptr pValue = NULL;
	int iResult = 1;

	if ( !xrtChannelInit(&Channel, 0) ) {
		return 1;
	}
	pCancel = xrtCancelCreate();
	if ( (pCancel != NULL) && xrtCancelRequest(pCancel) ) {
		Case = xrtChannelCaseRecv(&Channel, &pValue);
		Result = xrtChannelSelectUntilCancel(
			&Case,
			1u,
			XRT_DEADLINE_NEVER,
			pCancel
		);
		printf("select wait result: %d\n", (int)Result.Wait);
		iResult = (Result.Wait == XWAIT_CANCELLED) ? 0 : 2;
	}
	xrtCancelDestroy(pCancel);
	if ( !xrtChannelUnit(&Channel) && (iResult == 0) ) {
		iResult = 3;
	}
	return iResult;
}
