#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 Channel 基础路径。 */
int main(void)
{
	xchannel tChannel;
	ptr pItem = NULL;

	if ( !xrtChannelInit(&tChannel, 1u) ) {
		return 1;
	}
	if ( xrtChannelTrySend(&tChannel, (ptr)(uintptr_t)17u) != XCHANNEL_OK ) {
		return 2;
	}
	if (
		(xrtChannelTryRecv(&tChannel, &pItem) != XCHANNEL_OK) ||
		((uintptr_t)pItem != 17u)
	) {
		return 3;
	}
	return xrtChannelUnit(&tChannel) ? 0 : 4;
}
