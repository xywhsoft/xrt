#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 Channel Select 基础路径。 */
int main(void)
{
	xchannel tChannel;
	xchannelcase tCase;
	xchannelselectresult tResult;
	ptr pItem = NULL;

	if ( !xrtChannelInit(&tChannel, 1u) ) {
		return 1;
	}
	if (
		xrtChannelTrySend(
			&tChannel,
			(ptr)(uintptr_t)17u
		) != XCHANNEL_OK
	) {
		return 2;
	}
	tCase = xrtChannelCaseRecv(&tChannel, &pItem);
	tResult = xrtChannelSelectTry(&tCase, 1u);
	if (
		(tResult.Wait != XWAIT_OK) ||
		(tResult.Index != 0) ||
		((uintptr_t)pItem != 17u)
	) {
		return 3;
	}
	return xrtChannelUnit(&tChannel) ? 0 : 4;
}
